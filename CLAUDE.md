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

### Griffin (unity build)
Console targets (`Makefile.psp1`, `Makefile.ctr`, `Makefile.ps2`, `Makefile.wii`, `Makefile.wiiu`, ...) build via **`griffin/griffin.c`**, which `#include`s the .c sources directly (`-DHAVE_GRIFFIN=1`). When you add or rename a source file, neither build picks it up on its own: add its `.o` to `Makefile.common` under the right `HAVE_*` block, and — if it should build on consoles — add it to `griffin/griffin.c` (or `griffin_cpp.cpp` / `griffin_objc.m`), or **griffin builds will silently miss it**. Verify every file meant for both appears in both lists.

### Other variants (one-liners)
- **macOS bundle** — `make bundle` after `make` produces ad-hoc-signed `RetroArch.app`; min-OS derived from `-mmacosx-version-min`. `BUNDLE_*` overrides at the bottom of the root `Makefile`.
- **ANGLE** — `HAVE_ANGLE=1` flips `TARGET` to `retroarch_angle` (Win32 GLES via ANGLE). Don't hardcode the binary name in scripts.
- **Qt frontend** — built only when `HAVE_QT=1`; pulled in via `MOC_SRC`/`MOC_OBJ`. Default off.

## Tests

No integration-level test runner. What exists:

- **libretro-common unit tests** — `cd libretro-common && make -f Makefile.test` (needs `libcheck`; builds with ASan+UBSan+gcov). Covers stdstring, hash, queues, lists, utils. Add new tests under `libretro-common/test/<area>/` and wire into `libretro-common/Makefile.test`.
- **Replay-based input tests** — `tests-other/*.ratst` are JSON action recordings replayed by RetroArch itself (verbose log compared against expectation). Not wired into a CI target here; see the `HAVE_TEST_DRIVERS` block in `Makefile.common` for the build flag (gates `test_joypad.o` + `test_input.o`).
- **Manual** — runtime issues: run with `-v` and reproduce.

Applying `~/.claude/standards/testing.md` §1 (test first) here: for libretro-common bugs, write the failing libcheck test under `libretro-common/test/<area>/` first; for runtime/input bugs, check whether a `.ratst` replay can capture the symptom before patching.

## Architecture

### Driver pattern (everywhere)
Every subsystem (video, audio, input, joypad, menu, camera, location, record, MIDI, microphone, Bluetooth, Wi-Fi, ...) follows the same shape:

1. Interface struct in `<subsystem>_driver.h` (vtable of function pointers, plus `const char *ident`).
2. Concrete implementations in the subsystem's drivers directory, each defining an instance named `<subsystem>_<name>` (`audio_driver_t audio_alsa`, `video_driver_t video_gl2`; joypads are `<name>_joypad`). Directory and type names vary by subsystem; `docs/private/standards/file-naming-standard.md` §1 gives examples.
3. NULL-terminated array `<subsystem>_drivers[]` in `<subsystem>_driver.c`, gated on `HAVE_*` macros from `config.h`.
4. Selection by string `ident` from configuration (or first available); the chosen driver pointer lives on the subsystem's static state struct.

Examples: `video_drivers[]` in `gfx/video_driver.c`, `audio_drivers[]` in `audio/audio_driver.c`, `input_drivers[]` in `input/input_driver.c`, `menu_ctx_drivers[]` in `menu/menu_driver.c`. **Adding a new driver:** write `<name>.c`, add to `Makefile.common` under the right `HAVE_*` block (and to `griffin/griffin.c` if it should build on consoles), insert the `extern` + array entry in the subsystem's driver registry.

### Singleton state
Each subsystem keeps a single file-static struct accessed via `<subsystem>_state_get_ptr()`:
- `runloop_state` (in `runloop.c`) — main loop flags, msg queue, performance counters, frame counters
- `video_st`, `audio_st`, `input_driver_st`, `menu_st`, etc.

**Not threadsafe by default**; protection is per-field (`runloop_st->msg_queue_lock`, etc.). When touching shared fields from a task thread, find the existing lock — don't add new ones.

