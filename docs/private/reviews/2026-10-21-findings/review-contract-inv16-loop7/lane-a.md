**Subject line count:** `2026-10-03-self-updater.md`, read with native `Read`: 337 lines. That matches the brief.

**Already in my context on arrival:** the global `~/.claude/CLAUDE.md` and my agent definition (`review-lane`). I also had the environment block (working directory `/home/ants/.cache/lane-neutral`, not a git repository, so no git snapshot), a SessionStart hook message saying Ants MCP is connected, and the user's email address. There was no project `CLAUDE.md` and no memory index. The brief came next, then the packet inside it.

**Disagreements between my brief and agent file:** none needed resolving. I did not open the facts file: the packet quotes all of its lines.

## Findings

None.

## Confirmed

- **§4.5 memory sums.** I recomputed them by hand from the stated sizes:
  - Linux comes to 779,341,072 B.
  - Win64 block 1 comes to 579,456,478 B, using the doubled unpacked size plus `retroarch.exe`'s 18,641,482 B.
  - Win64 block 0 matches only if its largest needed member is 1,824 B. I could not check that member size; see Open questions.
- **§4.2's list of file operations** matches `struct self_update_fileops` in `self_update.h`. It has no file-write operation, which supports §4.6's reason for making the record a folder.
- **Pending marker, §4.7 and INV-11.** `PACKAGE_VERSION` ≥ target means "updated", and the marker is removed. This matches `self_update_marker_consume` (`cmp(current, v) >= 0`, removal before parsing). The test's rows (below → success, equal → success, above → failure) agree with it.
- **§4.6 installer route, step 3.** The marker is written only after the launch succeeds, matching `task_self_update.c:1225-1246`. On the portable and AppImage routes the marker is written and then staging is removed, matching the same window.
- **§4.6 cleanup.**
  - It walks the program folder without recursing, and `filters/` and `platforms/` with recursion, matching `self_update_cleanup`.
  - Staging is removed only under the two folder names (`su_is_staging_name`).
  - A file still in use is left alone, because each remove's result is ignored.
  - The root walk passes over folders, so the new `.retroarch-rollback` record folder would not be swept.
- **§4.6's statement that `su_fs_rename` calls `MoveFileW` with no flags** matches the window and the packet's fact.
- **INV-16 against its other appearances.** It agrees with §4.6 steps 3–4, the start-up repair paragraph, the §6 rename row, the §7 lane list and the §10 table. I found no pair of passages that would lead a builder to different behaviour.
- **Searches:** I ran none, so no `exclude_glob` was needed. I did not read the loop log; its section in the copy says only "withheld".

## Open questions

- **Old files that fall outside the current update.** While the record exists, start-up repair renames *every* `p.retroarch-old` in the program folder, `filters/` and `platforms/` back to `p`. Suppose a file is in neither the previous update's set nor this one's, and its `.retroarch-old` copy was still in use at an earlier start, so it survived. Repair would then put that stale old file back over the current `p`. Is that case meant to be reachable? If so, the repair would need to be limited to this update's files, which it has no list of.
- **§4.6 line 144's reason for keeping `.retroarch-new` files.** It says they are deleted only when no record exists, "since a half-done repair can leave one as the only copy". I could not build a sequence where a `.retroarch-new` file is the only copy of anything except the *new* build; the old file always remains in `p.retroarch-old`. The rule builds the same either way, but the reason may be wrong.
- **§4.6 line 144 versus the existing code.** The production `mkdir_p` (`su_fs_mkdir_p`) succeeds when the folder already exists. So the "record already exists" check has to be a separate `exists` call before the folder is made. That is a detail for the builder, not a defect in the spec. On the code side: `su_undo` throws away rename results and `self_update_replace` returns only 0 or -1, but the packet's facts say that gap is expected.
- **Win64 block 0's largest needed member** would have to be 1,824 B for the stated 749,876,939 B total. Nothing in the packet confirms that figure.

## NEEDS MEASUREMENT

None.
