## Files read
- `patch-L6-runloop-save-netplay.diff`: lines 1–455, the whole file.
- `shared-context.md`: lines 1–271, the whole file.
- `tasks/task_save.c`: lines 395–504, 860–969, 1940–2049 and 2460–2539. I did not read lines 1400–1500, which hold the `replay_set_serialized_data` hunks; see Not checked.
- `save.c`: lines 520–609.
- `runahead.c`: lines 300–449, 545–580, 1060–1119, 1160–1180, 1290–1334, 1570–1619 and 2300–2409.
- `runloop.c`: lines 555–614, 1895–1964, 2190–2219, 2655–2684, 3020–3089 and 9490–9530.
- `network/netplay/netplay_frontend.c`: lines 3120–3290, 3340–3370, 6030–6050, 6140–6172, 6490–6610 and 9115–9160.
- Cross-references: `content.h` lines 44–58, and `libretro-common/streams/file_stream.c` lines 2205–2295.

I did not get the total line count of any context file. My reads were page reads and slice reads, and neither returns a total. The brief lists these as context files, and I read each hunk's enclosing function, not the whole file.

## What I started with
- The global `~/.claude/CLAUDE.md`.
- The review-lane system prompt.
- A SessionStart hook message advertising the Ants MCP verbs.
- The user's email address as attached account context.

There was no project `CLAUDE.md`, no memory index and no git snapshot.

## Summary
This part of RetroArch runs the game loop and keeps save data safe.

- **Saves:** the patch makes game saves and save states write to a temporary `<name>.tmp` file first, then move it over the old file. If the machine crashes mid-save, the previous save survives.
- **Run-ahead:** it stops run-ahead and "preemptive frames" if a game's save-state size grows mid-play. Before, that growth could overflow memory.
- **Run-ahead temp folder:** it refuses to copy a game core into a shared temp folder that another user could tamper with.
- **Untrusted cores:** several requests from a game core now refuse to act when they are given nothing (a NULL pointer). Before, those requests would crash.
- **Netplay:** the netplay changes are a mechanical sweep. Bit masks change from signed `1 <<` to unsigned `1u <<`, and two log format codes change from `%d` to `%u`.

## Findings

**[Q1] MEDIUM `tasks/task_save.c:423` — `bool content_replace_file(const char *tmp_path, const char *path)`**
- **Where the copies are:** this function copies the rename tail of `filestream_write_file_atomic` (`libretro-common/streams/file_stream.c:2263-2287`), word for word, comments included. The rzip "stage to .tmp, write, replace or delete" sequence is also written out twice, at `save.c:559-569` and `task_save.c:2500-2507`. And the `.tmp` naming rule exists twice: `content_tmp_path` and `file_stream.c:2240-2242`.
- **How they differ:** the existence check is `path_is_valid` in one copy and `filestream_exists` in the other.
- **What a stranger gets wrong:** someone fixing the replace step, for example using `MoveFileEx` on Windows to make it truly atomic, or changing the suffix, will fix one copy and miss the other. The uncompressed lanes (`save.c:574`, `task_save.c:2511`) go through the libretro-common copy. The compressed lanes and the save-state task go through the `content_*` copy.
- **Fix:** have both lanes call a single replace helper and a single tmp-name helper, plus one "write buffer atomically, with or without rzip" helper.
- **Constraint:** `file_stream.c` is vendored, so if the shared helper lives there, the change must be one upstream would take.
- **Searched:** `content_tmp_path|content_replace_file|write_file_atomic\(` across the tree.

**[Q4] MEDIUM `runahead.c:1081` — `/* Defensive against signed-overflow on `list->capacity * 2` below`**
- **What is wrong:** the guard `if (list->capacity < 0)` runs *before* the multiplication. It only rejects a capacity that is already negative. It does not stop `list->capacity * 2` overflowing at line 1101 when capacity is above `INT_MAX/2`.
- **What a stranger gets wrong:** they would trust the comment and believe overflow is handled.
- **Other problems:** the comment is seven lines, cites the analyzer run (history), and repeats a constant's value ("MAX_RUNAHEAD_FRAMES (12)"). That value is correct today (`runahead.h:27`) but will go stale.
- **Fix:** shorten the comment to what the check actually rejects, or make the check test `capacity > INT_MAX/2`.

**[Q5] MEDIUM `runloop.c:568` — `/* Per the libretro spec, behavior is undefined if data is NULL. … The brief is to treat the core as untrusted`**
- **What is wrong:** the reason for the NULL guards is stated once, in `runloop_environ_cb_get_system_info`. Yet `SET_SUPPORT_NO_GAME` (559), in the same switch, still dereferences `data` unguarded. In `runloop_environment_cb`, guards were added silently to SET_MESSAGE, SET_MESSAGE_EXT, SET_FRAME_TIME_CALLBACK, SET_SUBSYSTEM_INFO and SET_CONTROLLER_INFO. Neighbours stay unguarded: GET_MESSAGE_INTERFACE_VERSION (1898), GET_SAVE_DIRECTORY (2190) and GET_USERNAME (2196).
- **What a stranger gets wrong:** someone adding or editing a case cannot tell whether the rule is "guard every case" or "guard only these". Someone reading the comment would assume every case is guarded.
- **Fix:** state the rule once at the top of the switch, and either apply it to every case or say which ones it covers and why.

