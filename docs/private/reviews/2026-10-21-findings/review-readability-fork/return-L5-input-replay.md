## Files read
- `shared-context.md`: all 272 lines.
- `patch-L5-input-replay.diff`: all 405 lines.
- Context files: I read only the function around each hunk, not whole files, so I have no total line counts.
  - `input/bsv/bsvmovie.c`: lines 330–369, 540–790, 1505–1564, 1735–2109.
  - `input/bsv/uint32s_index.c`: lines 95–115 and 280–306.
  - `input/common/wayland_common.c`: lines 375–400 and 585–625.
  - `input/drivers/wayland_input.c`: lines 108–130.
  - `input/drivers_joypad/gx_joypad.c`: lines 440–453.
  - `input/drivers_joypad/xdk_joypad.c`: lines 296–305.
  - `input/drivers_joypad/xinput_joypad.c`: lines 125–484.
  - `input/input_driver.c`: lines 2085–2354, 4885–4935, 5510–5535, 5865–5900, 7935–7965, 8500–8552.
  - `input/input_driver.h`: lines 58–106 and 1230–1252.
  - `input/input_types.h`: lines 50–80.
- Cross-references I opened to check findings: `input/bsv/bsvmovie.h` lines 30–45, `tasks/task_movie.c` lines 160–180, `libretro-db/rmsgpack_dom.c` lines 221–241 (search context only).

## What I started with
- The global `~/.claude/CLAUDE.md`.
- A session-start hook note saying Ants MCP is connected.
- An environment block: working directory `/home/ants/.cache/lane-neutral`, not a git repo, date 2026-10-21.
- No project `CLAUDE.md`, no memory index, no git snapshot.

## Summary
This code records and replays game input, and handles controller and touch input. The replay part saves snapshots of the game ("checkpoints") inside a recording file and reads them back. The patch mostly hardens that reader so a damaged file is rejected instead of crashing. It also fixes several array-overrun bugs in controller drivers (Wii/GameCube, Xbox, XInput), Wayland touch and screen handling, and the turbo/hold button logic. One patched function, `bsv_movie_load_checkpoint`, is now hard to follow, and the two copies of the "grow the superblock sequence" step disagree.

## Findings

**[Q1] HIGH `input/bsv/bsvmovie.c:1760` — `if (!movie->superblock_seq || movie->superblock_seq_len < superblock_count)`**
- The patch added two copies of "free `superblock_seq`, clear `cur_save_valid`, calloc a bigger one, set `_len`". The other is at `bsvmovie.c:2018-2029`.
- The read-side copy checks for a failed allocation and bails out (2024). The write-side copy does not, and line 1844 writes `movie->superblock_seq[superblock]` anyway.
- A stranger fixing or changing one copy has no pointer to the other, and nothing says which behaviour is intended.
- Fix: reconcile them into one grow helper that both call, with the failure check.

**[Q1] MEDIUM `input/input_driver.c:2140` — `if (     turbo_bind >= 0 && turbo_bind <  RARCH_CUSTOM_BIND_LIST_END`**
- The patch added this guard in two places (2140 and 2328), each with the same comment: "-1 (the default) means the dedicated turbo key".
- A third reader of the same setting, `input_driver.c:7947`, still uses the old rule: `if (settings->ints.input_turbo_bind != -1) turbo_btn_id = settings->ints.input_turbo_bind;`. That value then indexes `libretro_input_binds[i][turbo_btn_id]` at line 7960 with no range check.
- So the three sites disagree on what counts as "no bind". A stranger changing the meaning of turbo_bind will update the two commented sites and miss the poll site.
- Fix: one "turbo bind is a real button" test used at all three sites.
- Searched: `ints.input_turbo_bind` (other hits are in configuration, menu and cache code, which only copy the value).

**[Q5] MEDIUM `input/bsv/bsvmovie.c:626` — `input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;`**
- In `bsv_movie_load_checkpoint`, the patch added the same four-line failure block (log, set `MOVIE_END`, `ret = false`, `goto exit`) three times: lines 640–644, 664–668 and 717–721. Two older copies sit at 624–628 and 653–656.
- Other failure paths in the same function stop without setting `MOVIE_END`: zlib (681), zstd (702), unknown compression (709), statestream decode (738), unknown encoding (745).
- Nothing says why some failures end the movie and others do not. Anyone adding a check has to guess which kind to copy.
- Fix: one comment, or one shared failure label, saying which failures end the movie and why.

**[Q3/Q4] MEDIUM `input/bsv/bsvmovie.c:757` — `if (handle->cur_save && size && size <= handle->cur_save_size)`**
- `cur_save_size` is used as the buffer's capacity at lines 607, 639 and 716, but the exit block shrinks it to the size of the loaded content.
- `bsv_movie_read_deduped_state` then reads it back as the state size (line 1877).
- A stranger will assume `cur_save_size` is how big the `cur_save` buffer is. It is really the content size, and it can be smaller than the buffer. That rule is written down nowhere.
- Fix: state that rule once beside the field, or keep separate capacity and content-size fields.

**[Q4] LOW `input/bsv/bsvmovie.c:615-620` — `The previous order (size = size; malloc) left cur_save NULL on OOM...`**
- Five lines of history (what the old code did wrong) hide the one-line rule a reader needs: record the size only after the allocation succeeds.
- The exit block at 753–756 already states that rule.
- Fix: cut it down to the current rule.

