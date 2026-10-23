## Files read
- `menu/menu_driver.c`: lines 9280–9369 (`menu_playlist_random_selection` and the new `menu_playlists_tab_random_selection`)
- `menu/menu_driver.h`: lines 845–848, from search context only (both declarations)
- `menu/drivers/materialui.c`: lines 11100–11189
- `menu/drivers/ozone.c`: lines 9280–9339, plus 9150–9160 and 9200–9225 for context
- `menu/drivers/rgui.c`: lines 9560–9609
- `menu/drivers/xmb.c`: lines 6270–6319
- `samples/menu/playlists_scan/Makefile`: 13 / 13
- `samples/menu/playlists_scan/build.sh`: 67 / 67
- `samples/menu/playlists_scan/playlists_scan_test.c`: 329 / 329
- Cross-reference: `samples/menu/playlist_nav/build.sh`, 72 / 72
- Also the diff `pr-menu.diff`, 160 lines

## What I started with
- The global `~/.claude/CLAUDE.md`, which was injected.
- The review-lane agent file.
- A SessionStart hook note about the Ants MCP tools.
- No project `CLAUDE.md`, no memory and no git snapshot of this repository.

## Summary
In RetroArch's Playlists tab, pressing "Scan" jumps the cursor to a random playlist. Before this change, each of the four menu styles had its own copy of that jump. Each copy kept drawing random positions until it hit a different playlist, so with no other playlist to land on it drew forever and the menu froze. The change replaces the four copies with one shared function. That function counts the other playlists first, picks one of them fairly, and leaves the cursor where it is when there are none. The new sample program starts a real RetroArch menu, presses Scan with zero, one and two playlists, and a timer turns a freeze into a failure.

## Findings
[Q1] HIGH `menu/drivers/ozone.c:9290-9292` — `while (new_selection == tab_selection)` — Ozone's sidebar Scan uses the same "redraw until different" loop this change removed from the four Playlists-tab copies, and it sits 15 lines above the fixed Ozone block. Someone who reads the new helper's comment and the harness header ("the draw never ended and the menu froze") will believe Scan's freeze is fixed everywhere. Neither the harness nor the helper reaches this copy, so they would miss it. With exactly one playlist tab in the sidebar and the cursor on it, `random_range(min, min)` returns the same value every time. — Fix: give the sidebar loop the same "nothing else to land on → leave the selection" guard. Note the range here is sidebar tabs, so the new helper cannot be called directly.
Cost: one edit. The fix touches only code inside `ozone_parse_menu_entry_action` and renames or moves nothing; no ANCHORS entry. Searched: the regex `while \((new_selection|selection) ==` over `menu/` (complete, not truncated). The hits were this line and `menu_driver.c:9327`.

[Q1] MEDIUM `samples/menu/playlists_scan/build.sh:20-67` — `make -n 2>/dev/null | grep -E '\-o obj-unix/release/retroarch\.o' | head -1 > "$cc_line"` — this script is a line-for-line copy of `samples/menu/playlist_nav/build.sh` lines 25–72. Only the directory and target names differ. A `Grep` for `playlist_nav` under `samples/` found 24 files, among them `build.sh` in 15 other sample directories: `setting_command`, `stereo_headset`, `menu_icon_ownership`, `nav_repeat_stall`, `exit_drain`, `verbosity_toggle`, `packed_rows`, `rgui_aspect_defer`, `resolution_reset`, `ozone_color_theme_defer`, `shader_copy`, `log_atomicity`, `mixer_retirement` and others. I read only the `playlist_nav` one, so the rest may be copies too, but I did not confirm that. If the build recipe changes (the `obj-unix/release` path, how the compile line is pulled from `make -n`), someone has to fix every copy and is likely to miss some. — Fix: one shared script under `samples/` that takes the sample directory and target name as arguments.
Cost: unsure. The other copies may belong to other unmerged changes, and I did not search the project's specs for those script paths.

[Q4] LOW `samples/menu/playlists_scan/build.sh:37` — `touch retroarch.c` — no comment says why. It is what makes `make -n` print the `retroarch.o` compile line, the same trick that line 55 does explain for `rm -f retroarch`. Someone tidying the script could delete it as a stray side effect, and the script then fails with "could not determine the compile command". — Fix: a short reason on the line before, like the one at line 55.
Cost: one edit (comment only).

[Q3] LOW `menu/menu_driver.h:848` — `size_t menu_playlists_tab_random_selection(size_t selection);` — the name differs from its neighbour `menu_playlist_random_selection` (line 845) only by `s_tab`. Someone wiring Scan in a new driver could easily call the wrong one. It would still compile and mostly work, but it can land on a row that is not a playlist. The comment above the definition does explain the difference, but the header has no comment. — Fix: a one-line comment on the new declaration saying it picks only among the Playlists tab's playlist entries.
Cost: one edit (comment only).

[tool?: C89 conformance] `samples/menu/playlists_scan/playlists_scan_test.c:42` — `#define CHECK(cond, ...) \` — variadic macros (`__VA_ARGS__`) and `snprintf` are C99. Project rule 3 says C89. Whether this matters depends on the flags the borrowed compile line uses.

No other findings in the hunks. The four driver call sites are identical apart from upstream-owned lines outside the hunks. Examples: MaterialUI passes `false` to `navigation_set` where the others pass `true`, and its Playlists-tab branch ends with `break;`. Both are outside the hunks and stay out of scope under project rule 2.

## Candidates dismissed
None were given.

## Correctness leads
- `menu/drivers/ozone.c:9290` — Scan in the sidebar freezes when the sidebar has exactly one playlist tab and the cursor is on it. Separately, when `show_playlist_tabs` is off, `horizontal_list_size` is 0 (lines 9155–9157). Then `random_range(end+1, end)` returns `end+1`, which is not a playlist tab, and `ozone_sidebar_goto` is called on it. — Searched: `horizontal_list_size\s*=` in `ozone.c`, and `random_range` in `libretro-common/include/retro_math.h:205-207`.
- `samples/menu/playlists_scan/build.sh:37,56` — the script deletes the user's built `retroarch` binary and touches `retroarch.c`, and restores neither. The next `make` relinks and recompiles. — Searched: none.
- `menu/menu_driver.c:9365` — the comment `random_range() returns one past max when rand() == RAND_MAX` is true but understates the case. `random_range` computes in `float`, so any `rand()` value close enough to `RAND_MAX` to round to 1.0 also gives `max+1`. The fallback still handles that correctly. — Searched: `retro_math.h:205-207`.

## Not checked
- **A search that did not finish.** `workspace_search` ignored `timeout_sec: 30` and hit its 5-second limit on the first two searches. The rerun of the main pattern, narrowed to `menu/`, finished and was not truncated. The `playlist_nav` search was rerun with `Grep`.
- **The other sample scripts.** I read only `playlist_nav/build.sh`, so I did not confirm that the other 14 are copies of it.
- **Where the scan driver code lives.** I did not check whether any driver code outside `menu/` has a Scan handler.
- **The harness as a test.** I did not judge it as a test, per the brief. I did not check whether `msg_hash_lbl_str.h` and the other included headers exist.
- **Spec search.** I did not search the project's specs for the build-script fix, so that fix's cost is graded "unsure".
