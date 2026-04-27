# Audit suppressions catalog

Known false-positive classes from cppcheck / clang-tidy / semgrep on this fork. Each entry names the rule, the trigger pattern, the verification basis, and a note about how to consume the suppression (inline `// cppcheck-suppress` markers vs project-wide rule disable vs hand-triage).

The point of this file is to **stop re-triaging the same known-FPs every audit cycle**. The 2026-04-25 run produced 660 raw findings; ~96% noise rate. This catalog captures the recurring classes so the triage subagent can pre-drop them.

When a suppression entry says "verified resolved-stale," the underlying issue does not exist in the current code at all — the static analyzer's report drifted from a long-fixed line. Those are listed for completeness so a future audit doesn't re-flag them as new findings.

---

## Active suppressions (deferred / FP / out-of-scope)

### location_drivers[] / translation_drivers[] OOB (clang-analyzer)

**Rule:** `clang-analyzer-core.NullDereference` / `core.NonNullParamChecker` reporting Out-of-bound past `location_drivers[]`.

**Sites:** `retroarch.c:435, 1186, 2270`; `tasks/task_translation.c:177`.

**Why FP:** The driver-table arrays are NULL-terminated at definition (`{ ..., &location_null, NULL }`). The iteration `for (d = 0; location_drivers[d]; d++)` is correctly NULL-guarded; `find_driver_nonempty` only returns indices 0..N-1. The path-sensitive analyzer doesn't track the NULL-terminator semantics across the helper boundary.

**Action:** **Suppress at the rule level** — add to clang-tidy's per-tool `--header-filter` or `.clang-tidy` (when introduced) `Checks: -clang-analyzer-core.NullDereference (driver_drivers* arrays)`. Do not inline-suppress; the FP is the analyzer's, not the code's.

**Status:** 🔄 Deferred. Will revisit if a concrete reproducer surfaces.

---

### runahead.c:640, 662 — input-state-list walk OOB (clang-analyzer)

**Rule:** `clang-analyzer-core.uninitialized.ArraySubscript` "Out-of-bound access preceding heap area."

**Sites:** `runahead.c:640, 662` (input-state-list walk, two sites).

**Why FP-class:** The loop iterates `i = 0` upward against `input_state_list->size` and the deref `data[i]` is bounded. Path-sensitive analyzer may be tracking platform-conditional branches that can't be reached together. Possible TP if there's a real interaction between platform `#ifdef` blocks and the list-resize path that nobody has reproduced yet.

**Action:** Hand-flag for next reproducer-driven pass. Do **not** rule-suppress — there's a real chance this one IS a bug.

**Status:** 🔄 Deferred pending reproducer.

---

### gfx/gfx_thumbnail.c — stack-array FPs (cppcheck)

**Rule:** Various `cppcheck` deref-before-NULL-check / OOB warnings.

**Why FP:** `thumbnail_path` is a stack array (`char thumbnail_path[PATH_MAX_LENGTH]`); cppcheck's deref-before-check pattern doesn't apply. Reported by Bundle 5 triage and ruled won't-fix.

**Action:** Inline suppress at the offending line(s) with `// cppcheck-suppress nullPointerArithmeticRedundantCheck` once cppcheck's `.cppcheck-suppress.txt` file is introduced (S12).

**Status:** ❌ Won't fix. Inline-suppress when the suppress file lands.

---

### Larger-file deref-before-check FPs (cppcheck)

**Rule:** `cppcheck` deref-before-NULL-check on stack-array / locally-checked allocs.

**Files:** `ozone.c`, `materialui.c`, `xmb.c`, `netplay_frontend.c`, `menu_cbs_ok.c`.

