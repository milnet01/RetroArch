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

## 🔬 Indie-Review 2026-04-25

8-lane independent multi-agent review of master @ `6ff3332ea2`. Each agent reviewed one subsystem cold (no session context, no test files) against `libretro.h` / inline contract docs / external standards (HTTP/1.1, RFC 7616 digest auth, RFC 8628 device flow, AWS SigV4, RFC 4918 WebDAV).

Lanes: libretro env-callback boundary, configuration triad, driver-pattern meta, task queue, network commands/IPC, cloud sync + cheevos, menu core + 4 drivers, save state + replay + runahead.

Threat model applied to severity calibration: this is a personal fork running on a single-user desktop, but RetroArch ingests untrusted data from many sources — downloaded save states/replays, malicious LAN routers (UPnP), MITM-able HTTPS endpoints (cloud sync, cheevos), and netplay opponents. Items that depend ONLY on the local machine being trustworthy are downgraded; items that depend on remote data being well-formed are kept at raw severity.

**Cross-references** with the audit run above are noted as `(audit Cn/Hn/Sn)`.

### 🔥 Cross-cutting themes (caught by ≥2 independent reviewers)

These are the highest-confidence findings — multiple reviewers coming at the code from different angles all flagged the same root cause.

- 📋 **🔥 No atomic write on disk-save paths.** Every "save user data to disk" path opens the destination directly with truncating-write mode. Power loss / OOM kill / `kill -9` mid-save = file corrupt and previous save gone. Affected:
  - `tasks/task_save.c:556-561` (save state)
  - `tasks/task_save.c:1413+` (auto save)
  - `tasks/task_save.c:1846+` (RAM-state-to-file)
  - `disk_index_file.c:344-413` (disc index — multi-disc games revert to disc 1)
  - `input/bsv/bsvmovie.c:759-797` (BSV replay checkpoint)
  - `libretro-common/file/config_file.c:1410-1432` (every cfg save)
  - `network/cloud_sync/{google_drive,webdav,s3}.c` (every downloaded synced file)
  - **One architectural fix** — a `path.tmp` + fsync + `rename(2)` helper, applied to ~7 sites. Fixes all of these. (audit M9 was a leak class; this is the data-loss class.)
- 📋 **🔥 Buffer-overflow class in malicious save-state / replay parsing.** Multiple reviewers flagged the same trust boundary:
  - `tasks/task_save.c:884-986` `content_load_rastate1` reads `block_size` as 32-bit LE from attacker bytes, then advances `input += CONTENT_ALIGN_SIZE(block_size)` without checking against `stop`. Each block is then passed to `core_unserialize`, `replay_set_serialized_data`, or `rcheevos_set_serialized_data` with attacker-chosen length.
  - `input/bsv/bsvmovie.c:822-863` `bsv_movie_read_next_events` — `key_event_count` is `uint8_t` (max 255) writing into `key_events[128]`. `input_event_count` is `uint16_t` (max 65535) writing into `input_events[512]`. No bounds check on either before the read loops.
  - `input/bsv/bsvmovie.c:1370-1421` `replay_set_serialized_data` reads attacker-controlled `loaded_len` and writes that many bytes into the user's replay file.
  - **Threat is real** — RetroArch users routinely load save states and replays shared online. Single-byte fix on each plus a unified bounds-check helper.
- 📋 **🔥 `task_http`-class UAF pattern still present at the caller layer.** The recent `t->title` UAF fix (commit `19fb8692be`) addressed one site. Reviewers confirmed the same dangling-task-pointer pattern still exists in:
  - `tasks/task_core_updater.c:326-348` — `list_handle->http_task` retained after the queue may have freed it.
  - `tasks/task_pl_thumbnail_download.c:358` — `pl_thumb->http_task` same pattern.
  - ASan + a fast 4xx response will hit it. The fix is the same shape as the title fix: hoist the read before push, or weak-reference via task ID instead of pointer.