### Main loop & lifecycle
- `rarch_main` (declared in `frontend/frontend.h`, defined in `retroarch.c`; its doc comment still calls it `main_entry`) -> `retroarch_main_init` (in `retroarch.c`) -> `runloop_iterate` (`runloop.c`).
- `retroarch.c` is the libretro environment-callback dispatcher, command-line parser, and core/content load orchestrator. Its single-file size is **deliberate** — function-call overhead is measurable on consoles.
- `command.c` — network/stdin command IPC (pause, save state, etc.).
- `runloop.c` loads the libretro core via `dylib_load` and binds its symbols; `libretro_get_system_info` is declared in `runloop.h`; `dynamic.h` declares `libretro_free_system_info` and `libretro_find_subsystem_info`.

### Configuration
- `config.def.h` — every default value.
- `configuration.c` — parser, saver, override merging, per-core/per-content/per-game cfg layering. **Avoid getters/setters**; settings are accessed as struct fields on `settings_t`.
- `menu/menu_setting.c` — the entire user-visible settings tree (label, range, callback) for the menu UI.
- `intl/msg_hash_*.h` — translatable strings, keyed by enum. Translations come from Crowdin (`Fetch translations from Crowdin` commits); don't edit non-`us` files by hand.

**A new setting spans many files**: at least an entry in `menu/menu_setting.c`, a default in `config.def.h`, a load/save line in `configuration.c`, and its `settings_t` field in `configuration.h`, plus its label enum and strings. To find the full set, pick an existing setting of the same type, search the tree for its name in lower case (`video_shader_delay`) and upper case (`VIDEO_SHADER_DELAY`, which finds its `MENU_LABEL(...)` enum and strings), and mirror every registration hit: menu entry, default, load/save, `settings_t` field, label enum, sublabel and `us` strings. Hits in behaviour code (such as `runloop.c`) are where that one setting is used, not part of the pattern. The default is a `DEFAULT_*` macro in `config.def.h` that neither search matches; find its name in the `configuration.c` hit.

### Menu
Four interchangeable menu drivers in `menu/drivers/`: **rgui** (low-spec text-grid), **ozone** (sidebar, default on desktop), **xmb** (PS3-style horizontal), **materialui** (touch). All read from the same `menu_displaylist`/`menu_entries`/`menu_setting` substrate. Cross-driver UI logic lives in `menu/`; driver-specific rendering lives in the driver file.

### Tasks
`tasks/task_*.c` files implement async background work (HTTP downloads, save state, content scan, decompress, screenshot, ...). Each exposes a `task_push_<name>(...)` API; the task runs on the libretro-common task queue (`libretro-common/queues/task_queue.c`). **Tasks must be cancellation-safe** — their lifetime is independent of the requesting menu/screen. The bug class to watch for: use-after-free on shared fields like `t->title` when the task is freed concurrently with a callback (see `task_http: fix UAF race on t->title` and `task_http: fix heap-buffer-overflow in GET fast-path`).

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

- Version lives in `version.all` (a C/Make/shell polyglot). The lockstep set is every file a search for the current version string finds outside `deps/`, `libretro-common/`, `intl/` and `docs/private/`: `version.all` (`PACKAGE_VERSION`), `version.dtd`, `com.libretro.RetroArch.metainfo.xml` (`<release version="…" date="…">` block — newest entry), and the platform manifests under `pkg/`.
  - `version.all`'s own top-of-file comment names `pkg/snap/snapcraft.yaml`, but that file does **not** exist in this tree (snap packaging lives elsewhere); ignore that line of the comment unless snap is re-introduced.
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
- **`local/fixes-2026-04`** — source-fix branch. Its worktree is `/mnt/Games/Scripts/Linux/ra-fixes`; if `git worktree list` does not show it, create it with `git worktree add /mnt/Games/Scripts/Linux/ra-fixes local/fixes-2026-04`. Never under `/tmp`, which is RAM on this machine. cppcheck / clang-tidy / clazy fix bundles commit here. Build verification (`make -j$(nproc) retroarch`) runs from this worktree.

Bundle commits cross-reference each other by SHA in `docs/private/ROADMAP.md`. When asked to "fold in" or "log a bundle", write it through `roadmap_log` on the audit branch — the roadmap store is the source of truth and the file is rendered from it; when asked to fix a finding, switch to the fixes-branch worktree.

`docs/private/audit/aggregate.py` is the fork's local audit-aggregator that drives `last_audit_summary` / `audit_run` MCP integrations; `.cppcheck-suppress.txt` at repo root holds the cppcheck inline-suppression set the aggregator respects. See `docs/private/AUDIT-POLICY.md` for the false-positive-pattern +
suppression contract.
