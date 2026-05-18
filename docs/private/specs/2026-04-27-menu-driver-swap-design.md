# Menu Driver Swap — Runtime Tree Rebuild (Design)

**Date:** 2026-04-27
**Source:** `docs/private/ROADMAP.md` — indie-review HIGH (recurrence from audit S2 cluster). (Section anchor; line numbers churn each bundle.)
**Status:** draft, awaiting user review — **REQUIRES REFRESH** before implementation (see banner below).
**Target:** `local/fixes-2026-04` once approved
**Effort estimate:** 1–2 days — but **D2 is moot** (see banner) so the lift estimate (0.5 day) is removable; revise total to ~1.5 days.

> **⚠️ Cold-eyes 2026-05-18 status update.**
>
> A cold-eyes pass against current source flagged three load-bearing inaccuracies in this spec. The spec stays as the historical design record; the corrections below MUST be applied before any implementation bundle.
>
> 1. **Gating-site line numbers (Current behaviour table, below) are stale by ~26 lines.** Spec says `menu_setting.c:18806 / :19016 / :19308 / :19552 / :19833 / :20330 / :20533 / :20804 / :20857 / :20921 / :20990 / :22801 / :22980`; current source has them at `:18832 / :19042 / :19334 / :19578 / :19859 / :20356 / :20559 / :20830 / :20863 / :20883 / :20947 / :21016 / :22827 / :23006`. Re-run the grep against HEAD before keying any code change to the table; do not edit the table here — line numbers will be stale again by the next bundle.
> 2. **API names `menu_settings_list_new` / `menu_settings_list_free` do not exist.** The real names are **`menu_setting_new`** (`menu/menu_setting.c:25955`, decl `menu/menu_setting.h:102`) and **`menu_setting_free`** (`menu/menu_setting.c:25743`, decl `menu/menu_setting.h:104`). Every architecture-box mention of `menu_settings_list_{new,free}` below should be read as `menu_setting_{new,free}`.
> 3. **Decision D2 is moot.** `menu_setting_free` already exists as a callable function and is already invoked **outside shutdown** at `menu/menu_driver.c:2283` (`menu_entries_settings_deinit`), reachable from menu deinit paths. The Phase 1 "lift" (0.5 day) is therefore unnecessary; the rebuild path calls the existing function. D2's A-vs-B framing should be deleted.
> 4. **Gating-site count.** "20+" undercounts. Current source has 25 sites (21 `string_is_equal(...)` form + 4 `memcmp(..., "glui", 5)` form). Sample table here is illustrative, not exhaustive.
> 5. **D1 precedent appeal.** "Matches audio/video driver-swap precedent" is unverified — grep across `runloop.h`/`runloop.c`/`retroarch.c` for `RECONFIGURE` returns zero hits; the actual precedent is the synchronous `video_driver_reinit` call at `gfx/video_driver.c:4827`, invoked from `command.c`. Re-evaluate D1(A) vs (B) against the actually-existing pattern, not the imagined deferred-flag pattern.
> 6. **`glui` vs `materialui` aliasing** — spec uses `glui` everywhere; that is the legacy ident the menu config stores (set in `menu/drivers/materialui.c:12232` and ROADMAP closes the aliasing question in S11 follow-up). One sentence at first mention would prevent reader confusion.
> 7. **Failure-fallback ordering (D3).** Architecture box deinit-frees-old before init-new; if init-new fails, the "fall back to old driver" path requires re-running `menu_driver_init(old)`. Tighten the order: build-new-first (with a flipped `menu_driver` copy), tear-down-old only on full success.
> 8. **Missing edge cases.** Swap during context-menu popup; swap during a task overlay; swap with content loaded vs unloaded. None covered.
>
> These corrections are tracked in `docs/private/ROADMAP.md` under the cold-eyes-2026-05-18 fold-in block; resolve before the implementation bundle opens.

---

## Summary

The menu setting tree is built once at startup, gated per-driver via `string_is_equal(settings->arrays.menu_driver, "xmb")`-style branches in `menu/menu_setting.c`. There are **20+ such gating sites** (xmb / ozone / glui / rgui / materialui — sites listed below). When the user changes menu driver mid-session via *Settings → User Interface → Menu Driver*, the new driver loads but the setting tree is not rebuilt — XMB-only entries vanish if you swap to ozone, ozone-only entries vanish if you swap to RGUI, and so on. The user's only fix is to restart RetroArch.

This spec defines the rebuild trigger, the rebuild mechanism, the state preserved across the swap, and the failure-fallback policy.

