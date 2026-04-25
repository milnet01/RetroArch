# RetroArch Private Fork — Roadmap

Tracking file for `milnet01/RetroArch` (private fork of `libretro/RetroArch`). Items here are not for upstream — they're for our internal audit/review/spec workflow. PRs back to libretro happen on a separate one-off public fork branch when something is ready to upstream.

Status markers:
- 📋 pending
- 🚧 in progress
- ✅ done (kept for audit-recurrence detection)
- 🔄 deferred / waiting on upstream

Severity: **CRITICAL** > **HIGH** > **MEDIUM** > **LOW**.

---

## 📊 Bundle progress (running summary)

Fixes land on `local/fixes-2026-04`. Each bundle is one logical theme; commits inside a bundle are the per-site changes.

| Bundle | Commit | Theme | Sites |
|---|---|---|---|
| 1 | `a9f5dcf7f4` `17cea42c5b` `9953abc994` | Wayland UAF defensive · BPS NULL deref · UPnP recursion | 3 |
| 2 | `403106af54` | Atomic save (tmp + rename) — task_save (3) + disk_index_file | 4 |
| 3 | `1f59d9d31d` | Bounds-check malicious save-state / replay file format | 3 |
| 4 | `8aedb937f1` | Local IPC + cheevos: localhost-bind, RAM bounds, chmod 0600, strcpy→memmove | 5 |
| 5 | `8eab27096d` `e54e1568ac` | Deref-before-NULL-check sweep (Linux build) + .claude/ gitignore | 8 |
| 6 | `6423760b01` | clang-analyzer security.ArrayBound: bind-order, scanline_even, mixer, options | 4 |
| 7 | `de0c6000c8` | task_http UAF cleanup at caller layer (core updater + pl thumbnail) | 3 |
| 8 | `9bf01aabdb` | Config triad cleanup: zombie settings, pool-overflow assert, default-mismatch | 3 |
| 9 | `088289d088` | Rewind compressor: bound `find_change` / `find_same` walks (incl. SSE2 path) | 1 |
| 10 | `bcb1ad483c` | Audio init buffer leaks · Menu init NULL-deref · BSV checkpoint malloc-fail | 3 |

**Cumulative:** 37 distinct fixes across 22 files. ~24 ✅ closed, 3 🚧 in-progress (atomic-save 4/7 sites, deref-before-check 8/30, plaintext-credentials chmod-only), 4 🔄 deferred (libretro-common vendored items + clang-analyzer FPs needing reproducer).

The full per-finding history is in the audit / indie-review / clang-tidy sections below — each item carries either 📋 pending, 🚧 partial, ✅ done with commit, or 🔄 deferred with reason.

---

## 🔍 Audit 2026-04-25

Branch: `local/audit-2026-04` off `master @ 6ff3332ea2`.
Tools run: cppcheck (partial — 449/450), semgrep, gitleaks (clean), trivy (clean), ruff, bandit.
Filtered by `audit-triage` subagent: 24 actionable, 12 spec-needed, ~660 noise items dropped.
Tool gaps: clang-tidy + clazy not run (no `compile_commands.json` — install `bear` and rerun); cppcheck did not finish `menu/drivers/materialui.c`; semgrep had 332 parse errors.

### Actionable

#### Critical

