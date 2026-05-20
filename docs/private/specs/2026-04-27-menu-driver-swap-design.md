# Menu Driver Swap — Runtime Tree Rebuild (Design)

**Date:** 2026-04-27
**Source:** `docs/private/ROADMAP.md` — indie-review HIGH (recurrence from audit S2 cluster). (Section anchor; line numbers churn each bundle.)
**Status:** draft, awaiting user review. Cold-eyes 2026-05-18 corrections folded in via Bundle 74 (see banner below; the load-bearing changes are the `menu_setting_{new,free}` API correction, the D2 deletion, the D1 re-evaluation against the real `CMD_EVENT_REINIT` precedent, the build-new-first fallback ordering, and the gating-table re-anchor to a grep recipe).
**Target:** `local/fixes-2026-04` once approved
**Effort estimate:** ~1.5 days. D2's "lift" phase is removed (the free function already exists — see banner item 3), so the original Phase 1 drops out.

> **⚠️ Cold-eyes 2026-05-18 status update (Bundle 74 fold-in).**
>
> A cold-eyes pass against current source flagged eight load-bearing issues; all eight are now folded into the spec body below. The spec stays as the historical design record; read the corrected body, not the pre-correction prose this banner summarises.
>
> 1. ✅ **Gating-site table de-pinned.** The old table hard-coded `menu_setting.c` line numbers that drift ~26 lines per bundle. The `## Current behaviour` table is now a **grep recipe** keyed on `string_is_equal(...menu_driver..., "<drv>")` / `memcmp(..., "glui", 5)`, with three illustrative examples by setting label rather than line. Re-run the grep against HEAD when implementing.
> 2. ✅ **API names corrected.** `menu_settings_list_new` / `menu_settings_list_free` do **not** exist. The real functions are `menu_setting_new(void)` (def `menu/menu_setting.c:25955`, decl `menu/menu_setting.h:102`) and `menu_setting_free(rarch_setting_t *setting)` (def `:25743`, decl `:104`). All architecture-box / data-model mentions now use the real names + signatures.
> 3. ✅ **D2 deleted (moot).** `menu_setting_free` already exists and is already invoked **outside shutdown** at `menu/menu_driver.c:2283` (`menu_entries_settings_deinit`, also reached from `:3665` and `:6770`). The rebuild path just calls it; the Phase-1 "lift" (0.5 day) is removed and D2's A-vs-B framing is gone.
> 4. ✅ **Gating-site count fixed.** "20+" undercounted. Current source has **25** sites — 21 `string_is_equal(...)` + 4 `memcmp(..., "glui", 5)`. The `## Current behaviour` section states this explicitly.
> 5. ✅ **D1 precedent corrected.** No `RUNLOOP_FLAG_*_RECONFIGURE` exists (zero `RECONFIGURE` hits in `runloop.h`/`runloop.c`/`retroarch.c`). The real precedent is the synchronous `video_driver_reinit` (`gfx/video_driver.c:4827`) inside `command_event_reinit` (`command.c:2484`), dispatched via `command_event(CMD_EVENT_REINIT, …)`. D1 is re-framed around that `CMD_EVENT` pattern.
> 6. ✅ **`glui` aliasing noted.** A one-line note at first mention records that `glui` is the legacy ident string for the `materialui` driver (registered at `menu/drivers/materialui.c:12232`).
> 7. ✅ **Failure-fallback ordering tightened.** The architecture box + D3 now **build-new-first** (init new driver + build new tree against a scratch copy) and tear down the old only on full success — no recursive `menu_driver_init(old)` re-entry on failure.
> 8. ✅ **Edge cases added.** New `## Edge cases` section covers swap during a context-menu popup, swap during a task-progress overlay, and swap with content loaded vs unloaded.
>
> These corrections were tracked in `docs/private/ROADMAP.md` under the cold-eyes-2026-05-18 fold-in block; resolved in Bundle 74.

---

## Summary