## Current behaviour

**Setting tree gating sites** (sample — 20+ total, full list from grep):

| File:line | Driver gate |
|-----------|-------------|
| `menu/menu_setting.c:18806` | `xmb` |
| `menu/menu_setting.c:19016` | `rgui` |
| `menu/menu_setting.c:19308` | `xmb` |
| `menu/menu_setting.c:19552` | `xmb` |
| `menu/menu_setting.c:19833` | `ozone` |
| `menu/menu_setting.c:20330` | `glui` |
| `menu/menu_setting.c:20533` | `ozone` |
| `menu/menu_setting.c:20804` | `rgui` |
| `menu/menu_setting.c:20857–20875` | xmb / ozone / rgui / glui (4-way disjunction with per-driver inner gating) |
| `menu/menu_setting.c:20921` | `xmb` |
| `menu/menu_setting.c:20990` | `rgui` |
| `menu/menu_setting.c:22801–22802` | xmb / ozone |
| `menu/menu_setting.c:22980–22981` | ozone / xmb |

Each branch reads `settings->arrays.menu_driver` at the moment the setting tree is being built (during `menu_settings_list_new`). This happens once at startup. The result is a tree shaped to the *boot-time* driver; the post-swap driver inherits a stale tree.

**The swap path itself works.** `Settings → User Interface → Menu Driver` writes the new value to `settings->arrays.menu_driver`, the menu deinitialises the old driver, the new driver initialises, and the user lands in the new driver's UI. The tree is just stale.

**Three concrete examples from the gating sites:**

1. *XMB ribbon shader* (`menu_setting.c:18806`) — only registered if boot driver was XMB. Swap from RGUI to XMB at runtime: the ribbon-shader setting is missing.
2. *RGUI symbol-set* (`menu_setting.c:19016`) — only registered if boot driver was RGUI. Swap from XMB to RGUI at runtime: the symbol-set setting is missing.
3. *Ozone collapse-sidebar* (`menu_setting.c:19833`) — only registered if boot driver was Ozone. Swap to Ozone at runtime: the collapse-sidebar setting is missing.

## Goals (v1)

1. Setting tree refreshes on driver swap. Post-swap, every driver-specific entry visible to the new driver appears; every entry visible only to the old driver disappears.
2. Swap is **interactive** — user clicks the menu-driver dropdown, picks a new value, the menu redraws on the new driver with the rebuilt tree. No restart required.
3. **State preservation:** the user's **menu position** (top-of-tree after rebuild is acceptable for v1; deep-stack restore is v2). The user's **scroll position within Settings → User Interface** does NOT need to survive the swap; landing on the menu-driver dropdown post-swap is acceptable.
4. **Failure mode:** if the rebuild fails (allocation failure, driver-init returned an error after partial setup), fall back to the *previous* driver and the *previously-built* tree, with an `RARCH_ERR` and a `runloop_msg_queue_push` error message. Do not abort to a default driver mid-session — that loses the user's other unsaved settings work.

## Non-goals (v1)

- **Capability flags on `menu_ctx_driver_t`.** The audit S11 follow-up suggests replacing every `string_is_equal(menu_ident, "xmb")` with a capability flag (`flags & MENU_DRV_HAS_RIBBON_SHADER`). That is a **larger refactor** and a separate spec; v1 just rebuilds the tree, leaving the gating sites in place.
- **Deep-stack position restore.** If the user is six menus deep in *Drivers → Audio → Resampler → Configure → Sinc → Quality* when they swap, v1 lands them at the menu root (or at the nearest valid ancestor). Restoring the deep stack is fragile because the stack entries reference the old driver's setting pointers; v2 work.
- **Animated transition.** v1 may flash blank or show the freeze-frame from the previous driver during the rebuild. Animated cross-fade is v2.
- **Driver swap from a CLI / API.** Always interactive via the menu UI.
- **Persisting the user's pre-swap menu position across a process restart.** Out of scope.

## Architecture

```
 User clicks Menu Driver dropdown    (current: xmb → ozone)
   │
   ├─ menu_setting handler writes settings->arrays.menu_driver = "ozone"
   │  (no other state change; flagged "needs rebuild")
   │
   ├─ runloop iteration sees the rebuild flag
   │     │
   │     ├─ menu_driver_deinit(old)                  ← unchanged
   │     │
   │     ├─ menu_settings_list_free(old_settings)    ← NEW: free the old tree
   │     │
   │     ├─ menu_driver_init(new)                    ← unchanged
   │     │
   │     ├─ menu_settings_list_new(new_settings)     ← NEW: rebuild tree
   │     │     (driver-gated branches now read settings->arrays.menu_driver
   │     │      = "ozone" and register ozone-specific entries)
   │     │
   │     └─ menu_st->selection_ptr = 0
   │        menu_st->menu_stack    = root
   │
   └─ on rebuild failure: fall back to old driver + old tree (saved before deinit)
```

