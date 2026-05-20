# Cloud Sync — Streaming Upload (Design)

**Date:** 2026-04-27
**Source:** `docs/private/ROADMAP.md` — indie-review MEDIUM, "cloud_sync_update reads entire file into RAM" entry (section anchor; body line numbers churn).
**Status:** draft — **Phase 1 SHIPPED 2026-04-29 (Bundle 32, commit `c3ebdb11e4`).** Phases 2–4 still draft. Cold-eyes 2026-05-18 corrections folded in via Bundle 73 (see banner below; the load-bearing new pieces are the `## Failure modes` section, the `## Cross-spec dependency` note, the multipart Phase-4 re-sizing, and the function-name re-anchoring of all `s3.c` cites).
**Target:** `local/fixes-2026-04` (Phases 2–4)
**Effort estimate:** Phase 1 done; Phases 2–4 ~2 days (Phase 2 ~0.5, Phase 3 ~1, Phase 4 ~1 — Phase 4 revised up from 0.5 once the multipart-state shape was confirmed to hold the whole file, see banner item 4).

> **⚠️ Cold-eyes 2026-05-18 status update (Bundle 73 fold-in).**
>
> A cold-eyes pass against current source flagged nine staleness + factual issues. All nine are now folded into the spec body (sections below). The spec stays as the historical design record; the corrections are applied — read the body, not the pre-correction prose this banner summarises.
>
> 1. ✅ **Phase 1 has shipped.** Bundle 32 (commit `c3ebdb11e4`) added `cloud_sync_max_upload_mb` to `configuration.h`, `DEFAULT_CLOUD_SYNC_MAX_UPLOAD_MB = 2048` to `config.def.h`, the load/save wiring in `configuration.c`, and the menu UI at `menu/menu_setting.c` (`MENU_ENUM_LABEL_CLOUD_SYNC_MAX_UPLOAD_MB`). The cap-check landed at the **dispatch layer** (`cloud_sync_update` in `network/cloud_sync_driver.c` — wraps every backend) rather than the per-backend layout this spec proposed. The dispatch-layer pattern is strictly better — one site serves all three backends. Goal 1, the Data-model cap blocks, and Phase 1 are now marked **SHIPPED (dispatch-layer)**; the per-backend cap snippets are flagged SUPERSEDED.
> 2. ✅ **All `s3.c` line cites re-anchored to function names.** Spec previously pinned `s3.c:1807` (`s3_update`), `:1838` (malloc), `:1860+` (multipart). Those drift ~130 lines per audit cycle (Bundles 17/26/32/52/56/63 grew the file; today they are 1941/1972/1994). The body now keys against `s3_update` / `s3_sha256_hash` / `s3_multipart_*` by function name with no line numbers.
> 3. ✅ **mbedtls SHA-256 API corrected to the *unsuffixed legacy* shape.** The body previously proposed `mbedtls_sha256_starts_ret` / `_update_ret` / `_finish_ret`. Vendored `deps/mbedtls/mbedtls/sha256.h` declares **only** `mbedtls_sha256_starts(ctx, is224)`, `_update(ctx, in, ilen)`, `_finish(ctx, out)` — the helper now uses the unsuffixed names. The new helper extends the existing `s3_sha256_hash(const char *data, size_t len)` (one-shot) with an incremental sibling rather than introducing a parallel helper.
> 4. ✅ **Multipart re-sized — definite refactor, not audit.** Verified: `s3_multipart_state_t::file_data` carries the **entire file buffer** for the duration of every part; each part SHA + body reads via `mp_st->file_data + offset` (`s3_sha256_hash(mp_st->file_data + offset, content_len)`). Multipart carries the full *file*, not full *parts*. Phase 4 is a definite refactor with known shape (mp state needs `RFILE *` + offset, seek per part) and is re-estimated to ~1 day.
> 5. ✅ **`## Failure modes` section added.** Covers network drop mid-upload, S3 5xx retry budget + backoff bound, OAuth-token expiry, quota exhaustion (HTTP 507), conflict resolution on stale-remote.
> 6. ✅ **Numeric SLOs added.** Max heap inflation per concurrent upload (256 KB desktop / 128 KB console-class) + 5xx retry/backoff bound, pinned in `## Failure modes`.
> 7. ✅ **TLS-spec dependency cross-linked.** New `## Cross-spec dependency` note: TLS verification (sibling spec) MUST be `required` or `optional` before this work hardens privacy claims; streaming changes the data-at-risk shape (incremental vs one-shot) but not the auth surface.
> 8. ✅ **Third D3 option added.** D3 now lists `x-amz-content-sha256: UNSIGNED-PAYLOAD` (HTTPS single-PUT) + `Content-MD5` for integrity as option (C) — one-pass, no SigV4-chunk complexity — alongside two-pass (A) and streaming-signed-chunks (B).
> 9. ✅ **Settings-UI placement reconciled.** Ground-truth check: the shipped cap lives on the dedicated **Cloud Sync** page (`SETTINGS_LIST_CLOUD_SYNC`), reached from **Settings → Saving → Cloud Sync** (`DISPLAYLIST_SAVING_SETTINGS_LIST` parents `MENU_ENUM_LABEL_CLOUD_SYNC_SETTINGS`) — not "Cloud Sync → Advanced" and not under Network. The body now cites the real path and explains why this and the TLS spec's *Network → Advanced* placement are domain-scoped, not in conflict.
>
> These corrections were tracked in `docs/private/ROADMAP.md` under the cold-eyes-2026-05-18 fold-in block; resolved in Bundle 73.

