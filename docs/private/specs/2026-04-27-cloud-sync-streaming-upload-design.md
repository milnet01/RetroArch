# Cloud Sync — Streaming Upload (Design)

**Date:** 2026-04-27
**Source:** `docs/private/ROADMAP.md` line 202 — indie-review MEDIUM. (`s3.c:1838` reads entire file into RAM.)
**Status:** draft, awaiting user review
**Target:** `local/fixes-2026-04`
**Effort estimate:** 2 days for the cap+stream path; +1 day if we extend to WebDAV / Google Drive in the same bundle.

---

## Summary

`network/cloud_sync/s3.c::s3_update` (line 1807) reads the entire local file into a single `malloc(file_size)` buffer at line 1838 before signing and uploading. The buffer is sized to whatever the local file is — a 5 GB save state on a 256 MB-class console means an OOM crash; even on desktop, a multi-GB upload blocks the runloop on the malloc + the `filestream_read`. The same shape exists in the **multipart-upload** path (line 1860+) but at the part level — if a part is sized larger than available RAM, the same problem appears one layer down.

This spec defines:

1. A **hard size cap** (per-file refusal threshold) that protects memory-constrained targets from OOM and unhappy-path desktop users from runaway uploads.
2. A **streaming-read path** for files between "small enough to fit in one buffer" and "large enough to trip multipart" that doesn't require holding the whole file in RAM.
3. A **multipart-already-streaming check** to confirm the existing multipart path doesn't itself mallog full parts.

The same shape exists in `webdav.c` and `google_drive.c` — this spec covers the S3 path concretely and recommends the WebDAV / Google Drive scope as a v1.1 extension once the S3 cap pattern is validated.

## Current behaviour

```c
/* network/cloud_sync/s3.c:1807 — s3_update entry */
char *file_data = NULL;
size_t file_size = 0;

if (file)
{
   filestream_seek(file, 0, SEEK_END);
   file_size = filestream_tell(file);
   filestream_seek(file, 0, SEEK_SET);

   if (file_size > 0)
   {
      file_data = malloc(file_size);             /* ← unbounded allocation */
      if (file_data)
         filestream_read(file, file_data, file_size);
      else
      {
         RARCH_ERR(S3_PFX "Failed to allocate memory for upload (%zu bytes)\n", file_size);
         goto cleanup;
      }
   }
}
/* ... */
if (file_size > multipart_threshold)
   /* multipart upload path */
else
   /* single-PUT path uses file_data */
```

**The multipart threshold** (`s3_get_multipart_nominal_threshold_bytes`) controls only when the *single-PUT* flow versus the *multipart* flow is chosen. The bug: the malloc above runs **before** that branch — so even files that will be uploaded multipart get fully loaded into RAM first to compute the SigV4 payload hash and to slice into parts. The multipart path doesn't help.

**The SigV4 constraint.** S3 SigV4 requires `x-amz-content-sha256: <full-payload-sha256>` in the auth header for single-PUT uploads. To compute that hash we need to read the entire file once. We do **not** need to hold all of it in RAM simultaneously — we can stream the file through a SHA-256 incremental hasher, then stream it again into the request body. SigV4 also supports `x-amz-content-sha256: UNSIGNED-PAYLOAD` for multipart parts, which simplifies that path.

## Goals (v1)

1. **Hard cap at upload entry.** Files above `CLOUD_SYNC_MAX_UPLOAD_BYTES` are rejected with a logged error and a sync-state failure — no malloc attempted. Default cap: **2 GB** (configurable via `settings->uints.cloud_sync_max_upload_mb`, default 2048). This protects every target from runaway allocations.
2. **Streaming single-PUT path** for files small enough to PUT but too large to comfortably hold in RAM. Uses a fixed 64 KB scratch buffer. Two passes through the file: pass 1 computes the payload SHA-256 (required by SigV4); pass 2 streams the body into the HTTP request. The 64 KB buffer is the only allocation regardless of file size.
3. **Multipart path verified streaming.** Audit the existing multipart code to confirm it doesn't malloc full parts. If it does, fix it — each part should also use the 64 KB streaming pattern.
4. **Same cap applied to WebDAV and Google Drive** in v1. The pattern is the same; the cap is uniform across all three back-ends. Streaming-read path for WebDAV / Google Drive is **v1.1**.

