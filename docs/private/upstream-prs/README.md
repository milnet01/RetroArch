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
| Damaged replay checkpoint: double free, overrun, and a size larger than the buffer | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19683 (branch `pr/replay-checkpoint-bounds`, `e99abce20f`); fork fix `aab917b1d2`. Draft: [pr-replay-checkpoint-bounds.md](pr-replay-checkpoint-bounds.md). Closed 2026-10-03: the maintainer's d7c14a0ac8 fixed it; replaced by #19695. |
| Damaged replay checkpoint: six more (statestream over-read, `superblock_seq` overflow, undefined index, `last_save_size`, legacy frame allocation, uninitialised size) | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19685 (branch `pr/replay-checkpoint-dedup`, `5d702ed87d` + CI fixes `f99ba62444`, `27d2ba37f9` for the TSan sweep), stacked on #19683; fork fixes `5faedc02c4`, `2d368a11a3`. Draft: [pr-replay-checkpoint-dedup.md](pr-replay-checkpoint-dedup.md). Closed 2026-10-03: the maintainer's d7c14a0ac8 and c6f3adb51a fixed four of the six; the other three fixes moved to #19695. |
| AppStream metainfo: add releases 1.20.0 to 1.22.2 (upstream stops at 1.9.11) | RETR-0020 | Opened 2026-10-02 as RetroArch PR #19689 (branch `pr/metainfo-releases`, `8d88874b47`); fork commits `cfb745a736`, `13602e0689`. Draft: [pr-metainfo-releases.md](pr-metainfo-releases.md). |
| Menu: selection past the entry list in `ozone_selection_changed` and xmb's imageviewer update (#18797's remaining reads); not reproduced | RETR-0019 | Opened 2026-10-02 as RetroArch PR #19691 (branch `pr/menu-selection-bounds`, `df07c52ced`); fork commit `73c7a9c0e3`. Draft: [pr-menu-selection-bounds.md](pr-menu-selection-bounds.md). |
| Replay index: a lookup of a garbage-collected block reads one past the additions log | RETR-0019 | Opened 2026-10-03 as RetroArch PR #19694 (branch `pr/replay-index-gc`, `8651a11c1e`), stacked on #19685; fork fix `ecd66eb3c7`. Draft: [pr-replay-index-gc.md](pr-replay-index-gc.md). Closed 2026-10-03: the maintainer's c6f3adb51a fixed it. |
| Replay file parsing hardening: commit interval 1, unbounded header block sizes, statestream with no index, timeline input count, savestate replay length, event capacity, pop leak, checkpoint-before result, backwards seeks, failed decode ends playback | RETR-0019 | Opened 2026-10-03 as RetroArch PR #19695 (branch `pr/replay-parsing`, `65f7674a8b`), rebuilt on upstream master with the three surviving #19685 fixes; fork `30ef35574d`. It first rode in #19694, now closed. Body: the PR itself. |
| Menu: `menu_entry_get` bounded for every caller (XMB database-manager and Explore branches), and Ozone's restored tab selection clamped | RETR-0019 | Added 2026-10-03 as the second commit of RetroArch PR #19691 (`45d7548f2b`); fork `33eef2765b`. Its playlist_nav lane fails without the fix on a `--disable-qt` build. |
| `net_http`: a redirect from `https` to `http` ends the transfer with an error instead of dropping TLS | RETR-0021 | Opened 2026-10-03 as RetroArch PR #19696 (branch `pr/net-http-redirect-tls`, `6ede9a842b`); fork fix `ec4d20f509`. Its `http_limits_test` lane fails without the guard on upstream master `e5c48508a3`. Body: the PR itself. |
| Quit controller combo skips Confirm Quit: the combo counts on every held frame, so one press is also the second | RETR-S0115 | Upstream issue #19642, no replies. Opened 2026-10-05 as RetroArch PR #19711 (branch `pr/quit-combo-confirm`, `fc7f0d2f49`). Reproduced on master with the test joypad driver. Merged 2026-10-07. Draft: [pr-quit-combo-confirm.md](pr-quit-combo-confirm.md). |
| TLS: a once-per-session on-screen notice when a certificate is refused, and when verification is Disabled | RETR-0014 | Follow-up to #19626. Opened 2026-10-05 as RetroArch PR #19712 (branch `pr/tls-verify-notices`, `5f8d278ce4`); fork `dd9de469f0`. Tested headless against a local self-signed server in all three modes. Body: the PR itself. |
| Replay loading: a v0/v1 state shorter than its header, and a statestream checkpoint value of the wrong type, leak their buffers | RETR-0022 | Opened 2026-10-05 as RetroArch PR #19713 (branch `pr/replay-decoder-leaks`, `bccfa40615`), on upstream master `5b04bb6729`. New ASan lanes `v1_short_state` and `wrong_type` leak before the fix. Merged 2026-10-09. Body: the PR itself. |
| Replay: a short replay copy into a savestate is refused instead of saved damaged; the checkpoint-config header word is stored little-endian like the other fields | RETR-0023 | Opened 2026-10-05 as RetroArch PR #19714 (branch `pr/replay-serialize-byteorder`, `8d3298ba21`), on upstream master `1e9dec8d32`; fork `740ff95143`. Its `serialize_short` lane fails without the fix; the byte-order lanes were not run on a big-endian machine. Merged 2026-10-09. Body: the PR itself. |
| PipeWire mic open: the error path dereferences a NULL mic | RETR-0028 | Opened 2026-10-08 as RetroArch PR #19737 (branch `pr/pipewire-mic-null`, `75fa1f5002`), on upstream master `38d7bdf492`; fork `a3424c3d6e`. Not reproduced. Merged 2026-10-08. Body: the PR itself. |
| Content information: the Path line dereferences a NULL content path | RETR-0028 | Opened 2026-10-08 as RetroArch PR #19738 (branch `pr/content-info-null-path`, `dda04277d5`), on upstream master `38d7bdf492`; fork `a3424c3d6e`. Not reproduced. Merged 2026-10-08. Body: the PR itself. |
| GLCore: a failed link with an empty info log returns the unlinked program | RETR-0028 | Opened 2026-10-08 as RetroArch PR #19739 (branch `pr/gl3-link-empty-log`, `d4cacf900e`), on upstream master `38d7bdf492`; fork `a3424c3d6e`. Not reproduced. Merged 2026-10-08. Body: the PR itself. |
| glslang: the include-cache hit returns leak the scratch block | RETR-0028 | Opened 2026-10-08 as RetroArch PR #19740 (branch `pr/glslang-include-cache-leak`, `927c0d21a6`), on upstream master `38d7bdf492`; fork `a3424c3d6e`. Not measured. Merged 2026-10-08. Body: the PR itself. |
| Savestate thumbnail path: a `strdup` leaked on every call | RETR-0028 | Opened 2026-10-08 as RetroArch PR #19741 (branch `pr/savestate-thumb-path-leak`, `21963cce7d`), on upstream master `38d7bdf492`; fork `a3424c3d6e`. Not measured. Merged 2026-10-08. Body: the PR itself. |
| Playlist: a pushed entry inherits the thumbnail-name flags of the old top entry | RETR-0028 | Opened 2026-10-08 as RetroArch PR #19742 (branch `pr/playlist-push-thumb-flags`, `30fdc89e23`), on upstream master `38d7bdf492`; fork `a3424c3d6e`. Its `lane_push_starts_without_thumbnail_flags` lane fails without the fix on that master. Merged 2026-10-08. Body: the PR itself. |
| AI service translation: a BMP reply trusted for its size, and scaled by the frame's width | RETR-0028 | Opened 2026-10-08 as RetroArch PR #19743 (branch `pr/translation-bmp-bounds`, `42b3072f3e`), on upstream master `38d7bdf492`; fork `a3424c3d6e`. The body explains the stride and frame-size changes beyond the scanner's finding. Not run against a translation server. Merged 2026-10-08. Body: the PR itself. |
| GL3 shader chain: a failed pass allocation returns a chain without its passes | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19744 (branch `pr/gl3-chain-oom`, `15d5bf3dc9`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Not reproduced. Merged 2026-10-09. Body: the PR itself. |
| Vulkan shader chain: a failed pass allocation returns a chain without its passes | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19745 (branch `pr/vulkan-chain-oom`, `2d5b352056`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Not reproduced. Merged 2026-10-09. Body: the PR itself. |
| `realloc_checked`: a failed first `malloc` reports success | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19746 (branch `pr/coord-array-realloc-checked`, `b534a4a58c`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Not reproduced. Merged 2026-10-09. Body: the PR itself. |
| Vulkan buffer chain: a node whose buffer was not mapped is kept | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19747 (branch `pr/vulkan-buffer-node-unmapped`, `235e235479`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Not reproduced. Merged 2026-10-08. Body: the PR itself. |
| Keysym lookup table: `calloc` unchecked before an indexed write | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19748 (branch `pr/keymap-rlut-oom`, `001bba4270`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Not reproduced. Merged 2026-10-09. Body: the PR itself. |
| GL2/GL3 init: the context lookup writes `gl` before the NULL check | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19749 (branch `pr/gl-init-null-check`, `2866fab122`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Not reproduced. Merged 2026-10-09. Body: the PR itself. |
| Win32 companion Add Files: `realloc` straight into `paths` | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19750 (branch `pr/win32-add-files-realloc`, `c2d9042027`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Syntax-checked with mingw only; not built for Windows. Merged 2026-10-09. Body: the PR itself. |
| PS3 RSX texture upload: `rsxMemalign` unchecked | RETR-0029 | Opened 2026-10-08 as RetroArch PR #19751 (branch `pr/rsx-texture-memalign`, `77a4a01f50`), on upstream master `5d6fbbee8f`; fork `1e6aa76481`. Not compiled: no PS3 toolchain. Merged 2026-10-09. Body: the PR itself. |
| 3DS texture load: the image is sized before its NULL check | RETR-0030 | Opened 2026-10-09 as RetroArch PR #19757 (branch `pr/ctr-load-texture-null`, `15c7bf0eab`), on upstream master `9312a11000`; fork `d166a27b52`. Not compiled: no 3DS toolchain. Body: the PR itself. |
| PS2 font init: texture allocations unchecked | RETR-0030 | Opened 2026-10-09 as RetroArch PR #19758 (branch `pr/ps2-font-alloc`, `e24fadc9bc`), on upstream master `9312a11000`; fork `31d46752e0`. Not compiled: no PS2 toolchain. Body: the PR itself. |
| Network RetroPad: a test-file button shifted past bit 31 | RETR-0030 | Opened 2026-10-09 as RetroArch PR #19759 (branch `pr/net-retropad-test-shift`, `458d3902b9`), on upstream master `9312a11000`; fork `e407c237f4`. Red then green headless under UBSan with a `.ratst` naming button 40; built with make -j4, no warnings in the file. Body: the PR itself. |
| Emergency save name: `struct tm` unwritten when `localtime` fails | RETR-0030 | Opened 2026-10-09 as RetroArch PR #19760 (branch `pr/save-recovery-tm`, `827086a710`), on upstream master `9312a11000`; fork `2c95d6a70e`. Built with make -j4, no warnings in the file; not reproduced. Body: the PR itself. |
| Vita shader log: `strlen` of an unwritten buffer | RETR-0030 | Opened 2026-10-09 as RetroArch PR #19761 (branch `pr/vita-shader-log-init`, `a26e1f5d77`), on upstream master `9312a11000`; fork `f32cb76d73`. Not compiled: no Vita SDK. Body: the PR itself. |
| Screenshot rotation: buffers indexed as `uint32_t` whatever the pixel size | RETR-0034 | Opened 2026-10-09 as RetroArch PR #19763 (branch `pr/screenshot-rotate-bpp`, `c0a86ce17d`), on upstream master `cfbc895b78`; fork `15d5ca8a58`. Red then green under ASan in a harness; built with make -j4, no warnings in the file. Body: the PR itself. |
| User modeline: a zero horizontal or vertical total divides by zero | RETR-0034 | Opened 2026-10-09 as RetroArch PR #19764 (branch `pr/modeline-zero-total`, `434a6fa737`), on upstream master `cfbc895b78`; fork `8572c6a744`. Red then green under ASan and UBSan in a harness; built with make -j4, no warnings in the file. Body: the PR itself. |
| NTSC filter: the left-edge clamp never fires on an unsigned position | RETR-0034 | Opened 2026-10-09 as RetroArch PR #19765 (branch `pr/ntsc-left-edge-taps`, `3a8791ac2d`), on upstream master `cfbc895b78`; fork `e60f563268`. Red then green under ASan and UBSan in a harness; plugin built with its Makefile, no warnings. Body: the PR itself. |
| WinMM MIDI: the unwind after a failed prepare runs on an unsigned counter | RETR-0034 | Opened 2026-10-09 as RetroArch PR #19766 (branch `pr/winmm-midi-unwind`, `aa11330028`), on upstream master `cfbc895b78`; fork `47ec6f33db`. Red then green under wine in a harness; cross-compiled with mingw, no warnings. Body: the PR itself. |
| XMB: an out-of-range animation setting pushes an uninitialised animation | RETR-0034 | Opened 2026-10-09 as RetroArch PR #19767 (branch `pr/xmb-animation-default`, `7429108d20`), on upstream master `cfbc895b78`; fork `ab2df8784a`. Built with make -j4, no warnings in the file; not reproduced. Body: the PR itself. |
| `/proc/acpi` battery: `endptr` tested before `strtol` sets it, so no capacity is read | RETR-0034 | Opened 2026-10-09 as RetroArch PR #19768 (branch `pr/acpi-battery-parse`, `db89cf01ae`), on upstream master `cfbc895b78`; fork `0d566bece7`. Red then green under ASan and UBSan in a harness; built with make -j4, no warnings in the file. Body: the PR itself. |
| Qt `FloatSlider`: the repaint compares an integer quotient with the float setting | RETR-0034 | Opened 2026-10-09 as RetroArch PR #19769 (branch `pr/qt-floatslider-compare`, `b7688aec08`), on upstream master `cfbc895b78`; fork `1e3936617a`. Built with make -j4 and Qt 6, no warnings in the file; not reproduced. Body: the PR itself. |

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
