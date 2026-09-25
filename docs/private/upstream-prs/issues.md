# Issue drafts

All four were checked against libretro/RetroArch master at `b40db2251a`. None was dropped.

---

## 1. libretro/RetroArch: run-ahead secondary core is copied to a predictable path in the shared temp directory

**Title:** runahead: secondary core copy uses a predictable path in /tmp

**Body:**

**Observation.** For the second-instance run-ahead mode, `runahead.c` copies the core library to a temporary directory and loads the copy as the secondary core.

- `get_tmpdir_alloc()` uses `$TMPDIR` or `/tmp` on non-Windows, non-Android systems.
- `runahead_copy_task_begin()` builds `<tmp>/retroarch_temp/<core basename>`. `path_mkdir()` accepts the directory if it already exists, with no check on who owns it or its permissions. The copy uses `RETRO_VFS_COPY_OVERWRITE`, so `retro_vfs_copy_begin_impl()` removes any existing file at that path and writes a new one.
- If that fails, `copy_begin_with_random_name()` tries `tmpNNNNN<ext>`. The names come from an LCG seeded with `time(NULL)`, so they can be predicted.

**Risk.** On a multi-user system, another local user can create `/tmp/retroarch_temp` first and keep write access to it. They can then replace the copied library between the copy and the load, and it runs as the RetroArch user. Guessable fallback names make that easier.

**Suggested direction.** Put the copy in a per-user directory (for example under the config or cache directory, or `$XDG_RUNTIME_DIR`). Alternatively, create a private directory with `mkdtemp()` and mode 0700, and check its owner when reusing it. Use a random name from the OS, not a time-seeded LCG.

This issue was found and written with the help of Claude Code (an AI assistant), then checked against current upstream code. Happy to adjust anything.

---

## 2. libretro/RetroArch: core updater reads the HTTP task's flags after it may have been freed

**Title:** task_core_updater: can the worker call task_get_flags() on an HTTP task that was already freed?

**Body:**

**Observation.** In `tasks/task_core_updater.c`, the `CORE_UPDATER_LIST_WAIT` and `CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER` states do the following, in this order:

1. If `http_task` is non-NULL and `http_task_finished` is false, call `task_get_flags(http_task)` and `task_get_progress(http_task)`.
2. Only afterwards, check `http_task_complete`.

`http_task` is never cleared. In the threaded queue, `task_queue.c`'s gather step runs the HTTP callback on the main thread, which sets `http_task_complete`, and then calls `free(task)`. If the gather step runs after the HTTP task finished but before the updater's next tick, `http_task_finished` is still false. The worker then calls `task_get_flags()` on freed memory.

**Question.** Does something in the queue guarantee the worker always sees `RETRO_TASK_FLG_FINISHED` before the gather step frees the task? I could not find one. If there is nothing, this is a use-after-free read. Recent commits 70b606ea85 and 2f9703049c made the completion flags atomic but did not change this order.

**Suggested direction.** Check `http_task_complete` first, and skip `task_get_flags()`/`task_get_progress()` once it is set. Or stop touching `http_task` from the worker and publish progress through the callback. (`tasks/task_pl_thumbnail_download.c` only compares the pointer and never dereferences it, so it is not affected.)

This issue was found and written with the help of Claude Code (an AI assistant), then checked against current upstream code. Happy to adjust anything.

---

## 3. libretro/libretro-common: linked_list remove-matching functions crash on a NULL callback

**Title:** linked_list: remove_*_matching dereference a NULL `matches` callback

**Body:**

**Observation.** In `lists/linked_list.c`, `linked_list_remove_first_matching()` and `linked_list_remove_all_matching()` check only `list`, then call `matches(item->value)`. So does `linked_list_remove_last_matching()`. With a NULL `matches` and a non-empty list, each one calls a NULL function pointer. The lookup functions nearby, `linked_list_get_first_matching()` and `linked_list_get_last_matching()`, already check `!list || !matches`, and `linked_list_foreach()` checks `!fn`.

**Risk.** A crash if a caller passes NULL. RetroArch itself has no such caller today, but libretro-common is shipped with cores, and this is the only part of the list API that does not tolerate a NULL callback.

**Suggested direction.** Add `|| !matches` to the entry check of all three remove functions, matching the lookup functions, and add a NULL-callback case to `test/lists/test_linked_list.c`.

This issue was found and written with the help of Claude Code (an AI assistant), then checked against current upstream code. Happy to adjust anything.

---

## 4. libretro/libretro-common: filestream_write_file_atomic can lose both the old and new file

**Title:** filestream_write_file_atomic deletes the temp file after it deleted the destination

**Body:**

**Observation.** In `streams/file_stream.c`, `filestream_write_file_atomic()` writes `<path>.tmp` and then:

1. calls `filestream_rename(temp_path, path)`;
2. if that fails, calls `filestream_delete(path)` (for Win32, where rename does not replace a file) and renames again;
3. if the second rename also fails, calls `filestream_delete(temp_path)` and returns false.

**Risk.** Step 2 deletes the destination before it is known whether the retry will work. If the retry fails (sharing violation, antivirus lock, a transient error), step 3 then deletes the temp file, which is the only complete copy. Both the old and new data are gone. Callers use this for save data, so the result is a lost save instead of a failed write.

**Suggested direction.** In step 3, keep the temp file whenever the destination no longer exists, and return false so the caller can report it. On Windows, `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING)` or `ReplaceFileW` would avoid the delete-then-rename window altogether.

This issue was found and written with the help of Claude Code (an AI assistant), then checked against current upstream code. Happy to adjust anything.
