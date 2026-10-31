## Files read
- `network/cloud_sync/webdav.c`: 1510 / 1510 lines
- `samples/network/webdav_reauth/webdav_reauth_test.c`: 441 / 441 lines

## What I started with
- **Instructions and context:** the global `~/.claude/CLAUDE.md`, the review-lane instructions, an account email line, the session-start hook notes about Ants MCP, and the environment block.
- **Not present:** no project `CLAUDE.md`, memory index or git snapshot.
- **Disagreement inside the brief, and how I resolved it:** the shared context puts tests out of scope, while the lane block says both files stay in review. I applied Q1–Q5 to the harness's own readability and did not judge whether its tests are adequate.

## Summary
This code lets RetroArch keep saves and settings on a WebDAV server: it checks the server, then downloads, uploads, deletes and backs up files. Servers can demand a password challenge (HTTP Digest). The driver answers it and retries each request once, so a wrong password gives an error rather than an endless loop. With "destructive sync" off, it keeps the server's old copies in a `deleted/` folder before replacing or removing them. The test harness plays the server and checks that one-retry rule for most request types.

## Findings
Under the project limit, each fix below is for when a real fix next touches that code, or for raising upstream. None justifies a tidy-only change. "Doc search" below means `*.md/*.txt/*.rst` searched for `webdav_(needs_reauth|do_backup|backup_cb|create_ha1)|webdav\.c`, with no hits.

[Q1] HIGH `network/cloud_sync/webdav.c:887` — `if (!success && data)` — Seven callbacks hand-copy "check the challenge, retry, report", and they disagree. A stranger fixing retry or logging in one would miss the others.
- **Order differs.** Read (887/890), update (1147/1152), delete (1367/1372) and backup (1410/1415) log the failure before deciding to retry. So a stale nonce that then recovers still logs `Failed: … HTTP 401` plus all headers. Stat (730/748), mkdir (1052/1071) and update-backup (1265/1281) decide first and stay quiet.
- **Guards differ.** Only stat checks for a NULL auth header before re-sending (737). The others re-send without one.
- **Warnings differ.** The `!data` warning exists in stat, update, delete and backup, but not in read.
- **Comments.** No comment ties any of these differences to a promise.

Fix: choose one order and one guard, and put them in one shared retry-and-report step.
Cost: one edit — Searched: doc search.
dup:3913c2f5e0fa, dup:3f26d4ccae0f

[Q4] HIGH `network/cloud_sync/webdav.c:734` — `/* With no credentials configured there is nothing new to put in` — The comment and the error at 744 are false.
- `webdav_retry_auth` is true only if `webdav_create_digest_auth` succeeded, and that returns false when both username and password are empty (257). `webdav_get_auth_header` applies the same test (557). So with no credentials this branch is never entered, and the error "no username or password is configured" never prints for that case.
- `auth_header` is NULL here only when building the digest header fails, for example on a URL with too few slashes or out of memory. Then the message blames the wrong thing.
- A stranger would copy this guard into the other callbacks as "the no-credentials handling", and the user with no credentials would still see only line 633's misleading `Failure creating WWW-Authenticate: Digest header`. That text describes parsing a server challenge, not creating a header.

Fix: make the comment and message name the real cause, and report missing credentials where the challenge is refused.
Cost: one edit — Searched: doc search (only comment and log text change, with the check moved within this file).

[Q1] MEDIUM `network/cloud_sync/webdav.c:216` — `snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",` — Four copies of "allocate 33 bytes, run MD5, write hex": 199-222, 378-396, 399-421 and 430-467.
- The copies differ only in their inputs and cleanup, but the bare `33` and `32` (384, 441, 456) are spread across them.
- A likely change, adding SHA-256, means finding every one. A missed `32` silently hashes the wrong number of bytes.
- `webdav_create_cnonce` (195-196) has its own hex loop.

Fix: one static "finish digest to hex" helper with a named length.
Cost: one edit — Searched: doc search; `%02x%02x%02x%02x` across the tree found no shared helper to reuse (hits are `verbosity.c`, `rcheevos`, `os/wiiu/hbl.c`, `win32_modeline_ati.c`, the Apple WebDAV server, a Python tool).
dup:0b15d99a60e7, dup:033528692281

[Q1] MEDIUM `network/cloud_sync/webdav.c:504` — `_len  = STRLEN_CONST("Authorization: Digest ");` — The header's fields are listed twice by hand: sizing at 504-516 and writing at 526-547. I checked them and they still match. A stranger adding a field such as `algorithm=` to the writer only would get a silently cut header, because `strlcpy` truncates without error. The lost tail would include `response=` and the line ending, so the server would simply reject it. Separately, the nonce count is formatted at 447 and again at 501, where it is also incremented. Moving that increment would break the digest, and no comment says so.