## Decision points

### D1 — When does the rebuild fire?

- **(A) Synchronously in the dropdown's `setting_change` handler.** Pros: deterministic, the user clicks and immediately sees the new tree. Cons: the handler runs inside the menu draw loop; tearing the menu down from inside a menu draw call is fragile (the draw call expects `menu_st->driver_ctx` to remain valid for the rest of the frame).
- **(B) Defer to the next runloop iteration via a `RUNLOOP_FLAG_MENU_DRIVER_RECONFIGURE`.** Pros: clean — the menu finishes drawing, the runloop sees the flag at the top of the next iteration, performs the swap before the next draw. Matches the existing pattern for swap-pending operations (audio-driver swap, video-driver swap). Cons: one frame of "stale tree drawn on new driver" between the click and the rebuild — but we can also avoid this by suppressing the menu draw for that one frame.

**Recommendation:** (B). Matches existing audio/video driver-swap precedent at `runloop.c:retroarch_main_iterate`. The "one frame of suppressed draw" is below visual-perception threshold and far simpler than a synchronous in-handler swap.

### D2 — Who frees the old setting tree?

The setting tree is currently allocated inside `menu_settings_list_new` and held by the menu state. There is no existing `menu_settings_list_free` because the tree was assumed to live for the duration of the process.

- **(A) Add `menu_settings_list_free(rarch_setting_t *list)`.** Walks the list, frees each setting's owned strings (name, short_description), frees the array. Mirror of `menu_settings_list_new`'s allocation discipline.
- **(B) Re-use the existing `setting_free` shutdown path.** The codebase already has shutdown-time tree teardown when RetroArch exits. Lift that into a callable function.

**Recommendation:** (B). The shutdown path already handles every setting type correctly; lifting it into a named function avoids duplicating teardown logic. Required side-work: confirm the shutdown path doesn't depend on global state being torn down (it shouldn't — it just walks the array).

### D3 — Failure-fallback policy

If `menu_driver_init(new)` fails or `menu_settings_list_new` returns NULL after the new driver has loaded:

- **(A) Fall back to old driver + old tree.** Requires saving a snapshot before the deinit. Old tree must remain valid through the failed init.
- **(B) Fall back to default driver (`rgui`, always present).** Saves the snapshot work but loses the user's prior driver state.
- **(C) Abort the runloop with a fatal error.** Hostile to the user — they lose all unsaved work in other settings.

**Recommendation:** (A). Save a `settings_t *prev_menu_settings_list` and `char prev_menu_driver[NAME_MAX_LENGTH]` before tearing down the old. On failure, restore both. The snapshot is small (one pointer + a string).

### D4 — User-visible feedback

- **(A) Show a transient message:** `runloop_msg_queue_push("[Menu] Reloading driver...", ...)` for 1 second during the swap.
- **(B) Silent.** The user clicked the dropdown; the new tree is just there.

**Recommendation:** (B), with one exception: if the rebuild **fails**, the failure-fallback path pushes an error message: `"[Menu] Driver swap failed; reverted to previous driver."` Success is silent.

## Data model changes

### New / added

- `menu_settings_list_free(rarch_setting_t *list)` in `menu/menu_setting.c`. Lifts the existing shutdown teardown into a callable function. Public to runloop.
- `RUNLOOP_FLAG_MENU_DRIVER_RECONFIGURE = (1u << <next free bit>)` in `runloop.h`. Note: this lands AFTER the S1 sweep that converts existing flags to `1u <<`; until S1 lands, this should also use the existing convention (`1 <<`) and migrate together. **Coordinate with the S1 implementation bundle.**
- `runloop_state_t::pending_menu_driver_reconfigure` — small struct holding `prev_menu_driver[NAME_MAX_LENGTH]` and `prev_settings_list` for the fallback path. Cleared after a successful swap.

### Changed

- The menu-driver dropdown's `change_handler` (currently in `menu/menu_setting.c`'s menu-driver setting registration site) sets the runloop flag, copies the current driver name into the snapshot, and returns. It does NOT call deinit/init directly.

