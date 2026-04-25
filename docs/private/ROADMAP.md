# RetroArch Private Fork — Roadmap

Tracking file for `milnet01/RetroArch` (private fork of `libretro/RetroArch`). Items here are not for upstream — they're for our internal audit/review/spec workflow. PRs back to libretro happen on a separate one-off public fork branch when something is ready to upstream.

Status markers:
- 📋 pending
- 🚧 in progress
- ✅ done (kept for audit-recurrence detection)
- 🔄 deferred / waiting on upstream

Severity: **CRITICAL** > **HIGH** > **MEDIUM** > **LOW**.

---

## 🔍 Audit 2026-04-25

Branch: `local/audit-2026-04` off `master @ 6ff3332ea2`.
Tools run: cppcheck (partial — 449/450), semgrep, gitleaks (clean), trivy (clean), ruff, bandit.
Filtered by `audit-triage` subagent: 24 actionable, 12 spec-needed, ~660 noise items dropped.
Tool gaps: clang-tidy + clazy not run (no `compile_commands.json` — install `bear` and rerun); cppcheck did not finish `menu/drivers/materialui.c`; semgrep had 332 parse errors.

### Actionable

#### Critical

- 📋 **CRITICAL — UPnP XML parser trusts attacker-influenced router responses.** `network/natt.c:271-289`. Recursive `natt_parse_desc_node` walks IGD-supplied XML; subsequent `strstr`/`strlcpy` on `service_type->data` (lines 294-300) can be triggered by a malicious LAN gateway. Validate `node->name`/`child->name`/`->data` for non-NULL and bounded length before string ops.

#### High

- 📋 **HIGH — OOM null-deref in network/IPC `READ_CORE_MEMORY` handler.** `command.c:1187-1188`. `reply = malloc(64 + nbytes*3)` with no NULL check, `nbytes` from socket-parsed integer (no upper bound). Reproducer: `READ_CORE_MEMORY ffffffff 1000000000` on the command socket.
- 📋 **HIGH — `strchr(key, '/') + 1` on attacker-supplied cloud-sync manifest.** `tasks/task_cloudsync.c:640`. `NULL+1` is UB if server-supplied `key` lacks `/`. Same trust-boundary class as the recent `task_http` UAF fix.
- 📋 **HIGH — `strcpy` with overlapping buffers (UB).** `cheevos/cheevos_client.c:169`. Both `start` and `next+1` point into the same URL string. Replace with `memmove(start, next+1, strlen(next+1)+1)`.
- 📋 **HIGH — OOM null-deref on per-value-hash malloc.** `core_option_manager.c:721-727, 1006-1007`. `uint32_t *value_hash = malloc(...); *value_hash = ...` — no NULL check, two sites, identical pattern.
- 📋 **HIGH — Cluster sweep: ~30+ deref-before-NULL-check sites.** Audio: `audio/drivers/wasapi.c:1471-1473`, `audio/drivers/oss.c:99`. GFX: `gfx/drivers/d3d10.c:984-985`, `gfx/drivers/d3d11.c:1055-1056`, plus `d3d9hlsl,vita2d,gx2,ctr,vulkan,gl2,gl3,dispmanx,ps2,gdi`_gfx.c. Frontend/tasks: `frontend/drivers/platform_orbis.c:159`, `tasks/task_pl_thumbnail_download.c`, `gfx/gfx_thumbnail.c`. Each derefs `p` before its `if (!p)` guard. Sweep: move init below the NULL check, or remove the redundant check if NULL is impossible.
- 📋 **HIGH — Wii `pad_type[8]` indexed up to pad 15.** `input/drivers_joypad/gx_joypad.c:435`. `return pad < MAX_USERS && pad_type[pad] != …` — `MAX_USERS` is 16 but `pad_type` is `[DEFAULT_MAX_PADS=8]` on Wii. OOB read when pad ≥ 8. Fix: `pad < DEFAULT_MAX_PADS && …` or extend `pad_type` to MAX_USERS.

#### Medium

- 📋 **MEDIUM — `oi` may be NULL after empty `wl_list_for_each`.** `gfx/common/wayland_common.c:289-296`. Reproducer: hot-unplug last monitor before frame size query. One-line guard fix.
- 📋 **MEDIUM — calloc'd struct leaks on `memalign` failure.** `audio/drivers/audioworklet.c:255-257` and `audio/drivers/rwebaudio.c:86-90`. Add `free(self); return NULL;` on the failure path.
- 📋 **MEDIUM — Font-init leaks on renderer-create failure across 4 backends.** `gfx/drivers/caca_gfx.c:92`, `gdi_gfx.c:205`, `sixel_gfx.c:95`, `vga_gfx.c:82`. Identical `free(font); return NULL;` fix at each site.
- 📋 **MEDIUM — Cluster: 52 OOM null-deref findings in cloud-sync HTTP request builders.** `network/cloud_sync/google_drive.c` (29), `network/cloud_sync/webdav.c` (23). The recent `task_http` hardening series didn't reach these. Cluster-level audit needed.
- 📋 **MEDIUM — calloc'd `ft_info` not NULL-checked before strdup/snprintf.** `cores/libretro-video-processor/video_processor_v4l2.c:681-690`. Bundled core; opt-in build.

#### Low

