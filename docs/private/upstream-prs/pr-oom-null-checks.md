# PR draft — check allocations that were dereferenced unchecked

Branch `pr/oom-null-checks` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit on upstream/master `861bd6a089`: `b8c508c438` (RETR-0019). Opened
2026-10-02 as libretro/RetroArch#19681.

## Title

Check allocations that were dereferenced unchecked

## Body

Each of these writes through an allocation result without checking it,
so an allocation failure crashes instead of failing cleanly:

- `menu_driver.c`: `driver_ctx->init()` returns NULL when an allocation
  fails (xmb, ozone, rgui and materialui all can), but
  `menu_driver_init_internal` wrote through it before its own
  `!menu_st->driver_data` check.
- `task_screenshot.c`: `task_init()` returns NULL on OOM; the threaded
  screenshot path wrote through it. It now frees the state and fails.
- `bsvmovie.c`: `bsv_movie_load_checkpoint` read into `handle->cur_save`
  and `compressed_data` without checking either `malloc`, and
  decompressed into an unchecked `calloc` on the zlib and zstd paths.
- `uint32s_index.c`: `uint32s_bucket_expand` copied into an unchecked
  `calloc`, and assigned `realloc`'s result over its only pointer to
  the old block. On failure the bucket is now left as it was.

### Testing

Linux: full `make -j4`, no new warnings. `bsvmovie.c` also compiled
with `-DHAVE_ZLIB` (off in this build). The failure paths were not
exercised at runtime.

Made with Claude Code, reviewed and build-tested on our fork.
