**Line counts, as read:**
- The subject, `2026-10-03-self-updater.md` (the scrubbed copy), is **337** lines.
- The brief, `shared-context.md`, is **1,194** lines. It is not the subject; I give its count only because I read it in full.

**Already in my context when I arrived, before reading the subject:**
- the global `~/.claude/CLAUDE.md`;
- the account email context;
- a SessionStart hook note saying Ants MCP is connected;
- environment details: working directory `/home/ants/.cache/lane-neutral`, not a git repo, and the date.

There was no project `CLAUDE.md`, no memory index and no git snapshot.

**Resolved disagreement:** none between the brief and my agent file. Both require the subject to be read with native `Read`, and I did that.

## Findings

- [Q2] `/mnt/Games/Scripts/Linux/RetroArch/docs/private/specs/2026-10-03-self-updater.md:241` — "A failed undo never costs the old file." This is contradicted by §4.6 at line 144: "Without the record, a `p.retroarch-old` whose `p` is missing is still renamed back rather than deleted, in case the record could not be made."
  - The fallback only covers a missing `p`. Take the case where the undo fails because the new file could not be moved back to staging. `p` still holds the new bytes and `p.retroarch-old` holds the old ones. If making the record (`mkdir_p`) fails too, the next start finds no record. `p` is present, so the ordinary rule on line 144 deletes `p.retroarch-old`.
  - The spec expects the record can fail to be made: line 144 says so itself.
  - → A builder following §4.6 would write a cleanup that deletes the only old copy. That leaves a mixed install, which is exactly INV-16's first "Breaks when" (line 243).
  - → fix: make the record **before the first rename**, remove it on success or on a clean rollback, and refuse to start the replace if it cannot be made. Then "no record" always means no rollback is pending.

- [Q3] `…/2026-10-03-self-updater.md:141` — "returns a result apart from a clean rollback". Line 241 says the same: "returns a result apart from a clean rollback".
  - The return value is never pinned. Two other pieces of code bind to it:
    - the caller `self_update_install_files` (`tasks/task_self_update.c`, the packet's window starts at line 1005). Today it maps any non-zero result to "Nothing was changed".
    - the INV-16 `replace` lane, which asserts "the incomplete-rollback one".
  - The packet's header window (`self_update.h:175-232`) shows only 0 / -1.
  - → An implementer would have to make up the value (-2, an enum, or a positive code). If the caller keeps its `!= 0` test, it still reports "Nothing was changed", which is the message INV-16's "Breaks when" forbids.
  - → fix: give the value in §4.6 step 4 (for example, "returns -2; -1 stays a clean rollback"), and say the caller maps it to the partly-undone message.

## Confirmed

- I opened the `struct self_update_fileops` window, which runs `exists, rename, remove, mkdir_p, get_mode, set_mode, list, read`. It has no way to write a file, so §4.6 is right that "the file-operation table can make a folder but not write a file". The §4.2 list of operations matches the struct.
- `su_is_staging_name` accepts only `.retroarch-update` and `retroarch-update`. This matches §4.6 line 144.
- `self_update_replace` and `su_undo` undo in reverse. A placed file goes back to staging first, then the set-aside file goes back to its name. This matches §4.6 step 4's "undo every rename already made, in reverse". `su_undo` ignores both rename results, as the facts file says.
- The start-up cleanup walks only the exe folder (not into subfolders), `filters/` and `platforms/` (with their subfolders). This matches the folders §4.6 names for the repair.
- I ran no searches, so `exclude_glob` never applied. I opened no file outside the brief and the subject.

## Open questions

- In the existing `lane_cleanup` (`self_update_test.c`, packet window from line 728), `SDL2.dll.retroarch-old`, `Echo.dsp.retroarch-old` and `qwindows.dll.retroarch-old` are planted **without** their originals. Under INV-16's no-record rule they are now renamed back, not deleted. The `olds_in == 0` check would still pass, but the label "removed" no longer describes what happens. INV-13 (line 230) says its fixture plants them "beside their originals". So the test needs rewriting to match. That is code drift, not a spec defect.
- Line 144: "It removes the record once no `.retroarch-old` is left." I take this to mean left in the three folders the cleanup walks. If it meant anywhere under the exe folder, the planted `saves/game.srm.retroarch-old` would keep the record forever. Worth one word in the spec, though I believe the walk settles it in practice.

## NEEDS MEASUREMENT

None. The facts file puts Windows runtime behaviour out of scope, so I judged the INV-16 region for internal consistency only.
