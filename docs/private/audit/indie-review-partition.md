# Indie-review subsystem partition (memoized)

Lane partition for `/indie-review` on this fork. Loaded by the orchestrator at Phase 1 instead of being re-decided every run.

## Why this exists

The 2026-04-25 indie-review run took ~1.5M tokens across 8 lanes. Re-deciding "what are the subsystems? what's the line range for materialui.c? what specs apply to s3.c?" every run wastes ~10-15% of orchestrator tokens before the first agent dispatches. The partition rarely changes (it's the project's architecture, not its current state) — pin it.

A second motivator: each lane's gotcha list is **author-internalised knowledge** the agents would otherwise miss. The roadmap documents these in narrative form ("✅ HIGH — Buffer-overflow class in malicious save-state / replay parsing") but a fresh agent reading source cold can't extract them. This file lifts each lane's known-traps into a per-lane brief block.

## How orchestrator should consume

```
Phase 1, step 1c — load this partition
   for each lane:
      brief = base_brief
            + per-lane source paths (with file ranges)
            + per-lane contract docs
            + per-lane external specs
            + per-lane gotchas (verbatim from the "Known traps" section)
            + per-lane ROADMAP slice (grep ROADMAP.md for the lane's file paths;
              attach matching lines so agent can dedup against existing findings)
      dispatch agent with this brief
```

The ROADMAP-slice attachment is **the point** — it's the difference between "8 agents independently re-flag MENU_LIST_GET_SELECTION as a HIGH" and "1 agent in the menu lane sees that's already on the roadmap as S2 and skips it." Agent input goes up by ~5k tokens; agent output drops by 30-50%.

## Big-file pre-split (line ranges from author banner comments)

The 18 files >5k LoC each have author-marked module boundaries. Each line range below maps to one logical concern; an agent reviewing the lane should be told **only** to look at the relevant ranges, not the whole file.

| File | LoC | Boundaries (verify against current banner comments before each run) |
|---|---:|---|
| `menu/menu_setting.c` | 26056 | `populate_settings_bool` / `_int` / `_uint` / `_float` / `_size` / `_array` / `_path` blocks; line 11608-11671 already uses table-driven pattern |
| `menu/menu_displaylist.c` | 16785 | per-driver gating sites at 2587, 2695, 2697, 5176, 6789, 6848, 6870, 15317; coupling-smell cluster |
| `menu/drivers/ozone.c` | 13458 | sidebar (~0-2500), main grid (~2500-5500), thumbnails (~5500-7500), playlists (~7500-10000), settings views (~10000-13458); boundaries are author-banner-comment-driven |
| `menu/drivers/materialui.c` | 12244 | themes ~1200 lines; 5 view-types' compute/render triples; navigation; gestures (boundaries ARE banner comments — read the file head for the canonical list) |
| `menu/drivers/xmb.c` | 10381 | category bar / item list / thumbnails / drawer animations |
| `network/netplay/netplay_frontend.c` | 10357 | discovery, handshake, sync, frame, message routing |
| `menu/cbs/menu_cbs_ok.c` | 10219 | per-action OK handlers (1 file = ~150 handlers); split by action class |
| `retroarch.c` | 9167 | global init, runloop entry, driver wiring, signal handling |
| `ui/drivers/ui_qt_widgets.cpp` | 8835 | Qt UI — single lane (UI/Qt) |
| `runloop.c` | 8552 | env-callback dispatcher, frame timing, env-callback NULL guards |
| `menu/drivers/rgui.c` | 8467 | retro UI driver — single concern, no further split |
| `input/input_driver.c` | 8385 | bind config, autoconfig, hotkey, remap |
| `gfx/drivers/vulkan.c` | 8353 | Vulkan backend — single concern |
| `menu/menu_driver.c` | 8335 | menu state machine, driver dispatch, transitions |
| `gfx/drivers/d3d9hlsl.c` | 8312 | Windows-only, **out of Linux scope** — drop unless reviewing Windows port |
| `configuration.c` | 7768 | populate, save, load, defaults — paired with menu_setting.c |
| `gfx/drivers/d3d12.c` | 7125 | Windows-only, **out of Linux scope** |

