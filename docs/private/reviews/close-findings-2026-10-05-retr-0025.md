# close-findings ledger — RETR-0025, 2026-10-05

Findings: the 2026-10-02 whole-tree check-code run on `local/fixes-2026-09`
at 73c7a9c0e3 (report and raw output in
`/mnt/Games/claude-scratch/audit-1002/`). Verified against 740ff95143, the
same branch three days later. Each group was read site by site, and
clang-analyzer was re-run on HEAD to recover its path notes. Every finding
called real was re-read by hand before being recorded here.

Subjects closed in this run: semgrep's 26 survivors, all 138
clang-analyzer findings, and five rare cppcheck classes (196 rows). The
style classes are one decision, below. Every other class is queued unread
as RETR-0031.

## Dispositions

| finding (sites at 740ff95143) | tool / class | verified | disposition |
|---|---|---|---|
| turbo clear reads `input_remap_ids[port][-1]`: `input/input_driver.c` 2136-2137, 2321-2322 | found next door by the cppcheck read | yes; default `DEFAULT_TURBO_BIND -1`, line 7937 guards -1, these two do not | queued as RETR-0028 item 1 |
| `playlist.c` 1773: `entries[0].attr` thumbnail bits never reset | clang-analyzer UndefinedBinaryOperatorResult | yes; the setters keep bits 19-23 (`playlist.h` attr layout) | RETR-0028 item 2 |
| `gfx/drivers_shader/glslang_util.c` 678, 681: `scratch` leaks on an include cache hit | clang-analyzer unix.Malloc | yes | RETR-0028 item 3 |
| `gfx/gfx_thumbnail.c` 3805/3834: strdup'd `state_name` never freed | clang-analyzer unix.Malloc | yes; no `free(state_name)` in the function | RETR-0028 item 4 |
| `tasks/task_translation.c` 424-428, 555 BMP: unchecked malloc, `w*h*3` unchecked against the reply length | clang-analyzer NullPointerArithm | yes | RETR-0028 item 5 |
| `menu/menu_displaylist.c` 6280: `*content_path` with `entry->path` NULL | clang-analyzer NullDereference | yes; the same function guards `content_path &&` earlier | RETR-0028 item 6 |
| `audio/drivers/pipewire.c` 542: `mic->pw` with `mic` NULL | clang-analyzer NullDereference | yes | RETR-0028 item 7 |
| `gfx/drivers_shader/shader_gl3.c` 417-436: link failure falls through when the info log is empty | found next door by the semgrep read | yes | RETR-0028 item 8 |
| `shader_gl3.c` 1257, 2088, 2995, 3488; `shader_vulkan.c` 925 | clang-analyzer NullDereference | yes (OOM only) | queued RETR-0029 |
| `uint32s_index.c` 171, 184; `menu_setting.c` 16336, 16392; `video_driver.c` 433-445 (`realloc_checked`) | clang-analyzer unix.Malloc / NonNullParamChecker | yes (OOM only) | queued RETR-0029 |
| `gfx/drivers/vulkan.c` 3805, 3881: node kept after `vkMapMemory` fails | clang-analyzer NonNullParamChecker | yes (driver error path) | queued RETR-0029 |
| `input_keymaps.c` 2303; `gl2.c` 5390 / `gl3.c` 3219 ordering; `ui_win32_companion.c` 2676; `rsx_gfx.c` 1018 | cppcheck nullPointerOutOfMemory, and next door | yes (OOM only) | queued RETR-0029 |
| the other `nullPointerOutOfMemory` rows: alloc then use, a plain crash on OOM | cppcheck nullPointerOutOfMemory | class shape read at every allocation site | dismissed: a NULL crash on OOM is upstream's accepted behaviour; the corrupting sites are queued above |
| `uint32s_index.c` 36-47, 205, 207 | cppcheck nullPointerOutOfMemory | yes | dismissed: fixed since the scan (30ef35574d) |
| `ctr_gfx.c` 2550-2551; `shacccgpatch.c` 55; `ps2_gfx.c` 159, 169, 176; `save.c` 466; `net_retropad_core.c` 1344; `tools/ps3/ps3py/crypt.c` 28-31 | cppcheck nullPointerRedundantCheck / uninitvar / nullPointerOutOfMemory / shiftTooManyBits | yes | queued RETR-0030 |
| `input/input_driver.c` 5890, 5894: NULL joypad in `input_key_pressed` | clang-analyzer NullDereference | unsure: no in-tree caller | queued RETR-0030 |
| `menu_setting.c` sites under `SETTINGS_LIST_APPEND` | clang-analyzer NullDereference | yes | dismissed as tool noise; suppressions.md |
| menu drivers assuming `menu_st->entries.list` NULL: rgui 8228, 8429; materialui 10011, 10475; ozone 9401 | clang-analyzer NullDereference | yes | dismissed as tool noise; suppressions.md |
| array-member address taken as NULL: `menu_setting.c` 16055, `video_driver.c` 5765 | clang-analyzer | yes | dismissed as tool noise; suppressions.md |
| libwayland list frees: `gfx/common/wayland_common.c` 531, 537; `input/common/wayland_common.c` 600 | clang-analyzer unix.Malloc | yes | dismissed as tool noise; suppressions.md |
| `wl_list_for_each` iterators in the three wayland_common files | cppcheck uninitvar | yes | dismissed as tool noise; suppressions.md |
| `core_updater_list.c` 930; `task_translation.c` 175 | clang-analyzer | yes | dismissed: existing suppressions.md entries |
| success returns before cleanup: core_option_manager 781, shader_gl3 489/632/633, slang_process 1288/1289, video_shader_parse 3827, rgui_bitmapfont 182/355, task_save 1665 | semgrep double-free / use-after-free | yes | dismissed: S9 class, entry widened |
| freed then reallocated on the next line: dispserv_x11 877, webdav 345, mcp_server 555 | semgrep use-after-free | yes | dismissed as tool noise; suppressions.md |
| semgrep by-name rules (strcpy, strcat, strtok, printf, `shell=True`, SHA1) | semgrep | yes, each read | dismissed: per-site reasons below |
| vendored `libretro-common` rows: vector_list 56, 57; file_path 567, 941, 942; file_stream 1558 | cppcheck | yes | dismissed: existing vendored entry |
| every other clang-analyzer and cppcheck row | various | yes, each read | dismissed: per-site reasons below |

