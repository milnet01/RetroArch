# PR draft — replay index: collected-block lookup reads past the additions log

Branch `pr/replay-index-gc` in `/mnt/Games/Scripts/Linux/ra-pr`, stacked
on `pr/replay-checkpoint-dedup` (#19685): `28d8ecbc23` on `f99ba62444`
(RETR-0019). The fork fix is `ecd66eb3c7` on `local/fixes-2026-09`.
**Not pushed, not opened**: waiting on the user's pick (2026-10-02).

Stacked rather than standalone because the test needs the
`STATESTREAM=1` build of `samples/tasks/bsv_replay_init`, which #19685
adds; upstream master's test builds without `uint32s_index.c`. The fix
itself applies to master unchanged. Alternative: add the commit to
#19685 instead of a third stacked PR.

## Title

replay: collected-block lookup reads past the additions log

## Body

Builds on #19685 (itself on #19683); once those merge, this PR is its
last commit alone.

`uint32s_index_get`, asked for a block the index has garbage collected,
walks the additions log backwards to log the frame the block came from.
The walk started at `additions[RBUF_LEN]`, one past the end, and never
looked at `additions[0]`. When the log is full, that read lands past its
allocation. The walk now indexes from `i - 1`.

The frame is a `uint64_t` printed with `%ld`; it is now printed as
`unsigned long`, which is right on LLP64 and 32-bit targets too.

### Testing

New `index_gc` lane in `samples/tasks/bsv_replay_init` (built with
`STATESTREAM=1`): it fills the additions log while collecting block 1,
then looks block 1 up. Without the fix, the `STATESTREAM=1` ASan build
reports a heap-buffer-overflow in `uint32s_index_get`; with it, `make
sweep` passes all lanes.

Made with Claude Code, reviewed and build-tested on our fork.
