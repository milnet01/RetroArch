# Fork engineering standards

Entry point for the standards that govern this downstream fork of
`libretro/RetroArch`. These are **fork-internal** — they live under
`docs/private/` so they never conflict with the upstream tree on a
re-vendor / re-sync.

The rule of this directory: **consolidate and point at the authoritative
source; echo only the highest-cost rules, don't wholesale restate.**
Several standards (coding style, portability constraints) already have an
upstream home; the docs here name that home, deliberately repeat the few
rules whose violation costs the most, and add the fork-specific operational
rules that aren't captured anywhere else.

## Map — which document is authoritative for what

| Topic | Authoritative source | Fork standard (here) |
|---|---|---|
| Coding style / C89 portability | `CODING-GUIDELINES` (root), `CONTRIBUTING.md` §Coding style (root), `CLAUDE.md` (root, fork ops) | [`coding-standard.md`](coding-standard.md) — consolidates + adds verification discipline |
| File / path / identifier naming | scattered in `CLAUDE.md` + `Makefile.common` conventions | [`file-naming-standard.md`](file-naming-standard.md) — single home |
| Documentation (roadmap, changelog, commits) | `CLAUDE.md`, `CHANGES.md`, `docs/private/ROADMAP.md` | [`documentation-standard.md`](documentation-standard.md) |
| Security posture / secure-coding rules | `SECURITY.md` (root — upstream vuln-reporting policy) | [`security-standard.md`](security-standard.md) — threat model + secure-coding rules + accepted-risk register |
| Dependency / version pinning | — | [`../DEPENDENCY-POLICY.md`](../DEPENDENCY-POLICY.md) |
| Static-analysis audit cadence + suppressions | — | [`../AUDIT-POLICY.md`](../AUDIT-POLICY.md) |

## What is NOT here (and why)

- **The public vulnerability-reporting policy** stays at root `SECURITY.md`
  (upstream, `report to libretro@gmail.com`). The fork does not fork it —
  see [`security-standard.md`](security-standard.md) §Relationship.
- **The full upstream coding standard** stays at the libretro docs site
  (`https://docs.libretro.com/development/coding-standards/`) and
  `CODING-GUIDELINES`. `coding-standard.md` references it and echoes only
  the highest-cost rules — it does not wholesale copy.

## Precedence

Global meta-rules in `~/.claude/CLAUDE.md` (e.g. the `/cold-eyes` gate)
apply across every project and sit outside this substance chain.

For a **substance** disagreement about a topic whose Authoritative source in
the Map above is **upstream** (coding style, portability), upstream wins —
the fork standard governs only its fork-operational additions. For fork-only
topics (naming, roadmap/doc conventions, security posture) the fork standard
is authoritative. Otherwise the more specific document wins:
per-project `CLAUDE.md` → these fork standards → upstream
`CODING-GUIDELINES` / `CONTRIBUTING.md`. A disagreement that survives that
ordering is a bug in the docs — fix it, don't work around it.

## Changing a standard

A standard is a contract document. Per `~/.claude/CLAUDE.md` rule 14, any
new or edited standard here is run through `/cold-eyes` until a pass returns
zero verified findings **before** it is relied on. Record the loop in the
commit message or an adjacent note.
