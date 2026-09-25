# Review loop log — `docs/private/standards/documentation-standard.md`

Review history for the fork's documentation standard, kept outside the
standard itself. `review-contract` writes one row per loop.

Rows 1 and 2 were recorded in commit bodies before this file existed:
`329f4b3cc0` (loop 1) and `77c6b39867` (loop 2), from the gate `5a551008ed`
armed. That run's loop 3 was never dispatched. The gate armed by
`4ba330bc59` and `38fb249c0a` started a fresh run, which replaced it.

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 3 | 2026-09-25 | 3 | 1 | 3 | 1 | 0 | Loop 1 of the run armed by `4ba330bc59` + `38fb249c0a` (store as source of truth, ids, 💭/🚫 vocabulary, dated-record carve-out). 3 lanes, every lane held every question. 5 verified, 0 dismissed, 5 fixed. [Q1] "the last two are kept after closure" named 💭, which is open — rewritten to ✅/🚫, and 💭 named as open in the Layman rule. [Q2] §5 forbade echoing upstream rules while `README.md` requires echoing the highest-cost ones — aligned to the README. [Q2] §6's table-cell and dated-record carve-outs narrowed `/mnt/Games/CLAUDE.md` with no authority — moved to the repo-root `CLAUDE.md` § Citation form, which as the deeper file can. [Q2] "a closed bullet cites its fix commit" conflicted with 🚫 closures — scoped to ✅, with a 🚫 form added. [Q3] the `Co-Authored-By` trailer had no stated source or scope — scoped to Claude Code commits and the harness's attribution instruction. Four open questions resolved clean: the lanes' harness-loaded `CLAUDE.md` predated this session's edit. Lanes disclosed seeing commit subjects naming earlier loops. Loop 2 dispatched. |
| 4 | 2026-09-25 | 3 | 0 | 1 | 1 | 0 | Loop 2 of this run. 3 lanes, every lane held every question. 2 verified, 0 dismissed, 2 fixed. [Q2] §4's bold opening restated rule 14's trigger without its exceptions while the same section said it restated none of it (A, B, C) — reduced to a pointer. [Q3] build plans had no fork location, so `write-spec --plan` would land in a top-level `docs/plans/` and collide with upstream (A, C) — the user chose `docs/private/plans/YYYY-MM-DD-<slug>.md`, and the repo-root `CLAUDE.md` override now names it. Both findings were pre-existing text, outside the span this run was armed on. Collateral fixed in passing: `file-naming-standard.md` gained its Plans line, and the `CLAUDE.md` branch list names `docs/private/plans/`. Loop 3 (the cap) dispatched. |
