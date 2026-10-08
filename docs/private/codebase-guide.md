# Codebase guide (fork)

Task-specific notes on the RetroArch codebase. The repo-root `CLAUDE.md` says when to read each section.

## Build variants

### Griffin (unity build)
Console targets (`Makefile.psp1`, `Makefile.ctr`, `Makefile.ps2`, `Makefile.wii`, `Makefile.wiiu`, ...) build via **`griffin/griffin.c`**, which `#include`s the .c sources directly (`-DHAVE_GRIFFIN=1`). When you add or rename a source file, neither build picks it up on its own: add its `.o` to `Makefile.common` under the right `HAVE_*` block, and — if it should build on consoles — add it to `griffin/griffin.c` (or `griffin_cpp.cpp` / `griffin_objc.m`), or **griffin builds will silently miss it**. Verify every file meant for both appears in both lists.

### Other variants (one-liners)
- **macOS bundle** — `make bundle` after `make` produces ad-hoc-signed `RetroArch.app`; min-OS derived from `-mmacosx-version-min`. `BUNDLE_*` overrides at the bottom of the root `Makefile`.
- **ANGLE** — `HAVE_ANGLE=1` flips `TARGET` to `retroarch_angle` (Win32 GLES via ANGLE). Don't hardcode the binary name in scripts.
- **Qt frontend** — built only when `HAVE_QT=1`; pulled in via `MOC_SRC`/`MOC_OBJ`. Default off.

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

## Versioning & release notes

- Version lives in `version.all` (a C/Make/shell polyglot). The lockstep set is every file a search for the current version string finds outside `deps/`, `libretro-common/`, `intl/` and `docs/private/`: `version.all` (`PACKAGE_VERSION`), `version.dtd`, `com.libretro.RetroArch.metainfo.xml` (`<release version="…" date="…">` block — newest entry), and the platform manifests under `pkg/`.
  - `version.all`'s own top-of-file comment names `pkg/snap/snapcraft.yaml`, but that file does **not** exist in this tree (snap packaging lives elsewhere); ignore that line of the comment unless snap is re-introduced.
