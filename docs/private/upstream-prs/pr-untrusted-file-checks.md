Title: security: validate replay, UPnP and cloud sync input

## Problem

Three paths trust data from outside RetroArch:

- `replay_set_serialized_data` takes the embedded replay length from the save state. A negative length, or one shorter than the replay header, reaches `intfstream_write`/`intfstream_seek`/`intfstream_truncate`.
- `natt_parse_desc_node` parses the router's IGD description. On a node without children it reads `child->next` with `child == NULL`, so a leaf node or empty root segfaults. It also only recurses into childless nodes, so the normal nested `<service>` is never found.
- `task_cloud_sync_fetch_server_file` takes the local path as `strchr(key, '/') + 1` from a server-supplied key. A key without `/` reads address 1; a key with `..` or an absolute path is joined onto the sync directory, letting a hostile server write outside it.

## Fix

- Refuse replay states whose length is below the header size.
- Always recurse into children; NULL-check `serviceType`/`controlURL` text.
- Refuse unsafe keys and count them as a sync failure.

The parser moves to `network/natt_desc.c` and the key check to `tasks/task_cloudsync_path.c`, so two new sample tests compile the shipped code. Both are added to `Linux-samples-tasks.yml`.

## Testing

- Linux: full build; touched files compile with `C89_BUILD=1`, no warnings.
- Ran the workflow commands for both new steps: pass. The existing cloudsync test still passes under ASan.
- With the old parser or old key handling put back, each test segfaults.

Not addressed: a replay length larger than the actual buffer, since this function is not given the buffer size.

This change was found and written with the help of Claude Code (an AI assistant), then reviewed and build-tested on a fork. Happy to adjust anything.