(LoCs above as of 2026-05-18 / Bundle 70 audit-branch state. The doc-header says "verify against current banner comments before each run" — the actual mechanism is `wc -l` on each path; re-run before any indie-review dispatch.)

Where the boundaries are unstable / unmapped, the lane brief should say so explicitly: "review materialui.c lines 0-1200 (themes); ignore the rest."

---

## Lane partition (8 lanes)

The 2026-04-25 sweep used these 8 lanes. They've held up across 33 fix bundles — the partition is stable.

### Lane 1 — libretro env-callback boundary

**Source paths:**
- `runloop.c` (focus on `RETRO_ENVIRONMENT_*` dispatch — main env-callback entry)
- Anywhere `RETRO_ENVIRONMENT_` appears (grep first; cluster ~30 sites)
- `libretro-common/include/libretro.h` (vendored — read for contract, do not flag)

**Contract docs:** `libretro.h` env-callback descriptions; `RETRO_ENVIRONMENT_*` numeric command list.

**External specs:** the libretro env-callback ABI — every env command has a documented `data` pointer type.

**Known traps:**
- Every env case that derefs `data` must NULL-guard first (Bundle 14 fixed 8 sites).
- `RETRO_ENVIRONMENT_GET_LANGUAGE` must write a defined value when `HAVE_LANGEXTRA` is undefined (Bundle 14).
- `RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK` (cmd 33) currently silently returns false — open question whether this is deliberate.

**Token budget:** 30-40k input, 5-10k output. Cheap lane.

---

### Lane 2 — configuration triad

**Source paths:** `configuration.c`, `configuration.h`, `config.def.h`, `menu/menu_setting.c` (yes, all 26k lines — but split by populate_settings_* functions per the big-file table above).

**Contract docs:** the four-touchpoint contract — `populate_settings_*` (in configuration.c), the menu setting registration (`menu_setting.c::menu_setting_get_*`), the load path (`config_load_internal`), the save path (`config_save_file`).

**External specs:** none external; the contract is internal.

**Known traps:**
- Zombie settings (declared but never read/written) — Bundle 8 found 3, fix-pattern is `SETTING_BOOL` line under the matching `#ifdef`.
- Pool-overflow from adding a new SETTING — Bundle 8 added `assert(count <= SETTINGS_*_COUNT_MAX)` at exit of every `populate_settings_*` function.
- Default-mismatch between populate / load / save paths (e.g. `rgui_show_start_screen` triad mismatch fixed in Bundle 8).
- Plaintext credentials class — chmod 0600 mitigation landed in Bundle 4; secrets-file split + OS-keyring still open.
- `string_is_equal(menu_ident, "...")` per-driver gating sweeping all four touchpoints — Tier 3 structural finding.

**Token budget:** 80-120k input, 15-25k output. Heavy lane — the populate_settings_* sprawl is the bulk.

---

### Lane 3 — driver-pattern meta

**Source paths:** the four `*_driver_find_driver` implementations (line ranges below are best refreshed by `grep -n '^bool .*_driver_find_driver\b'` before dispatch — file growth drifts the anchors each bundle):
- `audio/audio_driver.c:429` (also `microphone_driver_find_driver` at `:2272`)
- `input/input_driver.c:5047`
- `menu/menu_driver.c` (`menu_driver_find_driver` — re-grep)
- `gfx/video_driver.c:2903`

Plus the shared helper `driver_find_index` in `retroarch.c:1262`.

Plus the `*_null` driver triplet (`audio_null`, `input_null`/`video_null`/`menu_ctx_null`) for the "null-driver philosophy" review.

**Contract docs:** `*_driver_t` struct definitions in respective headers; `null` driver convention (currently inconsistent — see Tier 3 finding).

**External specs:** none external.