## Non-goals (v1)

- **Resumable uploads.** S3 multipart already supports resume via the `UploadId` token but this spec doesn't expose it as a user feature. Network-failure mid-upload restarts from scratch in v1.
- **Bandwidth throttling.** Out of scope. Uploads run as fast as the network allows.
- **Compression.** No client-side gzip / deflate. Save state files are usually already compressed binary; the round-trip cost isn't worth it.
- **Per-file size warnings before upload.** v1 fails the upload at the cap; v1.1 could surface a "this file is X MB, continue?" pre-flight prompt.
- **Multipart-on-WebDAV-equivalent.** WebDAV doesn't have a native multipart equivalent (CHUNKED transfer-encoding is HTTP-level, not WebDAV-level). v1 caps WebDAV uploads at the same 2 GB cap; streaming-read for WebDAV is v1.1.

## Architecture

```
                                        ┌──────────────────────┐
   s3_update(path, file, cb)            │ settings.            │
        │                                │ cloud_sync_max_      │
        ├── stat(file) → file_size       │ upload_mb (= 2048)   │
        │                                └──────────────────────┘
        ├── if file_size > MAX → log+fail+cb(-1) ── no malloc
        │
        ├── if file_size > multipart_threshold:
        │     │
        │     ├── start_multipart  → UploadId
        │     ├── for each part:
        │     │     ├── seek + sha256 over part bytes (no full-part malloc)
        │     │     ├── stream PUT part body (64 KB scratch)
        │     │     └── collect ETag
        │     └── complete_multipart
        │
        └── else if file_size > STREAMING_THRESHOLD (e.g. 64 KB):
              │
              ├── pass 1: streaming sha256
              │     ├── 64 KB scratch
              │     └── filestream_read → mbedtls_sha256_update
              │
              └── pass 2: stream PUT body
                    ├── HTTP request with x-amz-content-sha256 = the hash
                    └── 64 KB scratch fed via custom write callback
```

For files smaller than `STREAMING_THRESHOLD` (= 64 KB), the existing single-`malloc(file_size)` path is fine — the buffer is bounded and small. Don't refactor the cheap path.

## Decision points

### D1 — Cap size

- **(A) 2 GB.** Matches AWS's single-PUT object size cap. Above 2 GB, S3 *requires* multipart anyway (the multipart path handles up to 5 TB). Anything above 2 GB at the cap means the user has misconfigured something — a save state shouldn't be 2 GB.
- **(B) 100 MB.** Save states are normally <10 MB; 100 MB is plenty of headroom. Anything above is a user mistake. Pros: faster failure on misconfig. Cons: legitimate large state from emulators with rich savestate (3DS, PS2 with mem-card swap) might be 50–500 MB.
- **(C) Configurable, default 2 GB.** User can lower via `settings->uints.cloud_sync_max_upload_mb`.

**Recommendation:** (C) with default 2 GB. The default is the AWS single-PUT ceiling (matches the existing multipart threshold ceiling). Configurable means embedded targets can lower to 100 MB; desktop users with weird workflows can keep the default.

### D2 — Streaming-read scratch buffer size

- **(A) 64 KB.** Standard "large enough that syscall overhead is amortised, small enough that it fits in L2 cache" pick. mbedtls's recommended chunk for incremental SHA-256 is also 64 KB.
- **(B) 1 MB.** Larger throughput on fast disks. But also bigger memory footprint per concurrent upload.
- **(C) Same as `XFER_BUF_SIZE` (existing constant in `task_http.c` if it exists, otherwise 16 KB).** Match the existing HTTP transfer chunk size for symmetry.

**Recommendation:** (A) 64 KB. Standard pick, no benchmark needed for v1; if a future profile shows it matters, revisit.

### D3 — Two-pass SHA-256 vs. streamed-with-trailers

S3 SigV4 wants the payload hash in the auth header, **before** the body is sent. Two options:

