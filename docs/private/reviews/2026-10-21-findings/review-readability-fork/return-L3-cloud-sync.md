I've read the patch and the surrounding code, and checked the comments' claims against the source. Writing up the findings now.

## Files read
- `patch-L3-cloud-sync.diff`: 362 / 362 lines (the subject, read in full)
- `shared-context.md`: 272 / 272
- `network/cloud_sync_driver.c`: 170 / 170 (whole file)
- `network/cloud_sync_driver.h`: 103 / 103 (whole file)
- `network/cloud_sync/google_drive.c`: lines 1282–1356 (`gdrive_read_download_cb`). The brief asked for enclosing functions, so I did not read the whole file and have no total line count for it.
- `network/cloud_sync/s3.c`: lines 340–549 and 840–1039 (`s3_parse_url` tail, `s3_url_encode`, `s3_query_param_cmp`, `s3_canonicalize_query_string`, `s3_build_auth_header_ex`, `s3_read_cb`). Slices for cross-checks: 795–803, 1110–1135, 1170–1180, 1480–1495. No total line count.
- `network/cloud_sync/webdav.c`: lines 55–384 (state struct through `webdav_create_digest_auth`), 878–974 (`webdav_read_cb`), 1115–1159 (`webdav_ensure_dir`). No total line count.
- Cross-checks: `libretro-common/include/net/net_http.h` 288–304 and `libretro-common/net/net_http.c` 3574–3600 (`net_http_body_is_framed`).

The brief asked for enclosing functions, so the three backend files were read in slices; no disagreement with the lane file needed settling. I read the context ranges with native `Read` after a `read_regions` batch spilled to a handle.

## What I started with
- The global `~/.claude/CLAUDE.md`.
- The session's ants-MCP startup notice and an auto-attached user email (not used).
- The review-lane agent file, this brief, and the shared context.
- No memory index and no git snapshot.

## Summary
This code copies saved games and settings to and from online storage: Amazon-style S3, WebDAV servers, and Google Drive. The patch hardens downloads in two ways. It refuses a download whose size doesn't match what the server announced. And it writes the file to a temporary copy first, then swaps it in, so a crash can't leave a half-written save. It also fixes how S3 requests are signed, and makes the WebDAV password handshake use a fresh random-ish value each time instead of a fixed one.

## Findings

[Q4] MEDIUM `network/cloud_sync/s3.c:457-462` — `strcmp on the raw "key=value" string gives the spec-mandated order (... uses the '=' / value as the natural tiebreaker)` — This is false whenever one parameter name is a prefix of another and the next character sorts below `=`. Digits, `-`, `.` and `%` all do (`a1=x` sorts before `a=y`, but SigV4 wants `a` first). Someone adding a query parameter would trust this comment, and the bug only shows up as a 403 from S3. Today's callers (`partNumber`, `uploadId`) are not affected. Fix: compare only the name part (up to `=`) first, then the value, or narrow the comment's claim. Searched: `s3_url_encode_query_value` callers, s3.c 1484/1592/1693.

[Q4] MEDIUM `network/cloud_sync/s3.c:429-438` — `(line 1017: strpbrk(path_start, "?#"))` — The line number is stale: line 1017 is now inside `s3_update_cb`, and the `strpbrk` is at 1176. The comment is also mostly the story of the old bug ("Earlier code also excluded…", "Fix: encode all three"), which buries the current rule ("this encoder is for paths only; only `/` stays literal"). A reader following "line 1017" finds nothing. Fix: state the path-only rule in one line, name `s3_build_canonical_uri_from_url` without a line number, and move the history to the commit. Searched: `strpbrk` in s3.c.

[Q4] MEDIUM `network/cloud_sync/webdav.c:177-182` — `we mix three sources ... - time(NULL) ... - clock() ... - &local_var ... - the per-session nonce-count counter` — It says three but lists four. The nonce-count counter adds nothing: `webdav_create_cnonce(webdav_st->nc)` (372) always runs after `webdav_cleanup_digest()` (269) has set `nc = 1` (166). Also, `&local_var` is really `&mix`. A stranger judging how strong the cnonce is would over-count its randomness. Fix: list the real sources and drop the counter, or say it is always 1. Also, the opening history paragraph ("The previous hardcoded…") belongs in the commit. Searched: `cnonce|->nc|nc++` in webdav.c. **Candidate webdav.c:183 does not confirm this**; see below.

[Q5] MEDIUM `network/cloud_sync/webdav.c:290-291, 369-374` vs `285, 298, 313, 320, 326, 332, 343, 359` — `goto parse_fail;` alongside `return false;` — The patch added a `parse_fail` label that resets the digest state. But eight other failure exits in the same function still `return false` and leave half-parsed state (`realm`, `ha1hash`, `nonce`) behind. Nothing says why some exits clean up and others don't. Someone adding a new failure branch can't tell which form to use. Fix: route every failure through `parse_fail`, or comment why the plain `return false` exits are safe.

