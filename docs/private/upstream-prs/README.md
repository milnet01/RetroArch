# Upstream contributions — drafts (RETR-S0115)

Drafted 2026-09-26 for libretro/RetroArch and libretro/libretro-common.
Both batches below are opened; RETR-S0115 records how each one ended.
Fixes that could go upstream next are under "Candidates".

## Candidates — not yet opened

Opening a PR is public under the user's account, so each needs the
user's go-ahead. Checked 2026-10-01.

| Fix | Roadmap | Where upstream stands |
|---|---|---|
| Run-ahead temp core copy: use the temp dir only if it is private | RETR-0006 | Opened 2026-10-01 as RetroArch PR #19660, fixing issue #19632. Draft: [pr-runahead-private-tmpdir.md](pr-runahead-private-tmpdir.md). |
| `filestream_write_file_atomic` loses both files when the retry fails | RETR-0009 | Issue libretro-common #233 open, no reply. The fix rides in RetroArch PR #19629 (open). |
| gl driver: upload per-draw coords with `GL_STREAM_DRAW` (performance) | RETR-0016 | Opened 2026-10-01 as RetroArch PR #19661. Measured ~15% less time a frame in the font upload pattern (RX 6600). Draft: [pr-gl-stream-draw.md](pr-gl-stream-draw.md). |
| Unix signal handler: `_exit`, not `exit`, on the second quit signal, which otherwise can hang at shutdown | RETR-0017 | Same code in upstream master; no issue or PR found 2026-10-01. Fork fix `c2a527778c`. Opened 2026-10-01 as RetroArch PR #19664 (branch `pr/sighandler-safe-exit`, `4d77bf497d`). Draft: [pr-sighandler-safe-exit.md](pr-sighandler-safe-exit.md). |
| `linked_list` remove-matching crashes on a NULL callback | RETR-0007 | Issue libretro-common #232; another contributor opened PR #234. Not ours to send. |
| WebDAV digest login: a fresh cnonce per login instead of the fixed `"1a2b3c4f"` | RETR-S0115 | Opened 2026-10-01 as RetroArch PR #19666 (branch `pr/webdav-random-cnonce`, `c76212a34d`), on upstream's `crypto_random_bytes`. Draft: [pr-webdav-random-cnonce.md](pr-webdav-random-cnonce.md). |
| Screenshot directory: `new_screenshot_dir` tested before it is written when the directory is empty | RETR-S0115 | Opened 2026-10-01 as RetroArch PR #19667 (branch `pr/screenshot-dir-init`, `bfa74954e6`). Draft: [pr-screenshot-dir-init.md](pr-screenshot-dir-init.md). |
| Core options: categories array sized by option count, so more categories than options overflows it | RETR-S0115 | Opened 2026-10-01 as RetroArch PR #19668 (branch `pr/core-option-cats-size`, `c5bb0425b1`). Draft: [pr-core-option-cats-size.md](pr-core-option-cats-size.md). |
| Database scan: compute the file extension once per core-match call, and check the short database list first (performance) | RETR-0016 | Opened 2026-10-02 as RetroArch PR #19678 (branch `pr/scan-core-match`, `f79e1bf804`); merged the same day. Scan CPU time about halved on 1522 files with 301 cores installed (2026-10-01). Draft: [pr-scan-core-match.md](pr-scan-core-match.md). |
| Turbo Bind left/right read past `input_config_bind_order` | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19679 (branch `pr/turbo-bind-bounds`, `366b950d75`). Draft: [pr-turbo-bind-bounds.md](pr-turbo-bind-bounds.md). |
| S3 cloud sync: `&`, `=` and `?` left unencoded in the object key | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19680 (branch `pr/s3-path-encode`, `f33971784b`). Draft: [pr-s3-path-encode.md](pr-s3-path-encode.md). |
| Allocations written through unchecked (menu init, screenshot task, replay checkpoint, replay index) | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19681 (branch `pr/oom-null-checks`, `b8c508c438`). Draft: [pr-oom-null-checks.md](pr-oom-null-checks.md). |
| `GET_LANGUAGE` returns true without writing when `HAVE_LANGEXTRA` is off | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19682 (branch `pr/get-language-default`, `8f9c5f5869`). Draft: [pr-get-language-default.md](pr-get-language-default.md). |
| Damaged replay checkpoint: double free, overrun, and a size larger than the buffer | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19683 (branch `pr/replay-checkpoint-bounds`, `e99abce20f`); fork fix `aab917b1d2`. Draft: [pr-replay-checkpoint-bounds.md](pr-replay-checkpoint-bounds.md). |
| Damaged replay checkpoint: six more (statestream over-read, `superblock_seq` overflow, undefined index, `last_save_size`, legacy frame allocation, uninitialised size) | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19685 (branch `pr/replay-checkpoint-dedup`, `5d702ed87d` + CI fix `f99ba62444`), stacked on #19683; fork fixes `5faedc02c4`, `2d368a11a3`. Draft: [pr-replay-checkpoint-dedup.md](pr-replay-checkpoint-dedup.md). |
| AppStream metainfo: add releases 1.20.0 to 1.22.2 (upstream stops at 1.9.11) | RETR-0020 | Opened 2026-10-02 as RetroArch PR #19689 (branch `pr/metainfo-releases`, `8d88874b47`); fork commits `cfb745a736`, `13602e0689`. Draft: [pr-metainfo-releases.md](pr-metainfo-releases.md). |
| Menu: selection past the entry list in `ozone_selection_changed` and xmb's imageviewer update (#18797's remaining reads); not reproduced | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19691 (branch `pr/menu-selection-bounds`, `df07c52ced`); fork commit `73c7a9c0e3`. Draft: [pr-menu-selection-bounds.md](pr-menu-selection-bounds.md). |

