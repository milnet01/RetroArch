## Message 2 — synthesis

### 1. What each subsystem does, in plain words
- **Policy core (L1):** the part of RetroArch's self-updater that makes the decisions. It works out what kind of install is running, whether a newer stable release exists and where to get it, and whether there is enough memory to unpack it. It swaps old program files for new ones and puts them back if anything fails, but never touches the disk or network itself. The 7z changes let it ask an archive how much memory unpacking needs, and fix the decoder so each new block lands after the last one.
- **Tasks and glue (L2):** the part that does the work. It asks GitHub for the newest release, sizes the download, and on "Update Now" downloads, checks the checksum, unpacks, and either swaps the files or runs the Windows installer after RetroArch closes. At start-up it clears leftovers and says whether the last update worked. A separate small piece remembers which Content Downloader items finished, so they keep their mark after an archive is unpacked and deleted.

### 2. Fix first
None. No lane rated a finding HIGH. The two MEDIUM findings still open: L1-2 (staging names copied in self_update.c:723-724) and L2-1 (per-kind file facts restated at task_self_update.c:277 and :1419).

### 3. Correctness leads for review-code (ungraded)
- `self_update.c:418-427` — the memory sum adds unpack_size twice for a multi-coder block; BCJ2's four inputs may exceed that. Premise: terms[3] = unpack_size confirmed by read; whether BCJ2 inputs can exceed it not checked. **New**: review-code 2026-10-21 lane-01 lists memory_ok's sum in its coverage and raised nothing.
- `self_update.c:500-554` — the marker's second line (install kind) is written but never read. Premise **confirmed**: self_update_marker_consume reads only the first line; `grep -rn '\.pending'` finds the name only at task_self_update.c:585. New.
- `self_update.c:630` — `if (i < count)` on the failure path is always true. Premise confirmed: the four `goto fail` (:599, :609, :618, :623) are inside the loop. New; harmless.
- `tasks/task_self_update.c:1403` — empty (not NULL) config_dir writes the marker in the current folder. Premise not checked. New.
- `tasks/task_self_update.c:1425-1431`, `:1402-1405` — silent false returns; Update Now looks dead. **Already in review-code 2026-10-21 lane-02** (lead C1).
- `tasks/task_self_update.c:1306` vs `:1367` — download accepts any 2xx; checksum and HEAD accept only 200. Premise confirmed for :1306 (`status < 200 || status > 299`). New.
- `tasks/task_self_update.c:303-315` — su_fs_read silently truncates to len and reports success. Premise not checked. New.

### 4. Tool gaps, for check-code
No `[tool?]` tags from either lane. comment_claims' tool-decided findings bucket was empty. check-code status unknown.

### 5. Coverage (Q6)
- Read in full: self_update.c, self_update.h, tasks/task_self_update.c, menu/menu_content_downloads.c, menu/menu_content_downloads.h, both hunk diffs.
- Read in part, by design: the four r7z files (fork hunks plus context only; upstream code is out of scope); the glue files (self-updater hunks only).
- Not judged: the non-self-updater hunks the glue diff carried (TLS_VERIFY messages, KEYCHAIN and MCP_SERVER labels, a content_path guard). L1 did not trace whether other R7Z_ERROR_UNSUPPORTED returns reach decode_folder_slice (r7z_archive.c:2984-2986 comment).
- Verbs run: function_sizes, duplicate_blocks, comment_claims, source_scrapes (all whole; no truncation). source_scrapes found no test reader; a manual search of samples/ and libretro-common/samples found only #include of the headers, so ANCHORS were none.
- Lane split: L3 (r7z, a small diff) was folded into L1 rather than L2, because L2 was already the larger lane.
- The code is the fork's own, written in earlier sessions; neither lane had that context.
