**Subject line count:** `2026-10-03-self-updater.md` came to **338 lines** when read with native `Read`. Your brief says 337. Most likely the file has no trailing newline, but please check it against disk.

**In my context on arrival, before reading the subject:**
- the global `~/.claude/CLAUDE.md`
- the user's email address, from the harness context block
- a SessionStart hook note saying the Ants MCP is connected

There was no project `CLAUDE.md`, no memory index and no git snapshot.

**Disagreements between brief and agent file:** none came up.

## Findings

- **[Q2]** `/mnt/Games/Scripts/Linux/RetroArch/docs/private/specs/2026-10-03-self-updater.md:141`
  - Quote: "the report says the update was only partly undone and that restarting RetroArch finishes the undo (INV-16)"
  - Conflict: the same step's undo can leave `<exe dir>\retroarch.exe` missing, so there is nothing to restart.
  - How: `su_undo` (packet window `self_update.c` 577–767, of 172 shown) first renames the placed new file back to staging (`cur`→`src`), then the old file back (`old`→`cur`). If the second rename fails for `retroarch.exe`, `p` is absent and the old build sits only in `retroarch.exe.retroarch-old`.
  - The restart in step 5 (line 142) uses the captured `retroarch.exe` path, which is now absent. So the "next start" that INV-16 (line 241) relies on to put files back never happens.
  - The repair at line 144 has the same gap: it renames `p` to `p.retroarch-new` and then `p.retroarch-old` to `p`. If the second rename fails for `retroarch.exe`, no runnable `retroarch.exe` is left either. The spec admits this ("a half-done repair can leave one as the only copy") but still relies on a further start.
  - → An implementer would build a message and a repair path that can never run in exactly the case it exists for, and would not order the undo or repair so that `retroarch.exe` is always restorable.
  - → Fix: state what happens when the failed rename is `retroarch.exe`. For example, undo `retroarch.exe` first and retry it, or tell the user to rename `retroarch.exe.retroarch-old` by hand. Make INV-16's test plant that case.

- **[Q3]** line 144, quote: "When the rollback record exists, start-up repairs instead of deleting: for each `p.retroarch-old` it renames `p`, if present, to `p.retroarch-new`, then renames `p.retroarch-old` back to `p`."
  - The repair cannot tell a `.retroarch-old` made by the interrupted replace from a stale one left by an earlier update. A stale one survives when it was "still in use" at a start (line 144, "a file still in use is left for the next start").
  - Step 3 removes a stale one only for files the loop actually reaches (`self_update.c`, window line 486, "A stale one from an earlier update goes first").
  - So after a crash or a failed undo part-way through, the repair would move current files aside and restore versions from two updates back. That is the mixed install INV-16's *Breaks when* forbids.
  - → An implementer would build a repair that downgrades files the interrupted replace never touched.
  - → Fix: either have `self_update_replace` remove (or refuse on) every stale `*.retroarch-old` in the program set before it makes the record, or say the repair only acts on files the record lists.

## Confirmed

- Opened the packet windows and checked them against the spec:
  - `MoveFileW` is called with no flags.
  - The staging-name guard (`.retroarch-update` / `retroarch-update`) matches line 144.
  - The cleanup walk covers the exe folder without recursing, and `filters`/`platforms` recursively.
  - The marker's compare (≥ means success) and its removal-before-parse behaviour match §4.7 and the INV-11 test order.
  - The file-operation table has a folder-making operation and no write operation, as line 140 says.
- Recomputed the §4.5 sums: Linux 779,341,072 and Win64 block 1 579,456,478 both match. Win64 block 0's 749,876,939 implies a largest needed member of 1,824 B, which I could not check.
- Skipped the loop-log section. I ran no searches, so the `exclude_glob` exclusion never came into play.

## Open questions

- In the Win64 block 0 sum (line 132), is the largest needed program-set member really 1,824 B? That is the residual implied by the sum.
- The facts file describes the rollback record and the repair as not yet built. I found no packet fact that contradicts the spec.

## NEEDS MEASUREMENT

None.