**Known traps:**
- The four implementations are ~120 lines of duplication; the shared helper exists but the wrappers are the duplication.
- Null-driver inconsistency: `audio_null` is all-NULL (relies on every caller NULL-checking); `input_null`/`video_null`/`menu_ctx_null` stub everything. Pick one philosophy — Tier 3 finding.

**Token budget:** 20-30k input, 5-8k output. Cheap focused lane.

---

### Lane 4 — task queue

**Source paths:** `tasks/task_queue.c`, `tasks/task_queue.h`. Pull all `tasks/task_*.c` (~30 files) for caller-side review of common antipatterns (UAF on `http_task` cleanup, OOM-NULL-deref on init, leak on calloc failure).

**Contract docs:** `task_queue.c` header comments — the task lifecycle, lock semantics, msg_push callback contract.

**External specs:** POSIX threads — `pthread_mutex` lock-ordering; signal-safety.

**Known traps:**
- `task_http`-class UAF where outer handler calls `task_get_*` on the inner http_task pointer after the queue has freed it (Bundle 7 fix: NULL the pointer in cb before flipping COMPLETE).
- `task_queue_push_progress` invokes `msg_push` callback while holding `property_lock` — re-entrancy hazard, not currently exploited but contract isn't documented.
- `task_http_iterate_transfer` busy-spins with `retro_sleep(1)` — acknowledged FIXME, switch to event-driven.
- Lock-naming inconsistency: `title`/`error`/`progress`/`flags` use `property_lock`; `task_data` uses `running_lock`.
- Per-task OOM patterns — task_screenshot, task_overlay, task_save, task_pl_thumbnail all had different leak/NULL-deref patterns on init failure (Bundles 10-11).

**Token budget:** 60-100k input, 15-25k output. Medium-heavy.

---

### Lane 5 — network commands / IPC

**Source paths:** `command.c`, `command.h`. Plus `network/cloud_sync/*` for the IPC-over-HTTP class. Plus `cheevos/cheevos_client.c` for the auth/IPC pattern.

**Contract docs:** the `command_t` table (RETRO_CMD_*); the cloud-sync server API contract (vendor-specific, see specs).

**External specs:**
- HTTP/1.1 (RFC 7231)
- TLS — `mbedtls` `MBEDTLS_SSL_VERIFY_OPTIONAL` is the indie-review CRITICAL-1 finding
- AWS SigV4 (S3 signing)
- WebDAV (RFC 4918)
- WebDAV digest auth (RFC 7616)
- OAuth 2.1 / Google device flow (RFC 8628) — Google Drive
- OWASP injection / SSRF for any URL building

**Known traps:**
- `command.c` localhost-bind hardening (Bundle 4) — network command socket binds 127.0.0.1, not 0.0.0.0.
- `command_write_ram` heap-write OOB cap at COMMAND_WRITE_RAM_MAX_BYTES (Bundle 4).
- `command_read_ram` integer overflow + NULL-check (Bundle 4).
- `cheevos/cheevos_client.c:169` overlapping `strcpy` — fixed Bundle 4 with `memmove`.
- TLS verification disabled across the entire stack — vendored libretro-common; spec drafted in `docs/private/specs/2026-04-27-tls-verification-opt-in-design.md`.
- WebDAV digest parser hardening (Bundle 13).
- S3 SigV4 contract (path encoder + canonical query string) (Bundle 17).
- Cloud sync upload cap (Bundle 32) + Content-Length verification (Bundle 26).
- Path traversal via attacker-supplied cloud-sync manifest (Bundle 12).

**Token budget:** 100-150k input, 20-35k output. Heavy lane — biggest threat surface.

---

### Lane 6 — cloud sync + cheevos

**Source paths:** `network/cloud_sync/{webdav,google_drive,s3}.c`, `tasks/task_cloudsync.c`, `cheevos/{cheevos.c,cheevos_client.c,cheevos_menu.c,cheevos_rvz.c}` (the in-tree cheevos files — the rcheevos library lives vendored at `deps/rcheevos/` and is out-of-scope per `scope.txt`). Plus the streaming-upload spec in `docs/private/specs/2026-04-27-cloud-sync-streaming-upload-design.md`.

