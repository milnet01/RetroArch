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

- **The Ants roadmap store is the source of truth.** `docs/private/ROADMAP.md`
  is rendered from it, and a hand edit to the file is discarded by the next
  `roadmap_log` write. Every change goes through `roadmap_log`; read with
  `roadmap_query`.
- Every item carries an id. Items migrated from the old id-less file carry
  a synthesised `RETR-S<NNNN>` id; the store allocates ids for new items.
  Cite an item by its id.
- Emoji-status bullets, matching the legend at the head of
  `docs/private/ROADMAP.md`, which is authoritative for this vocabulary:
  `📋 pending`, `🚧 in progress`, `✅ done`, `🔄 deferred / waiting on
  upstream`, `❌ won't-fix / verified-FP / resolved-stale`. The last two
  are kept deliberately after closure — the analyser re-reports a
  suppressed false positive every run, and without its own mark a
  suppressed finding is indistinguishable from a live regression.
- An open item carries a `Layman:` summary. The store refuses a write
  that touches an open item without one.
- A closed bullet cites its fix commit(s). The prevailing form is
  `_(Fixed `<sha>` — <what/why>.)_`; fix-branch closures also use
  `_(Bundle N — fixed in `<sha>` on `local/fixes-2026-04`. <what/why>.)_`.
- The **Bundle progress (running summary)** table at the top is the index:
  one row `| N | commit(s) | theme | sites |` per bundle, appended in
  ascending bundle order.
- A bundle-progress table row is appended with `roadmap_log
  op:"bundle_row"`, which escapes a `|` inside a cell. A bare `|` in a
  cell splits the row and makes the render refuse.

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

- Live at `docs/private/specs/YYYY-MM-DD-<slug>.md`. This is an override of
  global rule 14a's fixed locations, declared and reasoned in the repo-root
  `CLAUDE.md` § Fork document locations — not a divergence this file grants
  itself.
- **A document whose edit changes what a conformer would do runs through
  `review-contract`, before implementation.** The loop is the gate; a
  self-read does not satisfy it, and loop 2+ runs cold. Rule 14 decides
  which documents are in scope — this file does not enumerate them, and an
  earlier enumeration here omitted policies while the sibling
  [`README.md`](README.md) § Precedence had them gated.
- `~/.claude/CLAUDE.md` rule 14 owns everything else about that gate — the
  trigger, its exclusions (including which test contracts are exempt and on
  what conditions), the loop cap, what a clean exit is, and what record is
  owed on each branch, gate or no gate. This file restates none of it, so
  that it cannot drift from it.

## 5. Standards & policies

- Policies: `docs/private/<TOPIC>-POLICY.md`.
- Standards: `docs/private/standards/<topic>-standard.md`, indexed by
  [`README.md`](README.md).
- Each **consolidates and references** its authoritative source; it must
  not duplicate upstream prose (`CODING-GUIDELINES`, `CONTRIBUTING.md`,
  root `SECURITY.md`). Duplication drifts.

## 6. Every factual claim is verified

Any doc statement naming a file, function, constant, commit, or version is
backed by a grep/read against current source before it is written — not by
recall. A citation that has drifted is a documentation bug. When a claim
can't be verified on disk because it concerns intent or future direction,
mark it as an open question rather than asserting it.

Citation **form** is `/mnt/Games/CLAUDE.md`'s rule, which binds inside this
repo: in prose, name the section, heading, filename or symbol — not a
count, a line number or a size. A line number drifts by construction, which
is why it is not a citation form here.

That governs prose. It does not reach a structured datum in a table cell,
which is a field rather than a citation — §2's bundle-table `sites` count
is the live case, and it stays.

## 7. Ants MCP feedback

Cross-session tooling feedback goes to the `<repo-dir>_Ants_MCP_Feedback.md`
file inside the sibling `Ants_MCP_Feedback_Files/` directory one level above
the repo root (today
`/mnt/Games/Scripts/Linux/Ants_MCP_Feedback_Files/RetroArch_Ants_MCP_Feedback.md`),
via the `feedback_query` / `feedback_log` verbs — append findings at the end, never
edit a maintainer tracking block, never self-assign `ANTS-NNNN` ids.
