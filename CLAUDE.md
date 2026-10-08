# CLAUDE.md

Project-specific guidance for Claude Code in this repository. Layers on top of `~/.claude/CLAUDE.md` (global rules). It adds RetroArch-specific institutional knowledge that isn't derivable from the code, and carries two deliberate overrides: one of global rule 14a, under "Fork document locations", and one of `/mnt/Games/CLAUDE.md`, under "Citation form".

## What this is

RetroArch is the reference frontend for the libretro API. The bulk of the codebase is C (with some C++/Objective-C/Metal for platform glue), and most of it ports to dozens of platforms — desktop, consoles, handhelds, mobile, web. Portability constraints drive almost every coding decision below.

## Build system

Hand-rolled `qb` shell script (`./configure` -> `qb/qb.*.sh`) produces `config.h` and `config.mk`, consumed by GNU Make. **No autoconf, no CMake.**

```sh
./configure                 # detects libs; --help for flags; generates config.h, config.mk
make -j$(nproc)             # builds ./retroarch
make V=1 DEBUG=1            # verbose / -O0 -g / separate obj-unix/debug/ tree
```

Per-platform Makefiles live in the repo root: `Makefile.<platform>` (`Makefile.win`, `Makefile.ctr`, `Makefile.libnx`, `Makefile.ps2`, `Makefile.emscripten`, `Makefile.apple`, ...). All but the most exotic ones `include Makefile.common` (the `HAVE_*` conditionals that form the actual source list).

`Makefile.local` is a per-developer override `-include`d by the main Makefile — use it for personal `CFLAGS`, never commit it.

Adding or renaming a source file needs a griffin step, and the macOS bundle, ANGLE and Qt builds have their own switches: read `docs/private/codebase-guide.md` § Build variants first.

## Tests

No integration-level test runner. What exists:

- **libretro-common unit tests** — `cd libretro-common && make -f Makefile.test` (needs `libcheck`; builds with ASan+UBSan+gcov). Covers stdstring, hash, queues, lists, utils. Add new tests under `libretro-common/test/<area>/` and wire into `libretro-common/Makefile.test`.
- **Replay-based input tests** — `tests-other/*.ratst` are JSON action recordings replayed by RetroArch itself (verbose log compared against expectation). Not wired into a CI target here; see the `HAVE_TEST_DRIVERS` block in `Makefile.common` for the build flag (gates `test_joypad.o` + `test_input.o`).
- **Manual** — runtime issues: run with `-v` and reproduce.

Applying `~/.claude/standards/testing.md` §1 (test first) here: for libretro-common bugs, write the failing libcheck test under `libretro-common/test/<area>/` first; for runtime/input bugs, check whether a `.ratst` replay can capture the symptom before patching.

## Architecture
Driver pattern, singleton state, main loop, configuration (the full file set a new setting needs), menu and tasks: `docs/private/codebase-guide.md` § Architecture. Read it before adding a driver, a setting or a task, or touching shared state.

### libretro-common and deps/
Both are **vendored**. `libretro-common/` mirrors github.com/libretro/libretro-common (same code shipped with cores); `deps/` holds 7zip, glslang, miniz, rcheevos, stb, etc. Don't make local-only changes — patch upstream and re-vendor, or the next sync clobbers the change. The libretro API itself: `libretro-common/include/libretro.h`.

## Coding rules (non-obvious)

From `CODING-GUIDELINES`, `CONTRIBUTING.md`, and the C89/console-portability constraints. Compilers don't always catch violations.