---

## Summary

`network/cloud_sync/s3.c::s3_update` reads the entire local file into a single `malloc(file_size)` buffer before signing and uploading. The buffer is sized to whatever the local file is — a 5 GB save state on a 256 MB-class console means an OOM crash; even on desktop, a multi-GB upload blocks the runloop on the malloc + the `filestream_read`. The **multipart-upload** path (`s3_multipart_*`) is *worse*, not better: `s3_multipart_state_t::file_data` holds the **whole file** for the entire upload, and each part hashes/streams via `mp_st->file_data + offset` — so choosing multipart does not bound memory, it just slices an already-resident buffer.

> **Cite stability.** All `s3.c` references in this spec are by **function name** (`s3_update`, `s3_sha256_hash`, `s3_multipart_*`), not line number — the file grows ~130 lines per audit cycle and absolute line cites rot. Re-grep by symbol when implementing.

This spec defines:

1. A **hard size cap** (per-file refusal threshold) that protects memory-constrained targets from OOM and unhappy-path desktop users from runaway uploads. **(SHIPPED — Bundle 32, dispatch layer.)**
2. A **streaming-read path** for files between "small enough to fit in one buffer" and "large enough to trip multipart" that doesn't require holding the whole file in RAM. *(Phases 2–3.)*
3. A **multipart streaming refactor** — confirmed necessary, not just an audit: the multipart path holds the whole file and must be reworked to seek per part from an `RFILE *`. *(Phase 4.)*

The same shape exists in `webdav.c` and `google_drive.c` — this spec covers the S3 path concretely and recommends the WebDAV / Google Drive scope as a v1.1 extension once the S3 cap pattern is validated.

## Current behaviour

