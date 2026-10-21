**Subject line count:** `2026-10-03-self-updater.md` has 337 lines as I read it with native `Read` (the scrubbed copy, which the brief says keeps the original's line numbers).

**Already in my context on arrival, before I read the subject:** the global `~/.claude/CLAUDE.md`, the user's email address in a context block, a SessionStart hook note about the Ants MCP verbs, and the environment block. I had no project `CLAUDE.md`, no memory index and no git snapshot.

**Disagreements between my brief and my agent file:** none found, so I resolved none.

## Findings

- **[Q2]** `/mnt/Games/Scripts/Linux/RetroArch/docs/private/specs/2026-10-03-self-updater.md:241`: "A failed undo never costs the old file." This conflicts with the only recovery rule the spec gives, at line 144: "The exception is a `p.retroarch-old` whose `p` is missing … start-up renames it back to `p` instead". It also conflicts with line 254: "the next start puts the old file back (INV-16)".
  - The undo puts a placed file back first, then the set-aside one (`su_undo`, `self_update.c:583-595`, a 173-line window starting at 575). If the first undo rename fails, `p` still holds the **new** file and `p.retroarch-old` holds the old one. That is the only copy of the old file.
  - In that state `p` is not missing, so line 144's rule deletes `p.retroarch-old` at the next start. The next `self_update_replace` would also delete it (`self_update.c:622-623`).
  - → An implementer would build a rename-back only for a missing `p`. The install would stay mixed old and new, and its old copy would be deleted, breaking INV-16's headline and INV-9.
  - → fix: say what cleanup does when both `p` and `p.retroarch-old` exist after a failed undo, and record which file was not undone. Or narrow INV-16's headline and the §6 row to the missing-`p` case.

- **[Q1]** `…/2026-10-03-self-updater.md:149`: "Write the pending marker, then write `%TEMP%\retroarch-update\run.cmd`". The code does the opposite. It launches the installer first and writes the marker after (`tasks/task_self_update.c:1000-1004`, a 45-line window starting at 1200). Its comment says: "After the launch: a marker for an installer that never started would report an update that did not finish."
  - → A builder following §4.6 would write the marker before launching. A failed launch would then make the next start report "did not finish" for an update that never began.
  - → fix: change step 2 to "write `run.cmd` and start it; only once it has started, write the pending marker". The code side is in Open questions.

- **[Q3]** `…/2026-10-03-self-updater.md:241`: "`self_update_replace` returns a result apart from a clean rollback". The spec never names that value. Today `self_update.h:531-534` (a 196-line window) says "-1 is returned" for every failure.
  - Three pieces of code must agree on the new value: `self_update_install_files`, which picks the user's message; the `replace` lane; and the header comment.
  - → The new value is invented while building, and the header's "-1 is returned" stays true for only some failures.
  - → fix: name it, for example "-1: rolled back cleanly; -2: one or more undo renames failed", and say it updates the header comment.

- **[Q2]** `…/2026-10-03-self-updater.md:230`: lane `cleanup` "asserts the start-up cleanup removes them all". Lines 144 and 242 say a `.retroarch-old` whose original is missing is renamed back, not removed.
  - In the existing fixture, three of the four planted `.retroarch-old` files have no original (`self_update_test.c:759-762`, an 80-line window starting at 728). Under INV-16 those get renamed back. Its `olds_in()==0` check passes whether the file was deleted or renamed back.
  - → A builder keeping INV-13's wording leaves the delete path covered by one file only, `retroarch.exe.retroarch-old`, and the existing checks cannot distinguish a delete from a rename-back.
  - → fix: INV-13: "removes every `*.retroarch-old` whose original exists"; the fixture should plant originals for the delete cases.

## Confirmed

- I opened `self_update_replace` and `su_undo` (`self_update.c:597-652`, `583-595`). The undo goes in reverse, places before setting aside, and returns -1 on every failure, as §4.6 step 4 and the packet facts say.
- I opened `self_update_cleanup` and `su_is_staging_name`. Only the two staging names are removed, which matches line 144.
- I opened `self_update_marker_consume`. The `current >= target` comparison matches §4.7.
- I opened the portable and AppImage install order (`task_self_update.c:1010-1014`). The marker is written and staging removed after install, which matches §4.6 step 5.
- I opened the `lane_replace` fail points {1, 4, 9} against 9 renames. They match INV-9's first, middle and last.
- I worked the memory sums by hand. Linux comes to 779,341,072 and Win64 block 1 to 579,456,478; both match line 132 and the test constants. I did not check block 0, because its largest needed member is not in the packet.
- I ran no searches, so `exclude_glob` never applied. I read nothing outside the packet and the subject.

## Open questions

- **Code side of the marker-order finding:** is the code's "launch, then marker" order the intended one? If so, only the doc changes.
- **Undo rename on Windows:** when the first undo rename fails, does the second (`old` → `cur`) fail, given that `MoveFileW` has no replace flag? That decides whether `p.retroarch-old` survives in that state. This is Windows runtime behaviour, so I judged it only for internal consistency.

## NEEDS MEASUREMENT

None.