### Unchanged

- Every per-driver gating site in `menu_setting.c`. The 20+ `string_is_equal(menu_driver, "xmb")` branches stay; the rebuild simply re-runs them with the new value. The capability-flag refactor is non-goal v1.
- `menu_driver_init` / `menu_driver_deinit` themselves. The bug isn't in the driver lifecycle — it's that the setting tree was never re-run.

## Test plan

### Manual conformance tests

1. **Boot into XMB. Settings → User Interface → Menu Driver → Ozone.** Expect: menu redraws as Ozone within 1 frame of the click. Settings → User Interface → Menu Driver → click into the *Ozone Theme* row. Expect: row exists, contains *Basic Black* / *Basic White* / *Nord* etc.
2. **Reverse:** Boot into Ozone, swap to XMB. Settings → User Interface → Menu → click into *XMB Menu Color Theme*. Expect: row exists.
3. **Round-trip:** Boot into XMB, swap to RGUI, swap back to XMB. Expect: every XMB-only setting from the boot tree is present after the round-trip.
4. **Failure injection:** patch `menu_driver_init` to return false on first call after the swap flag is set. Trigger swap. Expect: error message in the message queue, menu remains in the original driver, settings remain navigable.

### Build verification

`make -j$(nproc) retroarch` clean. No new warnings.

### Regression

- *Existing* startup path (no swap) must produce a tree byte-identical to pre-patch. Verifiable by snapshotting `setting_list` count + first 100 setting names before/after, on the same config.
- Audio-driver swap (existing feature) must still work — the new flag must not collide with `RUNLOOP_FLAG_AUDIO_DRIVER_RECONFIGURE` if such a thing exists. Grep first.

## Risks

- **The 20+ gating sites may not be the complete set.** A subsequent grep pass should also check `menu/menu_displaylist.c`, `menu/cbs/`, and the menu-driver-specific `*.c` files (xmb.c, ozone.c, materialui.c, rgui.c) for `string_is_equal(menu_ident, ...)` reads on per-frame paths. Anything that reads `menu_ident` per-frame and caches a result also needs invalidation on swap. **Add this as a step in the implementation plan.**
- **Setting pointers are held by the menu stack.** If the user's menu-stack entries reference setting pointers from the old tree, those become dangling after `menu_settings_list_free`. v1 resets the stack to root on swap (see Goals §3) — confirms this risk is mitigated. If a future v2 wants stack preservation, the entries must store *labels* (stable across rebuilds) and re-resolve to setting pointers post-rebuild.
- **`menu_settings_list_free` may not exist in lift-able form.** If the shutdown-time teardown is interleaved with other deinit (audio, video, gfx_ctx), lifting it cleanly may require splitting the function. **Estimate +0.5 day if the lift turns out to be ugly.**
- **Translations.** Per-driver entries have driver-specific labels (e.g. `MENU_ENUM_LABEL_VALUE_XMB_RIBBON_SHADER`). Rebuilds re-register these. The translation lookup is already keyed by the enum value, not the driver, so swap is safe — but verify the first time.

## Implementation phases

1. **Phase 1 (0.5 day):** lift `menu_settings_list_free` from the shutdown path. Verify shutdown still works. Commit.
2. **Phase 2 (0.5 day):** add the runloop flag + snapshot fields + the dropdown handler that arms the flag. Wire the runloop-iteration site that performs deinit / free / init / new / fallback. Commit.
3. **Phase 3 (0.5 day):** manual conformance tests (1–4 above). Add an `RARCH_DBG` log in the swap path so the test output proves the rebuild actually fired.
4. **Phase 4 (0.5 day):** secondary grep pass — find any `menu_ident` cache outside `menu_setting.c` and add invalidation. Commit. ROADMAP fold-in.

Total: ~2 days. The first three phases form one logical bundle; phase 4 is the secondary-grep cleanup which may extend.

## External references

None — this is RetroArch-internal behaviour. The gating-site list is empirically derived from `grep`.

## Spec status

Draft — awaiting:
1. User confirmation of the **state-preservation policy** (Goals §3): tree-root after swap vs. nearest-valid-ancestor restore. Default is tree-root for v1.
2. User confirmation that the failure-fallback **reverts to old driver** rather than aborting (Decision D3 → A).
3. User confirmation that v1 leaves the gating-site refactor (capability flags) for a v2.

Once those three are confirmed, the spec is ready for implementation under `local/fixes-2026-04` as one or two bundles.
