## Files read
- `network/cloud_sync/s3.c`: lines 430-849 with `Read`, which covers all three named functions and their neighbours. I also read slices 395-412, 850-872, 1155-1190 and 1762-1792. I did not get the file's total line count, because the brief limits this lane to part of the file.
- `samples/network/s3_backup/s3_backup_test.c`: lines 1-75 and 320-404 of 404. I used `read_regions` rather than native `Read`, which departs from the lane rule for subject files.

**Brief conflicts I resolved:**
- The shared context says to read every file in full, but the lane block says "not the rest of the file". I followed the lane block.
- The shared context puts tests out of scope, but the lane names the test harness. I report only on the harness's comments and do not judge the tests.

## What I started with
- The global `CLAUDE.md` and the review-lane instructions.
- The user's email address.
- Session-start notes about Ants MCP and know-how.
- Environment details: platform, date, working directory.

I had no memory index, no git snapshot and no project `CLAUDE.md`.

## Summary
RetroArch can sync saves to S3-style cloud storage, and every request must carry a cryptographic signature. To make that signature, the query part of the web address has to be put in a strict order. This change makes the code really sort those query parameters, where it used to pass them through unchanged. If memory runs out while copying the query, it now refuses to sign, because a wrong signature would only appear later as a confusing server rejection (403). The test file gains checks for the sort order and for that out-of-memory case.

## Findings
[Q4] MEDIUM `network/cloud_sync/s3.c:783` — `/* Build canonical query string.  Out of memory, the request is not` — A stranger reading this concludes the signer refuses to sign when memory runs out. Twenty lines earlier that is not so: if the `malloc` at line 763 fails, line 797 substitutes `""` and the request is signed with empty headers, which is the same delayed failure this comment warns about. Someone debugging a low-memory 403 would trust the comment and rule out the signer. `test_signer_out_of_memory` does not catch this, because the seam only fails `strdup`. — Fix: send the headers failure to the same `cleanup` exit, or say in the comment that it covers the query only.
Cost: one edit — Searched: ANCHORS is empty; the fix moves and renames nothing, so no spec search applies.
Searched: read 753-776 and 791-799; `canonical_headers` has no failure exit.

[Q4] MEDIUM `network/cloud_sync/s3.c:468` — ` * with s3_query_param_cmp, re-join.  The callers have already` — "per SigV4" and "URL-encoded" leave out what a caller must get right, and someone adding a query such as a `prefix=` listing would hit two traps:
- **Two encoders, not named:** `s3_url_encode` (line 400, described as "according to RFC 3986") leaves `/` as it is, and `s3_url_encode_query_value` (line 1160) escapes it. The first is the obvious pick and would produce a 403 on any value containing `/`.
- **The `=` is not added:** a bare parameter such as `uploads` stays bare, while SigV4 needs `uploads=`. That requirement is from my knowledge of AWS's rules and I did not check it against a source in the tree. The literal `"uploads="` at line 1768 agrees with it. The old comment listed encoding as a step the function should do, and the new one rightly drops it, but nothing replaces that with a stated precondition.

— Fix: have the comment name `s3_url_encode_query_value` and say every parameter must already be written `name=value`, even with an empty value.
Cost: one edit — comment only.
Searched: every `s3_build_auth_header` call site in `s3.c`. Today's non-empty queries are the `uploadId` and `partNumber` strings at 1466, 1567 and 1669, built from `s3_url_encode_query_value`, plus the literal at 1768. So "already encoded" is true today.

[Q3] LOW `network/cloud_sync/s3.c:493` — `   out    = (char*)malloc(i + 1);` — The buffer size relies on `i` holding the string's length only because the counting loop above happened to run to the end. Rewriting that loop, for example with `strchr` or an early exit, would leave `i` wrong, and the buffer would be undersized with no compile error. The comment at 491 explains why the input's length is enough, but not that `i` is that length. — Fix: give the length its own name instead of reusing the loop counter.
Cost: one edit — Searched: `.md` files for `s3_canonicalize_query_string`, `s3_query_param_cmp` and `s3_backup_test`, nothing found; the variable is local; ANCHORS is empty.

[Q4] LOW `samples/network/s3_backup/s3_backup_test.c:1` — `/* Harness for the S3 driver's backups under non-destructive sync.` — The opening comment still describes the file as backup-only, and its list of checks omits the two new tests. Lines 24-25 say the signature was cross-checked once and "here the test checks the header set", which no longer covers the query ordering now tested. Someone looking for signer tests would not look in this file and might write a second harness. They might also take the botocore cross-check to cover the new sort; I could not establish whether it does. — Fix: add the signer's query order and out-of-memory refusal to the comment's list, and say whether the cross-check predates the sort.
Cost: one edit — comment only.

## Candidates dismissed
- `s3_calculate_signature_v4_with_time` (`function_sizes`, 105 lines): a straight run of labelled steps with one exit, and the diff adds only four lines. I found no line where a reader loses the thread. Its one problem is the comment finding above.
- `s3_canonicalize_query_string` (`function_sizes`, 59 lines): count, allocate, cut, sort, join, in that order, with the non-obvious parts commented. Length alone is not a finding.

## Correctness leads
`network/cloud_sync/s3.c:763` — if the `canonical_headers` allocation fails, the request is still signed, with empty headers, instead of the signer returning NULL as the query path now does. — Searched: read 753-799 and the caller at 854-861, which does handle a NULL signature, so refusing would be safe.

## Not checked
- **SigV4 rules:** I did not check them against AWS's documentation. The comparator's byte reasoning I checked by hand: digits, `-`, `.` and `%` are all below `=`, and `_`, `~` and letters are above it, so the comment is accurate.
- **The `strdup` seam:** I did not read `stubs_retroarch.h`, so I don't know whether the macro also reaches any `strdup` there. The comment says "the driver's" calls only. `http_init_failure_test.c` has a function of the same name; I saw only its search hits, and did not compare the two.
- **Outside this lane's scope, not graded:** the comment at `s3.c:755-761` lists a header order that omits `x-amz-copy-source`, and the `headers` argument is only ever tested for NULL. The diff touches neither.

Review finished: four findings, one correctness lead, both candidates dismissed.
