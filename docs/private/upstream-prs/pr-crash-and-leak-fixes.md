Title: misc: fix crashes and leaks in wayland, drm, replay index and drivers

## Problem

Independent crash and leak bugs on reachable paths:

- wayland: `gfx_ctx_wl_get_video_size_common` dereferences a NULL output when no window output is set and `all_outputs` is empty. The DnD drop handler leaks the buffer if `fmemopen` fails and always leaks the `getline` line.
- drm: `drm_plane_setup` logs a NULL `drmModeGetPlaneResources()` result, then dereferences it. `init_drm` reads `connector` uninitialised when there are no connectors.
- uint32s_index: `uint32s_bucket_free` leaves the freed pointer and length in place. `RHMAP_CLEAR` keeps value slots, so a later `RHMAP_PTR` can free the vector again. `uint32s_index_pop` also underflowed on an empty index and let `RHMAP_PTR` create a bucket for a missing hash.
- ozone: `ozone_context_reset_horizontal_list` allocates a node for an entry without one and never stores it.
- discord: `discord_json_next_strdup` leaks when a key repeats.
- core_backup: `core_backup_add_entry` leaks an empty filename.
- audioworklet, caca, sixel, vga: init leaks its struct when a later allocation or the font renderer fails.

## Fix

A missing return or NULL check, a reset after free, or a free on the failure path. Each hunk stands alone.

## Testing

- Linux: full build, no new warnings in the touched files. The C89 build of the Wayland and DRM files fails here in system headers and existing lines, never on a changed line.
- `drm_gfx.c` compiled by hand (`HAVE_PLAIN_DRM` is off here).
- `samples/tasks/core_backup` passes.
- Not compiled: audioworklet, caca, sixel, vga (not buildable on this host).
- None of the crash paths were reproduced at runtime.

This change was found and written with the help of Claude Code (an AI assistant), then reviewed and build-tested on a fork. Happy to adjust anything.