- 📋 **LOW — Unbounded `strcpy`/`strcat` on 1024-byte buffers.** `ai/game_ai.c:153, 182-184`. Module is opt-in/experimental. Migrate to `strlcpy`/`strlcat`.
- 🔄 **LOW — `backend->compressed_file_read` derefs NULL backend.** `libretro-common/file/archive_file.c:555-557`. Vendored — patch upstream and re-vendor.
- 🔄 **LOW — `(json + 1)` arithmetic before NULL check.** `libretro-common/formats/json/rjson.c:971-975`. Vendored.

### Spec-needed

These look intentional but lack a documented contract. Each is a candidate for a reactive spec under `docs/private/specs/`. Cheap-to-spec items are tagged 💸 (priority for the spec phase).

- 📋 💸 **S5 — `input_key_pressed()` public-API-or-delete decision.** `input/input_driver.c:5430-5443`. No internal callers; unbounded `key` index allows OOB read of `input_config_binds[0][95+]`. Spec: declare it public API with bounds check, or remove. ~1 day.
- 📋 💸 **S2 — `MENU_LIST_GET` / `MENU_LIST_GET_SELECTION` macro contract.** `menu/menu_driver.h:72-78` plus ~17 callers in xmb/ozone/materialui/menu_displaylist/menu_explore/menu_driver. Macros return NULL when `list` is NULL but no caller checks. Either macros are NULL-safe-with-mandatory-caller-check, or `menu_list` is invariably non-NULL during draw. Spec the invariant; sweep callers. ~1 day.
- 📋 💸 **S11 — `identicalInnerCondition` policy.** `audio/audio_driver.c:447`, `audio/drivers/dsound.c:452`, `audio/drivers/openal.c:129`, `cheat_manager.c:1823`, `tasks/task_content.c:609`, `menu/menu_driver.c:4639`, `camera/camera_driver.c:155`. Each is a defensive double-check. Spec: defense-in-depth or copy-paste residue? ~2 hours.
- 📋 **S1 — `(1 << 31)` signed-overflow flag enums.** `runloop.h:129`, `input/input_driver.c:1980-1990`, `menu/cbs/menu_cbs_sublabel.c:1878-1880`, `network/netplay/netplay_frontend.c` (13 sites at 1858, 1860, 2756-2976, 3072-4020, 6182-7622). Switch to `1u << 31` and `uint32_t` fields, or document the supported-platform set where signed overflow is acceptable.
- 📋 **S10 — `(1 << pad)` runtime shifts.** `input/drivers_joypad/xdk_joypad.c:300`, `xinput_joypad.c:289` and similar. Coupled with S1 / S8.
- 📋 **S3 — `wayland_common.c` `#ifdef HAVE_LIBDECOR_H` if/else pairing.** `gfx/common/wayland_common.c:1037-1057`. `else` binds to whichever `if` survives the `#if/#else`. Document the non-libdecor monitor-selection contract.
- 📋 **S4 — Build-matrix question for qnx/vivante/xegl context drivers.** `gfx/drivers_context/qnx_ctx.c`, `vivante_fbdev_ctx.c`, `xegl_ctx.c:299`. `new_width`/`new_height`/`vid` only used inside `#ifdef HAVE_EGL`; safe at runtime if `HAVE_EGL` matches everywhere. Spec: are these ever built without EGL? If never, gate the whole file.
- 📋 **S6 — Bind-index arithmetic invariant unasserted.** `menu/cbs/menu_cbs_scan.c:185-188`. Spec the invariant `MENU_SETTINGS_INPUT_DESC_END − BEGIN == MAX_USERS * RARCH_CUSTOM_BIND_LIST_END` and add `static_assert`.
- 📋 **S7 — Wayland touch-index policy.** `input/drivers/wayland_input.c:127`. `wl->touches[16]` indexed by `idx <= 16`. Confirm whether 16 is OOB or upstream caps at 15.
- 📋 **S8 — XInput controller-port-count invariant.** `input/drivers_joypad/xinput_joypad.c:289`. `g_xinput_states[4]` vs `pad < DEFAULT_MAX_PADS` (8/16). XInput supports only 4 ports. Spec or harden to `min(DEFAULT_MAX_PADS, 4)`.
- 📋 **S9 — semgrep `double-free` cluster (29 sites) is a confirmed FP class.** Pattern: success-then-return / fail-label-cleanup. Files: `network/cloud_sync/s3.c`, `tasks/task_database_cue.c`, `gfx/drivers/d3d9cg.c`, `gfx/drivers_font/bitmapfont_*.c`, `tasks/task_save.c`, plus stb / video_shader_parse. Spec: project-wide suppression list, or refactor to single-cleanup-block style.
- 📋 **S12 — JNI-callback `missingReturn` FPs.** `play_feature_delivery/play_feature_delivery.c:117, 187`. Anchor for a project-level cppcheck suppression file (`.cppcheck-suppress.txt`).

### Pending audit re-runs

- 🚧 Install `bear` (`zypper in bear` on openSUSE), regenerate `compile_commands.json` via `bear -- make -j$(nproc)`, then run clang-tidy + clazy. Will likely corroborate items #2-#7 with concrete reachability and surface signed-integer-overflow (S1) findings.
- 🚧 Re-run cppcheck letting `menu/drivers/materialui.c` finish (expect +10-15 items in the S2 pattern).
- 🚧 Re-run semgrep with pinned parser (332 parse errors → expect FP-rate reduction on the double-free cluster).

---
