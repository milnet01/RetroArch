# PR draft — runloop: GET_LANGUAGE reports unavailable without HAVE_LANGEXTRA

Branch `pr/get-language-default` in `/mnt/Games/Scripts/Linux/ra-pr`,
one commit on upstream/master `861bd6a089`: `8f9c5f5869` (RETR-0019). Opened
2026-10-02 as libretro/RetroArch#19682.

## Title

runloop: GET_LANGUAGE reports unavailable without HAVE_LANGEXTRA

## Body

Without `HAVE_LANGEXTRA`, the `RETRO_ENVIRONMENT_GET_LANGUAGE` case
writes nothing to `data` and still returns true. The core then reads
its own uninitialised variable as the configured language.

Those builds have no language setting, so the call now returns false.
`libretro.h` tells cores to fall back to the OS language, or English,
when the call is unavailable.

Builds without `HAVE_LANGEXTRA` include `Makefile.ctr`,
`Makefile.psp1`, `Makefile.ps2`, `Makefile.ngc`, `Makefile.psl1ght`
and `Makefile.emscripten`, which never set it, and `./configure` on
DOS and PowerPC macOS (`qb/config.libs.sh`).

### Testing

Linux: full `make -j4`, no new warnings, and `runloop.c` compiled by
hand with `HAVE_LANGEXTRA` undefined, `-Wall`, no warnings. Not run on
an affected platform.

Made with Claude Code, reviewed and build-tested on our fork.
