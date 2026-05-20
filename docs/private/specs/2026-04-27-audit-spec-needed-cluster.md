# Audit 2026-04 — Spec-Needed Cluster (S1–S12)

**Date:** 2026-04-27
**Status:** ✅ **closed 2026-04-27** — full cluster (S1–S12) implemented across Bundles 31 (S6+S8), 33 (S7), 34 (S1+S2+S3+S5+S10+S11+S12 + S9 policy), 35 (S4). Plus Bundle 36 follow-up sweep (`44e71ead14`) for S2 extension to materialui+rgui. Per-item chosen-option + commit are in the `## Closure summary` table below (and the `docs/private/ROADMAP.md` "Spec-needed" subsection carries the per-site detail).
**Source:** `docs/private/ROADMAP.md` — "Spec-needed" section, 12 items filtered by `audit-triage` subagent. (Section anchor; body line-numbers churn.)
**Scope:** twelve invariant / contract decisions surfaced by cppcheck + semgrep that the static analyser cannot resolve without project-side intent. Each maps to one or more code sites that are correct *iff* the invariant holds. Once the decision is made, the enforcement is almost always one of: `_Static_assert`, runtime bounds harden, or a project-policy suppression file.

These are **contracts, not features.** None of the twelve are user-visible behaviour changes; they are "should this be defended at compile time, at runtime, or never" calls. The audit flagged them as cheap (most are <1 day, three are <2 hours) but they were pending until 2026-04-27 because each required a deliberate yes/no rather than an obvious mechanical fix.

> **⚠️ Cold-eyes 2026-05-18 status update (items 1 + 5 folded in via Bundle 75).**
>
> The cluster is closed; the spec stays as the historical decision record. Six corrections apply when reading the body. Items 1 and 5 are now folded into the spec (a `## Closure summary` table + corrected follow-on-spec paths); items 2/3/4/6 stay as **reading-notes** — the per-S body prose is deliberately preserved as the decision-rationale narrative, so read it with these substitutions:
>
> 1. ✅ **Outcome recorded.** Each S-section's "Decision" / "Recommendation" reads in the recommending tense. The new `## Closure summary` table below records the option that actually shipped + bundle + commit for all twelve, so the outcome is readable without cross-walking the ROADMAP. *(Bundle 75.)*
> 2. **`retro_static_assert` shim does not exist.** Spec line 70 parenthetical and the §S6 / §S8 test-shape code blocks reference `retro_static_assert`; current code uses plain **`_Static_assert(...)`** (verified at `menu/cbs/menu_cbs_scan.c:43, 47` and `input/drivers_joypad/xinput_joypad.c:140`). Read the test-shape blocks with that substitution.
> 3. **`menu_driver.h:255` is actually `:260`** — the cited `MENU_SETTINGS_INPUT_DESC_END` line for the S6 invariant. (`input_defines.h:103,124` is correct.)
> 4. **§S4 recommendation contradicts what shipped.** Body says "ask upstream"; the cluster closed via option (2) per-line cppcheck suppression — see `.cppcheck-suppress.txt` and Bundle 35 (`8c3d825be6`). The `## Closure summary` table records the shipped option.
> 5. ✅ **Follow-on-spec paths corrected.** The `<date>` placeholders are replaced with the real `2026-04-27-*.md` filenames (and the two that were renamed since — `tls-verification-opt-in`, `cloud-sync-streaming-upload`); the "(proposed)" framing is dropped since all three specs now exist. *(Bundle 75.)*
> 6. **Pre-fix line citations** in §S6 / §S8 (e.g. `xinput_joypad.c:136 g_xinput_states[4]` at spec line 78) reflect Bundle-31 pre-fix state. Post-fix the array is at `:148` and the bound is at `:300`. Treat numbered cites in the closed-spec body as historical, not navigational; the live anchors live in the ROADMAP closure entries.
>
> Tracked under cold-eyes-2026-05-18 fold-in in `docs/private/ROADMAP.md`.

---

## Implementation order (recommended)