**[Q4] LOW `input/bsv/bsvmovie.c:343` — `The recorder writes 16 blocks of 128 or 16384 bytes`**
- These numbers are a third hand-copy of `DEFAULT_*/SMALL_*_SIZE` in `tasks/task_movie.c:57-62`. The second copy is in `input/bsv/bsvmovie.h:38-39`.
- If the recorder's sizes change, this comment goes quietly wrong.
- Fix: name the constants and where they live instead of their values.
- Confirms census_number at bsvmovie.c:343 and :344.

**[Q4] LOW `input/input_types.h:58-61` — `Was int32_t prior to audit S1 — using the sign bit as a sentinel is UB...`**
- History in a struct comment. What a reader needs is what bit 31 means, and that part is already stated.
- Same pattern in `gx_joypad.c:449-450` ("The original `pad < MAX_USERS` guard let callers OOB-read...") and `xdk_joypad.c:301` ("the previous bound...").
- Fix: keep the current rule and move the history to the commit message.

**[Q4] MEDIUM `input/input_driver.c:5874` — `Public API — third-party front-ends may pass arbitrary @key values.`**
- The header doc block at `input_driver.h:1243-1244` says the same: "third-party libretro front-ends and test harnesses may link against this symbol."
- Searched `input_key_pressed\b` outside `deps/` and `libretro-common/`: the only hits are the declaration and the definition. Nothing in the tree calls it.
- A stranger would treat this as a promise to outside callers and not dare to change or remove it. Nothing here backs that promise.
- The doc also says `@key` "must be < RARCH_BIND_LIST_END" and in the next breath that an out-of-range key returns false.
- Fix: drop the public-API claim unless an outside consumer can be named, and say "out-of-range returns false" once.

**[Q1/Q5] LOW `input/drivers_joypad/xinput_joypad.c:398-401` — `&& (!g_xinput_states[3].connected))`**
- The patch replaced every literal `4` in this file with `MAX_XINPUT_USERS`. Its own comment says it did so to "use a named constant for the slot count".
- This check still names slots 0–3 by hand. A reader trusting the constant would not expect a hand-written copy of it.
- The XInput API fixes the value at 4, so this only goes wrong if someone changes the constant.
- Fix: loop over `MAX_XINPUT_USERS`.

**[tool?: format specifier mismatch]** `input/bsv/bsvmovie.c:1949` — `%d` used for `size_t` arguments. This is old code next to the patch, not patch code.

## Candidates dismissed
- **`input/drivers_joypad/gx_joypad.c:447` census_number "4 on this":** correct. GEKKO/HW_RVL defines `DEFAULT_MAX_PADS 4` (`input_driver.h`, the `GEKKO` branch), and the comment names the constant beside the value. I filed this comment's history half under the `input_types.h` finding.
- **`input/drivers_joypad/xinput_joypad.c:131` census_number "4 across":** this comment is where the constant's value comes from (the XInput API limit), and the value is pinned by an outside API. Its other claim, "4 on _XBOX and HAVE_XINPUT&&!HAVE_DINPUT", matches `input_driver.h`.
- **dup:9aee41c4f0cc** (xinput 384–393 / 464–473): two copies of the state-zeroing loop that have not drifted apart, and no comment shows a past fix had to patch both. The patch changed only the loop bound, the same way in both.

## Correctness leads
- `input/bsv/bsvmovie.c:1764`: an unchecked `calloc` result is used at 1844 (`movie->superblock_seq[superblock] = ...`), and `superblock_buf` at 1753 is not checked either. Searched: read 1735–1867.
- `input/input_driver.c:7947`: a `turbo_bind` that is not -1 but outside 0..`RARCH_CUSTOM_BIND_LIST_END` (from a hand-edited config) indexes `libretro_input_binds[i][turbo_btn_id]` out of bounds. Searched: `ints.input_turbo_bind`.
- `input/bsv/uint32s_index.c:108`: when the `realloc` fails, `uint32s_bucket_expand` returns silently and drops `idx`. It returns `void`, so the caller cannot tell, and the index loses an entry. Searched: none.
- `input/bsv/bsvmovie.c:599-605` with 757: on the SKIP / UPDATE+RAW path, `cur_save_size` is set to the header's `size` even though `cur_save` was not reloaded. `read_deduped_state` then uses it as `state_size`. The old code assigned it unconditionally, so this may not be new. Searched: none.
- `input/input_driver.c:2149`: `turbo_button` indexes `input_remap_ids[port][...]` without the range check the patch gave `turbo_bind`. This is old code. Searched: none.

## Not checked
- I did not read whole files, only each hunk's enclosing function, so I have no total line counts.
- `replay_set_serialized_data`: I read only up to line 1564. I did not check its callers for the new `len` parameter.
- I did not read the rest of `input_config_get_bind_string` (before line 4885) to see whether `bind` is dereferenced before the new `if (bind)` guards.
- I did not read the end of `input_key_pressed`.
- I did not read `driver_log_unknown` (driver.h:169, retroarch.c:1388). It has many callers (searched), so the one-caller private-helper rule does not apply.
- The new error paths in `bsv_movie_read_deduped_state` free `item` once before `goto exit`, and `exit` frees it again. I saw only the string branch of `rmsgpack_dom_value_free`, which clears its pointer; I did not read the array and binary branches. So I could not settle whether this double free is safe.
- The Wayland comment says `wl_touch_handle_down` bounds the slot. That is partly confirmed: lines 383–399 store into `active_touch_positions[num_active_touches]` only while `num_active_touches < MAX_TOUCHES`. I did not trace how that maps into `wl->touches[]`.
- No test anchors were given, so none were checked.