**Why mixed:** These files have a **mix** of real bugs and FPs (stack-array reads where cppcheck sees `if (!buf)` against a non-pointer, or pointer derefs where the locally-checked alloc precedes the deref by 30 lines and cppcheck's flow analysis loses track).

**Action:** Per-site triage by the indie-review big-file lane (see `indie-review-partition.md`). Don't blanket-suppress — there are real bugs in these files (Bundle 5 deferred 11/30 sites here).

**Status:** 📋 Deferred to the next bundle-5 follow-up.

---

### S9 — semgrep `double-free` cluster (29 sites)

**Rule:** semgrep `c.lang.security.double-free.double-free`.

**Sites:** `network/cloud_sync/s3.c`, `tasks/task_database_cue.c`, `gfx/drivers/d3d9cg.c`, `gfx/drivers_font/bitmapfont_*.c`, `tasks/task_save.c`, plus stb / video_shader_parse.

**Pattern:** success-then-return / fail-label-cleanup style — the rule fires because both branches reach `free(p)` if the analyzer doesn't track that the success branch returns before the cleanup label can be entered. Project-wide pattern; not a per-site bug.

**Action:** Project-policy permanent suppression. `aggregate.py` (KNOWN_FP_RULES, the semgrep entry) drops these before triage. Canonical pattern documented in `docs/private/AUDIT-POLICY.md`.

**Status:** ✅ Implemented — Bundle 34 close. Permanent suppression class.

---

### S12 — JNI-callback missingReturn FPs (cppcheck)

**Rule:** `cppcheck` `missingReturn`.

**Sites:** `play_feature_delivery/play_feature_delivery.c:117, 187`.

**Why FP:** Both functions are declared `JNIEXPORT void JNICALL Java_...` — genuinely void return. cppcheck 2.20 mis-classifies the JNICALL macro and emits `missingReturn` at the function-end brace. There is no return statement to add.

**Action:** Anchored at `.cppcheck-suppress.txt` in repo root with the two missingReturn entries; wired into the audit-config cppcheck flags as `--suppressions-list=.cppcheck-suppress.txt`. Future cppcheck FPs that need a project-wide policy decision land in the same file.

**Status:** ✅ Implemented — Bundle 34 close (commit `b27a516cf5`).

---

### S11 — identicalInnerCondition (cppcheck) — closed via strip

**Rule:** `cppcheck` `identicalInnerCondition`.

**Sites (resolved):** `audio/audio_driver.c:447`, `audio/drivers/dsound.c:452`, `audio/drivers/openal.c:129`, `cheat_manager.c:1820+1873`, `menu/menu_driver.c:4647`, `camera/camera_driver.c:155` — all stripped (option B: copy-paste residue) in Bundle 34. `tasks/task_content.c:609` is a confirmed FP (the trailing-slash strip can null `*dir` if `dir == "/"`); marked with inline `cppcheck-suppress identicalInnerCondition`.

**Why originally pending:** Each was a defensive double-check. Driver tables are `const`-qualified static arrays so the inner predicate could never have changed value mid-iteration; comprehension cost (every reader re-checking the same predicate twice) was paying for nothing.

**Action:** Done. The pre-triage drop rule for this class can be removed from `aggregate.py` — a regression re-introducing the pattern should now be flagged, not auto-dropped.

**Status:** ✅ Implemented — Bundle 34 close (commit `9ddc80cb84`).

---

### Vendored libretro-common items (out of scope)

**Files:** anything under `libretro-common/` (and similarly `deps/`).

**Why:** Vendored upstream tree. Patches must go upstream first and re-vendor. Audit-side, these are out-of-scope per `scope.txt`.

**Action:** Pre-triage rule: drop all findings whose file path begins with `libretro-common/` or `deps/`.

**Notes for the upstream-track:**
- `libretro-common/file/archive_file.c:555-557` — `backend->compressed_file_read` derefs NULL backend. Patch upstream.
- `libretro-common/formats/json/rjson.c:971-975` — `(json + 1)` arithmetic before NULL check. Patch upstream.
- `libretro-common/file/config_file.c:1410-1432` — every cfg save lacks atomic-write idiom. Patch upstream.

---

### core_updater_list.c:928 push_entry ownership-move (clang-tidy)

**Rule:** `clang-analyzer-cplusplus.NewDeleteLeaks` / leak-on-error-path.

**Site:** `core_updater_list.c:928` — `entry.local_info_path`.

**Why FP:** Walked the full ownership chain (`set_paths`/`set_core_info` allocate fields, `push_entry` transfers all six pointers on success, error label frees on failure). No leak path exists. Path-sensitive analyzer loses the field-by-field move in `push_entry`.

**Action:** Inline-suppress when the cppcheck-suppress file is introduced. Do not refactor `push_entry` solely to satisfy the analyzer — the function is correct.

**Status:** 🔄 Deferred (Bundle 28 triage).

---

## Verified resolved-stale (do not re-flag)

These are 2026-04-25 cross-checks where a prior audit / indie-review report named a site that **does not exist in the current code**. Static-analysis line numbers drift across releases; this list captures the verifications so they don't get re-flagged as new findings the next time someone reads the older report.

### `s3.c:1633-1640` heap-overflow (indie-review C2)

**Verified 2026-04-25, Bundle 16 triage.** Current code:
- `s3_log_http_failure` (s3.c:788-799) uses safe `%.*s` length-bounded printf, with explicit comment documenting why `data->data[data->len]=0` is a one-byte heap overflow.
- The multipart-initiate path malloc's `data->len + 1`, memcpy's `data->len` bytes, and writes the terminator on the **newly-allocated** buffer (`response_xml[data->len] = '\0'`), not on `data->data`.

**Action:** None — already correct. A `net_http_data_to_cstring` helper would be defense-in-depth but no concrete site to fix.

### `runloop.c:5701` `current_video->focus` NULL-deref

**Verified 2026-04-25.** Current code at the corresponding site uses the ternary `video_st->current_video ? video_st->current_video->alive(video_st->data) : true` — correctly check-before-deref. clang-tidy's reported line drifted; no equivalent unguarded `current_video->focus` deref found by grep.

### `input/input_driver.c:4501` `bind->key` NULL-deref

**Verified 2026-04-25.** Current `input_config_get_bind_string` derefs `bind->key` only inside `if (bind)` at the outer guard; prior `bind->joykey` / `bind->joyaxis` derefs all use `bind && ...` short-circuit. No unguarded site reachable.

---

## Pre-triage drop rules (consumed by `aggregate.py --drop-known`)

These are mechanical pre-triage rules applied **before** the audit-triage subagent sees the raw findings, to cut noise without losing signal. Each rule names the tool + rule-id and the suppression class above that justifies it.

| Tool | Rule pattern | Justification | Action |
|---|---|---|---|
| any | path starts with `libretro-common/` or `deps/` | vendored, out-of-scope | drop |
| any | path starts with `ctr/`, `vita/`, `wii/`, `wiiu/`, `dingux/`, `uwp/`, `webos/`, `emscripten/` | console-only, out-of-Linux-scope | drop |
| cppcheck | `missingReturn` in `play_feature_delivery/` | JNI calling convention (S12) — also in `.cppcheck-suppress.txt` | drop |
| cppcheck | `nullPointerArithmeticRedundantCheck` in `gfx/gfx_thumbnail.c` | stack-array FP | drop |
| clang-analyzer | `core.uninitialized.ArraySubscript` in `retroarch.c` (lines 435, 1186, 2270), `tasks/task_translation.c:177` | NULL-terminated driver-table FP | drop |
| clang-analyzer | NewDeleteLeaks at `core_updater_list.c:928` | ownership-move analyzer-blind FP | drop |
| semgrep | `c.lang.security.double-free.double-free` matching success-then-return / fail-label-cleanup pattern | S9 canonical free-on-fail-label class — see AUDIT-POLICY.md | drop (permanent) |
| any | finding's path matches the scope.txt prune list | out-of-scope | drop |

(S11 `identicalInnerCondition` row retired — pattern stripped in Bundle 34. A regression re-introducing it should now surface in triage.)

After applying these rules, residual findings go to the triage subagent. Expected reduction: 660 raw → ~30-50 candidates → ~5-15 actionable post-triage.

---

## Adding a new entry

When a new finding-class is verified as FP / won't-fix / awaiting-spec:

1. Add a `### <rule> — <class>` section above with the rule, sites, why-FP, action, status.
2. If pre-triage should mechanically drop the class, add a row to the drop-rules table.
3. Reference any related spec doc (`docs/private/specs/...`) and ROADMAP commit hash.
4. Cross-link from the corresponding ROADMAP entry: `(see suppressions.md § <anchor>)`.