## Branches

Local only, each a single commit on upstream/master `b40db2251a`, in the
worktree `/mnt/Games/Scripts/Linux/ra-pr`. Every branch passed a full
`make -j4`. Tests are listed in each draft's Testing section.

| Branch | Commit | Draft |
|---|---|---|
| `pr/tls-verify-by-default` | `1989b5fe37` | [pr-tls-verify-by-default.md](pr-tls-verify-by-default.md) |
| `pr/netcmd-local-hardening` | `96251986fd` | [pr-netcmd-local-hardening.md](pr-netcmd-local-hardening.md) |
| `pr/untrusted-file-checks` | `6a61d9f07a` | [pr-untrusted-file-checks.md](pr-untrusted-file-checks.md) |
| `pr/crash-safe-saving` | `c08c575b2c` | [pr-crash-safe-saving.md](pr-crash-safe-saving.md) |
| `pr/crash-and-leak-fixes` | `9bd122460d` | [pr-crash-and-leak-fixes.md](pr-crash-and-leak-fixes.md) |
| `pr/cloud-sync-robustness` | `01b12d09f7` | [pr-cloud-sync-robustness.md](pr-cloud-sync-robustness.md) |

Second batch, opened 2026-09-28 on upstream/master `41caa78885` with the
user's approval:

| Branch | Commit | PR | Draft |
|---|---|---|---|
| `pr/tls-verify-on-reload` | `976a8e46c4` | #19648 | [pr-tls-verify-on-reload.md](pr-tls-verify-on-reload.md) |
| `pr/netplay-password-hardening` | `30ca182756` | #19649 | [pr-netplay-password-hardening.md](pr-netplay-password-hardening.md) |

The netplay branch is adapted from the fork's `c6d9f37dc7`, and the fork
should take the adapted version back (its draft says why).

The four issue drafts are in [issues.md](issues.md). Two target
libretro/RetroArch and two target libretro/libretro-common.

## Before opening: review checklist

- Diff is topic-only: `git -C /mnt/Games/Scripts/Linux/ra-pr diff upstream/master <branch> --stat`.
- The C89 build of each touched `.c` file is clean.
- The draft claims nothing the Testing section did not run.
- `pr/netcmd-local-hardening` changes behaviour for anyone who drives
  the network command port from another machine. The draft offers a
  setting instead; decide whether to add that setting before opening.

## Found while drafting, not yet in the fork's own branch

The drafting agent changed or added code that `local/fixes-2026-09` does
not have. Port these back to the fork branch:

- An atomic SRAM write in `save.c`. The upstream sync never ported it,
  because upstream moved SRAM writing out of `task_save.c`.
- A shared `content_replace_file`: rename first, and keep the `.tmp`
  when the fallback fails, rather than deleting the destination first.
- One cloud-sync framing helper replacing the duplicated Content-Length
  verifiers, which used non-portable `strcasecmp`, `strtoull` and `%zu`.

All three were ported on 2026-09-26 in `d605dc0148`. The fork's helper
keeps its body-length check, which PR F's presence-only check drops.

## Correction to the sync record

The UPnP description-parser crash and the cloud-sync path traversal are
**still live upstream**. `upstream-sync-2026-09-decisions.md` implied
upstream already fixed them. Both fixes are in `pr/untrusted-file-checks`
and in `local/fixes-2026-09`.

## Left out on purpose

- BPS zero `target_size`: upstream now checks every write.
- Vulkan overlay gate and matrix `memcmp`: not reachable.
- Wayland `os->output` skip: that field is never NULL.
- `gfx_animation` delay-0 leak: the function has no callers.
- `s3.c` hunks: that file is not in upstream's build.
- `cloud_sync_max_upload_mb`: that setting was reverted.

Every PR body says the change was made with Claude Code (the user's
choice).