The menu setting tree is built once at startup, gated per-driver via `string_is_equal(settings->arrays.menu_driver, "xmb")`-style branches in `menu/menu_setting.c`. There are **25 such gating sites** (xmb / ozone / glui / rgui — the ident strings the config stores; see the grep recipe below). When the user changes menu driver mid-session via *Settings → User Interface → Menu Driver*, the new driver loads but the setting tree is not rebuilt — XMB-only entries vanish if you swap to ozone, ozone-only entries vanish if you swap to RGUI, and so on. The user's only fix is to restart RetroArch.

> **Naming note:** `glui` is the **legacy ident string for the `materialui` driver** — the config stores `glui`, registered at `menu/drivers/materialui.c:12232`. This spec uses `glui` throughout to match the gating-site source; read it as "materialui."

This spec defines the rebuild trigger, the rebuild mechanism, the state preserved across the swap, and the failure-fallback policy.

## Current behaviour

**Setting tree gating sites** — **25 total** in `menu/menu_setting.c`: 21 in the `string_is_equal(...menu_driver..., "<drv>")` form + 4 in the `memcmp(..., "glui", 5)` form. The list churns ~26 lines per audit cycle, so this spec does **not** pin line numbers; re-derive against HEAD with:

```sh
# 21 string_is_equal gates (xmb / ozone / glui / rgui)
grep -nE 'string_is_equal\([^,]*menu_driver[^,]*, "(xmb|ozone|glui|rgui)"' menu/menu_setting.c
# 4 memcmp glui gates
grep -nE 'memcmp\([^,]*, "glui"' menu/menu_setting.c
```

Some sites are multi-way disjunctions (e.g. an `xmb || ozone || rgui || glui` branch with per-driver inner gating); the two greps above are the authoritative count, not the example labels below.

Each branch reads `settings->arrays.menu_driver` at the moment the setting tree is being built (inside `menu_setting_new`). This happens once at startup. The result is a tree shaped to the *boot-time* driver; the post-swap driver inherits a stale tree.

**The swap path itself works.** `Settings → User Interface → Menu Driver` writes the new value to `settings->arrays.menu_driver`, the menu deinitialises the old driver, the new driver initialises, and the user lands in the new driver's UI. The tree is just stale.

**Three concrete examples from the gating sites** (by setting label — grep the label to find the current line):

1. *XMB ribbon shader* (`xmb` gate) — only registered if boot driver was XMB. Swap from RGUI to XMB at runtime: the ribbon-shader setting is missing.
2. *RGUI symbol-set* (`rgui` gate) — only registered if boot driver was RGUI. Swap from XMB to RGUI at runtime: the symbol-set setting is missing.
3. *Ozone collapse-sidebar* (`ozone` gate) — only registered if boot driver was Ozone. Swap to Ozone at runtime: the collapse-sidebar setting is missing.

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

**Build-new-first ordering.** The cheap, side-effect-free part (building the new tree) happens *before* anything is torn down, so the most likely failure (tree allocation) needs no fallback at all. Only the irreducible single-slot driver swap (deinit-old / init-new) carries a fallback, and that fallback is **bounded** — try prev once, then `rgui` (always present), then give up — never recursive.

```
 User clicks Menu Driver dropdown    (current: xmb → ozone)
   │
   ├─ menu_setting change_handler:
   │     settings->arrays.menu_driver = "ozone"
   │     snapshot prev_menu_driver = "xmb"; keep old list pointer
   │     command_event(CMD_EVENT_MENU_REINIT, NULL)
   │
   └─ command_event_menu_reinit   (synchronous; safe point — not mid-draw)
         │
         ├─ 1. new_list = menu_setting_new()           ← build NEW tree FIRST
         │        gated branches read menu_driver = "ozone"
         │        if NULL → restore menu_driver="xmb"; KEEP old driver+tree;
         │                  RARCH_ERR + msg push; return   (nothing torn down)
         │
         ├─ 2. menu_driver_deinit(old)                  ← only after new tree built
         │
         ├─ 3. if (!menu_driver_init(new)):             ← single-slot driver swap
         │        menu_driver_init("xmb")               (prev — once)
         │        if that also fails → menu_driver_init("rgui")  (terminating)
         │        menu_setting_free(new_list)           (discard new tree)
         │        RARCH_ERR + msg push; return          (old tree still installed)
         │
         ├─ 4. menu_setting_free(old_list)              ← free OLD tree on success
         │     menu_st->entries.list_settings = new_list
         │
         └─ 5. menu_st->selection_ptr = 0
               menu_st->menu_stack    = root
```