Fix: build the fields in one pass, or size the buffer from one list.
Cost: one edit — Searched: doc search.

[Q1] MEDIUM `network/cloud_sync/webdav.c:1344` — `size_t _len = strlcpy_lit(dir, "deleted/", sizeof(dir));` — The backup location is written in three places (1229, 1344, 1494), and they disagree on one input.
- `webdav_update_upload` guards `fill_pathname_basedir` with `strchr(path, '/')` (1241), with no comment saying why. Lines 1345 and 1495 have no guard.
- For a path with no slash, `path_basedir` writes `.` plus the platform slash, so the backup directory becomes `deleted/./`, or `deleted/.\` on Windows. That is the backslash problem the comment at 1220-1224 warns about.
- A stranger changing the folder or the guard would fix one site and miss the others.

Fix: one helper that builds the backup directory and carries the guard, with a comment saying why.
Cost: one edit — Searched: doc search; `"deleted/` across `*.c,*.h` (also `s3.c:2067`, another driver, not this behaviour).

[Q3] MEDIUM `network/cloud_sync/webdav.c:611` — `static bool webdav_needs_reauth(http_transfer_data_t *data)` — It reads as a harmless question, but it parses the challenge and rewrites the session. `webdav_create_digest_auth` wipes the nonce, realm and hash and resets the counter (261). A stranger calling it just to check a response would destroy working credentials state, and the harness has to wrap it as `parse_challenge`. `webdav_retry_auth` (645) has the same habit but has a comment beside it.

Fix: rename it to say it accepts the challenge and replaces the digest state, or add a comment saying so.
Cost: one edit — Searched: doc search; a rename also touches `webdav_reauth_test.c` (329, 381, 390). I did not read how the other two webdav samples use these names.

[Q3] MEDIUM `network/cloud_sync/webdav.c:1425` — `static void webdav_do_backup(bool success, webdav_cb_state_t *webdav_cb_st)` — This is the delete path, and it issues a MOVE, so the server's file leaves its original place. Its sibling `webdav_do_update_backup` (1291) issues a COPY and keeps it. A stranger reusing the plainly named "backup" to protect a file would remove the server's copy. Only the sibling has a comment explaining COPY against MOVE.

Fix: name it for what it does, such as moving to `deleted/`.
Cost: one edit — Searched: doc search; `webdav_backup_cb` is also called by the harness (180).

[Q3] MEDIUM `network/cloud_sync/webdav.c:368` — `static char *webdav_create_ha1(void)` — Nearly the same name as `webdav_create_ha1_hash` (199) and the field `ha1hash` (69), but they differ. The field holds plain HA1, and this function returns the session-adjusted one for MD5-sess. A stranger using `ha1hash` directly would produce answers that work on MD5 and silently fail on MD5-sess.

Fix: names that separate base HA1 from effective HA1.
Cost: one edit — Searched: doc search (covers `webdav_create_ha1` and the file name); the field `ha1hash` was not searched separately.

[Q2] MEDIUM `network/cloud_sync/webdav.c:290` — `while (ptr < tail)` — `webdav_create_digest_auth` is mostly an even chain of branches, and the length alone is fine. A stranger loses the thread here, where the main cursor `ptr` is reused to walk the qop list and is reset only at 304. Two other jobs are also hidden inside it:
- HA1 is computed as a side effect of seeing `realm` (278-281).
- The old session is wiped before parsing starts (261), and the consequence is explained only 220 lines later (481).

Fix: pull the qop list walk and the HA1 step out of the parsing loop.
Cost: one edit — Searched: doc search.

[Q5] MEDIUM `network/cloud_sync/webdav.c:375` — `if (!string_is_equal(webdav_st->algo, "MD5-sess"))` — Any algorithm other than MD5-sess is silently treated as plain MD5, including `SHA-256`. No comment explains this, while auth-int gets an explicit refusal (301). A stranger would assume the parser rejects what the code cannot do, and would then spend time on a server that keeps "rejecting the credentials".

Fix: refuse unknown algorithms in the parser, or state the fallback in a comment.
Cost: one edit — Searched: doc search.

[Q5] MEDIUM `network/cloud_sync/webdav.c:1068` — `|| (   data->status >= 400` — MKCOL treats 3xx as success, while every other callback requires 2xx. A redirected directory is also remembered as known (1084). No comment says whether this is intended, so a stranger cannot tell whether aligning it with the others would fix a bug or break a server, and would have to ask.

