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
| File / path / identifier naming | — | [`file-naming-standard.md`](file-naming-standard.md) — single home; consolidates conventions previously scattered across `CLAUDE.md` and `Makefile.common` |
| Documentation (roadmap, changelog, commits) | — | [`documentation-standard.md`](documentation-standard.md) — governs `CHANGES.md`, `docs/private/ROADMAP.md` and commit messages |
| Security posture / secure-coding rules | — (root `SECURITY.md` is the vuln-*reporting* policy and carries no coding rules; see below) | [`security-standard.md`](security-standard.md) — threat model + secure-coding rules + accepted-risk register |
| Dependency / version pinning | — | [`../DEPENDENCY-POLICY.md`](../DEPENDENCY-POLICY.md) |
| Static-analysis false-positive patterns + suppressions | — | [`../AUDIT-POLICY.md`](../AUDIT-POLICY.md) |

## What is NOT here (and why)

- **The public vulnerability-reporting policy** stays at root `SECURITY.md`
  (upstream, `report to libretro@gmail.com`). The fork does not fork it —
  see [`security-standard.md`](security-standard.md) §Relationship.
- **The full upstream coding standard** stays at the libretro docs site
  (`https://docs.libretro.com/development/coding-standards/`) and
  `CODING-GUIDELINES`. `coding-standard.md` references it and echoes only
  the highest-cost rules — it does not wholesale copy.

## Precedence

Global meta-rules in `~/.claude/CLAUDE.md` (e.g. the `review-contract` gate)
apply across every project and sit outside this substance chain. They are
not ranked by the clauses below, and nothing in this directory displaces
them. Only the repo-root `CLAUDE.md` may override a global rule, by the
mechanism `~/.claude/CLAUDE.md` § The foundation grants it, and it records
each override it carries. A fork standard that contradicts a global rule
with no such override recorded is a defect in the fork standard.

Resolve a **substance** disagreement by the Map's Authoritative-source
column. Read these in order; the first that applies wins, and the third is a
residual tie-break, not a ranking that overrides the two above it.

1. **The Authoritative-source cell names a file** — coding style and
   portability, the only row where it does: that file wins on substance,
   and the fork standard governs only its fork-operational additions. Root
   `CLAUDE.md` is listed there for its fork-ops content; where it
   paraphrases an upstream rule it yields to the upstream file, and where
   it states a fork-ops rule it does not.
2. **The cell is `—`**: the fork standard or policy in the right-hand
   column is authoritative for that substance, including over root
   `CLAUDE.md`. Both `../`-rooted policies are governed by this clause and
   by "Changing a standard" below.
3. **No Map row covers the topic**: the more specific document wins —
   per-project `CLAUDE.md` → these fork standards → upstream
   `CODING-GUIDELINES` / `CONTRIBUTING.md`.

A disagreement that survives that ordering is a bug in the docs — fix it,
don't work around it.

## Changing a standard

A standard is a contract document, and so is this index: an edit to the Map
or to Precedence changes what every conformer obeys. Both are governed by
this section.

`~/.claude/CLAUDE.md` rule 14 decides whether an edit needs the gate, what
its exceptions are, what the loop cap is, what a clean exit is, and what
record is owed on each branch. This file restates none of that, so that it
cannot drift from it. When the gate does run it is
`review-contract --genre standard`, and `review-contract` decides where its
loop log lives.

A new standard or policy adds its Map row in the same commit. Until it has
one, Precedence clause 3 governs it rather than clause 2.