`menu_setting_new()` reads only the `settings->arrays.menu_driver` *string*, not the live driver context — so building the new tree in step 1 while the old driver is still active is safe.

## Edge cases

The dim-10 lane brief flagged these; v1 must define behaviour for each (none are blockers, but each needs a deliberate choice):

- **Swap during a context-menu popup.** The menu-driver dropdown is itself reached via the normal menu stack, but a swap can be armed while a context-menu (right-click / quick-menu) overlay is open. Because the swap dispatches `CMD_EVENT_MENU_REINIT` and step 5 resets `menu_stack` to root, any open context-menu/overlay state must be closed as part of the reset — clear the popup/overlay flags before resetting the stack so the new driver doesn't inherit a dangling popup pointer into the freed old tree.
- **Swap during a task-progress overlay.** Background tasks (downloads, scans) render a progress overlay over the menu. The overlay is owned by the task system, not the menu driver, so it survives the driver swap — but it caches no setting pointers, so no invalidation is needed. Confirm the overlay re-attaches to the new driver's compositor on the first post-swap frame; if it draws against the old driver's surface, suppress it for the one swap frame (same one-frame suppression as D1).
- **Swap with content loaded vs unloaded.** With content loaded, the menu is an overlay on a paused/running core; with no content it is the standalone menu. The swap path is identical (it touches only the menu driver + tree, not the core), but with content loaded the post-swap `menu_stack = root` lands on the Quick Menu root, not the main menu root — verify the reset picks the correct root for the content-loaded case (the existing `menu_driver_init` already distinguishes these; the reset must use the same selector, not hard-code the main-menu root).

## Decision points

### D1 — When does the rebuild fire?

> **Precedent correction.** An earlier draft claimed a `RUNLOOP_FLAG_*_DRIVER_RECONFIGURE` deferred-flag precedent for audio/video swaps. That flag does **not** exist (zero `RECONFIGURE` hits in `runloop.h`/`runloop.c`/`retroarch.c`). The real precedent for mid-session driver reinit is the **`CMD_EVENT`** path: `command_event(CMD_EVENT_REINIT, …)` → `command_event_reinit` (`command.c:2484`) → synchronous `video_driver_reinit` (`gfx/video_driver.c:4827`). `CMD_EVENT`s run synchronously inside `command_event()`, but `command_event()` is only called from safe points (menu-action handlers, the runloop top) — never from inside a driver's draw call. The options below are re-framed against that real pattern.

- **(A) Synchronously in the dropdown's `setting_change` handler.** Pros: deterministic, the user clicks and immediately sees the new tree. Cons: the handler can run inside the menu draw loop; tearing the menu down from inside a menu draw call is fragile (the draw call expects `menu_st->driver_ctx` to remain valid for the rest of the frame).
- **(B) Dispatch a new `CMD_EVENT_MENU_REINIT` from the dropdown handler.** The handler writes `settings->arrays.menu_driver`, snapshots the old driver name, and calls `command_event(CMD_EVENT_MENU_REINIT, NULL)`. The `command_event_*` handler performs the build-new-first swap (see Architecture + D3). This mirrors `CMD_EVENT_REINIT`/`command_event_reinit` exactly — the established way RetroArch reinitialises a driver mid-session. The swap runs at command-dispatch time, which is already a safe point outside the draw call.