Fix: state the intent in the comment, or require 2xx.
Cost: one edit — Searched: doc search.

[Q1] LOW `network/cloud_sync/webdav.c:1128` — `data.status = 200;` — Two hand-built "fake success" responses differ. `webdav_mkdir_push` clears the struct first (1028), and `webdav_ensure_dir` leaves everything but `status` uninitialised (1106). Harmless today, because `webdav_mkdir_cb` reads only `status` on that path. A stranger making that callback read another field would read stack garbage on one route only.

Fix: one helper that builds the fake response.
Cost: one edit — Searched: doc search.

[Q1] LOW `network/cloud_sync/webdav.c:1329` — `webdav_cb_st->cb = cb;` — Setting up the request state is copied four times, not the two the tool saw: 831-832, 965-968, 1329-1332 and 1466-1468. `calloc` hides a missed line, so adding a field that must be set would quietly leave it zero at one entry point.

Fix: one small constructor.
Cost: one edit — Searched: doc search.
dup:2b486c8b36b6

[Q3] LOW `network/cloud_sync/webdav.c:49` — `char *last_slash;` — It is the walk's current slash, not the last one in the URL, and `post_slash` is the saved character after it. The URL is cut in place with a NUL to do the walk (1086-1091). A stranger would expect `strrchr` behaviour. The harness itself sets it that way (`test_mkcol`, line 202).

Fix: name it for the walk position, and describe the in-place cut once on the struct.
Cost: unsure — the doc search covered the driver's file name only, not these field names.

[Q3] LOW `network/cloud_sync/webdav.c:472` — `size_t _len, __len;` — Two nearly identical names. `__len` is only the length of the nonce-count text, and `_len` is first the size being computed and then the write offset. Swapping them while editing the header is easy, and a wrong size is only a silent cut.

Fix: names for what each holds.
Cost: one edit — Searched: local variables, none visible outside this function.

[Q3] LOW `network/cloud_sync/webdav.c:122` — `char        userpass[512];` — It starts as `user:pass`, then is reused for the base64 `Authorization` line (132-137). A stranger sizing it would reason about credentials only and miss the larger final content, which is the overflow in the leads below.

Fix: separate buffers named for each.
Cost: one edit — Searched: local variable, none visible outside this function.

[Q3] LOW `network/cloud_sync/webdav.c:34` — `char path[PATH_MAX_LENGTH];` — `path` is the location on the server and `file` (35) is a local filesystem path, and neither is commented. A stranger could easily pass one for the other. `webdav_read_cb` adds a local `RFILE *file` as well (875).

Fix: a short comment on the struct, or remote and local names.
Cost: unsure — a rename also touches the harness (`new_cb_state`), and the doc search did not cover these field names.

[Q4] LOW `network/cloud_sync/webdav.c:62` — `/* Collections this sync has already created on the server. */` — It also holds collections that already existed, because a 405 falls through to the remember at 1084. The comment at 1015 repeats "created". A stranger using `dirs` as a record of what this sync made, for example for cleanup, would be wrong.

Fix: say "known to exist".
Cost: one edit — comment only.

[Q5] LOW `network/cloud_sync/webdav.c:283` — `else if (string_starts_with(ptr, "qop=\""))` — Only `algorithm` has an unquoted form (324). An unquoted `qop=auth` drops silently into the skip branch (340), and the driver then answers without qop. No comment gives a reason, so a stranger debugging such a server would see only "rejected the credentials".

Fix: comment the choice, or accept the unquoted form.
Cost: one edit — Searched: doc search.

[Q4] LOW `samples/network/webdav_reauth/webdav_reauth_test.c:20` — `* the upload into memory and leaves the file at its end; the retry` — The harness's opening comment describes in present tense a first attempt that loads the upload into memory. The driver now streams it (`webdav.c:1198`), and the stub pulls it in pieces (`stubs_retroarch.c:100`). A stranger would look for a buffered read that no longer exists. The stub's comment would put them right quickly.

Fix: describe the current streaming case.
Cost: one edit — comment only.

[tool?: dead null check] `network/cloud_sync/webdav.c:1074` — `webdav_mkdir_st ? webdav_mkdir_st->url : "<unknown>"`, already checked at 1049.

[tool?: dead null check] `network/cloud_sync/webdav.c:1083` — `last_slash &&`, but 1086 dereferences it unconditionally.

[tool?: always-true comparison] `network/cloud_sync/webdav.c:796` — `!url` on a settings array.