| Pri | Item | Effort | Test shape | Blocker? |
|-----|------|--------|------------|----------|
| 1 | S6 — bind-index arithmetic | 30 min | `static_assert` | none |
| 2 | S8 — XInput XUSER_MAX_COUNT | 1 h | `static_assert` + harden index | none |
| 3 | S7 — Wayland touch idx bound | 30 min | runtime `<` not `<=` | none |
| 4 | S11 — identicalInnerCondition policy | 2 h | per-site decision + comment-or-strip | none |
| 5 | S2 — `MENU_LIST_GET_SELECTION` contract | 1 d | invariant doc + caller sweep | clang-tidy compile_commands.json |
| 6 | S5 — `input_key_pressed()` decision | 1 d | declared-public + bounds, or delete | confirm zero internal callers |
| 7 | S1 + S10 — `(1 << 31)` / `(1 << pad)` | 1 d | mechanical `1u <<` rewrite | none |
| 8 | S9 — semgrep double-free FP suppression | 2 h | `.semgrepignore` or per-rule suppress | semgrep rerun |
| 9 | S12 — JNI `missingReturn` FPs | 1 h | `.cppcheck-suppress.txt` anchor | cppcheck rerun |
| 10 | S3 — wayland `HAVE_LIBDECOR_H` if/else | 1 h | code-clarity refactor only | none |
| 11 | S4 — qnx/vivante/xegl `HAVE_EGL` matrix | needs a maintainer answer | gate the file or harden | depends on build matrix question |

S6 and S8 are the cheapest wins: each closes with one `static_assert` plus (for S8) a one-line index harden, and both turn the invariant violation into a build break that future-proofs the cluster.

---

## Closure summary (what shipped)

The cluster closed 2026-04-27. Each S-section below reads in the recommending tense (it is the historical decision rationale); this table records the option that **actually shipped** so the outcome is readable without cross-walking the ROADMAP. Code anchors drift — re-grep by symbol; the ROADMAP "Spec-needed" closure entries carry the per-site detail.

