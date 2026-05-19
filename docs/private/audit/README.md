# Audit infrastructure

Project-side configuration for the global `/audit` and `/indie-review` skills. Their default invocations rediscover scope, tool flags, FP classes, and subsystem partition every run. On a 1.56M-LoC tree that's expensive — the 2026-04-25 baseline run produced **660 raw → 24 actionable** findings (~96% noise rate) and ~1.5M tokens across 8 indie-review lanes. This directory pins the rediscovered facts so future runs start from the calibrated state.

## Files

| File | What it is | Loaded by |
|---|---|---|
| [`scope.txt`](scope.txt) | Canonical Linux-desktop-scope prune list. One subtree per line. | `aggregate.py`, cppcheck `-i`, semgrep `--exclude`, clang-tidy filter, human grep |
| [`audit-config.json`](audit-config.json) | Per-tool invocations: cppcheck/clang-tidy/semgrep/clazy/gitleaks/trivy/ruff/bandit flags + configs + known issues + presets (`default` / `quick` / `all` / `secrets`) + incremental-mode anchor | `/audit` orchestrator (when supported) — until then, copy commands by hand |
| [`suppressions.md`](suppressions.md) | Catalog of FP / won't-fix / awaiting-spec finding classes, with pre-triage drop rules | `aggregate.py --no-drop-known` to disable; triage subagent for narrative context |
| [`aggregate.py`](aggregate.py) | Pre-triage rule-id × directory aggregator. Reads cppcheck XML / semgrep JSON / ruff JSON / pre-normalized JSON. Cuts triage input ~10× when raw findings >200. | `/audit` step 8 (pre-triage aggregation) |
| [`indie-review-partition.md`](indie-review-partition.md) | Memoized 8-lane subsystem map with line ranges for the 18 mega-files, contract docs, external specs, and per-lane gotcha lists | `/indie-review` Phase 1 |

## Quick reference — running an audit

### One-shot all tools (manual orchestration)

```bash
# 1. Refresh compile_commands.json if any source moved/renamed since last build
bear -- make -j$(nproc)

# 2. Run each tool with project-pinned flags from audit-config.json.
#    These commands materialise the JSON manually until the /audit skill
#    learns to read project-side config.
SID=$(date +%s)

# cppcheck
cppcheck --enable=warning,style,performance,portability,information,missingInclude \
  --std=c11 --language=c \
  --suppress=missingIncludeSystem --suppress=unusedFunction --suppress=unknownMacro \
  --suppressions-list=.cppcheck-suppress.txt \
  --inline-suppr --xml --xml-version=2 -j 4 \
  $(awk '/^[^#]/ {print "-i"$0}' docs/private/audit/scope.txt) \
  . 2> /tmp/audit-cppcheck-$SID.xml

# clang-tidy (per-file, parallel — see compile_commands.json)
# Recommend running on a diff: git diff --name-only master..HEAD -- '*.c' '*.cpp'
# rather than the full tree.

# semgrep
semgrep scan --json --timeout=60 --metrics=off \
  --config=p/c --config=p/security-audit \
  $(awk '/^[^#]/ {print "--exclude="$0}' docs/private/audit/scope.txt) \
  . > /tmp/audit-semgrep-$SID.json

# 3. Aggregate before piping to triage
python3 docs/private/audit/aggregate.py --from-cppcheck /tmp/audit-cppcheck-$SID.xml > /tmp/audit-cppcheck-summary.md
python3 docs/private/audit/aggregate.py --from-semgrep  /tmp/audit-semgrep-$SID.json --format=json > /tmp/audit-semgrep-summary.json

# 4. Pipe summaries to /audit-triage subagent (or invoke /audit which does this)
```

### Incremental mode

When iterating on a fix bundle, scan only changed files:

```bash
git diff --name-only master..HEAD -- '*.c' '*.cpp' '*.h' '*.hpp' \
  | xargs -I{} cppcheck --enable=all --std=c11 --xml --xml-version=2 \
      --inline-suppr {}  2>> /tmp/audit-cppcheck-incr.xml
```

Fall back to a full scan if more than ~50 files changed (per `audit-config.json` `incremental.fall_back_to_full_scan_when`).

## Quick reference — running an indie-review

```
/indie-review            # default 8 lanes per indie-review-partition.md
/indie-review --quick    # 3 lanes (libretro env, driver-pattern meta, network commands / IPC)
/indie-review --thorough # 10 lanes (Lane 7 replaced by sub-lanes 7a + 7b + 7c — total 8 - 1 + 3 = 10)
```

