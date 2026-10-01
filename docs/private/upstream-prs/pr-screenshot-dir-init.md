# PR draft — task_screenshot: initialise new_screenshot_dir before it is tested

Branch `pr/screenshot-dir-init` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit (`bfa74954e6`) on upstream/master `6bf58823c6`. Fork commit
`6ccad2ebe1` carries the same fix. Found by the 2026-10-01 survey of
fork-only fixes (RETR-S0115).

## Title

task_screenshot: initialise new_screenshot_dir before it is tested

## Body

`screenshot_dump` writes `new_screenshot_dir` only when `screenshot_dir`
is set, then tests `!*new_screenshot_dir` to fall back to the content
directory. With the screenshot directory left at its default (empty),
that test reads uninitialised stack. When the leftover byte is not zero,
the screenshot goes to a directory named from garbage bytes, and
`path_mkdir` creates that directory.

The fix sets `new_screenshot_dir[0] = '\0'` before the branch.

### Testing

Linux, null drivers, `screenshot_directory = ""`, fceumm with an NES zip,
`--max-frames=30 --max-frames-ss`, under valgrind:

| Build | valgrind | Screenshot written to |
|---|---|---|
| master | 55 errors in 35 contexts, all in `screenshot_dump` (conditional jumps; `mkdir`, `openat` and `fstatat` on uninitialised bytes) | a new directory in the working directory, named with garbage bytes |
| this branch | 0 errors | beside the content, as intended |

Builds clean with `make`; `task_screenshot.c` compiles as gnu89 with
`-Werror=declaration-after-statement`.

Made with Claude Code, reviewed and build-tested on our fork.