- **(A) Two-pass.** Pass 1 reads the file once just to compute SHA-256. Pass 2 streams the body. File is read from disk twice. Simple, well-understood, no SigV4 v4-streaming wrinkles.
- **(B) Streaming-with-trailers (`STREAMING-AWS4-HMAC-SHA256-PAYLOAD`).** S3 supports a chunked-transfer mode where each chunk is signed individually with a rolling signature; the auth header says `STREAMING-...` instead of the payload hash. File is read once.

**Recommendation:** (A). One disk-read worth of latency is far cheaper than implementing the streaming-signed-chunks protocol — that protocol is an implementation hazard (rolling HMAC chain, per-chunk signature, end-of-stream marker). v1 does the simple thing. v1.1 can switch to (B) if benchmarks show the disk-read cost matters.

### D4 — Per-back-end vs. shared cap

- **(A) One cap (`cloud_sync_max_upload_mb`) applied across S3, WebDAV, Google Drive.**
- **(B) Per-back-end caps** (`s3_max_upload_mb`, `webdav_max_upload_mb`, etc.).

**Recommendation:** (A). The cap is about **client-side OOM protection**, not about back-end-specific limits. The Google Drive API caps individual files at 5 TB (effectively unlimited for our use case); S3 single-PUT caps at 2 GB / multipart at 5 TB; WebDAV is server-defined. The user-meaningful cap is "what my client can upload without crashing."

## Data model changes

### `configuration.h`

```c
struct
{
   /* ... */
   unsigned cloud_sync_max_upload_mb;   /* 0 = unlimited; default 2048 */
   /* ... */
} uints;
```

### `network/cloud_sync/cloud_sync.h` (new constant header — or inline in s3.c)

```c
#define CLOUD_SYNC_STREAMING_THRESHOLD     (64 * 1024)
#define CLOUD_SYNC_STREAMING_SCRATCH_SIZE  (64 * 1024)
```

### `network/cloud_sync/s3.c`

`s3_update`:
1. Add cap check after `file_size = filestream_tell(file)`:

```c
{
   size_t cap_mb = settings->uints.cloud_sync_max_upload_mb;
   if (cap_mb && file_size > (size_t)cap_mb * 1024 * 1024)
   {
      RARCH_ERR(S3_PFX "File '%s' exceeds upload cap (%zu MiB > %u MiB)\n",
            path, file_size / (1024 * 1024), cap_mb);
      goto cleanup;   /* cb signals failure via the existing path */
   }
}
```

2. Refactor the `if (file_size > 0) { malloc(file_size); ... }` block:

```c
if (file_size > CLOUD_SYNC_STREAMING_THRESHOLD)
{
   /* Streaming path — compute hash in pass 1, body in pass 2.
    * No full-file allocation. */
   if (!s3_compute_payload_sha256_streaming(file, file_size, payload_hash_out))
      goto cleanup;
   filestream_seek(file, 0, SEEK_SET);
   file_data = NULL;   /* sentinel: streaming path, request layer reads
                        * directly from `file` via callback */
}
else
{
   /* Small file — keep the existing cheap malloc path */
   file_data = malloc(file_size);
   if (!file_data)
      goto cleanup;
   filestream_read(file, file_data, file_size);
}
```

3. Pass either `file_data` (small path) or the `RFILE *` (streaming path) into the HTTP request layer. The HTTP request layer needs a new variant that pulls bytes from a callback rather than a pre-built buffer — see next section.

### `tasks/task_http.c` (or `network/net_http.c`, wherever the request is built)

Add a new request variant:

```c
typedef bool (*http_body_pull_cb_t)(void *userdata, char *buf, size_t want, size_t *got);

struct http_request_streaming
{
   /* existing fields ... */
   http_body_pull_cb_t pull;
   void *pull_userdata;
   size_t total_size;          /* Content-Length */
};
```

The pull callback fills `buf` with up to `want` bytes, returning the actual count in `*got`. End-of-stream is `*got == 0` with return true; error is return false.

