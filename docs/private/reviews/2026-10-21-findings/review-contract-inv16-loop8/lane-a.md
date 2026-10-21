**Subject file, as read:** `2026-10-03-self-updater.md` has **338 lines** in my native `Read`. The brief says 337. Line 338 holds content (the §14 restart-arguments bullet), so one of the two counts is wrong. Please check it against the original on disk.

**In my context on arrival:**
- the global `~/.claude/CLAUDE.md`
- a userEmail context block
- a SessionStart hook message about the Ants MCP
- environment details
- the agent definition

There was no project `CLAUDE.md`, memory index or git snapshot.

## Findings

- **[Q3]** `docs/private/specs/2026-10-03-self-updater.md:141` and `:242`.
  - The spec says what happens when the old file's rename back fails: "when the old file cannot be renamed back under `p`, the new file just moved to staging goes back to `p`".
  - It does not say what happens when the *first* undo step fails: moving the new file back to staging.
  - INV-16's test pins an outcome for that case: "once on the move back to staging … `p` exists, and `p.retroarch-old` still holds the old bytes".
  - That cleanup lane's fake rename "refuses an existing target, as `MoveFileW` does". The replace lane gets no such instruction. The current test `fs_rename` (`samples/tasks/self_update/self_update_test.c`, lines 1035–1041 of the packet window) calls POSIX `rename`, which overwrites.
  → If the builder still tries the old-file rename after the move to staging fails (a fair reading of "undo every rename already made, in reverse"), the POSIX rename overwrites `p` with the old bytes and removes `p.retroarch-old`. The test as specified then fails. The code and the test can each be built correctly from the spec and still disagree.
  → fix: in §4.6 step 4, say "when the new file cannot be moved back to staging, the old file's rename back is not attempted". Or require the replace lane's fake rename to refuse an existing target too.

- **[Q2]** `docs/private/specs/2026-10-03-self-updater.md:144`, which says "So a record that a successful replace could not remove ends as a reported undo, not a mixed install". INV-16 at `:243` agrees: "*Breaks when:* … leaves a mixed install in place".
  - The repair the same passage specifies works only from `p.retroarch-old` files: "for each `p.retroarch-old` it renames `p` …".
  - A file that is new in the release has no `.retroarch-old`. Step 3 creates nothing for it, and the test set includes `filters/new/Added.filt`. The record is an empty folder, so nothing tells start-up that such a file exists.
  → A builder following the mechanism leaves the new release's added DLLs and filters beside the restored old build: a mixed install, the very thing INV-16 says it prevents. No INV-16 test plants an added file, so nothing would catch it.
  → fix: either narrow the claim to "every replaced file is restored; files added by the release remain", or have the record list the files the replace added (for example, empty sub-folders named after them) and have the repair remove them.

## Confirmed

- I opened `self_update_marker_consume` (`self_update.c`; its window starts at line 543). Its result matches §4.7 and INV-11: `cmp(current, target) >= 0` means UPDATED, and the marker is removed before it is parsed.
- I checked that `su_fs_rename` calls `MoveFileW` with no flags (`tasks/task_self_update.c`, window from line 300), as §4.6 says.
- §4.2's list of file operations matches `struct self_update_fileops` (`self_update.h`, window from line 175). It has a `mkdir_p` but no file write, as §4.6 says.
- The marker is written only after the installer has launched (`task_self_update.c`, window from line 1225), as §4.6 says.
- The cleanup walks `<exe dir>` without recursing, and `filters`/`platforms` with recursion. The staging-name guard matches §4.6.
- I recomputed §4.5's memory sums. Linux (779,341,072 B) and Win64 block 1 (579,456,478 B) check out. For Win64 block 0 (749,876,939 B), the stated total implies the largest needed member is 1,824 B, which no source I was given states.
- I ran no searches, so the `exclude_glob` rule never came up. I read only the brief and the subject.

## Open questions

- Win64 block 0: is the largest program-set member really 1,824 B? Any other size makes the 749,876,939 B figure wrong.
- The spec never says what values `self_update_replace` returns for its three outcomes (success, clean rollback, incomplete rollback). It also never says how `self_update_cleanup` reports a repair; the function currently returns nothing. I treated both as choices the builder makes locally, because the code that uses them is written in the same change. If the orchestrator counts the test lanes as separate consumers, this becomes a Q3 finding.
- The line count differs from the brief (338 against 337), as noted at the top.

## NEEDS MEASUREMENT

None.