```c
/* network/cloud_sync/s3.c — s3_update entry */
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

1. **Hard cap at upload entry. ✅ SHIPPED (Bundle 32, dispatch layer).** Files above the cap are rejected with a logged error and a sync-state failure — no malloc attempted. Default cap: **2 GB** (configurable via `settings->uints.cloud_sync_max_upload_mb`, default 2048). The check landed once in `cloud_sync_update` (`network/cloud_sync_driver.c`), which wraps every backend, rather than per-backend — so this single goal also satisfies the old Goal 4 below.
2. **Streaming single-PUT path** for files small enough to PUT but too large to comfortably hold in RAM. Uses a fixed 64 KB scratch buffer. Two passes through the file: pass 1 computes the payload SHA-256 (required by SigV4); pass 2 streams the body into the HTTP request. The 64 KB buffer is the only allocation regardless of file size. *(Phases 2–3.)*
3. **Multipart streaming refactor — confirmed required.** Not an audit: `s3_multipart_state_t::file_data` holds the whole file. Rework to seek per part from an `RFILE *` so each part uses the 64 KB streaming pattern. *(Phase 4.)*
4. **~~Same cap applied to WebDAV and Google Drive in v1.~~ Subsumed by Goal 1.** The dispatch-layer cap already covers all three back-ends from one site. Streaming-read path for WebDAV / Google Drive remains **v1.1**.

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
- **(C) `UNSIGNED-PAYLOAD` + `Content-MD5`.** S3 accepts `x-amz-content-sha256: UNSIGNED-PAYLOAD` for HTTPS single-PUT — the payload is not folded into the SigV4 signature at all, so no payload SHA-256 is needed up front. Integrity is preserved by sending a `Content-MD5` header (computed in one streaming pass) which S3 verifies against the received body. One disk read, no rolling-HMAC machinery. The trade-off: relies on TLS for confidentiality (already required — see `## Cross-spec dependency`) and on `Content-MD5` rather than the request signature for integrity.

**Recommendation:** (A) for v1 — it's the least surprising and keeps the request shape byte-identical to today's signed-payload PUT, which the regression tests assume. **(C) is the strongest v1.1 candidate** (one disk read, far less code than (B)); revisit it once the two-pass path is proven and the regression baseline can absorb the header change. (B) stays a last resort — the rolling HMAC chain / per-chunk signature / end-of-stream marker is an implementation hazard not worth it unless a profile demands single-pass *and* signed payload.

### D4 — Per-back-end vs. shared cap

- **(A) One cap (`cloud_sync_max_upload_mb`) applied across S3, WebDAV, Google Drive.**
- **(B) Per-back-end caps** (`s3_max_upload_mb`, `webdav_max_upload_mb`, etc.).

**Recommendation:** (A). The cap is about **client-side OOM protection**, not about back-end-specific limits. The Google Drive API caps individual files at 5 TB (effectively unlimited for our use case); S3 single-PUT caps at 2 GB / multipart at 5 TB; WebDAV is server-defined. The user-meaningful cap is "what my client can upload without crashing."

## Data model changes

### `configuration.h` ✅ SHIPPED (Bundle 32)

```c
struct
{
   /* ... */
   unsigned cloud_sync_max_upload_mb;   /* 0 = unlimited; default 2048 */
   /* ... */
} uints;
```

Default `DEFAULT_CLOUD_SYNC_MAX_UPLOAD_MB = 2048` in `config.def.h`; load/save wiring in `configuration.c`.

### `network/cloud_sync/cloud_sync.h` (new constant header — or inline in s3.c)

```c
#define CLOUD_SYNC_STREAMING_THRESHOLD     (64 * 1024)
#define CLOUD_SYNC_STREAMING_SCRATCH_SIZE  (64 * 1024)
```

### `network/cloud_sync/s3.c`

1. ~~Add cap check after `file_size = filestream_tell(file)`.~~ **SUPERSEDED — shipped one layer up.** Bundle 32 placed the cap in the dispatch wrapper `cloud_sync_update` (`network/cloud_sync_driver.c`), so it fires before `s3_update`/`webdav`/`google_drive` are ever called. Do **not** add a per-backend cap. (Original per-backend snippet kept below for historical reference only.)