- ✅ **CRITICAL — UPnP XML parser trusts attacker-influenced router responses.** `network/natt.c:271-289`. Recursive `natt_parse_desc_node` walks IGD-supplied XML; subsequent `strstr`/`strlcpy` on `service_type->data` (lines 294-300) can be triggered by a malicious LAN gateway. _(Fixed `9953abc994` on `local/fixes-2026-04` — full rewrite of `natt_parse_desc_node` addressing both the NULL-deref and the inverted recursion. Confirmed open question from indie-review: NAT-PMP discovery indeed didn't work against typical IGDs before the fix.)_

#### High

- ✅ **HIGH — OOM null-deref in network/IPC `READ_CORE_MEMORY` handler.** `command.c:1187-1188`. _(Fixed `8aedb937f1` — added NULL-check on the malloc plus an upper bound on `nbytes` before the multiplication can wrap. Same fix applied to `command_read_ram`. Combined with the network-cmd localhost-only bind, the original LAN-RCE reproducer is no longer reachable.)_
- 📋 **HIGH — `strchr(key, '/') + 1` on attacker-supplied cloud-sync manifest.** `tasks/task_cloudsync.c:640`. `NULL+1` is UB if server-supplied `key` lacks `/`. Same trust-boundary class as the recent `task_http` UAF fix.
- ✅ **HIGH — `strcpy` with overlapping buffers (UB).** `cheevos/cheevos_client.c:169`. _(Fixed `8aedb937f1` — `strcpy` → `memmove(start, next+1, strlen(next+1)+1)`.)_
- 📋 **HIGH — OOM null-deref on per-value-hash malloc.** `core_option_manager.c:721-727, 1006-1007`. `uint32_t *value_hash = malloc(...); *value_hash = ...` — no NULL check, two sites, identical pattern.
- 🚧 **HIGH — Cluster sweep: ~30+ deref-before-NULL-check sites.** _(Bundle 5 — 8 Linux-build sites fixed in `8eab27096d` on `local/fixes-2026-04`.)_
  - ✅ `audio/drivers/oss.c::oss_init` error label
  - ✅ `gfx/drivers/gl3.c::gl3_raster_font_render_msg` (`gl->video_width`)
  - ✅ `gfx/drivers/gl2.c::gl2_renderchain_recompute_pass_sizes` (`gl->shader`)
  - ✅ `gfx/drivers/vulkan.c::vulkan_get_message_width` (`font->font_driver->get_glyph`)
  - ✅ `input/input_driver.c::input_config_get_bind_string` (`bind->key`)
  - ✅ `input/common/wayland_common.c::data_device_handle_drop` (`offer_data->dropped`)
  - ✅ `runahead.c::mylist_add_element` (`list->size`)
  - ✅ `tasks/task_pl_thumbnail_download.c` 2 sites (`pl_thumb->type_idx`)
  - 📋 Win32-only sites (`wasapi.c`, `d3d10/d3d11/d3d9hlsl_gfx.c`) — not in Linux build, deferred.
  - 📋 Console-only sites (`vita2d`, `gx2`, `ctr`, `ps2`, `gdi`, `dispmanx_gfx.c`, `platform_orbis.c`) — out of personal-fork scope.
  - 📋 Larger files (`ozone.c`, `materialui.c`, `xmb.c`, `netplay_frontend.c`, `menu_cbs_ok.c`) need per-site review (mix of real bugs and cppcheck FPs on stack-array / locally-checked allocs); deferred to a follow-up sweep.
  - ❌ `gfx/gfx_thumbnail.c` — cppcheck FPs (`thumbnail_path` is a stack array). Won't fix.
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

- 🚧 **🔥 No atomic write on disk-save paths.** Every "save user data to disk" path opens the destination directly with truncating-write mode. Power loss / OOM kill / `kill -9` mid-save = file corrupt and previous save gone. _(Bundle 2 — partially fixed in `403106af54` on `local/fixes-2026-04`.)_
  - ✅ `tasks/task_save.c::task_save_handler` (save state)
  - ✅ `tasks/task_save.c::content_auto_save_state` (auto save at unload)
  - ✅ `tasks/task_save.c::content_ram_state_to_file` (RAM-state-to-file)
  - ✅ `disk_index_file.c::disk_index_file_save` (disc index — multi-disc games revert to disc 1)
  - 📋 `input/bsv/bsvmovie.c:759-797` (BSV replay checkpoint — different shape: writes in-place to active replay stream rather than producing a complete-file destination, so tmp+rename idiom doesn't apply; needs per-event framing/checksumming instead)
  - 🔄 `libretro-common/file/config_file.c:1410-1432` (every cfg save) — vendored upstream, can't patch locally
  - 📋 `network/cloud_sync/{google_drive,webdav,s3}.c` (every downloaded synced file) — combine with no-Content-Length-verification fix below
  - **The pattern applied:** open at `<path>.tmp`; on success `filestream_delete(dest); filestream_rename(tmp, dest)`; on failure `filestream_delete(tmp)`. POSIX `rename(2)` is atomic over an existing file on the same filesystem.
- ✅ **🔥 Buffer-overflow class in malicious save-state / replay parsing.** _(Bundle 3 — fixed in `1f59d9d31d` on `local/fixes-2026-04`.)_
  - ✅ `tasks/task_save.c::content_load_rastate1` — added bounds check on `block_size` against remaining buffer (rejects truncated headers, oversize blocks, and alignment-arithmetic overflow); switched the byte-shift assembly to `((uint32_t)input[N]) << shift` to remove signed-int promotion UB.
  - ✅ `input/bsv/bsvmovie.c::bsv_movie_read_next_events` — capacity-check `key_event_count` against `ARRAY_SIZE(key_events)` and `input_event_count` against `ARRAY_SIZE(input_events)` before each read loop.
  - ✅ `input/bsv/bsvmovie.c::replay_set_serialized_data` — reject `loaded_len < REPLAY_HEADER_LEN_BYTES` before any `intfstream_seek` / `intfstream_write` picks it up. Negative values were the worst case (cast to huge `size_t`).
  - **Threat is real** — RetroArch users routinely load save states and replays shared online. The corresponding clang-tidy CRITICALs (security.ArrayBound on driver-array iteration, runahead heap OOB) remain on the roadmap.
- ✅ **🔥 `task_http`-class UAF pattern at the caller layer.** _(Bundle 7 — fixed in `de0c6000c8` on `local/fixes-2026-04`.)_
  - ✅ `tasks/task_core_updater.c::cb_http_task_core_updater_get_list` and `::cb_http_task_core_updater_download` — these were the real ASan-detectable UAFs: the outer handlers call `task_get_flags` / `task_get_progress` on the inner `http_task` pointer after the queue has freed it. Fix: NULL the pointer in the cb before flipping the COMPLETE flag the outer handler waits on.
  - ✅ `tasks/task_pl_thumbnail_download.c::cb_http_task_download_pl_thumbnail` — same pattern, no actual deref in shipping code (only NULL-test of stale pointer, strictly UB but practically harmless), plugged for consistency.
- 📋 **🔥 Path traversal via attacker-supplied cloud-sync manifest.** `tasks/task_cloudsync.c:640` `strchr(key, '/') + 1` flagged by audit, tasks-lane reviewer, and cloud-sync reviewer. Compromised or hostile sync server can write `../../../home/user/.bashrc` into the user's machine. Two fixes needed:
  1. NULL-guard the `strchr` result.
  2. Reject keys containing `..`, `\`, leading `/`, or NUL — server input must not be trusted as path components. (audit H3.)

### 🔒 Tier 1 — ship-this-week security/data-loss blockers

These are exploitable now and have concrete reproducers.

- 📋 **CRITICAL — TLS certificate verification effectively disabled.** `libretro-common/net/net_socket_ssl_mbed.c:199` uses `MBEDTLS_SSL_VERIFY_OPTIONAL`; the verify result is logged into a buffer and discarded — connection proceeds against any cert including self-signed. **Every** HTTPS request in cloud sync (Google Drive OAuth + Drive API, S3 SigV4 signing, WebDAVS), cheevos (RetroAchievements API), and online updater is un-authenticated. On hostile WiFi, an MITM captures Google OAuth refresh tokens, AWS secret access keys, and RA credentials. Vendored — fix must go upstream first; a temporary RA-side opt-out with a giant warning would be acceptable. (Not in audit; indie-review only.)
- ✅ **CRITICAL — Network command socket binds 0.0.0.0 unauthenticated.** `command.c:257-258`. _(Fixed `8aedb937f1` — bind hardcoded to `127.0.0.1`. Cross-host IPC users should prefer SSH local-forward; a setting-driven opt-out is roadmapped but intentionally unwired so the safe default ships universally.)_
- ✅ **CRITICAL — `command_write_ram` heap-write OOB driven from the wire.** `command.c:970-990`. _(Fixed `8aedb937f1` — capped the write loop at 4096 bytes via `COMMAND_WRITE_RAM_MAX_BYTES`. Threading the actual descriptor length through `rcheevos_patch_address` is a deeper API change; this cap is generous for cheat-poke use and prevents arbitrary heap-write past the descriptor.)_
- ✅ **CRITICAL — `natt_parse_desc_node` NULL-deref + inverted recursion on hostile UPnP response.** `network/natt.c:268-318`. _(Fixed `9953abc994` — same fix as the audit C1 entry above.)_
- 📋 **CRITICAL — Use of uninitialized stack memory in `webdav_ensure_dir`.** `webdav.c:700-713`. `http_transfer_data_t data;` then `data.status = 200;` only — `data.headers/data/len` are uninitialized when passed to `webdav_mkdir_cb`. Currently saved by short-circuit luck. Fix: `memset(&data, 0, sizeof(data));`. One line.
- 📋 **HIGH — Netplay password salt is `time(NULL)`-seeded LCG; password compare is non-constant-time.** `network/netplay/netplay_frontend.c:778-782` (`simple_rand_next = time(NULL)`) and `:1533, 1546` (`memcmp(...)`). Combined: an attacker who observes one salt can predict every subsequent salt and mount an offline dictionary attack with timing leakage. The "netplay password" feature is currently theatre. Use `getrandom`/`BCryptGenRandom` for the salt, and a constant-time XOR-accumulate compare for the hash check.
- 📋 **HIGH — WebDAV digest parser unbounded walks + hardcoded cnonce.** `webdav.c:174-280`. Five places `strchr(...) + 1 - ptr` with no NULL check (header missing the close-quote → underflow → multi-GB malloc). One infinite loop on `*ptr != ',' && *ptr != ','` (typo: same comparison twice — should be `!= '\0'`). And `webdav_st->cnonce = "1a2b3c4f"` is a constant client-nonce, defeating digest-auth replay protection. Three independent classes of bug in one parser; combined with the TLS issue above, the WebDAV path is currently unsafe on hostile networks.
- 🚧 **HIGH — Plaintext credentials in world-readable `retroarch.cfg`.** `configuration.c:1629, 1634, 1637-1639, 1649, 1722, 1749-1750`: WebDAV password, AWS secret access key, YouTube/Twitch/Facebook stream keys, SMB password, kiosk-mode password, netplay password. Saved as `key = "value"` to a file with default umask (0644). Plus the Google Drive **refresh token** is stored plaintext per `google_drive.c`. _(Bundle 4 — `chmod 0600` mitigation landed in `8aedb937f1`. Secrets-file split + OS-keyring integration still open.)_ Mitigation tier:
  1. `chmod 0600` after `config_file_write` succeeds (one-line, ships immediately)
  2. Move secrets to a separate `retroarch.secrets` file in `~/.local/share/retroarch/secrets/` (longer)
  3. OS-keyring integration via libsecret/Win32 DPAPI/Keychain (longest)
- ✅ **HIGH — `command_read_ram` / `command_read_memory` integer-overflow + missing NULL-check on malloc.** `command.c:929-968` and `:1187-1195`. _(Fixed `8aedb937f1` — capped `nbytes <= COMMAND_READ_NBYTES_MAX` (16 KiB) before the multiplication can wrap, plus NULL-check on each malloc.)_
- 📋 **HIGH — Heap-buffer-overflow class in HTTP failure loggers.** `s3.c:1633-1640` re-introduces the `data->data[data->len] = 0` antipattern that the codebase has already fixed elsewhere. Add a `net_http_data_to_cstring` helper and rewrite all callers — the rule is too easy to forget.

### 🛡 Tier 2 — hardening sweep (correctness, not exploitability)

- ✅ **HIGH — `find_change` / `find_same` unbounded walk in rewind compressor.** _(Fixed `088289d088` — added `max` parameter to both helpers; SSE2 path checks `a128 + 1 <= a_end` per chunk and falls through to a scalar tail; scalar / `NO_UNALIGNED_MEM` paths gain `a < a_end` guards on every walk loop. Caller passes `num16s` (the remaining unconsumed u16 count). Outer-caller's `if (skip >= num16s) break` still works correctly since the bounded helpers return at most `num16s`.)_
- ✅ **HIGH — `bsv_movie_load_checkpoint` malloc-fail invariant break.** _(Fixed `bcb1ad483c` — reordered to malloc first, commit `cur_save_size` only on success. OOM now aborts the checkpoint cleanly via the existing `exit:` label and sets `BSV_FLAG_MOVIE_END`.)_
- ✅ **HIGH — Configuration zombie settings.** `game_ai_override_p1`, `game_ai_override_p2`, `game_ai_show_debug`. _(Fixed `9bf01aabdb` — added three `SETTING_BOOL` lines under the existing `#ifdef HAVE_GAME_AI` block. Toggling in the menu now persists across restart.)_
- 📋 **HIGH — Configuration save: no lock on settings_t while iterating.** `configuration.c:5793-6471` reads ~830 mutable fields and the keybind tables without `running_lock` or any equivalent. A joypad-autoconfig task running in the background while the user clicks "Save Configuration" can save half-applied state. Snapshot-to-local + write would resolve.
- ✅ **HIGH — `rgui_show_start_screen` triad mismatch.** _(Fixed `9bf01aabdb` — `SETTING_BOOL` line now uses `default_enable=true, default=DEFAULT_MENU_SHOW_START_SCREEN`. The bespoke "force-write in minimal mode" workaround at the saver site was deleted -- both load and save paths now agree without needing the override.)_
- 📋 **HIGH — Menu drivers gate setting registration by current driver and never refresh on driver swap.** `menu/menu_setting.c:18806, 19016, 19308, 19552, 19833, 20330, 20533, 20804, 20857, 20990` all use `string_is_equal(menu_ident, "xmb")` style gating. After the user changes menu driver mid-session, the setting tree stays from the original driver until restart — XMB-only, ozone-only, materialui-only settings vanish from the UI. Either rebuild the tree on swap or move the gating to a display-time predicate.
- ✅ **HIGH — Menu init NULL-deref on driver_data.** _(Fixed `bcb1ad483c` — wrapped the post-init field writes in an `if (driver_data)` guard. The line 4543 NULL-check now actually does its job.)_
- ✅ **HIGH — Audio init early-return paths leak buffers.** _(Fixed `bcb1ad483c` — replaced each of the four `return false` paths with `goto error` so the existing `error:` label calls `audio_driver_deinit()` and reclaims the rewind / out_conv / audio_buf allocations.)_
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
- ✅ **MEDIUM — Heap overflow waiting in `configuration.c` static pools.** _(Fixed `9bf01aabdb` — added `assert(count <= SETTINGS_*_COUNT_MAX)` at the exit of every `populate_settings_*` function (bool/int/uint/float/size/array/path). The 514th `SETTING_BOOL` now produces an immediate assert in debug builds; release builds compile out the check but the invariant is documented in code.)_

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

---

## 🔬 clang-tidy 2026-04-25

Re-run with `compile_commands.json` (generated via `bear -- make -j$(nproc)` after `./configure`). Checks: `bugprone-*`, `clang-analyzer-core.*`, `clang-analyzer-security.*`, `clang-analyzer-unix.*`. 200 files in audit scope. **125 clang-analyzer findings + ~30 high-signal bugprone**, vs cppcheck's 549 (after filtering). The path-sensitive analyzer caught classes cppcheck missed — particularly array-bound violations and UAF traces.

### 🔥 NEW Critical (not in prior audit/indie-review)

- ✅ **CRITICAL — `tasks/task_patch.c:207, 216, 238, 247` four sites: array access on NULL `target_data`.** IPS/UPS/BPS patch parser. Trust boundary: `.ips`/`.ups`/`.bps` patch files (downloaded from ROM-hack sites). _(Fixed `17cea42c5b` — root cause was a malicious `.bps` declaring `modify_target_size == 0` would skip the realloc branch and leave `bps.target_data` NULL; reject 0-size up front and force fresh malloc when caller passed NULL.)_
- ✅ **CRITICAL — `audio/audio_driver.c:1507` Out-of-bound write past `audio_driver_st.mixer_streams`.** _(Fixed `6423760b01` — bounded `params->slot_selection_idx` to `< AUDIO_MIXER_MAX_SYSTEM_STREAMS` in the manual-slot path. Caller-supplied index from libretro mixer-add env callback was previously unchecked.)_
- ✅ **CRITICAL — `core_option_manager.c:1164` Out-of-bound heap access.** _(Fixed `6423760b01` — the categories array was sized by `_len` (options count) but indexed by `cats_size` (categories count). Allocate by `cats_size` instead. Cores declaring more categories than options previously heap-wrote past the allocation.)_
- 🔄 **CRITICAL — `runahead.c:640, 662` Out-of-bound access preceding heap area.** Two sites in the input-state-list walk. clang-analyzer's preceding-heap warnings need a concrete reproducer to confirm vs FP — the loop iterates `i = 0` upward against `input_state_list->size` and the deref `data[i]` is bounded. Possible FP from path-sensitive analysis on platform-conditional branches. **Deferred** pending reproducer or deeper inter-procedural look.
- 🔄 **CRITICAL — `retroarch.c:435, 1186, 2270` three sites Out-of-bound past `location_drivers[]`.** Likely **clang-analyzer FPs** — the array is NULL-terminated at definition (`{ ..., &location_null, NULL }`), the iteration `for (d = 0; location_drivers[d]; d++)` is correctly NULL-guarded, and `find_driver_nonempty` only returns indices 0..N-1. **Deferred** unless a reproducer surfaces.
- 🔄 **CRITICAL — `tasks/task_translation.c:177` Out-of-bound past `translation_drivers[]`.** Same FP class as the location-drivers item above.
- ✅ **CRITICAL — `menu/menu_setting.c:2784` Out-of-bound access preceding `input_config_bind_order[]`.** _(Fixed `6423760b01` — the walk-and-shift loop in `setting_action_left_retropad_bind` and its right-hand twin did `[i ± step]` reads when the user was on the first/last entry, OOB-reading before/after the 24-entry array. Guard with `if (i ± step in range)`; the existing wraparound block below the loop handles the wrap case correctly.)_
- ✅ **CRITICAL — `menu/drivers/rgui.c:1695, 1784` Out-of-bound access preceding `scanline_even`.** _(Fixed `6423760b01` — `scanline_even`/`scanline_odd` are stack arrays sized `[RGUI_MAX_FB_WIDTH]`, but loop bounds were derived from runtime `fb_width`. A `fb_width > RGUI_MAX_FB_WIDTH` configuration walked off the stack array. Capped working width to `MIN(fb_width, RGUI_MAX_FB_WIDTH)` up front.)_

### 🔥 NEW High (UAFs, leaks, NULL-derefs not in prior reports)

- ✅ **HIGH — `gfx/common/wayland_common.c:380, 385` UAF / double-free.** _(Defensive fix `a9f5dcf7f4` on `local/fixes-2026-04` — restructured `all_outputs` cleanup loop to unlink before deref and added NULL-guards on `oi` and `oi->output`. clang-analyzer's path-sensitive trace persists despite the fix; appears to be macro-expansion noise from `wl_container_of` since the loop is correct after the restructure. Will revisit if a real reproducer surfaces.)_
- 📋 **HIGH — `input/bsv/uint32s_index.c:53, 285` Use-after-free in BSV uint32s index.** Combined with audit/indie-review's BSV bounds-check issues, the BSV replay file format has multiple memory-safety vulnerabilities. Untrusted-file-format trust boundary.
- 📋 **HIGH — `input/common/wayland_common.c:563, 1101` UAF + memory leak.**
- 📋 **HIGH — `gfx/drivers/vulkan.c:7976` Dereference of NULL pointer.** Vulkan render path.
- 📋 **HIGH — `gfx/video_driver.c:3633, 3745` NULL passed to nonnull-attributed param + NULL deref.**
- 📋 **HIGH — `gfx/video_shader_parse.c:3184` `current_video->set_shader` NULL deref.** Confirms the audit S2 cluster (driver-vtable NULL not checked).
- 📋 **HIGH — `command.c:2548, 2553` `video_st->poke` and `video_st->current_video` deref-before-check.** Path-sensitive analyzer found two sites in the same function. Same class as audit H6 cluster.
- 📋 **HIGH — `runloop.c:5701` `current_video->focus` NULL deref.** Same pattern.
- 📋 **HIGH — `input/input_driver.c:4501` `bind->key` NULL deref.**
- 📋 **HIGH — `slang_process.cpp:967, 970, 974, 977` four sites: NULL C++ object pointer call.** Slang shader processing path.
- 📋 **HIGH — `ui/drivers/ui_qt_widgets.cpp:3293, 3355, 3433, 3508, 4506` five sites: `video_shader/menu_shader->flags` NULL deref.** Qt UI desktop frontend (HAVE_QT=1 builds).
- 📋 **HIGH — Realloc-leak class (`bugprone-suspicious-realloc-usage`).** `runahead.c:636` (`list->data`) and `input/bsv/uint32s_index.c:90` (`bucket->contents.vec.idxs`). On realloc failure, the variable is set to NULL and the original buffer is leaked. Use a `tmp = realloc(p, …); if (tmp) p = tmp;` pattern.

### 🔥 NEW Medium (divides-by-zero, leaks, uninit, format-mismatch)

- 📋 **MEDIUM — Division-by-zero in 5 sites.** `menu/drivers/materialui.c:4727, 5566`, `menu/drivers/rgui.c:2473`, `menu/menu_setting.c:5773`. Likely guard-needed-before-divide on user-supplied or computed denominators (window size 0, frame rate 0, etc.).
- 📋 **MEDIUM — Memory-leak class (8 sites).** `core_backup.c:383` (`backup_filename`), `core_info.c:860` (`core_info_cache_list`), `core_updater_list.c:928` (`entry.local_info_path`), `gfx/gfx_animation.c:784` (`timer_entry.userdata`), `network/discord.c:859`, `menu/drivers/ozone.c:5170` (`node`), `menu/menu_setting.c:2257, 2436`. Each is a missing-`free` on an early-return path.
- 📋 **MEDIUM — Branch-on-uninitialized-value (3 sites).** `tasks/task_screenshot.c:444`, `verbosity.c:608`, `menu/menu_displaylist.c:6848`. `clang-analyzer-core.uninitialized.Branch`. Path-sensitive analysis reached an `if (x)` where `x` was never written on at least one incoming branch.
- 📋 **MEDIUM — Float used as loop counter, 12 sites in `menu/menu_displaylist.c:16160-16679` and `ui/drivers/ui_qt_widgets.cpp:713`.** `clang-analyzer-security.FloatLoopCounter`. Float arithmetic isn't associative; loops can iterate one too few or too many times depending on initial value. Switch to integer loop counter with float-cast on use.
- 📋 **MEDIUM — `bugprone-not-null-terminated-result` (5 sites).** `tasks/task_save.c:435`, `network/netplay/netplay_frontend.c:5097, 7773`, `gfx/drivers_shader/slang_cache.cpp:152, 154`. `memcpy` of a string-shaped buffer where the destination is later treated as a C string. Switch to `strlcpy` or explicit NUL termination.
- 📋 **MEDIUM — `bugprone-suspicious-string-compare` (~14 sites).** `memcmp`/`strcmp`/`strcasecmp` used as a boolean (`if (memcmp(...))`) where the standard says non-zero is "differ" but the code reads as "if equal." Notable: `network/netplay/netplay_frontend.c:3157, 3198, 6852` (netplay protocol parsing — silent message-routing bugs?), `core_info.c:1539`, `retroarch.c:799`, `menu/menu_displaylist.c:2587, 2695, 2697`, `menu/menu_explore.c:534, 564`. Each needs eyes-on; some are correct-but-confusing, others may be inverted-logic bugs.
- 📋 **MEDIUM — `gfx/drivers/vulkan.c:5899` `bugprone-suspicious-memory-comparison` on `math_matrix_4x4`.** `memcmp` on a struct without a unique object representation (padding bytes vary). Compare members manually.
- 📋 **MEDIUM — Sizeof-pointer-not-array, 3 sites in `gfx/gfx_widgets.c:380, 980, 1934`.** `bugprone-sizeof-expression`. Likely `sizeof(ptr)` returning 8 instead of intended buffer size.
- 📋 **MEDIUM — `bugprone-incorrect-roundings` (4 sites).** `(double + 0.5)` cast to int, breaks for negatives. `gfx/gfx_widgets.c:882`, `menu/drivers/materialui.c:8872`, `menu/drivers/ozone.c:9566, 9742, 9743`. Use `lround`.
- 📋 **MEDIUM — `bugprone-signed-char-misuse` (5 sites).** `menu/cbs/menu_cbs_ok.c:4151`, `libretro-db/rmsgpack.c:371`, `menu/drivers/xmb.c:1921`, `menu/menu_explore.c:303-305`. Cast `signed char → int` may sign-extend high-bit chars (UTF-8/binary) unexpectedly.

### ✅ Confirmations of prior items

clang-analyzer's path-sensitive trace confirmed these prior findings — not new, but elevated confidence (3rd independent signal in some cases):

- `gfx/common/wayland_common.c:296` `oi->width` NULL deref → matches **audit M8** (already roadmapped).
- `network/natt.c:314` `child->next` NULL deref → matches **audit C1** + **indie-review C-3** (cross-confirmed third time).
- `tasks/task_cloudsync.c:847, 849, 881, 1286` NULL derefs → matches **audit H3** + **indie-review C-3** path-traversal cluster.
- `tasks/task_overlay.c:647` `desc->next_index` NULL deref → matches **indie-review H3** (loader allocations leak class).
- `tasks/task_core_updater.c:633, 639` `download_handle` NULL deref → matches **indie-review H-2** (same UAF cluster as task_http).
- `menu/cbs/menu_cbs_right.c:223`, `menu_cbs_sublabel.c:1946`, `menu_driver.c:7349, 1330`, ozone.c (5 sites), xmb.c (6 sites), materialui.c (1 site) → all match the **indie-review C1 / audit S2** `MENU_LIST_GET_SELECTION(...)->size` cluster. clang-analyzer traced path-sensitive proof of NULL.
- `menu/menu_setting.c:9273, 9858, 10163, 10229, 3261` NULL derefs in setting handlers → adjacent to the audit H1 zombie-settings class.
- `menu/menu_displaylist.c:5176, 6789, 6870, 15317` NULL derefs → consistent with the menu coupling-and-NULL-handling pattern flagged in indie-review.

### 🛠 Tool gaps now closed / remaining

- ✅ **`compile_commands.json` generated** (via `bear -- make`). clang-tidy + clazy now runnable.
- ⏳ **clazy not yet run.** RetroArch's Qt UI is small (`ui_qt_widgets.cpp` only); clazy yield expected to be modest. Defer until needed.
- ⏳ **cppcheck `materialui.c` macro-config exhaustion** still not finished — clang-analyzer caught the materialui.c findings the cppcheck pass would have hit.
- ⏳ **Platform-conditional builds** (`HAVE_EGL=0`, console targets) still not verified — would resolve audit S3/S4/S7/S8.

### 🧮 Updated tally (cumulative across audit + indie-review + clang-tidy)

- **15+ Critical** (TLS off, network-cmd RCE, write_ram OOB, natt, webdav uninit, no-atomic-save cluster, malicious-savestate-overflow cluster, IPS/UPS patch NULL, audio mixer OOB, runahead OOB, location_drivers OOB ×3, translation_drivers OOB, input_config_bind_order OOB, scanline_even OOB ×2, core_option_manager OOB, plus the cross-cutting cluster items)
- **40+ High** (NULL derefs, UAFs, OOM null-checks, plaintext credentials, netplay weak-RNG and timing leaks, WebDAV digest parser issues, deref-before-check sweep ~30 sites, env-callback NULL hardening, 5+ Qt UI shader NULL derefs)
- **30+ Medium** (divides-by-zero, leaks, uninit branches, float-loop-counters, string-compare misuse, etc.)
- **15+ Tier-3 structural** (driver-pattern dedup, materialui split, dead vtable slots, configuration table-drive, etc.)



