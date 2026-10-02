# PR draft — damaged replay checkpoint: six more memory errors

Branch `pr/replay-checkpoint-dedup` in `/mnt/Games/Scripts/Linux/ra-pr`,
stacked on `pr/replay-checkpoint-bounds` (#19683): `5d702ed87d` on
`e99abce20f` (RETR-0019). Opened 2026-10-02 as
libretro/RetroArch#19685. The fork fixes are `5faedc02c4` and
`2d368a11a3` on `local/fixes-2026-09`.

Second commit `f99ba62444`, pushed 2026-10-02 (fork `04868b553f`): the
CI step running `bsv_replay_init_test` under ASan lacked
`allocator_may_return_null=1`, so the "Build and run samples/tasks"
check failed with allocation-size-too-big on the legacy-frame case.
The step now sets it, as the samples/gfx steps do. Reproduced with
CI's old options, then passing with the new ones. The same run's
"Build and run samples/gfx" failure was an implicit
`retro_atomic_exchange_int` declaration in upstream's code at that
moment; master's later samples/gfx run passed.

## Title

replay: six more damaged-checkpoint memory errors

## Body

Builds on #19683, whose commit is the first of the two here; once that
merges, this PR is the second commit alone.

Each defect is reachable from a damaged `.replay` file, or from replay
data inside a loaded savestate:

1. **Over-read, uncompressed statestream checkpoint.** The decoder is
   handed the header's encoded length over a buffer sized by the stored
   length. An encoded length above the stored one is now refused. The
   writer always stores the two equal when uncompressed.
2. **`superblock_seq` overflow.** The array is allocated once, at the
   first sequence's length, and later sequences write up to their own.
   Its length is now recorded (`superblock_seq_len`) and the array
   regrows on both the read and write side. The frees keyed on
   `last_save_size` go.
3. **NULL read on an undefined index.** A sequence naming a superblock,
   or a superblock naming a block, that the file never defined makes
   the index lookup return NULL, and the decoder read through it. Both
   are refused, and a failed decode clears `cur_save_valid`.
4. **`last_save_size` describing the wrong buffer.** Loading a
   checkpoint set it to `cur_save`'s size. `last_save` is the
   recorder's other buffer, swapped back in on the next write, so it
   could then claim more than it holds. Loading no longer touches it.
5. **Read into a failed allocation, legacy checkpoint frame.** A size
   too large to allocate, or to fit a `size_t`, left `cur_save` NULL
   and the frame was read into it. The movie now ends instead, and
   `cur_save_size` is zeroed wherever `cur_save` is freed.
6. **Uninitialised size.** When the replay ends inside a checkpoint's
   three header words, `size` was never read, or only partly, and the
   exit block used it anyway. It is now reset on each header-read
   failure, and no size is recorded.

### Testing

`samples/tasks/bsv_replay_init`'s Makefile gains `STATESTREAM=1`, and
`make sweep` now also runs that build under ASan/UBSan, with
`allocator_may_return_null=1` so a failed allocation is seen as NULL. A
new `statestream` lane covers 1-3; the `checkpoint` lane gains cases
for 4-6. Its "decodes past its state" case moves to Zstandard, so it
still reaches the RAW size check from #19683 instead of stopping at
item 1's.

- Against #19683's code, each new case fails, run alone so ASan
  reaches it: heap-buffer-overflow (1, 2), SEGV on NULL (3 twice, 5),
  `last_save_size 4096 for a 16-byte buffer` (4). Under valgrind, 6
  reports an uninitialised read at the exit check, and the lane fails
  with a garbage `cur_save_size`.
- With the fix, `make sweep` passes all lanes in all three builds, and
  the plain build runs clean under valgrind.
- Each loading-side part of the fix, reverted alone, fails the sweep
  or valgrind (checked on our fork, which carries the same code). The
  recording-side regrow of `superblock_seq` has no test.
- Cases 1 and 2 are told apart only by ASan; without a sanitizer they
  pass either way. `make sweep` runs them under ASan.
- Full `make -j4` on Linux, no new warnings. Merges cleanly with
  #19681.

Made with Claude Code, reviewed and build-tested on our fork.

## Notes (not in the PR body)

- One cold read of the five-fix version found no defect in those fixes.
  It found item 6, which was then added, and noted that cases 1 and 2
  discriminate only under ASan (now said in the body).
- Not fixed, recorded on RETR-0019: after a load while recording,
  `superblock_seq` describes `cur_save` while the writer compares
  against `last_save`; and `uint32s_index_get`'s garbage-collected log
  loop starts one past the end of `additions`.