The implementation hooks into the existing libcurl-or-mbedtls request loop wherever the body is currently `memcpy`'d from a pre-built buffer. Replace that with a `pull(buf, scratch_size, &got)` loop that drives `mbedtls_ssl_write` (or curl `WRITEFUNCTION` if libcurl is the active back-end).

### `s3_compute_payload_sha256_streaming` (new helper)

```c
static bool s3_compute_payload_sha256_streaming(
      RFILE *file, size_t file_size, char *out_hex_64)
{
   uint8_t scratch[CLOUD_SYNC_STREAMING_SCRATCH_SIZE];
   uint8_t digest[32];
   mbedtls_sha256_context ctx;
   size_t remaining = file_size;

   filestream_seek(file, 0, SEEK_SET);
   mbedtls_sha256_init(&ctx);
   mbedtls_sha256_starts_ret(&ctx, 0);

   while (remaining > 0)
   {
      size_t want = remaining < sizeof(scratch) ? remaining : sizeof(scratch);
      ssize_t got = filestream_read(file, scratch, want);
      if (got <= 0)
      {
         mbedtls_sha256_free(&ctx);
         return false;
      }
      mbedtls_sha256_update_ret(&ctx, scratch, (size_t)got);
      remaining -= (size_t)got;
   }

   mbedtls_sha256_finish_ret(&ctx, digest);
   mbedtls_sha256_free(&ctx);

   /* Hex-encode digest into out_hex_64 (64 chars + null). */
   for (size_t i = 0; i < 32; i++)
      snprintf(out_hex_64 + i * 2, 3, "%02x", digest[i]);
   return true;
}
```

### `network/cloud_sync/webdav.c` and `google_drive.c`

Apply only the cap check in v1. The streaming-read refactor happens in v1.1 once the s3.c pattern is validated in production.

```c
/* near the top of the upload function */
{
   size_t cap_mb = settings->uints.cloud_sync_max_upload_mb;
   if (cap_mb && file_size > (size_t)cap_mb * 1024 * 1024)
   {
      RARCH_ERR("[<backend>] File '%s' exceeds upload cap...\n", ...);
      goto cleanup;
   }
}
```

### `menu/menu_setting.c`

New setting under *Settings → Cloud Sync → Advanced → Max Upload Size (MB)*: integer, default 2048, range [0, 8192]. 0 = unlimited.

## Test plan

### Manual conformance

1. **Cap rejection.** Set `cloud_sync_max_upload_mb = 100`. Upload a 200 MB file. Expect: `RARCH_ERR("File 'X' exceeds upload cap")` in log, sync UI shows failure, no malloc was attempted (verify with `valgrind --massif` or a pre/post `mallinfo` in test harness).
2. **Cap-disabled.** Set `cloud_sync_max_upload_mb = 0` (unlimited). Upload the same 200 MB file. Expect: success.
3. **Streaming path.** Upload a 50 MB file. Expect: `valgrind --massif` shows peak RSS rise of ~64 KB (the scratch), not 50 MB. Hash matches independent `sha256sum`.
4. **Small-file path (cheap).** Upload a 32 KB file. Expect: existing `malloc(32 K)` codepath, no streaming overhead. Round-trip diff is byte-identical.
5. **Multipart streaming verification.** Upload a 1 GB file (well above multipart threshold). Expect: peak RSS rise ≈ scratch size, not 1 GB. (If today's multipart code allocates full parts, this test fails until that's fixed too — a finding to fold into the bundle.)

### Build verification

`make -j$(nproc) retroarch` clean. Both with and without `HAVE_CLOUDSYNC` to verify the non-cloud-sync build is unchanged.

### Regression

- The existing single-PUT cheap path (file < 64 KB) must produce a byte-identical S3 PUT to pre-patch — same headers, same body, same SigV4 hash.
- The existing multipart path's behaviour up to the cap must be unchanged for files that fit the existing thresholds.
- WebDAV and Google Drive uploads under the cap behave identically pre/post-patch (only the cap check is new on those paths).

## Risks