| Item | Chosen option | Bundle | Commit |
|------|---------------|--------|--------|
| **S1** — `(1 << 31)` flag enums | mechanical `1u << N` rewrite (+ `turbo_pressed`/`hold_pressed` → `uint32_t`) | 34 | `d6bbeeab55` |
| **S2** — `MENU_LIST_GET[_SELECTION]` contract | (A) macro stays NULL-safe; all unguarded `->size`/`->list[]` callers swept + contract comment | 34 (+ debt-sweep + 55) | `335bb31cf9`, `382ee9f83f`, `3537f96a38` |
| **S3** — wayland `HAVE_LIBDECOR_H` if/else | (A) hoist the auto-monitor branch out of the `#ifdef` | 34 | `db21ebd07e` |
| **S4** — qnx/vivante/xegl `HAVE_EGL` | **(2)** per-line cppcheck suppression — EGL-by-design, no upstream answer needed (not the body's "ask upstream") | 35 | `8c3d825be6` |
| **S5** — `input_key_pressed()` | (A) declared public API in `input_driver.h` + entry-point bounds-check | 34 | `233568bfe0` |
| **S6** — bind-index arithmetic | compile-time `_Static_assert` ×2 (plain `_Static_assert`, not a `retro_static_assert` shim) | 31 | `630c7294fd` |
| **S7** — wayland touch-index | off-by-one `<=` → `<` + bound-owner invariant comment | 33 | `e67532e244` |
| **S8** — XInput port count | `#define MAX_XINPUT_USERS 4` (XUSER_MAX_COUNT) + `_Static_assert` + file sweep | 31 | `630c7294fd` |
| **S9** — semgrep double-free FP | project-policy suppression (`AUDIT-POLICY.md` + `aggregate.py` KNOWN_FP_RULES) | 34 | _policy — no code commit_ |
| **S10** — `(1 << pad)` shifts | coupled with S1 (`1u << pad`); xdk/xinput sites already swept in Bundle 31 | 34 | `d6bbeeab55` |
| **S11** — identicalInnerCondition | (B) strip inner checks (note: `task_content.c:609` kept — load-bearing FP, inline-suppressed) | 34 | `9ddc80cb84` |
| **S12** — JNI `missingReturn` FP | `.cppcheck-suppress.txt` anchor + `--suppressions-list` wire-up in `audit-config.json` | 34 | `b27a516cf5` |

---

## S6 — Bind-index arithmetic invariant unasserted

**Site:** `menu/cbs/menu_cbs_scan.c:180-185`

**The arithmetic.** The reset-bind handler decomposes a `MENU_SETTINGS_INPUT_DESC_*` enum value back into `(user_idx, key)` via:

```c
user_idx = (type - type_begin) / RARCH_ANALOG_BIND_LIST_END;
key      = (type - type_begin) - RARCH_ANALOG_BIND_LIST_END * user_idx;
settings->uints.input_remap_ids[user_idx][key] = RARCH_UNMAPPED;
```

Where `input_remap_ids` is dimensioned `[MAX_USERS][RARCH_CUSTOM_BIND_LIST_END]` (`configuration.h:177`). The decomposition is correct *iff*

```
MENU_SETTINGS_INPUT_DESC_END − MENU_SETTINGS_INPUT_DESC_BEGIN
   == RARCH_ANALOG_BIND_LIST_END × MAX_USERS
```

and `RARCH_ANALOG_BIND_LIST_END ≤ RARCH_CUSTOM_BIND_LIST_END` (so `key` lands inside the inner array). Both relations come from `menu/menu_driver.h:255` (`MENU_SETTINGS_INPUT_DESC_END = MENU_SETTINGS_INPUT_DESC_BEGIN + (RARCH_ANALOG_BIND_LIST_END * MAX_USERS)`) and `input/input_defines.h:103,124`, but they are not asserted anywhere — adding a new bind keyword today silently breaks the decomposition and the OOB write goes undetected.

**Decision:** assert the invariant at compile time.

**Test shape (drop into `menu/cbs/menu_cbs_scan.c` near the call site):**

```c
#include <retro_assert.h>

retro_static_assert(
   (MENU_SETTINGS_INPUT_DESC_END - MENU_SETTINGS_INPUT_DESC_BEGIN)
   == (RARCH_ANALOG_BIND_LIST_END * MAX_USERS),
   "MENU_SETTINGS_INPUT_DESC range must equal RARCH_ANALOG_BIND_LIST_END * MAX_USERS");

retro_static_assert(
   RARCH_ANALOG_BIND_LIST_END <= RARCH_CUSTOM_BIND_LIST_END,
   "RARCH_ANALOG_BIND_LIST_END must fit inside input_remap_ids' inner dim");
```

(`retro_static_assert` is the project's `_Static_assert` shim; the `retro_assert.h` shim already exists.)

**Effort:** 30 min (write, build, push).

---

## S8 — XInput controller-port-count invariant

**Sites:** `input/drivers_joypad/xinput_joypad.c:136` (`g_xinput_states[4]`), `:289` (`pad < DEFAULT_MAX_PADS && g_xinput_states[pad].connected`).

**The mismatch.** `g_xinput_states[4]` is a fixed 4-slot array. The bounds check uses `DEFAULT_MAX_PADS`, which (`input_driver.h:60-103`) varies by platform — 4 on `_XBOX` and `HAVE_XINPUT && !HAVE_DINPUT`, but **8** for some platforms and **16** for the default Linux/desktop build. On a build where `DEFAULT_MAX_PADS = 8` is reached at runtime and someone wires the XInput joypad driver in (e.g. via Wine or a future ports decision), `pad = 4..7` reads `g_xinput_states[4..7]` — past the array end.

**External anchor.** XInput's `XUSER_MAX_COUNT` is the documented maximum and is **4** across all XInput versions (1.3, 1.4, 9.1.0); the `dwUserIndex` parameter of `XInputGetState` is documented as "in the range 0–3" (Microsoft, *Win32 XInput API reference*; mingw `Xinput.h` defines `#define XUSER_MAX_COUNT 4`). The cap is the same on UWP. The successor `Microsoft.GameInput` API removes the limit, but that is a different API surface — any code path that reaches `XInputGetState` is bounded by 4.

**Decision:** rebase the bound on `XUSER_MAX_COUNT` (or a project alias `MAX_XINPUT_USERS = 4`), not `DEFAULT_MAX_PADS`. Add a `static_assert` so the array size and the bound stay in sync if either is touched.

**Test shape:**

```c
#define MAX_XINPUT_USERS 4   /* matches XUSER_MAX_COUNT, see Microsoft Win32 docs */

static xinput_joypad_state g_xinput_states[MAX_XINPUT_USERS];

retro_static_assert(MAX_XINPUT_USERS <= DEFAULT_MAX_PADS,
   "MAX_XINPUT_USERS must fit inside the project pad-id range");

static INLINE int pad_index_to_xuser_index(unsigned pad)
{
   return pad < MAX_XINPUT_USERS
      && g_xinput_states[pad].connected ? (int)pad : -1;
}
```

(The same bound applies to `xinput_joypad.c:373-388` connection-table iteration and `xdk_joypad.c:300` `pad < MAX_USERS` at the per-poll path — sweep all sites in one pass.)

**Effort:** 1 h.

---

## S7 — Wayland touch-index policy

**Site:** `input/drivers/wayland_input.c:117` — `if (idx <= MAX_TOUCHES)` followed by `wl->touches[idx].x` reads.

**The off-by-one.** `MAX_TOUCHES = 16` (`input/common/wayland_common.h:67`). The array is dimensioned `wl->touches[MAX_TOUCHES]` (i.e. valid indices 0–15). The check `idx <= MAX_TOUCHES` permits `idx = 16` and reads `touches[16]` — one past the end.

**External anchor.** The Wayland `wl_touch` protocol (`/usr/share/wayland/wayland.xml`, interface `wl_touch` v10) types the `id` parameter as a signed `int` and explicitly **does not cap** the value: the description says only that the ID is "unique" for the lifetime of the contact and "may be reused" after `up`. There is no protocol-level concurrent-touch-points cap — the compositor is free to assign any int. Clients cannot assume `id` is dense or bounded; the canonical pattern in upstream Wayland clients (weston, SDL) is a slot table searched by ID with explicit overflow handling, not direct array indexing by ID.

This means the bug is two-layered:
1. **Off-by-one** — `<=` should be `<`. Cheap to fix.
2. **Implicit "id maps to a small dense slot"** — the project converts `id` to `idx` somewhere upstream of this check; if that mapping does not own the bound, the off-by-one fix only buys a few touches' worth of headroom.

**Decision:** fix the off-by-one at `:117` with `idx < MAX_TOUCHES`, and document that the upstream `id`→`idx` mapping owns the bound. If the mapping is the one at `wayland_input.c:97-112` (the `wl_touch_handle_down` slot allocation), that needs its own audit pass — call it out as a follow-up but don't bundle here.

**Test shape (line 117):**

```c
if (idx < MAX_TOUCHES)
{
   /* ... existing body ... */
}
```

Plus a one-line invariant comment: `/* idx is a slot index, not the wl_touch id; the wl_touch id is mapped to a slot in wl_touch_handle_down. */`

**Effort:** 30 min for the off-by-one. The mapping audit is a separate item.

---

## S11 — identicalInnerCondition policy

**Sites:** `audio/audio_driver.c:447`, `audio/drivers/dsound.c:452`, `audio/drivers/openal.c:129`, `cheat_manager.c:1823`, `tasks/task_content.c:609`, `menu/menu_driver.c:4639`, `camera/camera_driver.c:155`. Each is a defensive double-check of the form

```c
for (d = 0; audio_drivers[d]; d++)        /* outer condition */
{
   if (audio_drivers[d])                   /* inner condition — same predicate */
      RARCH_LOG_OUTPUT("\t%s\n", audio_drivers[d]->ident);
}
```

(The `audio_driver.c:445-449` site, verbatim.)

**The decision.** Two interpretations:

- **(A) defense-in-depth.** Inner check guards against a hypothetical mid-iteration mutation of the `audio_drivers[]` table. Pros: zero runtime cost, quiet to readers familiar with the pattern. Cons: the table is `const` and immutable for the lifetime of the build — the inner check can never fire.
- **(B) copy-paste residue.** Inner check is dead code that survived a refactor. Pros: removing it is mechanical, matches the rest of the codebase's enumeration style. Cons: cppcheck stops flagging the file but the project loses one bit of "future-proof against mutation" insurance.

**Recommendation:** (B) — strip the inner checks. The driver tables (`audio_drivers[]`, `input_drivers[]`, `menu_ctx_drivers[]`, etc.) are all `const`-qualified static arrays; defense-in-depth here is paying with comprehension cost (every reader has to figure out why the same predicate is checked twice) for protection against a mutation that is forbidden by the type system.

**Test shape:** none — strip the inner `if`. Add a project-policy note in `CONTRIBUTING.md` (or wherever style guidance lives): "Driver enumeration loops use the outer NULL-terminator predicate only; do not add an inner NULL-check."

**Effort:** 2 h, mostly mechanical — 7 sites, two-line edit each, plus build-verify each affected file.

---

## S2 — `MENU_LIST_GET_SELECTION` macro contract

**Sites (macros):** `menu/menu_driver.h:72-78`.

```c
#define MENU_LIST_GET(list, idx) ((list) ? ((list)->menu_stack[(idx)]) : NULL)
#define MENU_LIST_GET_SELECTION(list, idx) ((list) ? ((list)->selection_buf[(idx)]) : NULL)
#define MENU_LIST_GET_STACK_SIZE(list, idx) ((list)->menu_stack[(idx)]->size)
#define MENU_ENTRIES_GET_SELECTION_BUF_PTR_INTERNAL(menu_st, idx) ((menu_st->entries.list) ? MENU_LIST_GET_SELECTION(menu_st->entries.list, (unsigned)idx) : NULL)
```

**Sites (callers, ~30 unguarded `->size` derefs):** see ROADMAP.md line 209 — `menu_cbs_sublabel.c:1946`, `menu_cbs_right.c:223`, `menu_driver.c:7561`, `ozone.c` (5 sites), `xmb.c` (9 sites), `materialui.c` (4 sites), `rgui.c` (4 sites). All of the form:

```c
size_t list_size = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0)->size;
```

Where the macro can return `NULL` (when `list` is NULL) and the caller dereferences `->size` unconditionally. This is the same pattern that **clang-analyzer C1** and **audit S2** independently flagged — the indie-review confirms the recurrence.

**The decision.** Three interpretations of the macro contract:

- **(A) NULL-safe with mandatory caller-check.** Document that any `MENU_LIST_GET_SELECTION(...)` result must be NULL-checked before deref. Sweep all ~30 callers to add the check. Build-verify with clang-analyzer.
- **(B) `menu_list` is invariably non-NULL during the menu-draw lifecycle.** Lift the NULL-check out of the macro (return `(list)->selection_buf[(idx)]` directly) and add a `retro_assert(list)` at the menu-init boundary. Callers stop checking; the invariant is documented at the entry point.
- **(C) Hybrid.** Keep macro NULL-safe (callers can use it from cold paths during init/teardown), but add `MENU_LIST_GET_SELECTION_OR_RETURN(list, idx, retval)` for hot-path callers that want bail-on-NULL semantics in one line.

**Recommendation:** (A). Empirically the macro returning NULL is a real path (the indie-review item came with a path-sensitive proof from clang-analyzer), and the menu-draw lifecycle includes states (driver swap, menu init failure, post-deinit cleanup) where `menu_list` is genuinely NULL. Lifting the check (option B) would either move the bug to the assert (production crash) or require a separate "is the menu still alive" flag. Option C adds a vocabulary the codebase doesn't have. Option A is the canonical "audit found a class of dereferences, add the check" pattern that the rest of the audit cluster has been resolved with.

**Test shape (per caller):**

```c
file_list_t *selection_buf = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
if (!selection_buf)
   return /* appropriate-for-callsite */;
size_t list_size = selection_buf->size;
```

Plus a comment near the macro definition:

```c
/* Returns NULL if `list` is NULL.  Callers MUST check before dereferencing
 * the returned pointer.  See indie-review C1 (2026-04-25) for the path-
 * sensitive proof that NULL is reachable in the live menu lifecycle. */
```

**Test verification:** rerun clang-analyzer with `compile_commands.json` (the audit infra item) and assert the C1 path-sensitive findings drop to zero.

**Effort:** 1 day. ~30 callers × 5 minutes per-site review + edit, plus build-verify. Cluster-sweep, not per-bundle.

---

## S5 — `input_key_pressed()` public-API-or-delete decision

**Site:** `input/input_driver.c:5430-5470`.

**The function.** `input_key_pressed(int key, bool keyboard_pressed)` reads `input_config_binds[0][key].joykey` and `[key].joyaxis`. There is no upper bound on `key` — the function trusts the caller. `input_config_binds[0]` is dimensioned `RARCH_BIND_LIST_END`, so any `key >= RARCH_BIND_LIST_END` reads past the end.

**The empirical state.** No internal callers. The function is reachable only from external consumers — i.e. it is a de-facto exported symbol. The audit flagged it as either a documented public API (in which case the bounds check is mandatory) or dead code (in which case it should be deleted).

**The decision.** Two options:

- **(A) Declared public.** Add the function to `input_driver.h` (it is already external-linkage but undeclared in the header), gate it with `RETRO_BEGIN_DECLS` + a Doxygen comment naming it as a public API, and **harden the bounds check** as the first statement: `if (key < 0 || key >= RARCH_BIND_LIST_END) return false;`. The internal early-out at `:5435` already does `(key < RARCH_BIND_LIST_END) && keyboard_pressed` for the keyboard fast path, but the joypad branch reads `input_config_binds[0][key]` without re-checking — the new bounds check moves to the function entry.
- **(B) Delete.** No internal callers means no in-tree justification. External consumers (if any) get a one-release deprecation cycle — leave a stub that `RARCH_ERR`s and returns false, then remove next minor. Risk: third-party libretro front-ends or test harnesses that call this directly will break silently.

**Recommendation:** start with (A). It is reversible (the function still exists; if the audit later finds zero external callers we can switch to B with no in-tree churn). The bounds check is a one-liner and closes the OOB-read concretely. If a future audit confirms zero external callers (e.g. by grepping libretro-frontends and the WiiU/3DS forks), promote to (B).

**Test shape (entry-point harden):**

```c
bool input_key_pressed(int key, bool keyboard_pressed)
{
   if (key < 0 || key >= RARCH_BIND_LIST_END)
      return false;
   /* ... existing body unchanged ... */
}
```

Plus a header declaration:

```c
/* input_driver.h */
/**
 * input_key_pressed:
 * @key:               RARCH_BIND_LIST_END-bounded bind id.
 * @keyboard_pressed:  current keyboard-state hint.
 *
 * @return true if the named bind is currently active on user 0.
 *
 * Public API — third-party front-ends and test harnesses may link against
 * this symbol.  Out-of-range @key returns false.
 **/
bool input_key_pressed(int key, bool keyboard_pressed);
```

**Effort:** 1 day if the libretro-frontends external-callers grep is needed for confidence; 1 hour for the bounds-check + header declaration alone.

---

## S1 — `(1 << 31)` signed-overflow flag enums

**Sites:** `runloop.h:129` (`RUNLOOP_FLAG_IS_INITED = (1 << 31)`), `input/input_driver.c:1980-1990`, `menu/cbs/menu_cbs_sublabel.c:1878-1880`, `network/netplay/netplay_frontend.c` (13 sites at 1858, 1860, 2756-2976, 3072-4020, 6182-7622).

**The standard.** `1 << 31` in C is undefined behaviour when the operand type is `int` (signed). The fact that GCC and Clang both produce the expected `0x80000000` bit pattern in practice does not make the construct portable — `-fsanitize=undefined` will trap, and the C standard explicitly permits the compiler to assume the bit isn't set (which has produced surprising optimiser behaviour in other codebases — e.g. the kernel's `(1U << 31)` sweep circa 2014). RetroArch already uses `1u << N` in some flag headers (e.g. `tasks/task_content.c`); the cluster is the holdouts.

**Decision:** mechanical rewrite to `1u << N` and store flag-set fields as `uint32_t` (or `uint64_t` if the count exceeds 32 — `runloop_flags` already uses 32 bits with the high bit = `IS_INITED`). The enum constants are then `unsigned` and the OR-into-field is well-defined.

**Test shape:** none beyond the rewrite — `1u << 31` is well-defined, and the rewritten code preserves the bit pattern the existing code relies on. Optional: build with `-fsanitize=undefined -fsanitize-trap=undefined` for one CI run to verify no remaining sites.

**Effort:** 1 day total (≤30 sites, each a 2-character `1` → `1u` edit; build-verify each affected TU). Couples cleanly with **S10** below — same edit shape.

---

## S10 — `(1 << pad)` runtime shifts

**Sites:** `input/drivers_joypad/xdk_joypad.c:300` (`g_xinput_states[pad]` — see also S8), `xinput_joypad.c:289` (same), and similar `1 << pad` patterns.

**The bug shape.** `1 << pad` where `pad` is `unsigned` and can reach 31 produces UB on the same grounds as S1. Some sites use this in mask-update operations (`mask |= 1 << pad`) which trip identical sanitiser warnings.

**Decision:** same as S1 — switch to `1u << pad`, store mask as `uint32_t`. Cluster the edits with S1 in one bundle; the build-verify cost is amortised.

**Effort:** rolls up into S1's day — same bundle.

---

## S9 — semgrep `double-free` cluster (29 sites) is a confirmed FP class

**Sites:** `network/cloud_sync/s3.c`, `tasks/task_database_cue.c`, `gfx/drivers/d3d9cg.c`, `gfx/drivers_font/bitmapfont_*.c`, `tasks/task_save.c`, plus stb / video_shader_parse — pattern is

```c
ret = function_that_takes_ownership(p);
if (ret) {
   p = NULL;            /* ownership transferred */
   return success;
}
goto error;             /* on failure: free at the cleanup label */

error:
   free(p);             /* semgrep flags this as double-free */
```

The success path nulls out `p` before returning; the failure path frees the still-owned pointer. semgrep's pattern matcher loses the null-out and reports a double-free FP for every site in the cluster.

**Decision:** project-wide suppression rather than per-site refactor. The pattern is canonical for "function takes ownership on success, caller frees on failure" — refactoring to a "single cleanup block" idiom (`if (!ret) goto error; ... error: free(p); return ret;`) would touch 29 well-tested code paths for no behavioural change.

**Test shape:** add a `.semgrepignore` entry (or per-rule suppression in the audit config) explicitly naming the rule + the cluster. Document the canonical pattern in `docs/private/AUDIT-POLICY.md` (new file) so the next audit cycle skips this cluster automatically.

**Effort:** 2 h for the suppression + a one-paragraph policy doc.

---

## S12 — JNI-callback `missingReturn` FPs

**Sites:** `play_feature_delivery/play_feature_delivery.c:117, 187`. Each is a JNI callback that ends in a switch on a Java-side enum where every branch returns; cppcheck's flow analysis loses the exhaustiveness and reports `missingReturn`.

**Decision:** project-level cppcheck suppression file. This item is the anchor — once `.cppcheck-suppress.txt` exists in the repo root, future cppcheck FPs (the audit also flagged a handful of identical ones across the `cores/`-vendored tree) can be added incrementally without churn.

**Test shape:** create `.cppcheck-suppress.txt` (project root) with an explicit per-line entry:

```
# JNI callbacks return via switch-on-enum where every branch returns;
# cppcheck flow analysis loses exhaustiveness.
missingReturn:play_feature_delivery/play_feature_delivery.c:117
missingReturn:play_feature_delivery/play_feature_delivery.c:187
```

Wire it into the audit-driver Makefile target (or whatever invokes cppcheck in `docs/private/`) via `--suppressions-list=.cppcheck-suppress.txt`.

**Effort:** 1 h.

---

## S3 — `wayland_common.c` `#ifdef HAVE_LIBDECOR_H` if/else pairing

**Site:** `gfx/common/wayland_common.c:1031-1059`.

**The shape:**

```c
#ifdef HAVE_LIBDECOR_H
   if (video_monitor_index <= 0) {
      RARCH_LOG("[Wayland] Auto fullscreen monitor index, letting compositor decide.\n");
   }
#else
   if (video_monitor_index <= 0 && wl->current_output != NULL) {
      oi = wl->current_output;
      ...
   }
#endif
   else {
      wl_list_for_each(od, &wl->all_outputs, link) { ... }
   }
```

The `else` binds to whichever `if` survived the preprocessor step. In the libdecor build, the `else` runs the manual monitor-walk **even when `video_monitor_index <= 0`** — because the libdecor `if` at `:1032` only logs and does not gate the `else`. In the non-libdecor build, the `else` correctly binds to the `:1037` `if` and the gating is intended.

This is technically working code (the libdecor build's `else` runs an extra walk that is harmless because libdecor handles fullscreen monitor selection separately), but it is a footgun for any future maintainer.

**Decision:** restructure so the `else` is unambiguous in both build configurations. Two refactors are equivalent:

- **(A) Hoist the auto-monitor branch out of the `#ifdef`:**

```c
if (video_monitor_index <= 0) {
   RARCH_LOG("[Wayland] Auto fullscreen monitor index, letting compositor decide.\n");
#ifndef HAVE_LIBDECOR_H
   if (wl->current_output != NULL) { oi = wl->current_output; ... }
#endif
} else {
   wl_list_for_each(...) { ... }
}
```

- **(B) Duplicate the `else` body inside each `#ifdef` arm:** verbose but clearest.

**Recommendation:** (A) — minimal duplication, the libdecor-only branch is a single nested `#ifndef`.

**Test shape:** none beyond build-verify with both `HAVE_LIBDECOR_H=1` and `HAVE_LIBDECOR_H=0`.

**Effort:** 1 h.

---

## S4 — Build-matrix question for qnx/vivante/xegl context drivers

**Sites:** `gfx/drivers_context/qnx_ctx.c`, `vivante_fbdev_ctx.c`, `xegl_ctx.c:299`. Each declares `new_width`/`new_height`/`vid` outside any `#ifdef` but uses them only inside `#ifdef HAVE_EGL`. cppcheck flags the "unused outside `HAVE_EGL`" branch.

**The question:** are these context drivers ever built without `HAVE_EGL`?

This is a **maintainer answer**, not a code change. Three possible answers:

- **(1) Never.** Then gate the entire file with `#ifdef HAVE_EGL` and the cppcheck warning vanishes.
- **(2) Yes, on platform X.** Then the `new_width`/`new_height`/`vid` declarations are intentionally outside the `#ifdef` because non-EGL platform X uses them. cppcheck warning is a FP — suppress per-file in `.cppcheck-suppress.txt`.
- **(3) Don't know — needs per-platform build verification.** Defer behind a CI matrix expansion (`HAVE_EGL=0` builds for qnx/vivante/xegl).

**Recommendation:** ask the libretro upstream maintainer (`libretro/RetroArch` issue referencing this spec). The personal-fork roadmap can pin "S4 deferred — needs upstream answer" until a definitive build-matrix audit lands.

**Effort:** depends on the answer. (1) is 30 min; (2) is 30 min + suppression-file entry; (3) is project-side CI work outside the audit cycle.

---

## External references

- **S1** — *ISO/IEC 9899:1999* §6.5.7p4 ("the result is undefined" for `E1 << E2` where `E1` has signed type and the result overflows). GCC's documented behaviour at `-O2` is to produce the expected bit pattern but `-fsanitize=undefined` will trap; clang behaves the same. The Linux kernel's `1U << N` sweep circa 2014 is the canonical industry precedent.
- **S7** — *Wayland protocol*, interface `wl_touch` v10, `down` event. `id` is typed `int` with documentation "the unique ID of this touch point" / "may be reused after up". No protocol-level cap on concurrent IDs. (`/usr/share/wayland/wayland.xml` on this system; the upstream spec at `wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_touch`.) Canonical client pattern: slot table searched by ID with explicit overflow handling — see weston, SDL_wayland.
- **S8** — *Microsoft Win32 XInput API reference*, `XInputGetState`. `dwUserIndex` documented as "Index of the user's controller. Can be a value from 0 to 3." `XUSER_MAX_COUNT` is `4` across XInput 1.3, 1.4, 9.1.0, and UWP. The successor API `Microsoft.GameInput` removes the cap (`IGameInput::RegisterDeviceCallback`) but is a different surface.

---

## What this spec is not

- **Not a feature spec.** None of S1–S12 changes user-visible behaviour.
- **Not a single PR.** Each item closes independently; the "Implementation order" table is a sequencing recommendation, not a dependency graph.
- **Not exhaustive of audit-needed work.** Indie-review HIGH items (menu-driver-swap rebuild, TLS opt-out, s3_update streaming) are real **feature** specs and warrant separate documents. This file is the cheap-to-spec invariant cluster only.

---

## Follow-on spec docs

These were surfaced by the audit + indie-review as feature specs in their own right; all three have since been drafted (sibling docs in `docs/private/specs/`, all dated 2026-04-27):

1. **Menu driver swap — runtime tree rebuild** (`docs/private/specs/2026-04-27-menu-driver-swap-design.md`). Indie-review HIGH. Rebuilds the 25 driver-gated setting-tree sites + the menu init path on a mid-session driver swap. Behaviour spec: when the tree rebuilds, who owns the rebuild, what user-visible state is preserved.
2. **TLS verification — default-on with documented opt-out** (`docs/private/specs/2026-04-27-tls-verification-opt-in-design.md`). Indie-review CRITICAL. Vendored in `libretro-common`; needs upstream coordination plus a RetroArch-side warning UI for the interim opt-out. *(Filename keeps the `-opt-in-` stem for stable cross-refs; the spec title was corrected to "Default-On with Documented Opt-Out" in Bundle 72.)*
3. **Cloud-sync streaming upload** (`docs/private/specs/2026-04-27-cloud-sync-streaming-upload-design.md`). Indie-review MEDIUM. `s3_update` read the entire file into RAM; the spec covers the size cap (Phase 1 shipped Bundle 32) + the streaming-read path.

The current spec doc closes the audit's "spec-needed" backlog; the three above are the indie-review's "feature behaviour" backlog.
