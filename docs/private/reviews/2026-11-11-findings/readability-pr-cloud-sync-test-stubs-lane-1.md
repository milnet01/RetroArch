## Files read
- `samples/network/s3_backup/stubs_retroarch.c`: 1–176 / 176 (all of it, including the includes and `task_push_http_download_file_with_headers` at 168–176)
- `samples/network/gdrive_backup/stubs_retroarch.c`: 1–177 / 177 (stub at 93–101)
- `samples/network/webdav_backup/stubs_retroarch.c`: 1–145 / 145 (stub at 121–129)
- `samples/network/webdav_options/stubs_retroarch.c`: 1–157 / 157 (stub at 129–137)
- `samples/network/webdav_reauth/stubs_retroarch.c`: 1–163 / 163 (stub at 136–147)
- `samples/network/webdav_reauth/stubs_retroarch.h`: 1–28 / 28 (`stub_request_t` at 9–22)
- `samples/network/webdav_reauth/webdav_reauth_test.c`: lines 279–318, the whole of `test_stale_nonce_recovers`
- Cross-reference: `tasks/task_file_transfer.h` 118–142, the comment on the real function.

The `Read` tool showed one extra line number on each `.c` file and on the `.h` file (for example 177 / 178). That is the empty line after the final newline, so I report the content lines.

## What I started with
- The global `~/.claude/CLAUDE.md`.
- The SessionStart hook notes about the Ants MCP.
- The review-lane agent file.
- The shared context file. Its `coding.md` §§ 1.3, 1.4, 3 and 4 were quoted inline.

I had no project `CLAUDE.md`, no memory index and no git snapshot.

## Summary
These files are stand-ins for RetroArch's real download functions, used by small test programs. Each test program checks one cloud-backup driver (Amazon S3, Google Drive, WebDAV). When the driver asks to download a file, the stand-in writes down what was asked and does not touch the network. The test then plays the server and answers. Each test program has its own copy of the stand-in, and the copies behave differently on purpose:
- Three ignore the file the download should be saved into.
- One remembers that file, so the test can put the server's data there.
- One stops the program, because that test should never download anything.

## Findings
[Q4] MEDIUM `samples/network/webdav_reauth/stubs_retroarch.h:15` — `/* A GET's file, which the real task streams the body into. */` — The header's comments say this struct is "the most recent request" (line 9), and that `sink_path` is "a GET's file". But `sink_path` is only written by the download stub (`stubs_retroarch.c:145`). The shared `stub_record` (69–78) never clears it, and `task_push_http_transfer_with_headers` (128–134) records a "GET" without one.

So after a download, any later request (a PUT, a plain GET, a MOVE) still carries the old download's path until `stub_reset()` runs. Someone writing a new test that answers a second request with a body would trust the comment. Following the pattern at `webdav_reauth_test.c:305-306`, they would then write that body into the earlier download's file. — Fix: clear `sink_path` in `stub_record`, or say in the field comment that it belongs to the last *download*, not the last request.
Cost: one edit. Searched: `stub_record|stub_request_t|webdav_reauth` in every `*.md` in the tree (no hits), and `sink_path` across the tree (in the samples, only `webdav_reauth_test.c:305-306` reads it). ANCHORS: none.

[Q1] LOW `samples/network/webdav_options/stubs_retroarch.c:129-137` — `void *task_push_http_download_file_with_headers(const char *url,` — The download stub exists in five copies with three different behaviours:
- record and ignore the path: `s3_backup:170`, `gdrive_backup:95`, `webdav_backup:123`
- record the path: `webdav_reauth:138`
- abort: `webdav_options:129`

Every file's header comment says how its harness behaves, so the differences are intended, not drift. The risk is to someone extending a recording harness to answer a read with a body. If they copy `webdav_reauth`'s pattern into `s3_backup` or `gdrive_backup`, they will not get the file written, because those copies drop `path`. Their own comments do warn of this ("nothing is written there"). — Fix: none needed beyond keeping those three comments, which say why the path is dropped.
Cost: one edit (no code change proposed). Searched: `task_push_http_download_file_with_headers` across the tree, which found the five stubs and three driver callers (`webdav.c:905,966`, `s3.c:927`, `google_drive.c:1380`).

`samples/network/s3_backup/stubs_retroarch.c:8` — `#include <stdlib.h>` — [tool?: unused include] I saw nothing in this file use `malloc`, `free` or `abort`. Line 9's `<string.h>` may be unused too, because `strlcpy` comes from `compat/strl.h`.

`samples/network/webdav_backup/stubs_retroarch.c:9` — `#include <stdlib.h>` — [tool?: unused include] Same pattern as above, and `<string.h>` on line 10 also looks unused.

I checked the "no test answers a read with a body" comments in s3, gdrive and webdav_backup. They hold. Searched: `_read_cb|filestream_write|download|"GET"` in those three `*_test.c` files. None answers a download with a body.

## Candidates dismissed
None were given.

## Correctness leads
- `samples/network/webdav_reauth/stubs_retroarch.c:69-78` — `stub_record` does not clear `sink_path`, so it outlives its download (the same issue as the Q4 finding above). It is latent, because `test_stale_nonce_recovers` calls `stub_reset()` first (`webdav_reauth_test.c:294`). Searched: `sink_path` in the samples tree. Only line 305 of the test reads it.
- `samples/network/webdav_reauth/stubs_retroarch.c:145` — `sink_path` keeps the driver's own pointer rather than a copy. The test uses it before the callback runs (`webdav_reauth_test.c:305-307`), so the driver's storage is still alive. If the order changes, the pointer could point at freed memory. Searched: none. I did not read how `webdav.c:966` owns that string.

## Not checked
- The rest of `webdav_reauth_test.c` and the other harnesses' test files, apart from the one search above.
- The three driver call sites, beyond their line numbers.
- Whether the `<string.h>` and `<stdlib.h>` includes are truly unused needs a compiler or linter; I only read the files.
