Subject line count: `/home/ants/.local/share/claude-scratch/review-contract-2026-10-21-su/2026-10-03-self-updater.md` is **337 lines**, read in full with native `Read`.

**Already in my context when I arrived:**
- the global `~/.claude/CLAUDE.md`;
- a SessionStart hook message saying the Ants MCP was connected;
- environment details: working directory `/home/ants/.cache/lane-neutral` (not a git repo) and today's date, 2026-10-21.

I had no project `CLAUDE.md`, no memory index and no git snapshot. Before I read the subject, I read the brief file `shared-context.md` in full. It holds the packet and the facts.

## Findings

- **[Q2]** `/mnt/Games/Scripts/Linux/RetroArch/docs/private/specs/2026-10-03-self-updater.md:241` — "A failed undo never costs the old file." This clashes with the only mechanism the spec gives for keeping the old file, at `:144`: "The exception is a `p.retroarch-old` whose `p` is missing: an undo that failed left it as the only copy, so start-up renames it back to `p` instead".
  - **The uncovered case.** `su_undo` (`self_update.c`, window 575–770, 173 lines shown, around line 591) undoes in two renames: first the new file goes back to staging (`cur → src`), then the old file goes back (`old → cur`). If the *first* rename is the one that fails, the new file is still at `p` and the old file sits at `p.retroarch-old`.
  - **What happens next.** `p` exists, so the next start's cleanup deletes `p.retroarch-old`. That is the only copy of the old file. The install is left with the new build of that file mixed with old builds of the files already rolled back, which breaks INV-9 as well.
  - **Same-session retry.** In the same state, `self_update_replace` itself removes "a stale one from an earlier update" (`self_update.c:622-623`).
  - **Test gap.** INV-16's own tests (`:242`) only plant the case where `p` is missing, so they cannot catch this.
  - → An implementer would build the rename-back only when `p` is missing, and a failed placed-undo would still lose the old file at the next start.
  - → fix: say what happens when the *placed* undo fails. Either record which paths were left half-undone (for example in a file in the config directory), so cleanup restores from those rather than deleting them; or narrow INV-16's first sentence to the `p`-missing case and name the other case in §6 as accepted.

- **[Q1]** `…/2026-10-03-self-updater.md:149` — "Write the pending marker, then write `%TEMP%\retroarch-update\run.cmd`".
  - The code writes the marker *after* the launch: `tasks/task_self_update.c:1000-1004` (window 1200–1250, 45 lines shown) reads "After the launch: a marker for an installer that never started would report an update that did not finish", then calls `self_update_launch_installer(state)`, then `self_update_write_marker(state)`.
  - → An implementer following §4.6 would write the marker before launching. A launch that then failed would leave a marker, and the next start would say *the update did not finish* for an update that never began.
  - → fix: "Start `run.cmd` (step 3); only once it has started, write the pending marker; then quit."

## Confirmed

- I opened `self_update_replace`, `su_undo`, `su_walk_cb` and `self_update_cleanup`.
  - Cleanup today deletes every `*.retroarch-old`: top level only in `exe_dir`, recursively under `filters`/`platforms`.
  - Staging folders are removed only when named `.retroarch-update` or `retroarch-update`. This matches §4.6.
- I opened `self_update_marker_consume`. Its compare (`cmp(current, v) >= 0` → updated) matches §4.7.
- I opened the AppImage path in `self_update_install_files` and the INSTALL stage. The order — flush to disk, swap with mode bits, write marker, remove staging — matches §4.6 AppImage steps 2–4 and portable step 5.
- Memory sums: I added up the Linux figure (779,341,072) and Win64 block 1 (579,456,478) myself. Both match §4.5 and the test constants.
- `lane_cleanup` already plants `ra/SDL2.dll.retroarch-old` with no `SDL2.dll`. That is consistent with INV-16's planned cleanup case.
- I ran no searches, so no `exclude_glob` was needed. I opened no file outside the packet and the subject.

## Open questions

- I don't have the body of `self_update_launch_installer` (not windowed). Does it write `run.cmd` itself? That would confirm the order the code uses.
- §4.5's Win64 block-0 sum (749,876,939) implies the largest needed member in block 0 is 1,824 B. I could not check that from the packet.
- Code side of the first finding: `su_undo` ignores the result of its first rename and attempts the second anyway. Whether that should change is for the code owner.

## NEEDS MEASUREMENT

None.
