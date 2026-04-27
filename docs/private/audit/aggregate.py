#!/usr/bin/env python3
"""
Pre-triage aggregator for RetroArch /audit runs.

Reads raw findings from cppcheck XML / semgrep JSON / clang-tidy YAML / ruff JSON
or pre-normalized JSON, applies the scope and known-FP drop rules from
docs/private/audit/scope.txt + suppressions.md, and groups the survivors by
(rule_id, top-level directory). One representative finding per group is
written to stdout in markdown by default (--format=json for machine-readable).

The point: when a tool emits 200+ raw findings, piping all of them to the
triage subagent burns ~10x the input tokens of the same set aggregated by
rule x dir. The triage agent doesn't need 50 instances of "deref-before-check
in menu/" — it needs to see the class once with a count and pick a
representative.

Usage:
    aggregate.py --from-cppcheck  cppcheck-out.xml
    aggregate.py --from-semgrep   semgrep-out.json
    aggregate.py --from-ruff      ruff-out.json
    aggregate.py --from-normalized findings.json
    aggregate.py --from-cppcheck cppcheck-out.xml --format=json > triage-input.json
    aggregate.py --from-cppcheck cppcheck-out.xml --no-drop-known   # show drops too

Exit status:
    0 — aggregation succeeded
    1 — input parse error
    2 — bad arguments
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent
SCOPE_FILE = PROJECT_ROOT / "docs/private/audit/scope.txt"


@dataclass
class Finding:
    tool: str
    rule_id: str
    severity: str
    file: str
    line: int
    message: str
    raw: dict = field(default_factory=dict)

    @property
    def top_dir(self) -> str:
        parts = Path(self.file).parts
        if not parts:
            return "<root>"
        if parts[0] in ("input", "menu", "gfx", "audio", "network", "tasks"):
            return f"{parts[0]}/{parts[1]}" if len(parts) > 1 and not parts[1].endswith((".c", ".cpp", ".h", ".hpp")) else parts[0]
        return parts[0]


# ----------------------------------------------------------------------------
# Scope + known-FP drop rules
# ----------------------------------------------------------------------------

def load_scope_excludes() -> list[str]:
    if not SCOPE_FILE.exists():
        return []
    out = []
    for line in SCOPE_FILE.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            out.append(line.rstrip("/"))
    return out


# Static-analysis FP classes documented in suppressions.md. Each rule returns
# True when a finding should be dropped pre-triage.
KNOWN_FP_RULES = [
    # vendored / out-of-scope
    lambda f: f.file.startswith(("libretro-common/", "deps/")),
    # console-only
    lambda f: f.file.startswith(("ctr/", "vita/", "wii/", "wiiu/", "dingux/",
                                  "uwp/", "webos/", "emscripten/", "steam/")),
    # cppcheck S12 — JNI missingReturn (also covered by .cppcheck-suppress.txt;
    # this rule is defence-in-depth in case cppcheck is invoked without the
    # --suppressions-list flag).
    lambda f: (f.tool == "cppcheck" and f.rule_id == "missingReturn"
               and f.file.startswith("play_feature_delivery/")),
    # cppcheck S11 strip closed in Bundle 34 — pre-triage drop rule retired.
    # A regression re-introducing the inner-NULL-check pattern should now
    # surface in triage rather than be silently dropped.
    # cppcheck — gfx_thumbnail stack-array FP
    lambda f: (f.tool == "cppcheck"
               and f.file.endswith("gfx/gfx_thumbnail.c")),
    # clang-analyzer driver-table NULL-terminator FP
    lambda f: (f.tool == "clang-tidy"
               and "ArraySubscript" in f.rule_id
               and ((f.file.endswith("retroarch.c") and f.line in (435, 1186, 2270))
                    or f.file.endswith("tasks/task_translation.c"))),
    # clang-tidy push_entry ownership-move FP
    lambda f: (f.tool == "clang-tidy"
               and f.file.endswith("core_updater_list.c") and f.line == 928),
    # semgrep S9 — double-free cluster
    lambda f: (f.tool == "semgrep"
               and "double-free" in f.rule_id
               and any(f.file.startswith(p) for p in (
                   "network/cloud_sync/s3.c",
                   "tasks/task_database_cue.c",
                   "gfx/drivers/d3d9cg.c",
                   "gfx/drivers_font/bitmapfont_",
                   "tasks/task_save.c"))),
]


def is_in_scope(f: Finding, scope_excludes: list[str]) -> bool:
    return not any(f.file.startswith(p + "/") or f.file == p
                   for p in scope_excludes)


def is_known_fp(f: Finding) -> bool:
    return any(rule(f) for rule in KNOWN_FP_RULES)


# ----------------------------------------------------------------------------
# Parsers
# ----------------------------------------------------------------------------

def parse_cppcheck(path: Path) -> list[Finding]:
    """Parse cppcheck --xml --xml-version=2 output."""
    out = []
    tree = ET.parse(path)
    root = tree.getroot()
    for err in root.iter("error"):
        rule_id = err.get("id", "?")
        severity = err.get("severity", "info")
        msg = err.get("msg", "")
        for loc in err.iter("location"):
            file = loc.get("file", "?")
            line = int(loc.get("line", "0"))
            out.append(Finding("cppcheck", rule_id, severity, file, line, msg,
                                raw={"id": rule_id, "msg": msg}))
            break  # one location per finding (the primary)
    return out


def parse_semgrep(path: Path) -> list[Finding]:
    """Parse `semgrep scan --json` output."""
    data = json.loads(path.read_text())
    out = []
    for r in data.get("results", []):
        rule_id = r.get("check_id", "?")
        severity = r.get("extra", {}).get("severity", "INFO").lower()
        msg = r.get("extra", {}).get("message", "")
        file = r.get("path", "?")
        line = r.get("start", {}).get("line", 0)
        out.append(Finding("semgrep", rule_id, severity, file, line, msg,
                            raw=r))
    return out


def parse_ruff(path: Path) -> list[Finding]:
    """Parse `ruff check --output-format=json` output."""
    data = json.loads(path.read_text())
    out = []
    for r in data:
        rule_id = r.get("code", "?")
        msg = r.get("message", "")
        file = r.get("filename", "?")
        # ruff emits absolute paths; trim to repo-relative
        try:
            file = str(Path(file).relative_to(PROJECT_ROOT))
        except ValueError:
            pass
        line = r.get("location", {}).get("row", 0)
        out.append(Finding("ruff", rule_id, "warning", file, line, msg, raw=r))
    return out


def parse_normalized(path: Path) -> list[Finding]:
    """Parse a list of {tool, rule_id, severity, file, line, message} dicts."""
    data = json.loads(path.read_text())
    out = []
    for r in data:
        out.append(Finding(
            tool=r.get("tool", "unknown"),
            rule_id=r.get("rule_id", "?"),
            severity=r.get("severity", "info"),
            file=r.get("file", "?"),
            line=int(r.get("line", 0)),
            message=r.get("message", ""),
            raw=r,
        ))
    return out


# ----------------------------------------------------------------------------
# Aggregation
# ----------------------------------------------------------------------------

def aggregate(findings: list[Finding]) -> dict[tuple[str, str, str], list[Finding]]:
    """Group by (tool, rule_id, top_dir)."""
    groups: dict[tuple[str, str, str], list[Finding]] = defaultdict(list)
    for f in findings:
        groups[(f.tool, f.rule_id, f.top_dir)].append(f)
    return groups


def rep_finding(group: list[Finding]) -> Finding:
    """Pick a representative finding for a group (highest severity, earliest line)."""
    sev_rank = {"critical": 0, "error": 1, "high": 1, "warning": 2,
                "medium": 2, "style": 3, "low": 3, "info": 4, "performance": 4,
                "portability": 4}
    return min(group, key=lambda f: (sev_rank.get(f.severity.lower(), 5), f.line))


# ----------------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------------

def render_markdown(groups: dict, dropped_count: int, scope_excluded_count: int,
                    raw_count: int) -> str:
    lines = []
    lines.append(f"# Audit aggregation summary")
    lines.append("")
    lines.append(f"- Raw findings: **{raw_count}**")
    lines.append(f"- Dropped — out of scope: **{scope_excluded_count}**")
    lines.append(f"- Dropped — known FP: **{dropped_count}**")
    surviving = sum(len(g) for g in groups.values())
    lines.append(f"- Surviving findings: **{surviving}** (in {len(groups)} groups)")
    lines.append("")
    if not groups:
        lines.append("_No findings survive aggregation. Tools were either clean "
                     "or every finding matched a documented FP class._")
        return "\n".join(lines)

    lines.append("## Groups (representative finding + count)")
    lines.append("")
    by_tool: dict[str, list] = defaultdict(list)
    for (tool, rid, td), group in groups.items():
        by_tool[tool].append(((rid, td), group))
    for tool in sorted(by_tool):
        lines.append(f"### {tool}")
        lines.append("")
        for (rid, td), group in sorted(by_tool[tool], key=lambda x: -len(x[1])):
            rep = rep_finding(group)
            count = len(group)
            badge = f"x{count}" if count > 1 else ""
            lines.append(f"- **{rid}** in `{td}/` {badge} — `{rep.file}:{rep.line}` "
                         f"[{rep.severity}] {rep.message[:120]}")
        lines.append("")
    return "\n".join(lines)


def render_json(groups: dict, dropped_count: int, scope_excluded_count: int,
                raw_count: int) -> str:
    out = {
        "summary": {
            "raw": raw_count,
            "dropped_out_of_scope": scope_excluded_count,
            "dropped_known_fp": dropped_count,
            "surviving": sum(len(g) for g in groups.values()),
            "groups": len(groups),
        },
        "groups": [],
    }
    for (tool, rid, td), group in sorted(groups.items()):
        rep = rep_finding(group)
        out["groups"].append({
            "tool": tool,
            "rule_id": rid,
            "top_dir": td,
            "count": len(group),
            "representative": {
                "file": rep.file,
                "line": rep.line,
                "severity": rep.severity,
                "message": rep.message,
            },
            "all_files": sorted({f"{f.file}:{f.line}" for f in group}),
        })
    return json.dumps(out, indent=2)


# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

PARSERS = {
    "cppcheck":   parse_cppcheck,
    "semgrep":    parse_semgrep,
    "ruff":       parse_ruff,
    "normalized": parse_normalized,
}


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    src = p.add_mutually_exclusive_group(required=True)
    for name in PARSERS:
        src.add_argument(f"--from-{name}", metavar="FILE",
                         help=f"input file in {name} format")
    p.add_argument("--format", choices=("markdown", "json"), default="markdown")
    p.add_argument("--no-drop-known", action="store_true",
                   help="don't drop documented FP classes (debugging)")
    p.add_argument("--no-drop-scope", action="store_true",
                   help="don't drop out-of-scope subtrees (debugging)")
    args = p.parse_args(argv)

    src_format, src_path = next(
        (n, getattr(args, f"from_{n}")) for n in PARSERS
        if getattr(args, f"from_{n}", None)
    )
    try:
        findings = PARSERS[src_format](Path(src_path))
    except (ET.ParseError, json.JSONDecodeError, FileNotFoundError) as e:
        print(f"error: failed to parse {src_path}: {e}", file=sys.stderr)
        return 1

    raw_count = len(findings)
    scope_excludes = load_scope_excludes()
    in_scope = []
    out_of_scope_count = 0
    for f in findings:
        if not args.no_drop_scope and not is_in_scope(f, scope_excludes):
            out_of_scope_count += 1
            continue
        in_scope.append(f)

    surviving = []
    dropped_fp_count = 0
    for f in in_scope:
        if not args.no_drop_known and is_known_fp(f):
            dropped_fp_count += 1
            continue
        surviving.append(f)

    groups = aggregate(surviving)
    if args.format == "json":
        print(render_json(groups, dropped_fp_count, out_of_scope_count, raw_count))
    else:
        print(render_markdown(groups, dropped_fp_count, out_of_scope_count, raw_count))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