[Q1] MEDIUM `google_drive.c:1305-1316`, `s3.c:962-975`, `webdav.c:920-934` — the "body is framed" guard was added in three copies, and they have already diverged:
- Google Drive guards `!data` and logs only if `data`.
- S3 adds its own 2xx re-check.
- WebDAV relies on `found`.

The comments diverge too: two say "See cloud_sync_http_body_is_framed for the threat model", while WebDAV gives the full reason inline. The warning text is identical in all three. Someone changing the policy (say, the log text, or which statuses are exempt) has three places to update, with different guards to reason about. Fix: put the status check and the warning in one shared helper next to `cloud_sync_http_body_is_framed`. Searched: `body_is_framed` across the tree.

[Q1] LOW `google_drive.c:1324-1338`, `s3.c:979-993`, `webdav.c:944-958` — three identical copies of `filestream_write_file_atomic(…, data->data ? … : "", …)` followed by a read-only reopen, each with the same four-line comment. The three failure messages already differ ("Could not write", "Failed to write local file"). A change to the write-then-reopen contract has to be made three times. This is the Rule of Three. Fix: one shared helper in `cloud_sync_driver.c` that writes the file and returns it reopened. Searched: `filestream_write_file_atomic` across the tree.

[Q4] LOW `network/cloud_sync/google_drive.c:1307` and `s3.c:964` — `See cloud_sync_http_body_is_framed for the threat model` — The function has no threat-model comment. Its body has two short comments, and the header has four lines of description. The reader goes looking for an explanation that isn't there. Fix: point at the header comment, or carry WebDAV's one-sentence reason.

[Q5] LOW `network/cloud_sync/google_drive.c:1308-1311` — `if (success && (!data || ...)) { if (data) RARCH_WARN(...)` — `success` is only true when `data` is non-null (1293), so the `!data` branch and the `if (data)` test can never fire. A stranger reads them as "success can be true with no data" and reasons about a case that doesn't exist. Fix: drop the `!data` and `if (data)` checks.

[Q4] LOW `network/cloud_sync/s3.c:858` — `once HAVE_S3 was wired in (Bundle 63)` — "Bundle 63" is a task reference used as the reason (§ 3 says that belongs in the commit). It means nothing to a stranger, who would have to go and ask. Fix: drop the parenthetical.

[Q4] LOW `network/cloud_sync/webdav.c:1143-1144` — `(it is currently saved by short-circuit luck only)` — After the `memset` this sentence is no longer true. It describes the state before the fix, so a reader thinks something is still fragile. Fix: keep only "zeroed so the callee never reads uninitialised fields".

[Q4] LOW `network/cloud_sync/s3.c:468-474` — the `s3_canonicalize_query_string` header is mostly history ("The earlier `return strdup(query_string)` was a TODO…"). It buries the one-line contract (sort `&`-joined parameters per SigV4). Fix: one line of contract, with the history moved to the commit.

## Candidates dismissed
- `network/cloud_sync/s3.c:425:census_number:0` — "3986" is the RFC's number, not a count of anything in the code. It cannot go stale.
- `network/cloud_sync/s3.c:857:census_number:0` — "409 bytes" is a worked sum, and every term is stated next to it: 127 + 1 + 8 + 1 + 255 + 1 + 2 + 13 + 1 = 409. That matches `access_key_id[128]` and `region[NAME_MAX_LENGTH]` (s3.c 60, 62). It is a sizing invariant (§ 3 allows that), and the buffer is 512, so a drift would still have margin. Not confirmed as a finding. The real issue on this comment is "Bundle 63", reported above.
- `network/cloud_sync/webdav.c:183:census_number:0` — "16 bytes" is the fixed size of an MD5 digest, and it matches `digest[16]`. It cannot go stale. The miscount on this comment is "three sources" at line 177, reported above.

## Correctness leads
- `network/cloud_sync/s3.c:463-466` — `strcmp` on the whole `key=value` string puts a name that another name extends (`a1` vs `a`) in the wrong order for SigV4. Current callers are not affected. Searched: `s3_url_encode_query_value` callers.
- `network/cloud_sync/webdav.c:283-359` — the `return false` exits leave partly-filled digest state (for example `ha1hash` set while `nonce` is missing) until the next call's cleanup. Searched: none.
- `network/cloud_sync_driver.c:159-168` — `Content-Length: 123abc` is accepted as 123, because parsing stops at the first non-digit. Leading whitespace depends on `net_http_header_value`, which I did not read. Searched: none.
- `network/cloud_sync_driver.c:157-158` — when both `Content-Length` and chunked are present, the length is checked against the de-chunked body. Whether net_http drops `Content-Length` in that case is unverified. Searched: none.

## Not checked
- The rest of `google_drive.c`, `s3.c` and `webdav.c` outside the slices listed. I have no total line counts for those three files.
- That line 1128's enclosing function is really `s3_build_request_url`, as the comment at s3.c:431 says. I did not read that function's header.
- `net_http_header_value`'s whitespace handling, and how net_http handles a response with both framing headers.
- Anchor A2 touches none of these files (`menu/`, `network/` grep for `task_queue_wait`; `tasks/`). No fix above touches `task_queue_wait` or the save/load-state names, so none breaks it.