## Per-site reasons for the remaining dismissals

clang-analyzer: jack.c 138, 258, 274 (channels set before activate);
gfx_widget_volume.c 230 (lock held); glslang_util.c 769 (the realloc
always runs first); input_overlay_textures.c 98; video_thread_wrapper.c
2363, 3195 (reply always written), 2782, 4241 (deliberate
over-allocation); audio_driver.c 3679 (resampler pair), 3879, 3891, 5833,
5845 (channel layout validated); input_driver.c 1295, 7368; menu_cbs_ok.c
2776; runahead.c 781; runloop.c 4968 ×2; rgui.c 7761, 2005, 2096, 2099;
materialui.c 5284; vulkan.c 2405, 2524 (bpp never 0 for these formats),
3601, 5513, 6431; shader_vulkan.c 4024; dispserv_x11.c 1552;
menu_displaylist.c 7962, 8078, 16857; menu_explore.c 400; webdav.c 1101;
netplay_frontend.c 7605, 3576 (`accept` fills the address);
task_cloudsync.c 1164, 1234, 1236, 1268 (every manifest item has a key);
task_overlay.c 906; task_translation.c 698; task_database.c 2239;
companion_dock.c 884.

cppcheck nullPointerRedundantCheck, the pointer is never NULL at the
dereference: wasapi 2757, core_updater_list 802, platform_orbis 159,
ctr_gfx 1823, drm_ctx 998, gx2 853/854/1017/1018, gxm 1917/1918, ps2 746,
vulkan 9643, video_filter 457, iohidmanager 1087, menu_cbs_ok
1445/7261/7262, ozone 3716/3836/10404/13994/13995, menu_driver 1275/2733,
menu_setting 18185, google_drive 1329/1330, netplay_frontend
8923/8928/9240/9245, playlist 1705, ui_win32_companion 2232/3268.

cppcheck uninitvar, every path assigns before use: audio_bitstream
146/147, audio_driver 5117, stb_image.h 5293, vivante_fbdev_ctx 117,
dinput 268, xdk_joypad 243/244, netplay_frontend 2494, task_database 2239.

cppcheck arrayIndexOutOfBoundsCond, the index is bounded before use:
egl_common 938/944/946, shader_gl_cg 926 ×2, input_driver 2314,
menu_cbs_scan 193/196.

semgrep by-name rules: video_processor_v4l2.c 218 (bounded strncat), 587
×2 (single-threaded strtok on a local copy); shacccgpatch.c 41, 44, 47
(literals fit `char[8]`); iohidmanager_hid.c 714; libretrodb_tool.c 179
(constant format); ranetplayer.c 306; msg_hash_name_harness.py 31 and
settings_migrate_group.py 34 (no untrusted input reaches the shell);
pkg.py 545, 593 (the PS3 PKG format requires SHA1).

## Style classes — one decision

User decision 2026-10-05: these are upstream code style, not defects.
Dropped from `docs/private/audit/audit-config.json`, which records why:

- the nine clang-tidy bugprone checks with over 200 hits each (about
  36,000 sites on 2026-10-02);
- the cppcheck const*, variableScope, unreachableSwitchCase and
  unusedStructMember classes (about 4,700 sites on 2026-10-02).

## Run level

- cited_by: none; no fix in this run renames anything.
- swept: none yet. RETR-0028's fixes land on `local/fixes-2026-09`, and
  their sweep goes in RETR-0028's resolution note.
- collateral: none.
- surfaced: none.
- out_of_scope, found next door: turbo bind, playlist attr and the gl3
  link fall-through (all RETR-0028); vita `&shader` arithmetic
  (RETR-0030). Not filed: dispserv_x11's stored CRTC copy keeps freed
  `outputs` pointers, harmless while only `.x`/`.y` are read;
  menu_driver.c 2733 indexes the wrong list, in debug-only code;
  a failed realloc under `SETTINGS_LIST_APPEND` makes the caller rewrite
  the previous row, on OOM only;
  settings_migrate_group.py uses fixed `/tmp` paths, in a developer tool.
- falsified: none.
