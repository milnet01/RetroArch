# PR draft — menu: bounds-check the selection in ozone_selection_changed and xmb's imageviewer update

Branch `pr/menu-selection-bounds` in `/mnt/Games/Scripts/Linux/ra-pr`,
one commit on upstream/master `bfb6bb0a58`: `df07c52ced` (RETR-0019). Opened
2026-10-02 as libretro/RetroArch#19691.
Fork commit `73c7a9c0e3`.

## Title

menu: bounds-check the selection in ozone_selection_changed and xmb's imageviewer update

## Body

`selection_ptr` can exceed the entry list. #18797's fixes (8cbd0f1eac,
b2610c8264) guard ozone's `compute_entries_position` and its
imageviewer thumbnail read. Two reads of the same shape are left:

- `ozone_selection_changed` reads
  `selection_buf->list[selection_ptr].userdata` unchecked. One way
  there: `ozone_tab_set_selection` restores the selection saved for a
  sidebar tab when the user left it. If the playlist shrank meanwhile,
  as in #18797's refresh-from-sidebar steps, the saved index is past
  the end.
- `xmb_set_thumbnail_content`'s imageviewer branch, the XMB twin of
  b2610c8264.

Both now treat an out-of-range selection like an entry with no node.

### Testing

Not reproduced: I found these by reading the code, not from a crash.
Linux: full `make -j4`, no new warnings.

Made with Claude Code, reviewed and build-tested on our fork.
