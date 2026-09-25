Title: cloud_sync: refuse unframed download bodies, handle allocation failure

## Problem

`net_http` fails a transfer that ends before its Content-Length or final chunk. A response with neither ends when the connection closes, and `net_http` notes it cannot tell a complete body from a cut-off one. The WebDAV and Google Drive read callbacks accept such a body as a successful download and write it over the local save. A connection dropped mid-download can replace a save with a truncated copy, which the next sync uploads.

Several allocations in the WebDAV digest-auth chain, `webdav_delete` and `gdrive_begin_with_token` are also used without a NULL check.

## Fix

- A 2xx download without `Content-Length` or `Transfer-Encoding: chunked` is treated as failed and the local file is left alone. The check, `cloud_sync_http_body_is_framed`, uses the same header tests `net_http` uses to choose the body framing.
- The unchecked allocations now fail the operation instead of writing through NULL.

Trade-off: a server that only sends close-delimited responses to GET would now fail to sync. I have not seen one, but I can make this a warning instead if you prefer.

## Testing

- Linux: full build; touched files compile with `C89_BUILD=1`, no warnings.
- A small program linked against the built `cloud_sync_driver.o`: true for Content-Length and chunked (any case); false for no framing, `gzip, chunked` and NULL.
- Not tested against a live WebDAV or Google Drive server.

This change was found and written with the help of Claude Code (an AI assistant), then reviewed and build-tested on a fork. Happy to adjust anything.
