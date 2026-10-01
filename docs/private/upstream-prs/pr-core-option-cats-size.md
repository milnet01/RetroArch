# PR draft — core_option_manager: size the categories array by category count

Branch `pr/core-option-cats-size` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit (`c5bb0425b1`) on upstream/master `6bf58823c6`. Fork commit
`b351c83c4d` carries the same fix with two others not sent here
(RETR-0019). Found by the 2026-10-01 survey of fork-only fixes
(RETR-S0115).

## Title

core_option_manager: size the categories array by category count

## Body

`core_option_manager_new` allocates `opt->cats` with `calloc(_len, ...)`,
where `_len` is the number of options, then fills it by category count.
A core that declares more option categories than options writes past
the end of that heap block.

The fix allocates `cats_size` entries. Every read of `opt->cats` is
already bounded by `opt->cats_size`.

### Testing

A minimal test core passes `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2` with
four categories and one option. Null drivers, under valgrind:

| Build | valgrind |
|---|---|
| master | 35 errors in 13 contexts, all invalid writes in `core_option_manager_new` |
| this branch | 0 errors |

fceumm, which declares option categories, also loads on this branch
with no valgrind errors. Builds clean with `make`;
`core_option_manager.c` compiles as gnu89 with
`-Werror=declaration-after-statement`.

Made with Claude Code, reviewed and build-tested on our fork.
