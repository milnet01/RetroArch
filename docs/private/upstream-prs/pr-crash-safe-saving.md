Title: save: write states, SRAM and disk index through a temporary file

## Problem

Save states, SRAM, the RAM-state file, the disk index and cloud sync downloads are written by truncating the destination and writing into it. If RetroArch crashes, loses power or runs out of disk space mid-write, the previous good file is replaced by a truncated one. The result is a lost save, or a multi-disc game that restarts on disc 1.

## Fix

Each file is written to `<path>.tmp` and moved over the destination only after the whole write succeeded:

- disk index, uncompressed SRAM, RAM-state file, WebDAV and Google Drive downloads: `filestream_write_file_atomic`;
- compressed SRAM and RAM-state files: `rzipstream_write_file` to the temporary path;
- save state task and automatic save state: stream into the temporary path; the task deletes it on error or cancel.

The rename is shared as `content_replace_file`: plain rename first (atomic on POSIX), then delete-and-retry for Win32. If the retry fails, the temporary file is kept, since it is then the only complete copy.

Note: `filestream_write_file_atomic` itself deletes the temporary file when its retry fails. I can open a separate issue for that.

## Testing

- Linux: full build; touched files compile with `C89_BUILD=1`, no warnings.
- 2048 core: two `SAVE_STATE` commands over an existing compressed state, an uncompressed `.srm` on quit, and a compressed `.srm` over an existing one. All files were replaced; no `.tmp` files remained.
- Not tested: Windows rename path, cloud sync downloads against a live server.

This change was found and written with the help of Claude Code (an AI assistant), then reviewed and build-tested on a fork. Happy to adjust anything.