Phase 1 of the orchestrator should:
1. Read `indie-review-partition.md`.
2. For each lane, grep the relevant `ROADMAP.md` slice (file paths in the lane → matching ROADMAP lines) and attach as part of the brief. This is the dedup-during-review optimization that cuts agent output 30-50% on a project this far along.
3. Apply the per-lane gotcha list from the partition file as the "memory" attachment the indie-review skill expects.

## Adding a new tool

1. Add a section to `audit-config.json` under `tools.<name>` with `command`, `flags`, `scope_apply` (if applicable), and a `known_issues` array.
2. If the tool emits a non-trivial format, add `parse_<tool>` to `aggregate.py` and register it in the `PARSERS` dict. The `--from-<tool>` argument is derived automatically from `PARSERS` — do not add it by hand.
3. Add the tool to the `presets.default` list (if it should run by default) or only `presets.all` (if it's expensive / specialised).
4. Update this README's quick-reference command examples.

## Adding a new FP class

1. Add a section to `suppressions.md` with rule-id, sites, why-FP, action.
2. If pre-triage should mechanically drop the class, add a lambda to `aggregate.py`'s `KNOWN_FP_RULES` list.
3. Add a row to the suppressions.md drop-rules table.
4. Cross-link from the corresponding ROADMAP entry: `(see suppressions.md § <anchor>)`.

## Adding a new big-file boundary

When a file crosses 5k LoC:

1. Run `wc -l` on the file to confirm.
2. Read the file head for author banner comments — they often demarcate concerns.
3. Add a row to `indie-review-partition.md`'s big-file table with the line ranges per concern.
4. Decide which lane the file belongs to; add it to that lane's source paths with `(focus on lines X-Y)` qualifier.

The same `wc -l` step is also how the existing entries in
`indie-review-partition.md` are kept honest: file lengths drift +10 to +40
lines between bundles, so before each `/indie-review` run, re-run `wc -l` on
the big-file table's paths and refresh any line ranges whose endpoints have
slipped past their author-banner boundaries. The doc instructs reviewers to
"verify against current banner comments before each run" — `wc -l` is the
practical mechanism for that verification.

## Calibration anchors

The 2026-04-25 baseline (fixed historical anchor; do not move):
- **111 fixes across 55 files** in Bundles 1-33 on `local/fixes-2026-04`.
- **24 actionable / 12 spec-needed / 660 raw** from cppcheck + semgrep + ruff + bandit.
- **8 indie-review lanes**, ~1.5M tokens combined.
- **Tools clean every run** (last 3): gitleaks, trivy.
- **Tools partial** (last run): cppcheck (449/450 files; materialui.c timed out).
- **Tools not yet run**: clang-tidy (now runnable post-bear), clazy (deferred — Qt UI is small).

Use these as the target the next run should beat or meet.

### Current cumulative state

Updated when each bundle folds in; canonical count lives at the top of [`../ROADMAP.md`](../ROADMAP.md).

- **Latest cumulative count, including bundle-id, is the canonical figure at the top of [`../ROADMAP.md`](../ROADMAP.md).** Do not pin a snapshot here — the snapshots have historically drifted N bundles behind.
- **clang-tidy** has run (cumulative across bundles 36–44 + follow-ups, plus the Bundle 61 `clang-analyzer-*` sweep and Bundle 64 tree-wide `bugprone-integer-division` sweep); `compile_commands.json` is in tree.
- **cppcheck** `materialui.c` macro-config exhaustion was closed in Bundle 62 by running with `--max-configs=1` plus inline FP suppressions. Every cppcheck tool now finishes.
- **clazy** still deferred; Qt UI surface remains small.

The 2026-04-25 baseline numbers are anchors for the *next full audit* run to compare against — when re-running cppcheck/semgrep/ruff/bandit from scratch, expect raw counts to drop substantially relative to the 660-raw baseline as a result of the fix-stream above.

## What this is NOT

- Not a replacement for the global `/audit` and `/indie-review` skills — it's the **project-pinned config** they need to consume.
- Not a fix-the-bugs tool — `aggregate.py` produces a triage-input summary; the audit-triage subagent still picks actionable items.
- Not an upstream-able artifact — this is fork-only configuration. If RetroArch upstream wants similar tooling, this is a starting point, not a deliverable.

## See also

- [`../ROADMAP.md`](../ROADMAP.md) — fork roadmap, per-finding history, bundle commit table.
- [`../specs/`](../specs/) — feature/contract specs awaiting implementation.
- [`~/.claude/skills/audit/SKILL.md`](~/.claude/skills/audit/SKILL.md) — global audit skill (consumed by `/audit`).
- [`~/.claude/skills/indie-review/SKILL.md`](~/.claude/skills/indie-review/SKILL.md) — global indie-review skill (consumed by `/indie-review`).
