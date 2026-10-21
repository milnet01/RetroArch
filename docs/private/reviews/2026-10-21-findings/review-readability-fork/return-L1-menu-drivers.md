## Files read
The subject files are large, and I read windows around each change rather than whole files:

- `menu/drivers/materialui.c` — 12,949 lines in total. I read windows around every hunk, about 480 lines. Windows: 2758–2780, 6148–6182, 6420–6530, 7815–7850, 9395–9462, 10270–10292, 10690–10720, 11030–11075, 11625–11650, 12720–12770, 12895–12949.
- `menu/drivers/ozone.c` — 14,209 lines in total. Windows around every hunk, about 560 lines. Windows: 3720–3745, 3800–3812, 3890–3910, 3955–3965, 4485–4510, 4655–4700, 4860–4895, 5716–5770, 6355–6370, 7835–7845, 8415–8530, 8945–8970, 9100–9135, 10135–10185, 10810–10830, 12455–12490, 12610–12645, 13985–14006, 14170–14209.
- `menu/drivers/rgui.c` — 9,348 lines in total. Windows around every hunk, about 430 lines. Windows: 1955–2030, 2805–2925, 2980–3010, 5435–5480, 5925–5945, 7910–7930, 8180–8200, 8545–8565, 9150–9215, 9310–9348.
- `menu/drivers/xmb.c` — 11,381 lines in total. Windows around every hunk, about 560 lines. Windows: 510–520, 1115–1240, 2045–2060, 2085–2180, 2340–2365, 2400–2420, 2460–2480, 4120–4140, 5900–5920, 6925–6958, 7380–7390, 8015–8032, 8195–8210, 8780–8795, 9990–10012, 10720–10775, 11085–11100, 11160–11200, 11345–11381.
- The patch, `patch-L1-menu-drivers.diff`: read in full, 417 lines. The shared context: read in full, 272 lines.

**Two disagreements, and how I resolved them.**
1. The shared context says "read every file in your STEP 2 list, in full". The lane block and the shared context's *Scope* section say to read each hunk's enclosing function. My lane file says to read the subject in full. With files of 9,000 to 14,000 lines, I followed the lane block. Even that I did not do completely: several very large functions got windows only (see *Not checked*).
2. I read through `read_regions` rather than native `Read`. The windows came back through a spill handle.

## What I started with
- The global `~/.claude/CLAUDE.md`.
- A SessionStart hook message saying the Ants MCP is connected.
- The user's email, from the harness context.
- My review-lane system prompt.

I had no memory index, no git snapshot and no project `CLAUDE.md`.

## Summary
These four files draw RetroArch's on-screen menu in four different visual styles. This patch mostly makes them survive an empty or missing menu list. Before, they read the list's size without checking that the list exists; now most places check first. A few places now refuse a saved menu position that points past the end of a list that has since shrunk. The patch also:
- removes slots for menu features that no longer exist from each driver's function table;
- changes several screen-position divisions from whole numbers to fractions;
- adds comments explaining some code-checker warnings that are being suppressed.

## Findings

[Q1] HIGH `menu/drivers/materialui.c:11049-11073`
- **Quoted line:** `file_list_t *selection_buf = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);` / `/* While-loop above only exits with new_selection != selection. */`
- **The copies:** the "pick a random playlist" SCAN block exists in four drivers. The patch changed two of them, and differently:
  - MaterialUI got the new `selection_buf` guard, and lost its `if (new_selection != selection)` check.
  - RGUI (`rgui.c:9162-9190`) got the new guard and kept the check.
  - Ozone (`ozone.c:9107-9132`) and XMB (`xmb.c:6930-6955`) still have the old `menu_st->entries.list ? MENU_LIST_GET_SELECTION(...)->size : 0` form, and the check.
  - MaterialUI also passes `navigation_set(..., false)` where the other three pass `true`.
- **What a stranger gets wrong:** someone fixing this block will fix the copy they found and miss the others. They also cannot tell whether the `false`/`true` difference, or the dropped check, is deliberate.
- **Fix:** move the block into one shared function that all four drivers call. Until then, bring the four copies into line.
- Confirms dup:864bb134eead. Searched: read all four copies.