**[Q4] LOW `runloop.c:569` and `runloop.c:2212` — `The brief is to treat the core as untrusted … the original code would crash` / `Without HAVE_LANGEXTRA the case used to leave *data unwritten`**
- **What is wrong:** both comments narrate the task brief and the old behaviour instead of the current rule. "The brief" points at a document that is not in the code.
- **Fix:** keep only the rule, e.g. "a NULL `data` is refused" and "default to English so `*data` is always written".

**[Q4] LOW `tasks/task_save.c:2034` — `/* Replace the destination atomically. */`**
- **What is wrong:** on Win32, `content_replace_file` falls back to deleting the destination and then renaming (lines 432-436). That is not atomic, and a crash between the two steps leaves only the `.tmp`. The comments at 1992-1993 ("A power loss … leaves the previous savestate intact"), at 479-481, and the error string at 496 ("Failed to atomically replace save state") all promise more than that path delivers.
- **What a stranger gets wrong:** someone porting or debugging lost Windows saves would trust these comments.
- **Fix:** say "atomic on POSIX; on Win32 the old file is deleted first" once, at `content_replace_file`, and drop the word "atomically" at the call sites.

**[Q3] LOW `runahead.c:352` — `static bool runahead_tmp_dir_private(const char *dir)`**
- **What is wrong:** the name reads as a pure yes/no check, but it also `chmod`s the directory to 0700 (line 359). The comment above says the directory "must be … closed to everyone else" but does not say this function closes it.
- **What a stranger gets wrong:** someone reusing it as a side-effect-free test, or calling it on a directory whose permissions must not change, would be surprised.
- **Fix:** rename it to say it tightens permissions (e.g. "make tmp dir private"), or say so in the comment.

**[tool?: redundant assignment] `tasks/task_save.c:2502`** — `written = false;`. `written` is already initialised to `false` at 2494.

## Candidates dismissed
- **dup:715c42d0f3f0:** the patch's only edit to these four sites is the same `1 << x` to `1u << x` change in each, so the patch adds no new divergence. The existing difference is in the size test: `simstate->size != resstate->size` in the two merge functions, `!= dsize` in `netplay_resolve_input`. That code is upstream. It is likely the same value, but I did not confirm that `resstate->size == dsize`.
- **dup:47576434b141:** the two blocks are the "our own mode changed" branch (6030ff, which also sets `self_mode` and `self_devices`) and the "another client's mode changed" branch (6146ff). The difference fits their different subjects. The patch's edits are identical in both.
- **dup:10fa52fb8c88:** the two copies are identical, and they belong to the separate LOAD_SAVESTATE and RESET commands. Patching both is part of a file-wide mechanical sweep, not evidence of a shared behaviour fix. I searched for any `1 <<` left behind (regex `[^0-9a-zA-Z_.]1 ?<<` in `netplay_frontend.c`) and found none.

## Correctness leads
- **`runahead.c:1101`:** `list->capacity * 2` can still overflow `int` when capacity is above `INT_MAX/2`. The guard at 1088 does not prevent this. It is unreachable at today's bounds, according to the comment. Searched: `MAX_RUNAHEAD_FRAMES` is defined as 12 at `runahead.h:27`.
- **`runahead.c:2350` and `runahead.c:1599`:** `retro_serialize_size()` is now called on every frame by preemptive frames and by single-instance run-ahead. Some cores compute that size expensively. The comments do not state this cost, which § 3 asks for. Searched: `core_serialize_size_special` (`runloop.c:9508`); it is a plain pass-through to the core's `retro_serialize_size`.
- **`tasks/task_save.c:432-437`:** on Win32, if the second rename fails after the destination was deleted, the only copy is left as `<path>.tmp`. Nothing tells the user or the loader to look there. The `content.h` doc comment does say the tmp file is kept. Searched: none for any `.tmp` recovery on load.
- **`runahead.c:361`:** one warning, "not private to this user", covers four different causes: `lstat` failed, the path is not a directory, the owner is wrong, or `chmod` failed. Diagnosing from logs would be harder. Searched: none.

## Not checked
- **Total line counts:** I did not get them for any context file. My reads did not return totals.
- **`replay_set_serialized_data` hunks:** I did not read the hunks at `task_save.c:1421`, 1449 and 1484, which change the call to `replay_set_serialized_data(…, block_size)`, or the function's new definition. The change in its parameter is unjudged.
- **`resstate->size == dsize` in netplay:** not confirmed (see dup:715c42d0f3f0).
- **`%u` format change at `netplay_frontend.c:9143-9144`:** I did not open the declarations of `players` and `spectators`, so I could not check the change against their types.
- **Test anchor A2:** none of the fixes I name would move the text it pins (`task_queue_wait`, `content_wait_for_*_state_task`, `retroarch_main_deinit_pending`).