```c
/* SUPERSEDED — this is now done in cloud_sync_update, not here. */
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

2. Refactor the `if (file_size > 0) { malloc(file_size); ... }` block (Phases 2–3):

```c
if (file_size > CLOUD_SYNC_STREAMING_THRESHOLD)
{
   /* Streaming path — compute hash in pass 1, body in pass 2.
    * No full-file allocation. */
   if (!s3_sha256_hash_file(file, file_size, payload_hash_out))
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

### Streaming SHA-256 — extend `s3_sha256_hash`, don't fork it

`s3.c` already has a working one-shot helper:

```c
static char* s3_sha256_hash(const char *data, size_t len);   /* existing */
```

Add an **incremental sibling** that hashes straight from an `RFILE *` without a full-file buffer, sharing the same hex-encode tail. Don't introduce a parallel `s3_compute_payload_sha256_streaming` — the two should be one family.

> **mbedtls API note.** The vendored `deps/mbedtls/mbedtls/sha256.h` declares the **unsuffixed legacy** API only: `mbedtls_sha256_starts(ctx, is224)`, `mbedtls_sha256_update(ctx, in, ilen)`, `mbedtls_sha256_finish(ctx, out)`. The `_ret`-suffixed names (from newer upstream mbedtls) are **not** present — use the unsuffixed forms.

```c
/* Incremental sibling of s3_sha256_hash: hash a file in 64 KB chunks,
 * no full-file allocation. Writes a 64-char lowercase hex digest + NUL. */
static bool s3_sha256_hash_file(RFILE *file, size_t file_size, char *out_hex_65)
{
   uint8_t scratch[CLOUD_SYNC_STREAMING_SCRATCH_SIZE];
   uint8_t digest[32];
   mbedtls_sha256_context ctx;
   size_t remaining = file_size;

   filestream_seek(file, 0, SEEK_SET);
   mbedtls_sha256_init(&ctx);
   mbedtls_sha256_starts(&ctx, 0);              /* unsuffixed legacy API */

   while (remaining > 0)
   {
      size_t want = remaining < sizeof(scratch) ? remaining : sizeof(scratch);
      int64_t got = filestream_read(file, scratch, want);
      if (got <= 0)
      {
         mbedtls_sha256_free(&ctx);
         return false;
      }
      mbedtls_sha256_update(&ctx, scratch, (size_t)got);   /* unsuffixed */
      remaining -= (size_t)got;
   }

   mbedtls_sha256_finish(&ctx, digest);          /* unsuffixed */
   mbedtls_sha256_free(&ctx);

   /* Hex-encode digest into out_hex_65 (64 chars + NUL). Factor the
    * hex tail out of s3_sha256_hash so both helpers share it. */
   {
      size_t i;
      for (i = 0; i < 32; i++)
         snprintf(out_hex_65 + i * 2, 3, "%02x", digest[i]);
   }
   return true;
}
```

(Variable declarations are at block top and the `for` index is declared separately to stay C89-clean per the project coding rules.)

### `network/cloud_sync/webdav.c` and `google_drive.c`

~~Apply the cap check in v1.~~ **SUPERSEDED — no per-backend cap needed.** The dispatch-layer cap in `cloud_sync_update` already gates these two backends. v1.1 still owns the *streaming-read* refactor for WebDAV / Google Drive once the s3.c pattern is validated in production — but the OOM-cap part is done for all three.

### `menu/menu_setting.c` ✅ SHIPPED (Bundle 32)

Setting lives on the dedicated **Cloud Sync** page (`SETTINGS_LIST_CLOUD_SYNC`, `MENU_ENUM_LABEL_CLOUD_SYNC_MAX_UPLOAD_MB`) — *Max Upload Size (MB)*, integer, default 2048. Reached via **Settings → Saving → Cloud Sync** (`DISPLAYLIST_SAVING_SETTINGS_LIST` parents `MENU_ENUM_LABEL_CLOUD_SYNC_SETTINGS`), 0 = unlimited. See `## Cross-spec dependency` for why this sits under *Saving → Cloud Sync* while the sibling TLS spec's verify-mode sits under *Network → Advanced*.

## Failure modes

The cap (Phase 1) handles the OOM case at entry. These are the runtime failures the streaming path (Phases 2–4) must handle, with the expected log line and user-visible behaviour per mode.

| Failure | Trigger | Behaviour | Expected log | Recovery |
|---|---|---|---|---|
| **Network drop mid-upload** | TCP reset / timeout during pass 2 (single-PUT) or a part PUT (multipart) | Abort the in-flight request, fail the sync-state, leave remote unchanged. No partial object is committed (S3 single-PUT is atomic; multipart parts aren't visible until `complete_multipart`). | `RARCH_ERR(S3_PFX "Upload failed for '%s': network error\n", path)` | v1: restart from scratch on next sync (per Non-goals — no resume). Multipart `UploadId` is abandoned; rely on S3 lifecycle rule to GC incomplete uploads. |
| **S3 5xx (throttling / transient)** | `503 SlowDown`, `500 InternalError` | Retry with exponential backoff. **Budget: max 4 retries, base 500 ms, ×2 each, cap 8 s, total ≤ ~15 s.** After budget exhausted, fail the sync-state. | `RARCH_WARN(S3_PFX "5xx from S3 (%d), retry %d/4 in %d ms\n", code, n, delay)` then `RARCH_ERR` on exhaustion | Automatic within budget; user-visible failure only after exhaustion. |
| **OAuth-token expiry mid-upload** (Google Drive) | 401 partway through a long upload because the access token TTL elapsed | Refresh the token and retry the *current* request once; if refresh fails, fail the sync-state. | `RARCH_WARN("[GDrive] token expired mid-upload, refreshing\n")` | **v1.1** — v1 Google Drive uploads are capped and short enough that mid-upload expiry is rare; documented as a known v1 gap. |
| **Quota exhaustion** | `HTTP 507 Insufficient Storage` (WebDAV) / `403 quotaExceeded` (Drive) / `QuotaExceeded` (S3 is effectively unbounded) | Fail-closed immediately, no retry — quota errors don't clear on retry. | `RARCH_ERR("[<backend>] Upload rejected: storage quota exhausted\n")` | User must free remote space; surface a distinct sync-state so the menu can show "quota full" rather than a generic failure. |
| **Stale-remote conflict** | Remote object's ETag/mtime is newer than the local base the sync planned against | Defer to the existing cloud-sync conflict path (`cloud_sync_destructive` setting + the keep-local / keep-server resolve commands already in `cloud_sync_driver`). Streaming changes *how* bytes move, not the conflict policy. | (existing conflict logging) | User picks keep-local / keep-server via the existing resolve menu entries. |

### Numeric SLOs

- **Max heap inflation per concurrent upload:** **256 KB on desktop, 128 KB on console-class** (down from the current `file_size`). This is the streaming-path target — the 64 KB scratch plus request/header overhead, independent of file size. Multipart (post-Phase-4) holds the same budget by seeking per part instead of buffering the file.
- **5xx retry/backoff bound:** ≤ 4 retries, total wall-clock ≤ ~15 s before a hard fail (see table). Bounds the worst-case stall on the task thread.
- **Two-pass disk-read cost:** pass 1 (SHA-256) + pass 2 (body) read the file twice; acceptable for v1 (see D3 / Risks). No numeric latency SLO pinned — revisit only if a profile shows it matters on slow storage.

## Cross-spec dependency

This work streams credentials (Google OAuth tokens, AWS SigV4-signed headers, WebDAV basic-auth) over a TLS pipe. It therefore depends on the sibling **`2026-04-27-tls-verification-opt-in-design.md`**:

> **TLS verification (sibling spec) MUST be `required` or `optional` before this work hardens any privacy claim.** Streaming changes the *data-at-risk shape* (the credential is now sent incrementally across many `mbedtls_ssl_write` calls rather than one buffered request body) but **not** the auth surface — the same headers, the same secrets, over the same socket. If TLS verification is `disabled`, neither the one-shot nor the streaming path is safe against MITM; the streaming refactor neither helps nor hurts that. Land the TLS default-on change first, then any "uploads are protected" wording in release notes is true.

**Settings-UI placement (reconciled).** The two specs put their settings in different menu trees, and that is correct, not a conflict:

- This spec's cap (`cloud_sync_max_upload_mb`) is **cloud-sync-specific**, so it lives on the dedicated Cloud Sync page — **Settings → Saving → Cloud Sync → Max Upload Size** (verified: `DISPLAYLIST_SAVING_SETTINGS_LIST` parents `MENU_ENUM_LABEL_CLOUD_SYNC_SETTINGS`; the setting is `MENU_ENUM_LABEL_CLOUD_SYNC_MAX_UPLOAD_MB`).
- The TLS spec's `tls_verify_mode` is **network-wide** (every HTTPS connection, not just cloud sync), so it lives under **Settings → Network → Advanced → TLS Verification**.

The convention: a setting goes on the page that owns its domain. Neither spec should move its setting to match the other — there is no flat shared page that owns both.

## Test plan

### Manual conformance

1. **Cap rejection.** Set `cloud_sync_max_upload_mb = 100`. Upload a 200 MB file. Expect: `RARCH_ERR("File 'X' exceeds upload cap")` in log, sync UI shows failure, no malloc was attempted (verify with `valgrind --massif` or a pre/post `mallinfo` in test harness).
2. **Cap-disabled.** Set `cloud_sync_max_upload_mb = 0` (unlimited). Upload the same 200 MB file. Expect: success.
3. **Streaming path.** Upload a 50 MB file. Expect: `valgrind --massif` shows peak RSS rise of ~64 KB (the scratch), not 50 MB. Hash matches independent `sha256sum`.
4. **Small-file path (cheap).** Upload a 32 KB file. Expect: existing `malloc(32 K)` codepath, no streaming overhead. Round-trip diff is byte-identical.
5. **Multipart streaming verification.** Upload a 1 GB file (well above multipart threshold). Expect (after Phase 4): peak RSS rise ≈ scratch size, not 1 GB. Pre-Phase-4 this test fails by design — today's multipart holds the whole file in `s3_multipart_state_t::file_data`; the test is the acceptance gate for the Phase 4 refactor.

### Build verification

`make -j$(nproc) retroarch` clean. Both with and without `HAVE_CLOUDSYNC` to verify the non-cloud-sync build is unchanged.

### Regression

- The existing single-PUT cheap path (file < 64 KB) must produce a byte-identical S3 PUT to pre-patch — same headers, same body, same SigV4 hash.
- The existing multipart path's behaviour up to the cap must be unchanged for files that fit the existing thresholds.
- WebDAV and Google Drive uploads under the cap behave identically pre/post-patch (only the cap check is new on those paths).

## Risks

- **Two-pass disk read cost.** For very large files on slow storage (e.g. SD card on Pi-class hardware), reading the file twice is double the latency. Mitigation: this is the v1 trade-off; v1.1 can switch to streaming-signed-chunks (D3 option B). Document the trade-off in the spec and the release note.
- **Pull-callback failure mid-upload.** If `filestream_read` returns less than expected mid-pass-2, the request body is truncated; S3 sees a Content-Length mismatch. Mitigation: pre-compute file size; if a read returns short, treat as fatal, abort the request (the server will reject the truncated body anyway).
- **Multipart refactor is in-scope, not a surprise.** Confirmed: today's multipart code holds the whole file in `s3_multipart_state_t::file_data`. Phase 4 reworks it to seek per part from an `RFILE *`. The risk is the `s3_multipart_*` state machine touches several callbacks (`s3_multipart_upload_next_part`, `s3_multipart_fallback_single_put`) that read `file_data + offset` — all must move to seek-based reads together.
- **~~`mbedtls_sha256_*_ret` may not be the API name.~~ Resolved.** The vendored `deps/mbedtls/mbedtls/sha256.h` uses the unsuffixed legacy API (`mbedtls_sha256_starts/_update/_finish`); the helper above already uses those.
- **The HTTP request layer may not have a clean place to plug a pull callback.** If `task_http.c` builds the entire request as a single `memcpy(req_body, file_data, file_size)`, the streaming refactor touches the HTTP layer too. Estimate +0.5 day if the plumbing is uglier than expected.

## Implementation phases

1. **Phase 1 (DONE — Bundle 32) — cap-only.** Setting + dispatch-layer cap check in `cloud_sync_update`. Tests 1 + 2 pass. Closed the indie-review MEDIUM.
2. **Phase 2 (~0.5 day) — incremental SHA-256 helper.** Add `s3_sha256_hash_file` (incremental sibling of `s3_sha256_hash`), refactor s3.c's small-vs-streaming branch. Don't yet plumb pull-callback into the HTTP layer — in this phase the streaming path still mallocs the body, but the SHA-256 is computed without doubling memory. Verify test 3 (peak RSS = file_size + 64 KB, not 2× file_size). Commit.
3. **Phase 3 (~1 day) — pull callback into HTTP layer.** Plumb the streaming pull callback into `task_http.c` / `net_http.c`. Refactor s3.c streaming path to use it. Verify test 3 (peak RSS = scratch, not file_size). Commit.
4. **Phase 4 (~1 day) — multipart streaming refactor (confirmed, not an audit).** `s3_multipart_state_t::file_data` holds the whole file today (each part hashes/streams via `mp_st->file_data + offset`). Replace `file_data` with an `RFILE *` + per-part offset, seeking each part into the 64 KB scratch for both the part SHA and the body. This is a known-shape refactor of `s3_multipart_*`, not an investigation. Verify test 5 (peak RSS ≈ scratch on a 1 GB upload, not 1 GB). Commit.

Phase 1 (shipped) already addresses the OOM indie-review concern (the cap prevents the unbounded malloc); phases 2–4 are the streaming infrastructure that makes the cap less likely to bite legitimate users. They can ship as a follow-up bundle now that the immediate OOM relief is in place.

## External references

- **AWS S3 SigV4 signing.** *Authenticating Requests (AWS Signature Version 4).* `x-amz-content-sha256` header semantics, including the `UNSIGNED-PAYLOAD` value (D3 option C) and the `STREAMING-AWS4-HMAC-SHA256-PAYLOAD` chunked variant (D3 option B): *Signature Calculations for the Authorization Header: Transferring Payload in Multiple Chunks (Chunked Upload)*.
- **AWS S3 Multipart Upload.** *Uploading and copying objects using multipart upload.* Per-part 5 MB minimum / 5 GB maximum, 10,000 parts max — relevant if the multipart audit (Phase 4) finds bugs.
- **WebDAV (RFC 4918).** No native multipart; HTTP `Transfer-Encoding: chunked` is the closest equivalent and is HTTP-layer, not WebDAV-layer.
- **Google Drive API v3.** *Files: create — Resumable uploads.* The Drive API has a native resumable-upload primitive (`uploadType=resumable`) that's strictly better than chunked PUT for cloud sync; v1.1 should consider it.
- **mbedtls SHA-256 API.** Vendored at `deps/mbedtls/mbedtls/sha256.h` — confirmed the **unsuffixed legacy** API (`mbedtls_sha256_starts/_update/_finish`); the `_ret`-suffixed names are not present in this copy.

## Spec status

Phase 1 shipped (Bundle 32); Phases 2–4 draft.

Decisions resolved by the shipped Phase 1:
- **D1 (cap default):** ✅ 2 GB, configurable via `cloud_sync_max_upload_mb` (default 2048).
- **D4 (one cap vs. per-backend):** ✅ one cap, at the `cloud_sync_update` dispatch layer.
- **Phase split:** ✅ Phase 1 shipped standalone for immediate OOM relief; Phases 2–4 are a follow-up bundle.

Still open (Phases 2–4):
- **D2 (scratch size):** 64 KB recommended; fine unless an existing constant should be reused.
- **D3 (payload-hash strategy):** two-pass (A) recommended for v1; `UNSIGNED-PAYLOAD` + `Content-MD5` (C) is the v1.1 candidate; streaming-signed-chunks (B) last resort.
