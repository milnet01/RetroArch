# File & identifier naming standard (fork)

Naming conventions that are enforced by the build system or by long-standing
tree convention. Most are implicit in `Makefile.common` / `CLAUDE.md`; this
is their single written home. Follow the surrounding tree first — when in
doubt, grep for a sibling and match it.

## 1. Drivers (the dominant pattern)

Every subsystem (video, audio, input, joypad, menu, camera, location,
record, MIDI, microphone, Bluetooth, Wi-Fi, …) follows this shape. The
directory, type and array names vary by subsystem, so match a sibling in
the same directory:

- Interface struct + vtable in the subsystem's driver header
  (`audio/audio_driver.h`, `gfx/video_driver.h`, `menu/menu_driver.h`, …).
- Each implementation is a `.c` file in the subsystem's drivers directory
  (`audio/drivers/`, `gfx/drivers/`, `input/drivers_joypad/`, …), defining
  an instance named `<subsystem>_<name>`: `audio_driver_t audio_alsa`,
  `video_driver_t video_gl2`, `menu_ctx_driver_t menu_ctx_ozone`. Joypad
  and HID drivers are the exceptions: `input_device_driver_t sdl_joypad`,
  `hid_driver_t libusb_hid`.
  Microphone drivers have no directory of their own: most are defined in
  the matching audio backend's file (`microphone_alsa` in
  `audio/drivers/alsa.c`), some in a dedicated file beside it
  (`audio/drivers/coreaudio_mic_macos.m`), and `microphone_drivers[]` is in
  `audio/audio_driver.c`.
- Registered in the subsystem's NULL-terminated registry array
  (`video_drivers[]`, `audio_drivers[]`, `menu_ctx_drivers[]`, …), gated on
  a `HAVE_*` macro.

Adding a driver: write its `.c` file named like its siblings (`alsa.c`,
`gl2.c`; joypads `<name>_joypad.c`; HID `<name>_hid.c`; record
`record_<name>.c`); add its object under the right `HAVE_*`
block in `Makefile.common`; add it to `griffin/griffin.c` if it should
build on consoles (see [`coding-standard.md`](coding-standard.md) §6);
insert the `extern` + array entry in the registry.

## 2. Platform makefiles

- Per-platform build files are `Makefile.<platform>` at repo root
  (`Makefile.win`, `Makefile.ctr`, `Makefile.libnx`, `Makefile.ps2`,
  `Makefile.emscripten`, `Makefile.apple`, …). All but the most exotic
  `include Makefile.common`.
- `Makefile.local` is a per-developer override — **never commit it.**

## 3. Version-string files (lockstep — change together or not at all)

The version lives in `version.all` (a C/Make/shell polyglot). Every file in
the lockstep set is updated in one commit. The repo-root `CLAUDE.md`
§ Versioning & release notes defines that set by a search, which
finds `version.all`, `version.dtd`, `com.libretro.RetroArch.metainfo.xml`
and the platform manifests under `pkg/`.

`version.all`'s top-of-file comment also names `pkg/snap/snapcraft.yaml`,
which does **not** exist in this tree — ignore that line unless snap
packaging is reintroduced. The `cut-release` skill automates this list where a
`.claude/bump.json` recipe exists; this fork has none checked in, so update
the set by hand (or add the recipe first).

## 4. Settings

A new user-visible setting spans many files. The repo-root `CLAUDE.md`
§ Configuration owns the search that finds every one of them.

Translatable strings are keyed by enum in `intl/msg_hash_*.h`; only the
`us` file is hand-edited — the rest come from Crowdin.

## 5. Fork-internal artefacts

- **Branches:** `local/audit-<period>` (roadmap + docs) and
  `local/fixes-<period>` (source fixes, worked in the worktree the
  repo-root `CLAUDE.md` § Fork workflow names). Anything ready for upstream is rebased + PR'd separately.
- **Fork policies/standards:** `docs/private/<TOPIC>-POLICY.md` for
  policies (`AUDIT-POLICY.md`, `DEPENDENCY-POLICY.md`);
  `docs/private/standards/<topic>-standard.md` for standards (this dir).
- **Specs, design documents and ADRs:** `docs/private/specs/YYYY-MM-DD-<slug>.md`.
- **Plans:** `docs/private/plans/YYYY-MM-DD-<slug>.md`.
- **Spec and plan review loop logs:**
  `docs/private/reviews/YYYY-MM-DD-<slug>-loop-log.md` and
  `…-plan-loop-log.md`. The repo-root `CLAUDE.md` § Fork document locations
  declares these locations.
- **Other review loop logs** (a standard, a policy, the repo-root
  `CLAUDE.md`): `docs/private/reviews/<stem>-loop-log.md`, where `<stem>`
  is the document's filename lower-cased with `.md` dropped
  (`AUDIT-POLICY.md` → `audit-policy-loop-log.md`). One named exception:
  the repo-root `CLAUDE.md` is `claude-md-loop-log.md`.
- **Audit cache:** `.audit_cache/cppcheck-b<NN>[<letter>][-<scope>].xml`,
  where `NN` is the bundle number, an optional `<letter>` disambiguates
  re-runs within a bundle (`cppcheck-b58b.xml`), and an optional `<scope>`
  names the swept subtree **or** the run phase and may have multiple
  segments (`cppcheck-b65-menudrv.xml`, `cppcheck-b62-mui-ex.xml`,
  `cppcheck-b59-fixes.xml`). All suffixes are optional — a bare
  `cppcheck-b58.xml` is valid. Keeps per-bundle sweeps distinguishable.
- **cppcheck suppressions:** repo-root `.cppcheck-suppress.txt`, one
  line-anchored entry per confirmed false positive (anchor to the line so
  a rewrite forces re-triage). See `docs/private/AUDIT-POLICY.md`.
- **Audit scope prune list:** `docs/private/audit/scope.txt`, one subtree
  prefix per line (vendored / console-only / non-source trees).

## 6. Identifiers

- Match the file's existing convention; the tree is C, lower_snake_case for
  functions and variables, `UPPER_SNAKE` for macros/enums, `<subsys>_<name>`
  for driver instances (joypads `<name>_joypad`, HID `<name>_hid`; see §1).
- No new global state without a subsystem `*_state_get_ptr()` accessor
  following the existing singleton pattern.