[tool?: reserved identifier] `network/cloud_sync/webdav.c:472` — `__len`.

[tool?: missing prototype or static] `network/cloud_sync/webdav.c:82` — `webdav_state_get_ptr` is non-static. My search found it in `webdav.c` and three samples, and I did not check for a prototype.

## Candidates dismissed
- **dup:a15ae9d13420** — two identical copies in the harness, with no comment showing a past fix patched both. The surrounding loops count retries differently but return the same number.
- **`webdav_create_digest_auth_header` (size)** — long but straight-line. The real problem is the double list, reported under Q1.
- **`webdav_read_cb`, `webdav_stat_cb`, `webdav_sync_begin`, `webdav_mkdir_cb` (size)** — followable, and their comments carry the reasons. Their problems are the copying and names reported above.
- **`webdav_reauth_test.c` `main`, `test_rejected_challenge_after_digest` (size)** — plain setup and a simple loop.
- **`webdav.c:171` "32 hex digits"** — true: 16 bytes printed as two digits each, into `cnonce[33]`.
- **`webdav_reauth_test.c:290` "200 frames"** — not a count. It means "a 200 response frames its body", which agrees with `webdav_read_cb` line 911.

## Correctness leads
- `network/cloud_sync/webdav.c:808` — A URL typed without a scheme looks broken. The size passed is 7 but the literal needs 8 bytes, so `strlcpy_lit` falls to `strlcpy` and writes `http:/` plus a NUL at position 6, then returns 7. Line 809 writes the rest at position 7, after the NUL, so the stored URL reads as just `http:/`. — Searched: `strl.h` 106-115 read; `strlcpy`'s body not opened, standard behaviour assumed.
- `network/cloud_sync/webdav.c:133` — Possible stack overflow. Username and password fields are up to 256 bytes each (`NAME_MAX_LENGTH`, `configuration.h:1398`), and long enough credentials (roughly 360+ characters combined) make the base64 line exceed 512. `strlcpy` returns the full length, so lines 135-137 then write past the buffer. — Searched: `configuration.h`, `retro_miscellaneous.h`.
- `network/cloud_sync/webdav.c:375` — A `SHA-256` challenge is accepted and answered with MD5. Because `webdav_needs_reauth` stops at the first challenge it accepts, a server offering SHA-256 first would fail even if it also offers MD5. — Searched: none.
- `network/cloud_sync/webdav.c:509` — Header format doubts, from my memory of the RFC, not checked against it: `nc` and `qop` are quoted, `nc` and `cnonce` are sent even without qop, and `algorithm=` is never sent, even for MD5-sess. — Searched: none.
- `network/cloud_sync/webdav.c:739` — OPTIONS uses the raw configured URL (also 732 and 844), while every other request goes through `webdav_url_for_path`, which encodes it. A URL needing encoding would behave differently on the first request, and its digest URI would differ too. — Searched: `tasks/task_http.c` 734-749, which passes the URL through unchanged to `net_http_connection_new`; that function was not opened.
- `network/cloud_sync/webdav.c:1214` — Callbacks ignore a NULL return from `task_push_*` (also 739, 904, 1036, 1056, 1315, 1386, 1454, 1488). If a push failed, completion would never be reported and the state would leak. `webdav_read` (972) returns the failure but still leaks the state. — Searched: none.
- `network/cloud_sync/webdav.c:1345` — The unguarded `fill_pathname_basedir` from the Q1 finding, which sends `.\` on Windows for a path with no slash (also 1495). — Searched: `file_path.c` `path_basedir`, `fill_pathname_basedir`; whether cloud sync ever passes such a path was not checked.
- `network/cloud_sync/webdav.c:1068` — 3xx on MKCOL counts as "exists" and is remembered, the same issue as the Q5 finding. — Searched: none.

## Not checked
- **Doc search limits:** it covered `*.md/*.txt/*.rst` only, for the file name and four function names. Field names such as `last_slash` and `ha1hash` were not searched in docs.
- **`samples/network/webdav_options` and `webdav_backup`:** I searched for names but did not read them. They may cover the no-credentials 401 and the COPY retry, and I did not judge that. No file under `samples/` names `webdav_update_backup_cb`, and the harness's list of request types omits COPY.
- **Not opened:** `net_http.c`, `net_http_connection_new`, and `task_cloudsync.c` beyond the search hit. So the `cloud_backups` claim at `webdav.c:1338` rests only on that hit at `task_cloudsync.c:1126`.
- **Nothing was compiled or run:** every lead comes from reading the code.
