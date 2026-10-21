# review-readability — self-updater (RETR-0021), 2026-10-21 re-run

Tree: /mnt/Games/Scripts/Linux/ra-fixes, branch local/fixes-2026-09, HEAD 948fbe8f20.
Replaces the 2026-10-21 run whose lane returns were lost.

## Message 1 — the lanes' findings

Lane files (relayed below with preamble dropped; nothing else dropped):
- lane-01-su-core.result — L1, self_update.c, self_update.h, and the fork's r7z changes (L3 folded in here)
- lane-02-su-task.result — L2, tasks/task_self_update.c, menu/menu_content_downloads.{c,h}, the self-updater hunks of retroarch.c, menu/menu_displaylist.c, menu/cbs/menu_cbs_ok.c, msg_hash.h, msg_hash_lbl_str.h, qb/config.libs.sh

No finding is HIGH. Each finding below is relayed whole in its lane file; this list carries the lane's headline and the orchestrator's premise mark.

### What each lane started with
Both lanes: the global ~/.claude/CLAUDE.md, the review-lane agent prompt, an Ants MCP SessionStart note, and (L1) the user's email from harness context. Neither had a project CLAUDE.md, memory index or git snapshot. Neither was given the review-code returns.

### L1 — self-updater policy core and r7z fork changes
1. [Q4] MEDIUM `self_update.h:113,141,152,183,191` — comments cite "spec §4.5", "§4.6", "Windows portable step 3 and 4", "AppImage steps 2 and 3", which upstream never sees. — **Settled: ruling 2026-10-21 (1)** (rewrite to state the reason inline). Premise: quoted in-file, confirmed.
2. [Q1] MEDIUM `self_update.c:723-724` — the staging names ".retroarch-update" / "retroarch-update" are a hand-kept copy of the names task_self_update.c creates. — open. Premise **confirmed**: `grep -n retroarch-update self_update.c tasks/task_self_update.c` shows task_self_update.c:602, :615, :627 and self_update.c:723-724 only.
3. [Q4] LOW-MEDIUM `r7z_lzma_stream.h:172-174` — "Call this before the first block and at every LZMA2 state reset." contradicts the fork's new rule that a mid-folder reset must not rewind dic_pos. — open. Premise **confirmed**: `grep -rn 'rlzma_stream_reset\(_parts\)\?(' libretro-common` shows chunk resets use rlzma_stream_reset_parts (r7z_lzma2.c:283, :346); rlzma_stream_reset is called only at r7z_lzma2.c:127 and r7z_archive.c:2427.
4. [Q5] LOW `r7z_lzma_stream.c:600` (twin in try_dummy) — `dic[(dic_pos ? dic_pos : dic_size) - 1]` wrap fallback unexplained. — open. Premise confirmed: grep shows the expression at :600.
5. [Q5] LOW `self_update.c:328` — `arch = "x86_64";` for AppImage silently ignores the x86_64 argument; header does not say so. — open. Premise: in-file, confirmed by read (:322-330).
6. [Q5] LOW `self_update.c:217` — `strlen(s) == n` rejects an embedded NUL without saying so. — open. Premise: in-file, confirmed by read.
7. [Q5] LOW `self_update.c:155-158` — only the `parts < 2` failure zeroes `out`; earlier failures leave it partly filled. — open. Premise: in-file, confirmed by read (:140-158).

Candidates dismissed by L1: release_parse, replace, file_url sizes; self_update.h:146 "64 bits" accurate; r7z_lzma_stream.h census numbers outside the fork's hunks.

### L2 — self-updater tasks and glue
1. [Q1/Q4] MEDIUM `tasks/task_self_update.c:277` and `:1419` — which kinds have a small file, and whether it is a .7z, is restated here beside `self_update_file_url`'s table; the opening comment sends a stranger only to self_update.c. — open. Premise **confirmed**: read :274-279, :1416-1421 and self_update.c:322-330.
2. [Q4] LOW `tasks/task_self_update.c:18` — "Every decision is made by self_update.c" is not true of this file (staging dirs, installer-and-quit, pick_members, TLS notice). — open. Premise: in-file, confirmed by the lane's cited lines.
3. [Q4/Q5] LOW `tasks/task_self_update.c:550` — TLS_REQUIRED notify runs even on the quiet start-up check, against the comments at :72 and :140. — **Settled: ruling 2026-10-21 (3)** (start-up check stays silent when tls_verify_mode is not REQUIRED; not a finding). Premise confirmed by read (:548-554). Same point as review-code 2026-10-21 lane-02's open question on line 550, which that ruling answered.
4. [Q1] LOW `tasks/task_self_update.c:1308` — three drifted copies of "HTTP status, or a fallback reason" (:134-137, :1308-1311, :1378-1381); only the check's copy keeps `err`. Confirms dup:879cfacbff7e. — open. Premise **confirmed**: read all three sites.
5. [Q4] LOW `tasks/task_self_update.c:457` — "Static: ... kept while the decision reads them" — the decision reads them synchronously in this call. — open. Premise **confirmed**: `self_update_kind_decide(&env)` is called at :509 inside the same function, and uninstall_string is freed right after.
6. [Q3] LOW `tasks/task_self_update.c:722` — `archive_size` holds the download size on every route, archive or not. — open. Premise: in-file, not re-checked beyond the lane's cites.
7. [Q3] LOW `tasks/task_self_update.c:1394` — `preferred` reads as a user preference; it means "the small update file". — open. Premise: the meaning is stated at :73, confirmed by read.
8. [Q1] LOW `tasks/task_self_update.c:888` — "an empty entry has no block" restated at :888, :895, :1151. — open. Premise **confirmed** by read of all three lines.
9. [Q1/Q5] LOW `retroarch.c` (CMD_EVENT_SELF_UPDATE_APPLY and rarch_main hunks) — marker folder worked out twice. — open. Premise **confirmed**: `grep -n 'fill_pathname_basedir(config_dir' retroarch.c` shows :5264 and :6841.
10. [Q4] LOW `menu/cbs/menu_cbs_ok.c:5868` — "before extraction can delete it" sits after task_push_decompress (:5834); the comment does not say the task runs later. — open. Premise **confirmed** by grep and read of :5857-5875.
11. [Q3] LOW `menu/menu_content_downloads.h:42` — `menu_content_downloads_record` is a verb beside a constant and a parameter of the same name. — open. Premise: in-file, confirmed.

Candidates dismissed by L2: all six function_sizes rows; dup:4c7a21e0bbe2 (quiet difference follows from :93-94); comment_claims :1330 and :1365 accurate.

L2 also noted, under Not checked: the task file's comments cite "spec §4.5/§4.6" and "D5" at :587, :705, :786, :1017, :1365. That class is **settled by ruling 2026-10-21 (1)**. L2 left the KEYCHAIN_PASSPHRASE_STR and MCP_SERVER_*_STR hunks unjudged; they are **settled by ruling 2026-10-21 (2)**.
