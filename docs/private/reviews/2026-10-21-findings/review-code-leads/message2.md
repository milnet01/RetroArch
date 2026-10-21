## Message 2 — synthesis

**Scale.** 50 leads answered across 9 lanes. 27 hold as real (7 of them in upstream code or document-side), 23 do not. Lanes also found 22 new defects nearby. At raw severity: 0 CRITICAL, 3 HIGH, 11 MEDIUM, the rest LOW. That is a fix programme, not one pass: `close-findings` § Scope splits it by subject.

**What I ran to settle the lanes' unexecuted claims** (read-only, on `/mnt/Games/Scripts/Linux/ra-fixes`):
- Every HIGH and MEDIUM quote, and most LOW quotes, grep-checked against the tree. All present. A few sit a line or two off the cited number (`webdav.c` `qop_auth` is at 312, not 332; `s3.c` strcmp at 465; `task_translation.c` `+ 54` at 440).
- Lane 3's open question, "is the update archive verified before `self_update_enter_block`?": yes, when a `.sha256` is published. `task_self_update.c` finishes `sha256_stream_final` and compares ("checksum mismatch") before the OPEN and EXTRACT stages. A 404 on the checksum skips it (decision D5), and TLS is required. So lane 3's quadratic folder loop needs a hostile or compromised build server. It stays LOW.
- Lane 9's two "did not confirm": `.github/workflows/scripts/stack-frame-sweep.sh` exists, and so does `samples/menu/content_downloads/`.
- Lane 6's open question on `Makefile.common` near line 2464: it sets `HAVE_CLOUDSYNC = 1` for Linux and Win builds, then opens `ifeq ($(HAVE_CLOUDSYNC), 1)`.
- Lane 4's question, "does `ozone_sidebar_goto` bound its index?": it does not check the index against the list. For any index above `system_tab_end` it switches to the horizontal-menu tab and stores the index as `categories_selection_ptr`. Whether a later read uses that index into an empty horizontal list is **unsettled**. Settling it means reading the horizontal-tab draw and navigation paths.
- Lane 2's griffin lead (G1): settled as not a defect. `docs/private/codebase-guide.md` § Griffin says to add a file to griffin "if it should build on consoles". Spec §4.1 says no console target qualifies, and configure allows only Win32 and Linux x86_64. So there is no document conflict either. The brief's one-line paraphrase of the rule was the only source of conflict.

### 1. Cross-cutting themes (reached independently by two or more lanes)

- **A failure path that destroys the last good copy.** Lane 1: `su_undo` ignores its rename results, so a failed rollback leaves the original program file only in `.retroarch-old`, which start-up cleanup then deletes. Lane 8: `content_replace_file` (`task_save.c`) and `filestream_write_file_atomic` (`file_stream.c`) delete the good destination, then retry a rename that already replaces on every platform. Same class, three sites, two lanes.
- **Comments that state false facts, and code that followed one.** The Win32 rename comment in `task_save.c` and `file_stream.c` ("the Win32 one refuses an existing destination") is the cause of P1 and P2. The same pattern turned up in other lanes:
  - `r7z_lzma_stream.h` reset doc (lane 3);
  - the `s3.c` "spec-mandated order" comment (lane 6);
  - `cloud_sync_driver.h`'s chunked clause (lane 6);
  - the `wayland_common.c` re-entrancy comment (lane 8);
  - the `runahead.c` capacity-bound comment (lane 7);
  - `local-CI.sh`'s "verbatim" and "bit-exact" claims (lane 9).
- **The self-updater's failure paths tell the user the wrong thing, or nothing.** Lane 1 found A3 (a marker that never goes away gives a repeating notice). Lane 2 found B1 (a false "did not finish"), B2, B3, C1 (three silent `false` returns behind Update Now) and the new `action_ok_self_update` blank page. Two lanes, one subsystem seam: what the task reports after a failure.
- **Unchecked allocations on paths fed by untrusted input.** Lane 7 found these in `bsvmovie.c` (sizes taken from a `.bsv` file) and `uint32s_index.c`. Lane 6 found one at `s3.c:801` (out of memory, so a wrong request gets signed). Lane 8 found the translation reply read before its length is known (`task_translation.c` magic bytes on a 1–3 byte reply).

### 2. HIGH and above, at raw severity

**Build tooling (lane 9):**
- `local-CI.sh:366` — common-samples: no sanitizers, about 19 of the workflow's 80 run targets, no classification gate. The "exact block / single source of truth" comment is false (lead B1).
- `local-CI.sh:288` — samples-tasks runs 6 of about 37 steps, and none of the `samples-tasks-http` job (lead B2).
- `local-CI.sh:388-391` — linux-i686 skips `include_order_check.sh` and the stack-frame sweep (lead B3).

No other lane returned HIGH or CRITICAL.

### 3. Threat-model calibration

**The calibration applied.** The fork's `CLAUDE.md` names no threat model, so I used the trust boundaries the project's own audit partition (`docs/private/audit/indie-review-partition.md`) records:
- `.state` and `.bsv` files, and server replies (WebDAV, S3, AI service, buildbot), are untrusted.
- The user's own config file is the user's.
- RetroArch is a single-user desktop and console application.
- `local-CI.sh` is a developer tool that never ships, but it is the fork's only pre-push gate.

Every finding stays in the report. The re-ranks:

