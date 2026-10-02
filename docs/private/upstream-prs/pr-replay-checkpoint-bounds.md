# PR draft — damaged replay checkpoint: double free and buffer overrun

Branch `pr/replay-checkpoint-bounds` in `/mnt/Games/Scripts/Linux/ra-pr`,
one commit on upstream/master `bd35ea5496`: `e99abce20f` (RETR-0019).
Opened 2026-10-02 as libretro/RetroArch#19683. The fork fix is
`aab917b1d2` on `local/fixes-2026-09`.

## Title

replay: damaged checkpoint no longer double-frees or overruns cur_save

## Body

`bsv_movie_load_checkpoint` reads an uncompressed RAW checkpoint
straight into `handle->cur_save`. A damaged replay file broke that in
three ways:

1. **Double free.** A checkpoint cut short takes the "Truncated
   checkpoint" exit, which frees `compressed_data`. On this path that is
   `handle->cur_save`, so the handle keeps a dangling pointer and
   `bsv_movie_free()` frees it again.
2. **Overrun.** The read length is the file's stored length, not the
   buffer's. A checkpoint stored larger than its state size writes past
   the end of `cur_save`. The RAW `memcpy` after zlib or zstd
   decompression has the same shape with the decoded length.
3. **Size larger than the buffer.** The exit block sets `cur_save_size`
   to the header's size on every path, including a skipped checkpoint
   that allocated nothing. The buffer then claims more than it holds,
   and the next load or unserialize runs past it.

The fix refuses a stored or decoded length larger than `cur_save`. At
exit it never frees the shared buffer, and it only lowers
`cur_save_size`.

### Testing

`samples/tasks/bsv_replay_init` gains a `checkpoint` lane. It feeds
hand-made damaged checkpoints to the loader: one cut short, one stored
larger than its state, one decoding larger than its state, one skipped,
and one intact.

- On master, under `make sweep`'s ASan build, the lane stops on a
  double free in `bsv_movie_free`. With the earlier cases removed in
  turn, it also reports a heap-buffer-overflow in `memstream_read` and
  `cur_save_size 4096 for a 16-byte buffer`.
- With the fix, `make sweep` passes all lanes in both the plain and the
  ASan/UBSan builds.
- Each part of the fix, reverted alone, fails the lane.
- Full `make -j4` on Linux, no new warnings.

The RAW `memcpy` overrun after zlib/zstd decompression is fixed, but no
test case covers it; that would need a compressed damaged checkpoint.

Made with Claude Code, reviewed and build-tested on our fork.

## Notes (not in the PR body)

- All three failures were reproduced against master's own
  `bsvmovie.c` on 2026-10-02, removing earlier cases from a scratch copy
  of the test so ASan reaches each one.
- The undo-each-part check ran on the fork for all four hunks, and on
  this branch for the two hunks shaped differently here (the zero-copy
  check sits above the `if/else`, to stay clear of #19681's lines).
- `git merge-tree` of this branch with `pr/oom-null-checks` (#19681)
  reported no conflict.
- One cold read found no defect in the fix.