[Q1] HIGH `menu/drivers/materialui.c:9410-9416`
- **Quoted line:** `if (font_data->glyph_width < 1) font_data->glyph_width = 1;`
- **The copies:** this clamp stops a later division by zero. Ozone sets the same field the same way (`ozone.c:10149`, `font_data->glyph_width = (int)VIDEO_PX(font_size * (3.0f / 4.0f));`) with no clamp. Ozone also divides by it, at `ozone.c:6355`: `ticker.len = (...) / ozone->fonts.entries_label.glyph_width`.
- **What a stranger gets wrong:** the comment's reason applies to Ozone word for word. A stranger who meets the clamp in MaterialUI will assume Ozone is already protected, or not know it needs the same fix. This is a fixed copy beside an unfixed one.
- **Fix:** add the same clamp in `ozone_init_font`, or clamp inside one shared font-metrics helper.
- Searched: `glyph_width\s*=` across `menu/drivers`.

[Q1] MEDIUM `menu/drivers/xmb.c:2415`
- **Quoted line:** `end = menu_list ? (unsigned)MENU_LIST_GET_SELECTION(menu_list, 0)->size : 0;`
- **The two forms:** this patch writes the "list size, or 0" guard two ways.
  - `selection_buf ? selection_buf->size : 0` checks the selection buffer itself. Used at `materialui.c:11650`, `ozone.c:4882`, `ozone.c:8970`, `ozone.c:14002`, rgui ×4, `xmb.c:2059`, `xmb.c:7386`, `xmb.c:8029`.
  - `menu_list ? MENU_LIST_GET_SELECTION(menu_list,0)->size : 0` checks only the outer list, then uses the macro's result unchecked. Used at `ozone.c:10824`, `xmb.c:2415`, `2471`, `4133`, `8205`, `11091`.
- **Why the second form is wrong:** the comment on the macro at `menu/menu_driver.h:76-80` says "Callers MUST guard the result before dereferencing". In `xmb.c:2415`, `selection_buf` is already in scope (line 2405), yet the line calls the macro again.
- **What a stranger gets wrong:** someone copying the nearest example will pick whichever form is next to them. Nothing tells them which one is the rule.
- **Fix:** use one form everywhere, either the `selection_buf ?` guard or a NULL-safe size macro like `MENU_LIST_GET_STACK_SIZE`.
- Searched: read the macro definition and every patched site.

[Q4] MEDIUM `menu/drivers/ozone.c:12475-12478`
- **Quoted line:** `/* selection_ptr can exceed the list size: a selection remembered per tab is restored after the playlist shrank (cf. #18797). */`
- **The problem:** the only cause this comment gives is now filtered before the call. The patch's new guard in `ozone_tab_set_selection` (`ozone.c:4677-4680`) catches the shrunken saved position before it reaches `ozone_selection_changed`. But `ozone_selection_changed` has many other callers, including `ozone_navigation_clear` (12576+), 9055, 9143, 9486 and 13428. The more general reason, "list rebuilt before the navigation pointer is re-clamped", is written only on the twin guards at `ozone.c:8511-8513` and `xmb.c:2165-2167`.
- **What a stranger gets wrong:** reading 12475 next to 4677, they would conclude this guard is now redundant and delete it. That would leave the other callers unprotected.
- **Fix:** state the general cause here, as the twin guards do.
- Searched: `ozone_selection_changed(` in `menu/drivers`.

[Q4] LOW `menu/drivers/xmb.c:1128-1131` and `xmb.c:5909-5912`
- **Quoted text:** `same fix shape as Bundle 57's gfx_thumbnail.c invalidLifetime hoist` / `(same pattern as the `0 && X` placeholder closed in Bundle 62)`
- **The problem:** "Bundle 57" and "Bundle 62" are names from this fork's work history. A stranger cannot resolve them from the code, and they push the actual reason further down the comment.
- **Fix:** drop the bundle references and keep the one-line reason. The history belongs in the commit body.

