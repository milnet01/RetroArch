I've finished checking. Writing up the report now.

## Files read
- `patch-L4-frontend-drivers.diff`: read in full, 367 lines. This is the subject.
- `shared-context.md`: read in full, 271 lines.
- Context files. I read slices only, so I don't have a total line count for any of them:
  - `retroarch.c`: 470–500, 1220–1419, 1653–1745, 1994–2003, 2430–2469
  - `cheat_manager.c`: 1960–2069
  - `audio/audio_driver.c`: 1240–1275
  - `audio/drivers/oss.c`: 100–185
  - `audio/drivers/wasapi.c`: 2705–2760
  - `command.c`: 300–325, 2550–2600, 3455–3484
  - `configuration.c`: 1990–2012, 2925–2962, 9745–9790
  - `core_option_manager.c`: 700–745, 990–1020, 1140–1200
  - `record/record_driver.c`: 80–110
- Opened to confirm findings:
  - `configuration.h` 1234–1248
  - `tasks/task_save.c` 405–460
  - `network/cloud_sync_driver.c` 72–103
  - `settings/settings_def_notification_positions.h`, in full (77 lines)
- Not opened beyond their patch hunks: `bluetooth_driver.c`, `camera_driver.c`, `content.h`, `driver.h`, `gfx/video_driver.c`, `griffin.c`, `wifi_driver.c`. Each hunk there is a self-contained call or declaration.

## What I started with
- The global `~/.claude/CLAUDE.md`.
- A user email context block.
- The Ants MCP session-start hook note.
- The review-lane agent file.

There was no project `CLAUDE.md`, no memory index and no git snapshot.

## Summary
When RetroArch can't find the driver named in your settings (audio, video, camera, Wi-Fi and so on), it prints a warning and lists the drivers that are available. This patch replaces about ten hand-written copies of that warning with one shared routine. It also fixes several crash and leak risks: a missing NULL check, a double close, and a categories array that was too small. Settings files are now readable only by their owner, because they hold passwords. The network command port now listens only on this machine by default. Three Game AI settings that were never saved are now saved.

## Findings

[Q4] HIGH `configuration.c:5847` — `/* Fork default: loopback only. Empty binds on every interface, ...` — The patch changed the default to `127.0.0.1`, but two older comments still say the default is empty. `configuration.h:1241–1243` reads "Empty means every interface (the historical behaviour)". `command.c:313–316` reads "which is what this interface has always done ... users who only drive RetroArch from the same machine can set network_cmd_bind_address = "127.0.0.1"". A stranger reading either file would believe the port is open to the network by default. They might then add a second loopback guard, or write the help text the wrong way round. — Fix: update both comments to say the default is loopback and empty is the opt-in. Searched: `network_cmd_bind_address|NETWORK_CMD_BIND_ADDRESS` across the tree.

[Q1] MEDIUM `cheat_manager.c:2008` / `2059` — `/* outer branch: bits < 8 — clear just this slot's bits */` — The patch edits the DELETE case in both copies of the match-action `switch`. The two copies already differ, and no comment says why. The `bits < 8` copy posts its COPY and DELETE messages as `MESSAGE_QUEUE_CATEGORY_ERROR`/`SUCCESS` (1996, 2002, 2015). The `bits >= 8` copy posts them as `CATEGORY_INFO` (2047, 2053, 2065). Its `return` also sits inside the DELETE block rather than after it. A stranger fixing a message in one copy will not know whether the other is meant to differ. Coding rule § 1.3's exception covers exactly this: a copy the change touches whose twin has diverged. — Fix: merge the two branches into one, with only the mask (`mask << (byte_part*bits)` vs `0xFF`) and the clear step varying; settle the message category once. Confirms dup:3f2a906a432d.

[Q4] MEDIUM `retroarch.c:2459` — `/* See FP note at find_driver_nonempty (line ~1175). */` — The note is at line 1277. Line 1175 is inside the MIDI code. A stranger who jumps to that line finds nothing relevant. — Fix: refer to the function by name only, with no line number. Confirms retroarch.c:2459:census_number:0.

[Q4] MEDIUM `retroarch.c:1672` — `cued clang-analyzer to flag the unconditional read at line 1736 as potentially-NULL.  Same dead-defensive class as Bundle 54.` — Line 1736 is now a blank line in the refresh-rate block. The `menu_st` read it meant is at 1829. "Bundle 54" names an audit batch that no code document defines. `retroarch.c:6552` still has the same `if (menu_st)` guard, so a stranger can't tell which form is the rule. — Fix: say only that `menu_state_get_ptr()` never returns NULL; drop the line number and the bundle reference. Searched: `menu_st` in retroarch.c; `Bundle 54|cppcheck-suppress` (hits only in comments in retroarch.c, xmb.c, rgui.c and task_content.c, plus `.cppcheck-suppress.txt`).