- `local-CI.sh` B1, B2, B3: **HIGH → MEDIUM.** They never reach a user's machine. Their cost is CI failures discovered only after an upstream PR is opened. They are real contract breaks of the script's own stated purpose, so they are not lower than MEDIUM.
- `task_save.c:430` P1 and `file_stream.c:2272` P2: **MEDIUM → HIGH.** This is a real data-loss path for save states and SRAM. On Windows, antivirus and cloud-sync clients cause sharing violations routinely, and the next save then truncates the only surviving copy.
- `input_driver.c:7961` R2 (`input_turbo_bind` out of range): **MEDIUM → LOW.** It needs a hand-edited or third-party config value outside the menu's range. It is a read, not a write.
- `webdav.c` L2 (NULL dereference after a rejected Digest challenge): **stays MEDIUM.** A server, or anyone in the TLS path, can trigger it. The result is a crash with no data loss.
- `configuration.c:9780` F3 (credentials world-readable during every save): **stays MEDIUM.** It needs a second local user on a single-user desktop.
- `self_update.c:575-578` (`su_undo`): **stays MEDIUM.** The rollback must itself fail first, which is rare. But the outcome is a broken install.
- Menu Scan freeze M1: **stays MEDIUM.** An ordinary user action freezes the menu. Upstream code.
- `local-CI.sh` working-tree `make clean` on a failed `cd`: **stays MEDIUM.**

### 4. Tool gaps

`check-code` status unknown: no report of its was an input to this run. Each tag below is my suspicion, not a verdict.
- `local-CI.sh:350`, `:260` — `cd` with no `|| exit`. [tool: shellcheck, SC2164] Whether shellcheck ran on this script: unknown.
- `bsvmovie.c:1753`, `:1764`, `:1989` and `uint32s_index.c:169` — unchecked `calloc`/`malloc` results used. [tool: cppcheck `nullPointerOutOfMemory`] cppcheck is in the project's audit config; whether these sites were suppressed or simply never run against is unknown.
- `self_update.c:660` (A5, struct member order) — no tool in the set decides it.

### 5. Coverage

- **Lanes that ran:** 9, as one parallel batch through `neutral-lane`. All exited 0 with `denials=0`. Nothing was merged.
  1. Self-updater core: `self_update.c` and `.h`.
  2. Self-updater task and glue: `task_self_update.c`, `retroarch.c` glue, griffin and Makefile.
  3. libretro-common r7z.
  4. Menu drivers.
  5. Menu glue and frontend: `menu_cbs_scan`, `menu_explore`, `menu_setting`, `configuration.c`, `cheat_manager.c`, `retroarch.c`.
  6. Cloud sync, plus the `qb` S3 lead.
  7. Replay and run-ahead.
  8. Leaf modules: `task_save`, `file_stream`, `task_translation`, `task_patch`, the Qt widgets, Wayland, V4L2, Vulkan.
  9. Build tooling: `local-CI.sh`, `qb.comp.sh`.
- **Scope was the lead list, not the tree.** Lanes 1, 2 and 9 read their files whole. Every other lane read each lead's enclosing function and its callers, as briefed, and several say so. Fork code outside those functions was not reviewed. **That is a coverage statement, not a clean bill.**
- **Not reviewed, by ruling:** the four unrelated `msg_hash_lbl_str.h` defines from dab9d538f8 (laneC's second lead). The user ruled on 2026-10-21 that they are a readability and upstream-PR matter.
- **The partition** was built by lead subsystem. The memoized partition at `docs/private/audit/indie-review-partition.md` supplied only the trap lists, for lanes 2, 5, 6 and 7. `indie_review_partition` was not called: its merge suggestions and tree-coverage fields describe a whole-tree partition, which this lead-scoped run did not use.
- **Dimensions each lane was given** (dimensions 2 and 3 were in every lane's focus or one-line scan; 2b was one-line everywhere):
  1. #2 #3 #7 #9 #11 #16
  2. #2 #3 #7 #8 #9 #16 #17
  3. #2 #3 #4 #7 #11
  4. #2 #4 #7 #11 #16
  5. #2 #3 #4 #7 #9 #11
  6. #2 #3 #4 #7 #9 #16
  7. #2 #3 #5 #7 #9 #11 #16
  8. #2 #3 #7 #9 #11 #16
  9. #2 #7 #11 #16
- **External specs resolved:** AWS SigV4; RFC 7616 and RFC 9112 §6.3; the 7z format and LZMA2 control bytes; the BMP and BPS formats; POSIX `rename(2)` and Win32 `MoveFileEx`; libwayland `wl_proxy_destroy`; the Windows uninstall-registry convention.
  - Lane 8 did not open the BPS spec or the libwayland source, and answered from knowledge, as it says.
  - Lane 3 did not open the reference LZMA SDK. Its open question, whether a state-only reset zeroes `processedPos`, is **unsettled**. Settling it means reading `LzmaDec.c`.
- **Language standards:** none covers C or shell. `cpp.md` and `qt.md` were named for the Qt file.
- **Within-lane gaps the lanes reported:**
  - lane 1 did not open the `su_fs_*` operations;
  - lane 2 did not open `self_update.c` or the `frontend/drivers/platform_*` installer hooks;
  - lane 5 did not open the override, remap or autoconf save paths;
  - lane 7 did not open the VFS copy path (its R8 open question);
  - lane 9 did not open the bodies of the workflows run through act.
- **Contamination note:** each lane arrived holding only the global `~/.claude/CLAUDE.md`, with no project `CLAUDE.md` or git snapshot. Lane 2 says that two filenames and two lines from `samples/tasks/http/` tests reached it through a search exclusion that was too narrow, and that it used neither.
- The `review-code-last` tag was not moved: `--since` was not used, and this run did not cover the tree.