- 📋 **🔥 Path traversal via attacker-supplied cloud-sync manifest.** `tasks/task_cloudsync.c:640` `strchr(key, '/') + 1` flagged by audit, tasks-lane reviewer, and cloud-sync reviewer. Compromised or hostile sync server can write `../../../home/user/.bashrc` into the user's machine. Two fixes needed:
  1. NULL-guard the `strchr` result.
  2. Reject keys containing `..`, `\`, leading `/`, or NUL — server input must not be trusted as path components. (audit H3.)

### 🔒 Tier 1 — ship-this-week security/data-loss blockers

These are exploitable now and have concrete reproducers.

- 📋 **CRITICAL — TLS certificate verification effectively disabled.** `libretro-common/net/net_socket_ssl_mbed.c:199` uses `MBEDTLS_SSL_VERIFY_OPTIONAL`; the verify result is logged into a buffer and discarded — connection proceeds against any cert including self-signed. **Every** HTTPS request in cloud sync (Google Drive OAuth + Drive API, S3 SigV4 signing, WebDAVS), cheevos (RetroAchievements API), and online updater is un-authenticated. On hostile WiFi, an MITM captures Google OAuth refresh tokens, AWS secret access keys, and RA credentials. Vendored — fix must go upstream first; a temporary RA-side opt-out with a giant warning would be acceptable. (Not in audit; indie-review only.)
- 📋 **CRITICAL — Network command socket binds 0.0.0.0 unauthenticated.** `command.c:257-258`, `input/input_driver.c:5944-5961`. With `network_cmd_enable=true` (a single setting), any host on the LAN can issue `LOAD_CORE <path>` → `dylib_load` of attacker-named .so → unauthenticated remote code execution. Plus `READ_CORE_MEMORY` / `WRITE_CORE_MEMORY` / `WRITE_CORE_RAM` for memory peek/poke. Default behavior should be localhost-only (`bind 127.0.0.1`); LAN exposure should require explicit opt-in with a warning. (audit M5 covered the Wii pad index; this is its bigger sibling.)
- 📋 **CRITICAL — `command_write_ram` heap-write OOB driven from the wire.** `command.c:970-990`. `while (*arg) { *data = strtoul(...); data++; }` — no length bound. Combined with the previous item this is an unauthenticated remote heap-write primitive. Fix: thread the descriptor length through `rcheevos_patch_address` and add `--remaining > 0` to the loop guard. (Same class as audit H2.)
- 📋 **CRITICAL — `natt_parse_desc_node` NULL-deref + inverted recursion on hostile UPnP response.** `network/natt.c:268-318`. Empty XML root crashes RetroArch in the netplay-host startup path. The recursion is also wrong: the `else` branch (which fires when `child == NULL`) tries to walk `child->next`. Crash-on-startup any time a hostile or empty-rooted IGD response arrives. Fix is a 10-line rewrite. (audit C1, but indie-review found the structural bug, not just the trust-boundary issue.)
- 📋 **CRITICAL — Use of uninitialized stack memory in `webdav_ensure_dir`.** `webdav.c:700-713`. `http_transfer_data_t data;` then `data.status = 200;` only — `data.headers/data/len` are uninitialized when passed to `webdav_mkdir_cb`. Currently saved by short-circuit luck. Fix: `memset(&data, 0, sizeof(data));`. One line.
- 📋 **HIGH — Netplay password salt is `time(NULL)`-seeded LCG; password compare is non-constant-time.** `network/netplay/netplay_frontend.c:778-782` (`simple_rand_next = time(NULL)`) and `:1533, 1546` (`memcmp(...)`). Combined: an attacker who observes one salt can predict every subsequent salt and mount an offline dictionary attack with timing leakage. The "netplay password" feature is currently theatre. Use `getrandom`/`BCryptGenRandom` for the salt, and a constant-time XOR-accumulate compare for the hash check.
- 📋 **HIGH — WebDAV digest parser unbounded walks + hardcoded cnonce.** `webdav.c:174-280`. Five places `strchr(...) + 1 - ptr` with no NULL check (header missing the close-quote → underflow → multi-GB malloc). One infinite loop on `*ptr != ',' && *ptr != ','` (typo: same comparison twice — should be `!= '\0'`). And `webdav_st->cnonce = "1a2b3c4f"` is a constant client-nonce, defeating digest-auth replay protection. Three independent classes of bug in one parser; combined with the TLS issue above, the WebDAV path is currently unsafe on hostile networks.
- 📋 **HIGH — Plaintext credentials in world-readable `retroarch.cfg`.** `configuration.c:1629, 1634, 1637-1639, 1649, 1722, 1749-1750`: WebDAV password, AWS secret access key, YouTube/Twitch/Facebook stream keys, SMB password, kiosk-mode password, netplay password. Saved as `key = "value"` to a file with default umask (0644). Plus the Google Drive **refresh token** is stored plaintext per `google_drive.c`. Mitigation tier:
  1. `chmod 0600` after `config_file_write` succeeds (one-line, ships immediately)
  2. Move secrets to a separate `retroarch.secrets` file in `~/.local/share/retroarch/secrets/` (longer)
  3. OS-keyring integration via libsecret/Win32 DPAPI/Keychain (longest)
- 📋 **HIGH — `command_read_ram` / `command_read_memory` integer-overflow + missing NULL-check on malloc.** `command.c:929-968` and `:1187-1195`. `unsigned int alloc_size = 40 + nbytes * 3` — `nbytes` from socket-parsed integer, no bound, multiplication wraps. Crafted `nbytes` (e.g. `0x55555558`) → tiny alloc + 12 GB of writes. Same site has no NULL-check on the malloc result. (audit H2 covered the NULL-check; indie-review caught the overflow class.)
- 📋 **HIGH — Heap-buffer-overflow class in HTTP failure loggers.** `s3.c:1633-1640` re-introduces the `data->data[data->len] = 0` antipattern that the codebase has already fixed elsewhere. Add a `net_http_data_to_cstring` helper and rewrite all callers — the rule is too easy to forget.

### 🛡 Tier 2 — hardening sweep (correctness, not exploitability)

- 📋 **HIGH — `find_change` / `find_same` unbounded walk in rewind compressor.** `state_manager.c:110-114, 151-155`. `while (*a == *b) { a++; b++; }` with no length parameter. Walks past the savestate buffer when both buffers are identical (paused frame) or differ entirely. OOB read; on consoles with tight memory may segfault. Pass `num16s` and clamp. SSE2 fast path at lines 76-95 has the same issue.
- 📋 **HIGH — `bsv_movie_load_checkpoint` malloc-fail invariant break.** `bsvmovie.c:558-568`. `cur_save_size` is updated **before** the malloc; on OOM, `cur_save` stays NULL but `cur_save_size` records the requested size, so the next call won't retry the alloc. Subsequent NULL-deref. Move the size update after malloc success.
- 📋 **HIGH — Configuration zombie settings.** `configuration.h:1188-1190` declares `game_ai_override_p1`, `game_ai_override_p2`, `game_ai_show_debug` and `menu/menu_setting.c:24464-24507` exposes them in the UI, but `configuration.c` never persists them. User toggles, restarts, value reverts. Direct violation of the documented triad contract.
- 📋 **HIGH — Configuration save: no lock on settings_t while iterating.** `configuration.c:5793-6471` reads ~830 mutable fields and the keybind tables without `running_lock` or any equivalent. A joypad-autoconfig task running in the background while the user clicks "Save Configuration" can save half-applied state. Snapshot-to-local + write would resolve.
- 📋 **HIGH — `rgui_show_start_screen` triad mismatch.** `configuration.c:2149` — default hard-coded `false` next to a `TODO` comment, but `DEFAULT_MENU_SHOW_START_SCREEN` is `true`. A workaround block at `:6304-6313` papers over the load/save discrepancy in minimal mode but doesn't fix it. Six-month-test failure: nobody opening this in 6 months will know which is the real default.
- 📋 **HIGH — Menu drivers gate setting registration by current driver and never refresh on driver swap.** `menu/menu_setting.c:18806, 19016, 19308, 19552, 19833, 20330, 20533, 20804, 20857, 20990` all use `string_is_equal(menu_ident, "xmb")` style gating. After the user changes menu driver mid-session, the setting tree stays from the original driver until restart — XMB-only, ozone-only, materialui-only settings vanish from the UI. Either rebuild the tree on swap or move the gating to a display-time predicate.
- 📋 **HIGH — Menu init NULL-deref on driver_data.** `menu/menu_driver.c:4533-4540`. `init()` may return NULL on alloc failure; lines 4538-4539 deref unconditionally before the line 4543 check fires. Audio/video/input all NULL-check correctly; only menu doesn't. (Also see audit H6 — same bug class, different surface.)
- 📋 **HIGH — Audio init early-return paths leak buffers.** `audio/audio_driver.c:870-907`. Four `return false` paths bypass the `error:` label that would call `audio_driver_deinit`. `out_conv_buf`, `audio_buf`, `rewind_buf` allocated on `audio_driver_st` fields are leaked permanently. Replace each `return false` with `goto error`.
- 📋 **HIGH — `task_screenshot` task_init NULL-deref.** `tasks/task_screenshot.c:480-484`. The recent OOM-hardening pass missed this site. One-line fix.
- 📋 **HIGH — `task_overlay_handler` leaks loader allocations on `data` calloc OOM.** `tasks/task_overlay.c:1120-1145`. `data == NULL` + not-cancelled = permanent leak of `overlay_path`/`image_list`/`overlays`. Either set CANCELLED before returning or do the frees in this branch.
- 📋 **HIGH — libretro env callbacks deref `data` without NULL guard.** `runloop.c:2014-2026` (`SET_INPUT_DESCRIPTORS`), `:1473` (`SET_VARIABLES`), `:2762` (`SET_CONTROLLER_INFO`), `:2730` (`SET_SUBSYSTEM_INFO`), `:2385` (`SET_FRAME_TIME_CALLBACK`), `:2719` (`SET_MESSAGE`). The libretro spec says "behavior undefined if NULL" — the brief said treat the core as untrusted. A buggy or wrong-ABI core can pass NULL on a probe; current behavior is a crash. One-line `if (!data) return false;` per case.
- 📋 **HIGH — `RETRO_ENVIRONMENT_GET_LANGUAGE` returns true with `*data` unwritten when `HAVE_LANGEXTRA` is undefined.** `runloop.c:1977-1985`. The whole body is `#ifdef`'d but the case still returns `true`. Cores read uninitialized stack as their "configured language." Add `#else *(unsigned*)data = RETRO_LANGUAGE_ENGLISH;` or return `false`.
- 📋 **MEDIUM — Cloud sync no Content-Length verification.** `webdav.c:598`, `google_drive.c:1218`, `s3.c:819`. Connection drops mid-download but HTTP returned 200 → partial bytes written to local file → corrupt save. Combined with no atomic write (cross-cutting #1), transient network failures silently corrupt.
- 📋 **MEDIUM — WebDAV 404 response body written to local file.** `webdav.c:576, 598`. 404 is treated as success-with-NULL-file per contract, but if the server returns a 404 with HTML error body, that HTML is then `filestream_write`'d into the user's save file. Explicitly check 404 case.
- 📋 **MEDIUM — `s3_url_encode` doesn't encode `?` `&` `=` for path components.** `s3.c:419-441`. Object key with literal `?` collapses query+path → wrong canonical URI → SigV4 signature fails. Separate path-component encode from query-component encode.
- 📋 **MEDIUM — `s3_canonicalize_query_string` returns input verbatim.** `s3.c:445-459`. TODO. Currently works because all chosen query strings are already canonical by accident; future addition of `versionId` etc. silently misorders.
- 📋 **MEDIUM — `s3_update` reads entire file into RAM.** `s3.c:1838`. 5GB save state on a console with 256MB → OOM. Even on desktop: GB-scale uploads block the loop. Stream or cap.
- 📋 **MEDIUM — Netplay LAN-discovery memory exhaustion.** `network/netplay/netplay_frontend.c:386-402`. `discovered_hosts.allocated` grows by 4 per discovered LAN host, no upper bound. Attacker on hostile WiFi floods discovery → unbounded memory growth in user's "find LAN games" UI. Cap at 256.
- 📋 **MEDIUM — `task_database_cue` allocates from attacker-controlled CUE size.** `tasks/task_database_cue.c:1518-1523`. CUE-controlled `size` value with no upper bound. A multi-GB malloc request on a 64MB console = OOM-killer victim. Add `MIN(size, file_size)` plus a hard cap.
- 📋 **MEDIUM — Heap overflow waiting in `configuration.c` static pools.** `:90-96` defines `SETTINGS_BOOL_COUNT_MAX=512`, current bool count 419. The 514th `SETTING_BOOL` will heap-corrupt with no compile-time error. The comment at `:1779-1782` admits this. Add `static_assert` / runtime `assert(count < MAX)`.

### 🏗 Tier 3 — structural / cleanup

- 📋 **MENU — ~30 unguarded `MENU_LIST_GET_SELECTION(...)->size` derefs.** Already in audit roadmap as **S2 (cheap-to-spec)**. Indie-review confirmed and expanded — full list at `menu/cbs/menu_cbs_sublabel.c:1946`, `menu/cbs/menu_cbs_right.c:223`, `menu/menu_driver.c:7561`, `menu/drivers/ozone.c:7924, 8427, 10157, 13224, 12158`, `menu/drivers/xmb.c:1566, 1810, 1870, 1920, 3501, 6446, 7139, 10065, 10242`, `menu/drivers/materialui.c:3014, 3349, 10977, 12135`, `menu/drivers/rgui.c:5213, 7390, 7667, 8259`. Promote S2 to a full sweep — recurrence from the audit confirms this is the correct ordering.
- 📋 **MENU — Dead vtable slots.** `set_thumbnail_content` (declared at `menu_driver.h:406`, dispatched nowhere — every call uses the local static), `update_thumbnail_path` (dispatched at `menu_cbs_scan.c:148-153`, every driver registers NULL → dead dispatch), `list_prepend` (no driver fills, no caller dispatches), `navigation_increment` / `navigation_decrement` (dispatched 4× in `menu_driver.c`, every driver NULL). Single-PR cleanup deletes ~30 NULL stubs across drivers.
- 📋 **MENU — `materialui.c` is 12 231 lines with author-marked module boundaries.** Themes (~1200 lines), 5 view-types' compute/render triples, navigation, gestures. Author already drew the boundaries with banner comments. Same applies to `xmb.c` and `ozone.c`.
- 📋 **MENU — Per-driver gating in `menu_displaylist.c` is the coupling smell.** ~30 `string_is_equal(menu_ident, "...")` branches across menu_displaylist/menu_setting/menu_cbs_*. A capability-flag set on `menu_ctx_driver_t` (`flags & MENU_DRV_HAS_SIDEBAR_TABS`) would let each driver self-describe.
- 📋 **MENU — `materialui` ident is `"glui"` but file/struct is `materialui_*`.** UI labels say "MaterialUI". Three-way naming inconsistency means a `string_is_equal(menu_ident, "materialui")` check anywhere is a silent bug. (Recurrence from audit S11 — same class.)
- 📋 **DRIVER PATTERN — Four near-identical `*_driver_find_driver` implementations.** Audio:429-461, input:5044-5077, menu:4620-4648, video:3012-3034. ~120 lines of duplication that boils down to one `static bool generic_driver_select(label, prefix, &state_field, drivers[], verbosity)`. The shared helper already exists (`driver_find_index` in `retroarch.c:1262`); the wrappers are the duplication.
- 📋 **DRIVER PATTERN — Three different "null driver" philosophies in one repo.** `audio_null` is all-NULL (relies on every callsite NULL-checking); `input_null`/`video_null`/`menu_ctx_null` stub everything (removes that whole bug class). Pick one — preferably stub-everything — and apply it. Removes a class of "did the caller remember to NULL-check?" bugs by construction.
- 📋 **CONFIG — Boilerplate sprawl: 26 030 lines `menu_setting.c` + 7 725 lines `configuration.c` for a 4-touchpoint contract.** A table-driven approach (already partially used at `menu_setting.c:11608-11671` for bool entries) would let `populate_settings_*`, the menu list, and the load/save paths share one declarative table per type. ~3000 lines of duplication and the entire H1/H4 zombie/drift class go away by construction.
- 📋 **TASKS — `task_queue.c` lock-naming inconsistency.** `title`/`error`/`progress`/`flags` use `property_lock`; `task_data` uses `running_lock`. Asymmetric and undocumented. Pick one.
- 📋 **TASKS — `task_queue_push_progress` invokes `msg_push` callback while holding `property_lock`.** `task_queue.c:106-141`. Re-entrancy hazard if the callback ever calls `task_get_title` etc. — no current implementation does, but the API contract isn't documented.
- 📋 **TASKS — `task_http_iterate_transfer` busy-spins with `retro_sleep(1)`.** `tasks/task_http.c:113-114`. Acknowledged FIXME. Switch to event-driven `select`/`poll` on the underlying socket.
- 📋 **STATE — `runahead_save_state_size` cached at create time, never re-queried.** `runahead.c:937-970`. If a core update changes the state size, runahead operates on the wrong buffer. Re-query per frame.
- 📋 **STATE — `runahead` temp-DLL filename is predictable LCG seeded from `time(NULL)`.** `runahead.c:268-296`. Local-attacker race to plant a malicious DLL between `filestream_write_file` and `dylib_load`. Use `mkstemp`-style naming.
- 📋 **CHEEVOS — `rcheevos_filter_url_param` overlapping `strcpy`.** `cheevos_client.c:169`. Already in audit (#4). Confirmed by cloud-sync reviewer. Fix: `memmove(start, next+1, strlen(next+1)+1)`.

### 📝 Open questions (where intent is genuinely ambiguous)

These are things the agents flagged where they couldn't tell from the code alone whether the behavior is intentional. Worth asking in libretro discussions or filing as upstream issues:

- Is the network command socket *intended* to bind 0.0.0.0, or is "localhost-only" the assumed default and the AI_PASSIVE bind is an oversight? The setting name `network_cmd_enable` doesn't suggest LAN exposure.
- Is the netplay password feature considered real authentication or a casual gate? If real, the salt + memcmp issues are blockers; if casual, document loudly.
- Is the cloud-sync server expected to be authenticated and trusted, so path traversal is "by design"? README doesn't say.
- Why does `command_write_ram` not take a length bound when `command_write_memory` does?
- The `natt_parse_desc_node` recursion structure is broken — does NAT-PMP discovery actually work today against typical IGDs, or is this code path effectively dead?
- `RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK` (cmd 33) is unimplemented — silently returns false via the default case. Is this deliberate (RetroArch chooses not to expose extension hooks)? If so, an explicit case + comment would close the search.
- v0/v1 BSV replay format support — load-and-warn vs reject? Currently silently produces garbage emulation on size mismatch.

### 🧮 Tally

- **8 Critical** (TLS off, network-cmd RCE, write_ram OOB, natt NULL, webdav uninit, plus 3 cross-cutting clusters)
- **23 High**
- **18 Medium**
- **15 Tier-3 structural**
- **7 Open questions** — candidates for upstream issues