[Q4] LOW `retroarch.c:1282` — `Other 8 driver tables in this function follow the same pattern` — The function has 13 other branches; 11 of them index a driver table, and 2 go through a lookup function. A stranger checking the claim can't make the numbers match, and will wonder whether some tables were left out of the check. — Fix: say "the other driver tables", with no count. Confirms retroarch.c:1282:census_number:0.

[Q4] LOW `driver.h:155–157` — `warning sequence used by every *_driver_find_driver() failure path` — `network/cloud_sync_driver.c:85–95` still has its own copy of this code. It differs on purpose: it says nothing when the name is empty, and it falls back to "null" instead of the first driver. No comment explains that. A stranger trusting "every" won't look there, or will "fix" it onto the shared routine and change what it does. — Fix: say "most", and add a one-line comment at the cloud-sync copy saying why it stays separate. Searched: `Couldn't find any|Available %ss|driver_log_unknown`.

[Q4] MEDIUM `configuration.c:2002–2006` — `/* Compile-time-pool guard: GENERAL_SETTING writes tmp[count] without a bounds check. ... is now done as a runtime assert. */` — Calling this a "guard" suggests the overflow is prevented. It isn't: the `assert` runs after every write, so it only reports memory that is already corrupted. It also does nothing in builds that switch asserts off. The comment then ends on history ("The acknowledged ... is now done"). The text it quotes still sits, unchanged, at `configuration.c:2138` as a to-do, so the two contradict each other. A stranger adding settings would believe they are protected. — Fix: say it detects an overflow after the fact, in debug builds only, and update or remove the to-do at 2138. Searched: `negative array` in configuration.c.

[Q4] MEDIUM `content.h:48` — `/* Write "<path>.tmp" into @s. Returns false, and nothing may be written, if the suffix does not fit in @len. */` — Read naturally, "nothing may be written" means `@s` is left untouched. The code at `tasks/task_save.c:416` copies `path` into `s` before it checks the size, so `s` holds a cut-off path when the function returns false. A caller that keeps using `s` after a false return would be relying on a promise the code doesn't keep. — Fix: say "@s holds a truncated path on false; do not use it". Searched: `content_tmp_path`. The callers in task_save.c don't use `s` after a false return.

[Q5] LOW `audio/drivers/wasapi.c:2720` — `if (!w) return;` — The new early return makes the `if (w && w->clk_valid)`, `if (w)` and `if (w) wasapi_imm_stop_thread(w)` checks just below it dead. Meanwhile `w->client` and the `RELEASE(w->...)` lines dereference `w` unguarded. A stranger reading on will think `w` can still be NULL there, and won't know which form to copy. — Fix: drop the leftover `w` checks.

[Q4] LOW `core_option_manager.c:1160–1164` — `The old sizing -- calloc(_len, ...) but indexed up to cats_size in the parse loop below -- would heap-write OOB ...` — The rule a stranger needs (size the array by `cats_size`) is wrapped in a story about the old bug, which belongs in the commit body. — Fix: keep the first sentence and drop the history.

[tool?: implicit int/size_t conversion] `retroarch.c:1392` — `size_t i;` is passed as the `int i` parameter of `find_driver_nonempty`. The sister function `driver_find_index` (1371) uses `int`.

## Candidates dismissed
- None. All three candidates were confirmed: retroarch.c:1282, retroarch.c:2459 and dup:3f2a906a432d.

## Correctness leads
- `configuration.c:2007` (and the other six `assert(count <= ..._COUNT_MAX)`): checked after the overflow has already happened, and gone where asserts are compiled out. — Searched: `COUNT_MAX` in configuration.c. I did not check whether release builds compile asserts out.
- `cheat_manager.c:2047/2053/2065` vs `1996/2002/2015`: the same actions post their messages under different categories in the two copies. One copy is probably wrong. — Searched: none beyond the function.
- `configuration.c:9767`: the file permission is tightened only after a successful `config_file_write`. The file was written world-readable first and stays that way until `chmod` runs. Any other save route, such as overrides, is not covered. — Searched: none.
- `retroarch.c:6552`: still has the `if (menu_st)` guard that the patch called dead at 1669. Not a bug, just inconsistent. — Searched: `menu_st` in retroarch.c.

## Not checked
- The full text of the context files. I read each hunk's enclosing function, not whole files.
- Whether `core_option_manager_new`'s `error:` label really frees the half-built option, as the 1007 comment claims.
- The Game AI comment's claim about "the same idiom as the rows with no default near the top of this function". I did not read the top of `populate_settings_bool`.
- Whether `menu_content_downloads.c` builds under every configuration that compiles `griffin.c`'s MENU block.
- Anchors A2 and A3: none of the fixes above touches the text they pin (`task_queue_wait`, the `content_wait_for_*` names, `video_vp_param_snap` / `video_viewport_get_scaled_integer`).