**Contract docs:** `cloud_sync_driver.h` (the sync driver contract), the streaming-upload spec.

**External specs:** same as Lane 5 (HTTP, TLS, SigV4, WebDAV, OAuth) plus:
- RetroAchievements API contract (cheevos)
- Google Drive REST API
- AWS S3 multipart upload contract

**Known traps:**
- Every cloud-sync driver has the same 5-step shape: `*_init`, `*_read_cb`, `*_update`, `*_delete`, `*_dir_walk`. Discrepancies across the three are the bug class.
- Content-Length verification (Bundle 26) — applies to all three drivers.
- Atomic-write idiom on downloaded files — `tmp + rename` pattern; not yet applied to cloud-sync read (~3 sites open per cross-cutting #1).
- Phase 1 upload cap (Bundle 32) — closes OOM reachability; phases 2-4 (streaming SHA-256, pull-callback HTTP body, multipart audit) per the spec.
- `cheevos_client.c:169` overlapping strcpy (Bundle 4).
- WebDAV 404-body local-file write filter (Bundle 16).
- WebDAV `webdav_ensure_dir` uninitialized stack (Bundle 12).
- OOM-NULL-deref hardening on the per-driver state allocations (Bundle 45) — 17 calloc/malloc sites in google_drive.c + webdav.c (sync_begin / read / update / delete entry points + internal context-builders + the WebDAV digest-auth chain). `task_http`-layer OOM is separate.

**Token budget:** 80-120k input, 15-25k output. Heavy.

---

### Lane 7 — menu core + 4 drivers

**Source paths:**
- `menu/menu_driver.c`, `menu/menu_driver.h`, `menu/menu_setting.c` (alphabet-soup, large)
- Driver files: `menu/drivers/{rgui,materialui,xmb,ozone}.c` — each per its big-file split above
- `menu/cbs/menu_cbs_*.c` — per-action callback dispatch
- `menu/menu_displaylist.c` — display-list generator (per-driver gating)

**Contract docs:** `menu_driver.h` `menu_ctx_driver_t` vtable, the menu-driver convention (sidebar/list/thumbnail), the four-touchpoint setting contract (overlap with Lane 2).

**External specs:** none external; UI conventions are internal. (For a11y dim, GNOME HIG / WCAG 2.2 — but RetroArch has no claim of WCAG compliance; report INFO not finding.)

**Known traps:**
- ~30 unguarded `MENU_LIST_GET_SELECTION(...)->size` derefs (S2 — promoted to full sweep). **✅ Closed Bundles 34 + 36** (cbs + ozone + xmb + materialui + rgui sweep). Macro contract comment + path-sensitive proof reference live at `menu/menu_driver.h:72-78`. A regression re-introducing an unguarded site should now surface in clang-analyzer's `clang-analyzer-core.NullDereference`.
- Dead vtable slots: `set_thumbnail_content`, `update_thumbnail_path`, `list_prepend`, `navigation_increment`/`decrement`. **✅ Closed Bundle 46** — 5 slots stripped from `menu_ctx_driver_t`, 25 entries from the 5 driver vtable initialisers, 5 dispatch sites removed; net -59 lines. Static `ozone_set_thumbnail_content` / `xmb_set_thumbnail_content` retained as direct call-sites in their own files.
- Per-driver gating (`string_is_equal(menu_ident, "xmb")` style) — coupling smell across `menu_displaylist/menu_setting/menu_cbs_*`. The capability-flag refactor is Tier 3.
- `materialui` ident is `"glui"` but file/struct is `materialui_*` — three-way naming inconsistency. **✅ Closed Bundle 49** — invariant comment block at `menu/drivers/materialui.c:12196` locks the ident as `"glui"` for backward-compat; verified-no-live-mismatch (all 11 callsites already use `"glui"`). Any future `string_is_equal(menu_ident, "materialui")` check is a bug; the `"glui"` ident is the only valid string.
- Menu driver swap mid-session — settings registration gates on driver ident and never refreshes (spec drafted in `docs/private/specs/2026-04-27-menu-driver-swap-design.md`).
- Materialui specifically — 12231 lines with author-marked module boundaries; review by section, not whole-file.

**Token budget:** 200-300k input across the 5+ files. **Split into 3 sub-lanes if doing thorough mode:** (7a) menu core + cbs; (7b) rgui + materialui; (7c) xmb + ozone + displaylist.

---

### Lane 8 — save state + replay + runahead

**Source paths:** `tasks/task_save.c`, `input/bsv/bsvmovie.c`, `input/bsv/uint32s_index.c`, `runahead.c`, `disk_index_file.c`. Plus `verbosity.c` (BSV-related logging).

**Contract docs:** save-state file format (binary blob — version-prefixed), BSV replay file format (`bsv/bsvmovie.h` header comment).

**External specs:**
- POSIX `rename(2)` atomicity (atomic-save pattern)
- Trust boundary: `.state` and `.bsv` files are downloaded from the internet (CVE class — buffer overflow in malicious file parsing).

**Known traps:**
- Atomic save (tmp + rename) — partial: 4/7 sites done (Bundle 2). BSV checkpoint different shape, vendored config_file.c can't be patched locally.
- Buffer-overflow class in malicious save-state / replay parsing (Bundle 3) — bounds-check `block_size`, `key_event_count`, `input_event_count`, header-len rejection.
- `bsv_movie_load_checkpoint` malloc-fail invariant break (Bundle 10).
- `uint32s_index_pop` UAF/double-free + RHMAP_PTR auto-create gate (Bundle 21).
- Runahead temp-DLL filename is predictable LCG seeded from `time(NULL)` (Bundle 24 — CSPRNG suffix + O_EXCL/O_NOFOLLOW).
- Runahead savestate-size cached at create time (Bundle 30 — re-query each call).
- `find_change` / `find_same` rewind compressor unbounded walk (Bundle 9).
- Realloc-leak class: `runahead.mylist_resize`, `bsv.uint32s_bucket_expand` (Bundle 19).

**Token budget:** 80-120k input, 15-25k output. Medium-heavy.

---

## Cross-cutting lanes (always include in synthesis)

The 2026-04-25 sweep found three cross-cutting themes flagged by ≥2 lanes. These are **the gold signal** — the patterns that survive single-author blind spots. The synthesis phase must always check for:

1. **No atomic write on disk-save paths** (Bundle 2 partial; ~3 cloud-sync sites still open).
2. **Buffer-overflow class in malicious save-state / replay parsing** (closed Bundle 3).
3. **`task_http`-class UAF pattern at the caller layer** (closed Bundle 7).
4. **Path traversal via attacker-supplied cloud-sync manifest** (closed Bundle 12).

Adding new themes: when a finding is independently flagged by ≥2 lanes in a future run, add it to this section with the bundle commit hash that closes it (or 📋 if open).

---

## Token-budget guidance

| Mode | Lanes | Per-lane in/out | Total budget |
|---|---|---|---|
| `--quick` | 1, 3, 5 (cheapest, highest-threat) | 20-100k in, 5-25k out | ~400k |
| default (no arg) | all 8 | 30-300k in, 5-35k out | ~1.5M |
| `--thorough` | 8 + Lane 7 split into 3 sub-lanes (10 lanes) | up to 300k in, 35k out | ~2M |

For each lane, **the orchestrator should refuse to dispatch if the per-lane brief exceeds 50k tokens** — that's a signal the lane needs further splitting.

---

## When to update this file

- Subsystem boundary changes (rare — architectural).
- New big file crosses 5k LoC threshold (re-run `wc -l` periodically).
- Cross-cutting theme emerges from synthesis (≥2 lanes flag it).
- A spec doc lands that changes the contract for a lane.

When updating, also update the corresponding ROADMAP cross-references so a fresh agent reading the partition can find the narrative context.