- **Two-pass disk read cost.** For very large files on slow storage (e.g. SD card on Pi-class hardware), reading the file twice is double the latency. Mitigation: this is the v1 trade-off; v1.1 can switch to streaming-signed-chunks (D3 option B). Document the trade-off in the spec and the release note.
- **Pull-callback failure mid-upload.** If `filestream_read` returns less than expected mid-pass-2, the request body is truncated; S3 sees a Content-Length mismatch. Mitigation: pre-compute file size; if a read returns short, treat as fatal, abort the request (the server will reject the truncated body anyway).
- **Multipart audit may surface its own bugs.** If today's multipart code mallocs full parts, that's a separate fix bundled with this. Estimate +0.5 day.
- **`mbedtls_sha256_*_ret` may not be the API name in the vendored mbedtls.** Older mbedtls versions use `mbedtls_sha256_update` (no `_ret` suffix). Check the vendored copy first.
- **The HTTP request layer may not have a clean place to plug a pull callback.** If `task_http.c` builds the entire request as a single `memcpy(req_body, file_data, file_size)`, the streaming refactor touches the HTTP layer too. Estimate +0.5 day if the plumbing is uglier than expected.

## Implementation phases

1. **Phase 1 (0.5 day) — cap-only.** Add the setting, add the cap check at every cloud-sync upload entry (s3.c, webdav.c, google_drive.c). Verify test 1 + 2. Commit. This alone closes the indie-review MEDIUM.
2. **Phase 2 (0.5 day) — streaming SHA-256 helper.** Add `s3_compute_payload_sha256_streaming`, refactor s3.c's small-vs-streaming branch. Don't yet plumb pull-callback into the HTTP layer — instead, in this phase, the streaming path still mallocs the body but at least the SHA-256 is computed without doubling memory. Verify test 3 (peak RSS = file_size + 64 KB, not 2× file_size). Commit.
3. **Phase 3 (1 day) — pull callback into HTTP layer.** Plumb the streaming pull callback into `task_http.c` / `net_http.c`. Refactor s3.c streaming path to use it. Verify test 3 (peak RSS = scratch, not file_size). Commit.
4. **Phase 4 (0.5 day) — multipart audit.** Read the existing multipart code; if it mallocs full parts, fix it to use the same scratch streaming pattern. Verify test 5. Commit.

Phase 1 alone already addresses the OOM indie-review concern (the cap prevents the unbounded malloc); phases 2–4 are the streaming infrastructure that makes the cap less likely to bite legitimate users. The bundle could ship Phase 1 standalone if streaming-refactor risk needs to be deferred.

## External references

- **AWS S3 SigV4 signing.** *Authenticating Requests (AWS Signature Version 4).* `x-amz-content-sha256` header semantics. Streaming variant: *Signature Calculations for the Authorization Header: Transferring Payload in Multiple Chunks (Chunked Upload)*.
- **AWS S3 Multipart Upload.** *Uploading and copying objects using multipart upload.* Per-part 5 MB minimum / 5 GB maximum, 10,000 parts max — relevant if the multipart audit (Phase 4) finds bugs.
- **WebDAV (RFC 4918).** No native multipart; HTTP `Transfer-Encoding: chunked` is the closest equivalent and is HTTP-layer, not WebDAV-layer.
- **Google Drive API v3.** *Files: create — Resumable uploads.* The Drive API has a native resumable-upload primitive (`uploadType=resumable`) that's strictly better than chunked PUT for cloud sync; v1.1 should consider it.
- **mbedtls SHA-256 API.** `include/mbedtls/sha256.h` of the vendored copy. Confirm `_ret`-suffix vs. legacy unsuffixed API.

## Spec status

Draft — awaiting:
1. **D1:** cap default — 2 GB (recommend) vs. 100 MB.
2. **D2:** scratch size — 64 KB (recommend) is fine unless an existing constant should be reused.
3. **D3:** two-pass vs. streaming-signed-chunks. Recommend two-pass for v1.
4. **D4:** one cap or per-back-end. Recommend one cap.
5. **Phase split:** ship Phase 1 alone first (immediate OOM relief) then Phases 2–4 as a follow-up bundle? Or ship 1–4 as one bundle?