**Recommendation:** (B), via `CMD_EVENT_MENU_REINIT`. It reuses the codebase's actual driver-reinit idiom (`CMD_EVENT_REINIT` → `command_event_reinit`) rather than inventing a runloop flag, and runs the swap at a point where the menu is not mid-draw. If a one-frame "stale tree on new driver" flash is observed between dispatch and the swap completing, suppress the menu draw for that single frame.

### D2 — Who frees the old setting tree? ✅ RESOLVED (no decision needed)

The tree is allocated by `menu_setting_new(void)` and held by the menu state. A callable free function **already exists**: `menu_setting_free(rarch_setting_t *setting)` (def `menu/menu_setting.c:25743`, decl `menu/menu_setting.h:104`). It is already invoked outside shutdown by `menu_entries_settings_deinit` (`menu/menu_driver.c:2283`, also reached from `:3665` and `:6770`) — i.e. the menu deinit path frees and rebuilds the tree today. The rebuild path simply calls `menu_setting_free(old_list)` then `menu_setting_new()`. No new function, no "lift" — the original spec assumed there was no free function; there is. (Original A-vs-B framing removed.)

### D3 — Failure-fallback policy

Build-new-first (see Architecture) splits the failure surface into two cases:

1. **New tree fails to build (`menu_setting_new()` returns NULL).** This happens *before* any teardown, so recovery is trivial: restore `menu_driver` to the snapshot, leave the old driver + old tree exactly as they were, log + message. No driver re-init at all.
2. **New driver fails to init (`menu_driver_init(new)` returns false), after the old was deinit'd.** This is the only case needing a driver fallback.

- **(A) Bounded fallback: prev → rgui → give up.** On new-driver-init failure, `menu_driver_init(prev)` once; if *that* fails, `menu_driver_init("rgui")` (always present) as the terminating step; if even rgui fails, log fatal and leave the menu closed rather than recursing. The old tree (`old_list`) is still installed because step 4 (free old) only runs on success — so the user keeps their tree.
- **(B) Always fall back to default driver (`rgui`).** Simpler but loses the user's prior driver even when prev would have re-init'd fine.
- **(C) Abort the runloop with a fatal error.** Hostile — loses all unsaved work in other settings.

**Recommendation:** (A). The key property is that the fallback is **non-recursive** — the earlier draft's "re-run `menu_driver_init(old)`" could itself fail and loop; the bounded prev→rgui→stop chain terminates. Snapshot cost is tiny (`char prev_menu_driver[NAME_MAX_LENGTH]` + the old `rarch_setting_t *`).

### D4 — User-visible feedback

- **(A) Show a transient message:** `runloop_msg_queue_push("[Menu] Reloading driver...", ...)` for 1 second during the swap.
- **(B) Silent.** The user clicked the dropdown; the new tree is just there.

**Recommendation:** (B), with one exception: if the rebuild **fails**, the failure-fallback path pushes an error message: `"[Menu] Driver swap failed; reverted to previous driver."` Success is silent.

## Data model changes

### New / added

- `CMD_EVENT_MENU_REINIT` in `command.h`, handled by a new `command_event_menu_reinit` in `command.c` (mirror of `command_event_reinit` at `command.c:2484`). Performs the build-new-first swap sequence (see Architecture + D3).
- A small fallback snapshot held for the duration of the swap (local to the `command_event_menu_reinit` handler, or on `menu_state` if it must outlive the call): `char prev_menu_driver[NAME_MAX_LENGTH]` plus the *old* `rarch_setting_t *` list pointer, retained until the new tree is confirmed built so the old is freed only on success and restored on failure.

> **No new free function and no runloop flag.** `menu_setting_free` already exists (D2); the swap is dispatched as a `CMD_EVENT`, not via a `RUNLOOP_FLAG` (D1). The earlier draft's `menu_settings_list_free` + `RUNLOOP_FLAG_MENU_DRIVER_RECONFIGURE` + `pending_menu_driver_reconfigure` entries are removed.