- **C89 + ISO C++ compatible.** No declaration-after-statement, no `for (int i = ...)`, no VLAs, no `//`-only comments — these break Xbox 360 / older MSVC builds. Declare variables at the top of a function or block. (This is a deliberate exception to `~/.claude/standards/coding.md` §1.5's current-idioms rule: don't reach for current C idioms here even though they'd compile on most targets.)
- **Allman braces.** No braces for single-statement blocks (unless the body is a multi-line macro).
- `for (;;)` over `while (true)`.
- **Avoid one-line getter/setter functions.** Read/write the struct field directly. Function-call overhead is real on PSP/3DS/Wii.
- **Sort struct members by alignment** (`long double` -> `double` -> `int64_t` -> pointer -> `size_t` -> `int` -> `int16_t` -> `char` -> `bool`). Interleave pointer + matching `_len` (cache-locality + readability).
- **Stack is small** on consoles (down to 128KB). Don't put `char path[PATH_MAX_LENGTH]` arrays as locals in deep call stacks; prefer the caller's buffer or a single allocation.
- `-Wall -Wsign-compare` clean. Build with `MISSING_DECLS=1` for `-Werror=missing-declarations`.
- Every new feature should be gated by a `HAVE_*` macro from `config.h` so it can be toggled off for size-constrained targets.
- Update copyright headers in any file you substantially modify; add yourself to `AUTHORS.h` for significant contributions.

## Versioning & release notes

- Version bumps and the lockstep file set: `docs/private/codebase-guide.md` § Versioning & release notes.
- User-visible changes go to `CHANGES.md` under `# Future` until release.
- Fork-only audit/refactor work goes to `docs/private/ROADMAP.md`, **not** `CHANGES.md` — `CHANGES.md` is user-visible, the private ROADMAP is engineering-internal.

## Fork document locations (override of global rule 14a)

**Fork specs, design documents and ADRs live at `docs/private/specs/YYYY-MM-DD-<slug>.md`** — not at `docs/specs/<ID>-<topic>.md`, and not at `docs/design.md`. **Build plans live at `docs/private/plans/YYYY-MM-DD-<slug>.md`**, not at `docs/plans/<ID>-<topic>.md`, including a plan `write-spec --plan` produces. This overrides global rule 14a's fixed locations, using the mechanism `~/.claude/CLAUDE.md` § The foundation grants a per-project `CLAUDE.md`. A session following it says which it followed, as that section requires.

Design documents are named here deliberately: that directory already holds `-design.md` files, so an override naming specs alone would have left them claiming an authority that did not cover them.

Two fork-specific reasons:

- This is a downstream fork of a tree we do not own and re-sync from. Every fork-authored document lives under `docs/private/` so a re-vendor never collides with upstream — and a top-level `docs/specs/`, `docs/plans/` or `docs/reviews/` is exactly such a collision.
- The existing specs are named by date, and the roadmap and the fork's audit docs cite them by those names. The roadmap now carries ids (`docs/private/standards/documentation-standard.md` §2), but renaming the specs to `<ID>-<topic>` would break every existing citation, so new specs keep the date form for one naming scheme per directory.

**Review loop logs live at `docs/private/reviews/YYYY-MM-DD-<slug>-loop-log.md`** for a spec and `…-plan-loop-log.md` for a plan, not under `docs/reviews/`. The roadmap id goes in the document's title line, since the filename carries the date. `write-spec` reads this declared override for the spec, the plan and both loop logs, and is still how specs and plans are written. The override reaches **locations and filenames** only. Rule 14's gate, its trigger, its cap and its records are not touched, and `docs/private/standards/README.md` § Precedence states that nothing in this directory displaces a global rule.

## Citation form (override of `/mnt/Games/CLAUDE.md`)

`/mnt/Games/CLAUDE.md` § Writing and Editing Documents binds here: no counts, line numbers or sizes. This deeper file narrows it in two places, by the global rule that the deeper `CLAUDE.md` wins where two conflict:

- A structured datum in a table cell is a field, not prose. The `Sites` count in the ROADMAP's bundle table stays.
- A dated record may cite line numbers, and keeps the ones it has: a closed roadmap entry, a commit body, a review loop log. It is written once and never revised, so a line number in it stays true of its date, and rewriting it damages the record.

Every other new text follows the parent rule: names, not counts, line numbers or sizes.

## Fork workflow (private)

This checkout is a libretro/RetroArch fork carrying ongoing audit + refactor work. The fork is operated under a two-branch model that the upstream tree does not mirror:

- **`local/audit-2026-04`** — roadmap + docs branch. `docs/private/ROADMAP.md`, `docs/private/AUDIT-POLICY.md`, `docs/private/specs/`, `docs/private/plans/`, and `docs/private/audit/` live here. All cold-eyes / indie-review / audit-fold-in commits land on this branch.
- **`local/fixes-2026-09`** — source-fix branch. Its worktree is `/mnt/Games/Scripts/Linux/ra-fixes`; if `git worktree list` does not show it, create it with `git worktree add /mnt/Games/Scripts/Linux/ra-fixes local/fixes-2026-09`. Never under `/tmp`, which is RAM on this machine. cppcheck / clang-tidy / clazy fix bundles commit here. Build verification (`make -j4 retroarch`; RAM is short, so never `-j$(nproc)`) runs from this worktree.
- **`pr/*`** — upstream-PR branches, worktree `/mnt/Games/Scripts/Linux/ra-pr`. They are cut from upstream `master` and pushed only to open upstream PRs. Their pipeline is libretro's GitHub CI, not ours to mirror.

**The push gate.** GitHub Actions is disabled on the fork, so the only CI is `local-CI.sh` on `local/fixes-2026-09`, run by that worktree's pre-push hook. A push from the docs branch or from `ra-pr` reports "NO LOCAL GATE", and that is expected: docs pushes are documentation-only, and `pr/*` code is checked by libretro's CI on the PR. The three worktrees share one git config, so never set `ants.gate.command` repo-wide.

Bundle commits cross-reference each other by SHA in `docs/private/ROADMAP.md`. When asked to "fold in" or "log a bundle", write it through `roadmap_log` on the audit branch — the roadmap store is the source of truth and the file is rendered from it; when asked to fix a finding, switch to the fixes-branch worktree.

`docs/private/audit/aggregate.py` is the fork's local audit-aggregator that drives `last_audit_summary` / `audit_run` MCP integrations; `.cppcheck-suppress.txt` at repo root holds the cppcheck inline-suppression set the aggregator respects. See `docs/private/AUDIT-POLICY.md` for the false-positive-pattern +
suppression contract.
