# PR: runloop: the quit controller combo counts once per press

Opened 2026-10-05 as libretro/RetroArch #19711, branch `pr/quit-combo-confirm` (`796ebb9518`), on upstream master `e6ba0e6e2f`.

## Body

## Problem

Fixes #19642. With "Confirm Quit" on, the quit controller combo (for example L2+R2) quits at once instead of asking for a second press.

`input_driver_button_combo()` is true on every frame the combo is held, but the quit key is taken only on the frame it goes down. So the first frame of a press shows "Press again to quit", and the next frame, a few milliseconds later, counts as the second press.

## Fix

The combo counts only on the frame it is first held, as the key does (`old_quit_combo` beside `old_quit_key`). The hold combos (Hold Start, Hold Select) already report a single frame, so they behave as before.

## Testing

Linux, with the test joypad driver: null video, audio and input; `input_quit_gamepad_combo = "10"` (L2+R2); 2048 core. Script steps press and release L2+R2 (`param_num` 12288):

- One press held for 60 frames: on master RetroArch exits before the release; with this change it shows the prompt and keeps running.
- Two presses: quits on the second press.
- `confirm_quit` off: one press quits, as before.

Full `make` build, and `runloop.c` compiles with `C89_BUILD=1`, no warnings.

This change was found and written with the help of Claude Code (an AI assistant), then tested on a fork. Happy to adjust anything.
