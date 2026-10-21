Subject file: `2026-10-03-self-updater.md` has **337 lines**, read in full with native `Read` from the scrubbed copy. Findings cite the original path; the line numbers match.

**Already in my context before I read the subject:**
- the global `~/.claude/CLAUDE.md`;
- my agent definition (review-lane);
- a SessionStart hook note saying Ants MCP is connected;
- environment details: date 2026-10-22, working directory `/home/ants/.cache/lane-neutral`, which is not a git repository;
- no memory index, no git snapshot, no project `CLAUDE.md`.

This was a depth pass: the brief has no breadth marker. I found no disagreement between the brief and my agent file.

## Findings

- [Q4] `/mnt/Games/Scripts/Linux/RetroArch/docs/private/specs/2026-10-03-self-updater.md:242` — "a second call returns the same result and renames nothing" — INV-16 says it breaks when (line 243) "a retry before restart deletes a set-aside old file". The test cannot catch that break, because it checks only renames after the second call. Its "`p.retroarch-old` still holds the old bytes" assertion comes before the second call. In this test, removes work.
  → An implementer could check for the record *after* the stale-`*.retroarch-old` sweep in step 3 (line 140). The retry would then delete the set-aside old files and still return the incomplete-rollback result having renamed nothing, so the test passes on the very break it names.
  → fix: append to that clause: "…renames nothing and deletes nothing: `p.retroarch-old` still holds the old bytes after the second call."

- [Q4] `…self-updater.md:144` — "When it did, start-up removes the pending marker without its message and says instead that the update did not finish, the previous version was put back, and RetroArch must be restarted" — none of INV-16's statement (line 241), its test (line 242), INV-11 or §10 covers this start-up path. INV-16's test checks only that cleanup "reports a repair", and the marker lane (line 222) has no repair case. Yet §10 (line 312) lists INV-16 as fully caught by lanes `replace` and `cleanup`, not as "Partial".
  → A builder could wire `task_self_update_startup` to show the marker's normal "RetroArch was updated to X" after a repair. That is exactly the "user is told nothing changed" break on line 243, and no lane would fail.
  → fix: add the marker-suppression clause to INV-16 with a lane (marker present plus a repair → no success message, marker gone), or mark INV-16 "Partial" in §10 and name the gap.

## Confirmed

- I passed no `exclude_glob` because I ran **no searches at all**. Everything below comes from the subject and the packet's windows.
- Checked against code:
  - `su_fs_rename` calls `MoveFileW(wfrom, wto)` with no flags (`task_self_update.c:665-676` window), as line 144 says.
  - The fileops table has `mkdir_p` and no write op (`self_update.h` window), so line 140's "can make a folder but not write a file" is true.
  - The start-up sweep matches the code: `exe_dir` top level only, `filters`/`platforms` walked recursively, staging folders removed only under the two names (`self_update.c` `self_update_cleanup`, `su_is_staging_name`).
  - Marker: `cmp(current, v) >= 0` gives UPDATED, matching §4.7 and INV-11's "success, success, failure" rows.
  - Installer route writes the marker only after a successful launch (`task_self_update.c:1225-1246` window).
  - The AppImage swap copies the mode and then renames.
- Arithmetic: the §4.5 sums for Linux (779,341,072) and Win64 block 1 (579,456,478) add up from the stated sizes. Win64 block 0 implies a largest member of 1,824 B; I did not check that figure.
- INV-16's "removes failing → no record exists" row forces the sweep to run before the record is made, which agrees with line 140's "Otherwise it first deletes".

## Open questions

- Line 141 says an undo never leaves `p` missing. But the `replace` lane's `fs_rename` is POSIX `rename`, which overwrites an existing target, while production `MoveFileW` refuses one. So line 242's case of a "failure on the move back to staging" behaves differently in the lane than on Windows. Is that difference intended, given that only the cleanup lane's fake rename is told to refuse an existing target?
- Line 144 says "a half-done repair can leave one as the only copy". If both repair renames fail, `p` is absent. That seems to conflict with INV-16's "No … repair step leaves `p` missing" (line 241). It is only material if a double failure is meant to be in scope.

## NEEDS MEASUREMENT

None.
