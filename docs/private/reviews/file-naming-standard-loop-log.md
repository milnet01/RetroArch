# Review loop log — `docs/private/standards/file-naming-standard.md`

Review history for the fork's file-naming standard, kept outside the
standard itself. `review-contract` writes one row per loop.

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-09-25 | 3 | 2 | 1 | 0 | 0 | First gate on this file (RETR-S0146). The run is armed by `84d5339a93`, and by `71062ebbbc`, which wrote back the repo-root `CLAUDE.md` gate's findings. 3 lanes, every lane held every question. 3 verified, 0 dismissed, 3 fixed. [Q1] driver instances were given as `const <subsystem>_driver_t <name>_<subsystem>`, in §1 and again in §6. The tree uses `<subsystem>_<name>` (`audio_alsa`, `video_gl2`, `menu_ctx_ozone`), only record drivers are `const`, and joypads are `<name>_joypad` (A, B, C). [Q1] "every subsystem follows one shape" named directories, headers, types and arrays that video (`gfx/`), joypad (`input/drivers_joypad/`, `input_device_driver_t`) and menu (`menu_ctx_driver_t`, `menu_ctx_drivers[]`) do not follow (A, C). §1 now says names vary and gives the real ones. [Q2] "every file carrying the version string" was wider than the `CLAUDE.md` search it defers to (B, C). Collateral: the repo-root `CLAUDE.md` § Driver pattern step 2 carried the same false template, corrected in the same commit. Loop 2 dispatched. |
