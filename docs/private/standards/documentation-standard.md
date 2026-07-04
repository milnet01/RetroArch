# Documentation standard (fork)

Where each kind of prose lives, and the format it follows. The split below
is load-bearing: `CHANGES.md` is user-visible, the private roadmap is
engineering-internal — mixing them leaks fork-audit detail into the public
changelog.

## 1. The two changelogs

| Audience | File | Content |
|---|---|---|
| Users | `CHANGES.md` (root) | User-visible changes, under `# Future` until release. |
| Engineering | `docs/private/ROADMAP.md` | Fork audit / refactor / review work — findings, bundles, closures. |

Never put fork audit findings in `CHANGES.md`; never put user-facing
release notes only in the private roadmap.

## 2. ROADMAP format (ants-v1)

- Emoji-status narrator bullets: `📋 planned`, `🚧 in-progress`,
  `✅ shipped`, `💭 considered`. Bullets are id-less (legacy ants-v1);
  do not add `[PROJ-NNNN]` ids to existing bullets.
- A closed bullet cites its fix commit(s). The prevailing form is
  `_(Fixed `<sha>` — <what/why>.)_`; fix-branch closures also use
  `_(Bundle N — fixed in `<sha>` on `local/fixes-2026-04`. <what/why>.)_`.
- The **Bundle progress (running summary)** table at the top is the index:
  one row `| N | commit(s) | theme | sites |` per bundle, appended in
  ascending bundle order.
- Prefer the Ants MCP verbs (`roadmap_query`, `roadmap_log`) over hand
  edits where they apply. The bundle-progress **table row** is currently a
  hand edit: `roadmap_log op:"bundle_row"` exists but was non-functional as of
  2026-07-03 (its `cells` argument was unwired on the running server — see
  the fork's `*_Ants_MCP_Feedback.md`); re-check before relying on it.

## 3. Commit messages

- Subject: imperative, scoped — `<area>: <what>` (e.g.
  `gfx/drm: fix NULL-deref …`, `docs/roadmap: Bundle NN fold-in …`).
- Body: what changed and **why**, plus the verification performed
  ("builds; cppcheck clears the site"). A six-month reader must understand
  why the code looks this way without the author.
- Source fixes commit to `local/fixes-2026-04`; roadmap/docs commit to
  `local/audit-2026-04`. Bundle commits cross-reference each other by SHA.
- End the message with the required `Co-Authored-By:` trailer.

## 4. Specs and design docs

- Live at `docs/private/specs/YYYY-MM-DD-<slug>.md`.
- **Every spec / design / ADR / standard runs through `/cold-eyes` until a
  pass returns zero verified findings, before implementation.** This is a
  hard rule (`~/.claude/CLAUDE.md` rule 14) — the loop is the gate, a
  self-read does not satisfy it. Loop 2+ runs cold (do not brief the
  reviewer on prior findings). Record the loop.
- Per-feature test contracts (`tests/features/<name>/spec.md`) are exempt —
  a self-read suffices.

## 5. Standards & policies

- Policies: `docs/private/<TOPIC>-POLICY.md`.
- Standards: `docs/private/standards/<topic>-standard.md`, indexed by
  [`README.md`](README.md).
- Each **consolidates and references** its authoritative source; it must
  not duplicate upstream prose (`CODING-GUIDELINES`, `CONTRIBUTING.md`,
  root `SECURITY.md`). Duplication drifts.

## 6. Every factual claim is verified

Any doc statement naming a file, function, line, constant, commit, or
version is backed by a grep/read against current source before it is
written — not by recall. A citation that has drifted (stale line number,
wrong path) is a documentation bug. When a claim can't be verified on disk
because it concerns intent or future direction, mark it as an open question
rather than asserting it.

## 7. Ants MCP feedback

Cross-session tooling feedback goes to the `<repo-dir>_Ants_MCP_Feedback.md`
file that sits as a sibling of the repo root (today
`/mnt/Games/Scripts/Linux/RetroArch_Ants_MCP_Feedback.md`), via the
`feedback_query` / `feedback_log` verbs — append findings at the end, never
edit a maintainer tracking block, never self-assign `ANTS-NNNN` ids.