[Q4] LOW `menu/drivers/xmb.c:11177-11180`
- **Quoted text:** `/* Local tab_entry is renamed from `entry` so it does not shadow the function-parameter `entry` ...`
- **The problem:** the comment narrates a rename, which is history. `tab_entry` already explains itself.
- **What a stranger gets wrong:** they lose a moment working out what "renamed from" refers to.
- **Fix:** delete the comment, or cut it to "distinct from the `entry` parameter".

[tool?: integer division in floating-point context]
- Changes such as `/ 2` → `/ 2.0f` at `materialui.c:7831-7843`, `ozone.c:3733-3960`, `ozone.c:5765`, `ozone.c:6365-6366`, `ozone.c:7841`, `xmb.c:7205`, `xmb.c:8788-8789` and `xmb.c:9818` look checker-driven.
- They change where things land on screen by fractions of a pixel, and no comment says so.
- Some untouched `/ 2` remain beside them, for example the next line in `materialui.c:7844`.

## Candidates dismissed
- **dup:d9dadf97da71** — a draw-call pattern repeated across all four drivers. The hunks that overlap it are the `/2.0f` edits and the moved `node` check, and they introduce no behaviour that differs between copies. I did not read all 30 sites.
- **dup:e5fd25dba940 / dup:27d2dcb3d0f6** — the Ozone sidebar icon and text positions. The patch edited both copies identically (3733 with 3901, 3807 with 3960). This patch did have to touch both copies, but no comment beside them records that. Under the brief's rule for two copies, that keeps it from being a finding. The third site, `ozone.c:5722`, is unrelated footer text.
- **dup:5e00bd42285b** — the twin `materialui_render_selected_entry_aux_playlist_desktop` (6155) computes `background_x` without `node`. So the patch's move of the `node` check has nothing to match there. I did not read the rest of the twin.
- **dup:0a578a731208** — the playlist-index lookup. The XMB copies the patch touched now match the guard RGUI already has (`list && selection < list_size`). I did not read the line in RGUI that declares `list_size`.
- **dup:020afea598b3** — `materialui.c:2770` already uses `/ 2.0f`, and the patch brought 7831 into line with it.

## Correctness leads
- `materialui.c:11050-11057`, and the copies at `rgui.c:9163`, `ozone.c:9107`, `xmb.c:6930`
  - When the list is empty or missing, `selection_total` is 0. The new guard now produces exactly that value. Then `(unsigned)(selection_total - 1)` wraps around to the largest possible number, and the menu reads an entry far outside the list.
  - The `while` loop also never ends when the tab holds only one playlist collection.
  - Searched: read all four copies.
- `ozone.c:10149`
  - `glyph_width` can come out as 0: from a font size under about 1.33, or on the failure path at 10160, which returns after the field is set.
  - It is then used as a divisor at `ozone.c:6355`.
  - Searched: `glyph_width\s*=` in `menu/drivers`.
- `ozone.c:10824`, `xmb.c:2415`, `xmb.c:2471`, `xmb.c:4133`, `xmb.c:8205`, `xmb.c:11091`
  - These crash if `menu_list` exists but `selection_buf[0]` is NULL.
  - Searched: the macro definition only. I did not check whether that state can actually happen.

## Not checked
- **Large functions I saw only in windows around the hunk:** `ozone_draw_sidebar`, `ozone_render`, `ozone_frame`, `xmb_frame`, `xmb_render`, `xmb_draw_item`, `xmb_pointer_up`, `rgui_render`, `rgui_render_osk`, `materialui_parse_menu_entry_action`, `materialui_list_insert`.
- **One claim in RGUI's cppcheck comment** (`rgui.c:5452-5455`): it says `ticker_smooth` "is overwritten with fresh locals at every caller". I found the single caller (`rgui.c:6028`), but did not read how `ticker_smooth` is used after it.
- **The function-table changes:** the removed slots match the current `menu_ctx_driver_t` in `menu/menu_driver.h:356+`, but that header is outside this lane.
- **Test anchor A2:** none of the fixes above touch `task_queue_wait`, `content_wait_for_*_state_task` or `retroarch_main_deinit_pending`. No anchor is affected.
