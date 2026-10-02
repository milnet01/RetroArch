# PR draft — core_info: cheaper core match in the database scanner

Branch `pr/scan-core-match` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit on upstream/master `6bf58823c6`: `f79e1bf804` (RETR-0016). Opened
2026-10-02 as libretro/RetroArch#19678.

## Title

core_info: cheaper core match in the database scanner

## Body

`core_info_database_supports_content_path()` runs once per scanned file
per database, and its loop runs once per installed core. Inside that
loop it recomputed the file's extension with `path_get_extension()`, and
it checked each core's extension list before its much shorter database
list.

This computes the extension once before the loop and checks the
database list first. Both checks must pass for a core to match, and
neither has side effects, so the result is unchanged.

### Measurements

Measured 2026-10-01. `perf` on a `--scan` of 1522 files (NES, SNES,
GBA, Mega Drive zips) against 145 databases, with 301 core info files
installed, null drivers, files in the page cache. Before the change,
`string_list_find_elem` was 35% of samples. `strrchr`, `strchr`,
`path_get_extension` and `path_basename` were another 30%, and all four
dropped out of the profile after it.

Four alternating runs each:

| Build | CPU time (user) | Wall |
|---|---|---|
| before | 1.66-2.17 s | 2.12-2.90 s |
| after | 0.87-1.01 s | 1.36-2.43 s |

The machine had other work running, so the wall figures are noisy; CPU
time is the steadier measure. The gain grows with the number of
installed cores, so a setup with few cores sees less.

### Testing

The playlists written by the before and after builds are byte-identical
on the scan above.

Made with Claude Code, reviewed and build-tested on our fork.