### Changed

- The menu-driver dropdown's `change_handler` (in `menu/menu_setting.c`'s menu-driver setting registration site) writes `settings->arrays.menu_driver`, copies the previous driver name into the snapshot, and calls `command_event(CMD_EVENT_MENU_REINIT, NULL)`. It does NOT call deinit/init directly.

### Unchanged

- Every per-driver gating site in `menu_setting.c`. The 25 gating branches (21 `string_is_equal` + 4 `memcmp(..., "glui", 5)`) stay; the rebuild simply re-runs them with the new value. The capability-flag refactor is non-goal v1.
- `menu_driver_init` / `menu_driver_deinit` themselves. The bug isn't in the driver lifecycle — it's that the setting tree was never re-run.
- `menu_setting_new` / `menu_setting_free`. Both exist and are used today; the swap reuses them as-is.

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
- Existing `CMD_EVENT_REINIT` (video reinit) must still work — `CMD_EVENT_MENU_REINIT` is a new, distinct enum value in `command.h`, so no collision; verify the new case in the `command_event` switch doesn't fall through into an adjacent case.

## Risks

- **The 25 gating sites in `menu_setting.c` may not be the complete set.** A subsequent grep pass should also check `menu/menu_displaylist.c`, `menu/cbs/`, and the menu-driver-specific `*.c` files (xmb.c, ozone.c, materialui.c, rgui.c) for `string_is_equal(menu_ident, ...)` reads on per-frame paths. Anything that reads `menu_ident` per-frame and caches a result also needs invalidation on swap. **This is Phase 3 of the implementation plan.**
- **Setting pointers are held by the menu stack.** If the user's menu-stack entries reference setting pointers from the old tree, those become dangling after `menu_setting_free(old_list)`. v1 resets the stack to root on swap (see Goals §3) — confirms this risk is mitigated. If a future v2 wants stack preservation, the entries must store *labels* (stable across rebuilds) and re-resolve to setting pointers post-rebuild.
- **~~`menu_settings_list_free` may not exist in lift-able form.~~ Resolved.** `menu_setting_free` already exists and is already called outside shutdown (`menu_driver.c:2283`); no lift needed (see D2).
- **Translations.** Per-driver entries have driver-specific labels (e.g. `MENU_ENUM_LABEL_VALUE_XMB_RIBBON_SHADER`). Rebuilds re-register these. The translation lookup is already keyed by the enum value, not the driver, so swap is safe — but verify the first time.

## Implementation phases

1. **Phase 1 (0.5 day):** add the `CMD_EVENT_MENU_REINIT` event + snapshot fields + the dropdown handler that dispatches it. Wire the `command_event_*` handler that performs the build-new-first / swap / fallback sequence (see Architecture + D3). Commit. *(The old "lift `menu_setting_free`" phase is gone — the function already exists, see D2.)*
2. **Phase 2 (0.5 day):** manual conformance tests (1–4 above). Add an `RARCH_DBG` log in the swap path so the test output proves the rebuild actually fired.
3. **Phase 3 (0.5 day):** secondary grep pass — find any `menu_ident` cache outside `menu_setting.c` and add invalidation. Commit. ROADMAP fold-in.

Total: ~1.5 days. Phases 1–2 form one logical bundle; phase 3 is the secondary-grep cleanup which may extend.

## External references

None — this is RetroArch-internal behaviour. The gating-site list is empirically derived from `grep`.

## Spec status

Draft — awaiting:
1. User confirmation of the **state-preservation policy** (Goals §3): tree-root after swap vs. nearest-valid-ancestor restore. Default is tree-root for v1.
2. User confirmation that the failure-fallback **reverts to old driver** rather than aborting (Decision D3 → A).
3. User confirmation that v1 leaves the gating-site refactor (capability flags) for a v2.

Once those three are confirmed, the spec is ready for implementation under `local/fixes-2026-04` as one or two bundles.
