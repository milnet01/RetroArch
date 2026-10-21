<!-- ants-spec-format: 1 -->
# RETR-0021 — Let RetroArch update itself on Windows and as a Linux AppImage

**Status:** accepted (2026-10-03).
**Kind:** implement.
**Source:** ROADMAP RETR-0021 (user request 2026-10-02; scope and design decisions 2026-10-03).
**Target:** `local/fixes-2026-09` on the fork; one upstream PR once it is built and tested there (user's instruction).

**Layman:** RetroArch can check for a newer stable version and, when you say yes, download and install it — on Windows and as a Linux AppImage only, never where Steam, a store or a package manager already handles updates.

## 1. Goal

A RetroArch that was downloaded from libretro as a Windows installer, a Windows portable archive or a Linux AppImage can find out that a newer stable release exists and, only when the user agrees, replace its own program files with that release and restart. Every other kind of install never sees the feature and never makes an update request.

## 2. Problem

1. **Nothing updates the program.** The Online Updater downloads cores, core info, assets, cheats, databases, overlays and shaders from the buildbot (`DEFAULT_BUILDBOT_SERVER_URL`, `DEFAULT_BUILDBOT_ASSETS_SERVER_URL` in `config.def.h`). Nothing fetches a newer `retroarch` binary and nothing checks for one. A user on a portable copy or an AppImage only learns of a release by visiting the website.
2. **The last attempt was removed broken.** `ui/drivers/qt/updateretroarch.cpp` (added `4baecf84ca`, removed `b1f6fa4a2a`, upstream PR #12023: *"Not working atm. Will be replaced by a non qt specific method later"*) was Windows-only, nightly-only, downloaded over plain HTTP with no integrity check, and never restarted the program. Its three strings, `MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY` / `_FINISHED` / `_FAILED` in `msg_hash.h`, are still declared and referenced by no `.c`, `.cpp` or `.m` file (`grep -rn QT_UPDATE_RETROARCH --include=*.c --include=*.cpp --include=*.m .` → no output).
3. **Upstream publishes no integrity data for stable builds.** Checked 2026-10-03: `https://buildbot.libretro.com/stable/1.22.2/windows/x86_64/RetroArch.7z.sha256` → 404; the 1.22.2 Win64 installer has no Authenticode signature; the AppImage's `.upd_info`, `.sha256_sig` and `.sig_key` sections are empty; `http://buildbot.libretro.com/stable/1.22.2/` answers 200 without redirecting to HTTPS. The only defence available today is a verified TLS connection.
4. **The HTTP client followed a redirect from `https` to `http`.** `net_http_redirect` in `libretro-common/net/net_http.c` rebuilt the request from the `Location` URL and set `state->ssl` from that URL's scheme, so a redirect silently dropped TLS. For an updater that is a downgrade path to an unauthenticated binary. Fixed on the fork by `ec4d20f509` and upstream by `217c50838e` (PR #19696).

## 3. Scope decisions (agreed with the user)

Recorded in RETR-0021's body; each made by the user.

| # | Decision | Date |
|---|----------|------|
| D1 | Build it fully on the fork first, then send it upstream as one PR. | 2026-10-02 |
| D2 | Platforms: Windows installer, Windows portable, Linux AppImage. Never Flathub, Snap, Steam, app stores or distro packages. | 2026-10-02 |
| D3 | A "check for RetroArch update" item in the Online Updater, plus an optional start-up check that is **off by default**. Nothing is downloaded until the user says yes. | 2026-10-02 |
| D4 | Stable releases only. | 2026-10-02 |
| D5 | Integrity: HTTPS with certificate checking required; refuse if the user has turned checking off. Use upstream checksums if they are ever published. Ask upstream to publish them. | 2026-10-03 |
| D6 | An installer install updates by running the new official installer. | 2026-10-03 |
| D7 | A portable Windows update replaces program files only — never config, saves or assets. | 2026-10-03 |
| D8 | Accept the full-size download; show its size before asking. Ask upstream for a small stable update package and a bare AppImage, and use them when they exist. | 2026-10-03 |
| D9 | An installer install outside the folder the silent installer targets opens the installer visibly, with a note naming the folder to pick. | 2026-10-03 |
| D10 | Accept the real peak while unpacking the stable `.7z`, about 780 MB on Linux and 750 MB on Win64 for stable 1.22.2. Check free memory before each block from the archive's real block sizes, never estimates, and stop with a message when it is short. | 2026-10-20 |

One choice was mine, stated so it can be overturned: the latest version comes from GitHub's release API and the files from the buildbot (§8 says why).

## 4. Design

### 4.1 Build gate

A new `HAVE_SELF_UPDATER` in `qb/config.params.sh`, default `yes`, forced to `no` by `qb/config.libs.sh` unless all of these hold:

- `HAVE_NETWORKING` and `HAVE_ONLINE_UPDATER` are `yes` (so a Steam build, which `qb/config.libs.sh` already sets to `ONLINE_UPDATER no`, has no self-updater);
- `HAVE_7ZIP` is `yes` (`qb/config.params.sh`), since every archive in §4.5 is a `.7z`;
- `HAVE_SSL` is `yes` once `qb/config.libs.sh` has resolved its `auto`, since every request in §4.8 is HTTPS;
- the target is Win32 desktop on x86 or x86_64, or Linux on x86_64 — the only stable builds the buildbot publishes for these channels.

UWP (`uwp/`, `pkg/msvc-uwp`) does not build through `./configure` and never defines it. New sources go into `Makefile.common` under `HAVE_SELF_UPDATER`. They are not added to `griffin/griffin.c`: no console target qualifies.

### 4.2 Files

| File | Holds |
|------|-------|
| `self_update.c` / `self_update.h` | Pure policy, no I/O of its own: install-kind decision, version parse and compare, release-JSON parse, URL building, the Windows program-file filter, the pending-update marker format, the memory rule (§4.5), the restart target (§4.6), the start-up cleanup, the Windows replace-and-rollback loop and the AppImage swap. The last three, and reading and removing the pending marker at start-up, do their file operations (exists, rename, remove, make a folder, get and set mode, list a folder, read a small file) through a table of function pointers the caller passes; production passes one backed by the real calls, and the tests pass one that wraps the real calls and can fail a chosen one. Everything here is testable without a network or a Windows machine. |
| `struct self_update_endpoints` in `self_update.h` | The test seam: the GitHub API base URL, the buildbot base URL and whether the TLS-mode gate applies. Production code passes `self_update_endpoints_default` — the two §4.8 hosts, gate on — and nothing else. Only `samples/tasks/self_update/` passes another one, pointing at its local plain-HTTP server with the gate off. |
| `tasks/task_self_update.c` | The check task and the download-and-apply task, built on `task_push_http_transfer_with_user_agent`, `task_push_http_download_file` and the 7z reader (`r7z_archive_*`, `libretro-common/include/7z/r7z_archive.h`), which reports each entry's block and size. It also reads available memory: `MemAvailable` from `/proc/meminfo` on Linux, `GlobalMemoryStatusEx` on Windows. |
| `libretro-common/formats/7z/r7z_archive.c` / `libretro-common/include/7z/r7z_archive.h` | New, read-only: `int r7z_archive_folder_info(const r7z_archive_t *a, uint32_t folder, uint64_t *unpack_size, uint64_t *pack_size, uint32_t *num_coders)`, giving one block's unpacked size, packed size (the sum of its packed streams) and number of coders, as the archive header records them. `folder` is the block index `r7z_entry_t` carries. It returns `R7Z_OK`; an index past the last block returns `R7Z_ERROR_PARAM`. This is vendored code, so the function goes upstream in the self-updater PR, never as a fork-only change (§11). |
| `frontend/drivers/platform_win32.c`, `frontend/drivers/platform_unix.c` | The two platform hooks: launching the installer after exit (Windows), and restarting into the target `self_update.c` returns (AppImage). |

### 4.3 Install kind

```c
enum self_update_kind
{
   SELF_UPDATE_KIND_NONE = 0,          /* hide the feature, make no request */
   SELF_UPDATE_KIND_APPIMAGE,
   SELF_UPDATE_KIND_WIN_PORTABLE,
   SELF_UPDATE_KIND_WIN_INSTALLER,      /* silent installer targets this folder */
   SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE /* installer install elsewhere: D9 */
};
```

The decision is a pure function of injected facts, so both platforms' rules are tested on Linux:

- **Linux.** `APPIMAGE` is set, is an absolute path, names a regular file, and neither `FLATPAK_ID` nor `SNAP` is set → `APPIMAGE`. Anything else → `NONE`. This is positive detection: the AppImage runtime sets `APPIMAGE` and nothing else does, so a distro package, a Flatpak, a Snap or a source build reaches `NONE` without being recognised by name. No source file reads any of these variables today (`grep -rn 'APPIMAGE\|FLATPAK_ID\|"SNAP"' --include=*.c . | grep -v deps/` → no output).
- **Windows.** `uninstall.exe` sits beside `retroarch.exe` **and** the uninstall key's `UninstallString` names that same file → an installer install. Else → `WIN_PORTABLE`. If the folder holding `retroarch.exe` is unknown → `NONE`. The key is `SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\RetroArch` in `HKLM`, read through the 32-bit registry view (`KEY_WOW64_32KEY`), which is where the 32-bit NSIS installer writes it on 64-bit Windows (measured below). An installer install is `WIN_INSTALLER` only when the build is x86_64 and the folder is `C:\RetroArch-Win64`, the one folder measured; every other installer install is `WIN_INSTALLER_VISIBLE`. The x86 installer's silent folder was not measured, so x86 installer installs always take the visible route until it is (§14).

Measured on Windows 10 22H2, stable 1.22.2 `RetroArch-Win64-setup.exe`, 2026-10-03 (RETR-0021 body): silent `/S` installs to `C:\RetroArch-Win64` and ignores `/D=`; the key is `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\RetroArch` with `UninstallString C:\RetroArch-Win64\uninstall.exe` and `DisplayVersion 1.22.2.0`; the installer's top-level files are the portable archive's plus `uninstall.exe`.

### 4.4 Finding the latest release

```
GET https://api.github.com/repos/libretro/RetroArch/releases/latest
User-Agent: RetroArch/<PACKAGE_VERSION>
```

Parsed with `rjson`: `tag_name`, `draft`, `prerelease`. A release is offered only when `draft` and `prerelease` are both `false`, `tag_name` parses, and that version is greater than `PACKAGE_VERSION` (`version.all`).

```c
/* 0 on success; parts beyond those present are 0. Accepts one
 * optional leading 'v', then two to four dot-separated decimal
 * parts. Rejects empty parts, signs, other non-digits, fewer than
 * two or more than four parts, any part > 65535. */
int self_update_version_parse(const char *s, unsigned out[4]);
/* -1, 0, 1. */
int self_update_version_cmp(const unsigned a[4], const unsigned b[4]);
```

Four parts because upstream has shipped `v1.16.0.1`, `.2` and `.3` (`gh api 'repos/libretro/RetroArch/releases?per_page=40'`), and Windows reports `DisplayVersion 1.22.2.0`.

### 4.5 Where files come from

Base `https://buildbot.libretro.com/stable/<version>/`, `<version>` without the `v`:

| Kind | Preferred, used if HEAD → 200 (D8) | If the preferred HEAD → 404 | What is taken from it |
|------|------|------|------|
| `WIN_PORTABLE` | `windows/<arch>/RetroArch_update.7z` | `windows/<arch>/RetroArch.7z` | The program set, §4.6 |
| `WIN_INSTALLER*` | — | `windows/x86_64/RetroArch-Win64-setup.exe` or `windows/x86/RetroArch-Win32-setup.exe` | The whole file |
| `APPIMAGE` | `linux/x86_64/RetroArch-Linux-x86_64.AppImage` | `linux/x86_64/RetroArch.7z` | The one member at depth two ending `.AppImage` |

`<arch>` is `x86_64` or `x86`. For every file downloaded, `<url>.sha256` is fetched first (D5): 200 → its first 64 hex digits are the expected SHA-256, checked by reading the finished staging file back in chunks through `sha256_stream_init` / `_update` / `_final` (`libretro-common/include/lrc_hash.h`) — `task_push_http_download_file` writes straight to disk and offers no per-chunk hook; 404 → no checksum; any other result → the update stops.

Before asking the user, the check task issues a HEAD for the file it will download and shows its `Content-Length` (D8). A HEAD answer other than 200 or 404 stops the check (`cb_self_update_head`).

**Memory (D10).** A `.7z` is read whole into memory. `r7z_archive_extract_slice` decodes the whole solid block holding a member into one buffer, after copying the block's packed bytes into a buffer of their own (`decode_folder_slice`, `decode_folder_bcj2_slice`). A block with more than one coder holds a second buffer about its unpacked size: a BCJ2 block holds its four decoded inputs and its output at once, and a chain holds one stage's output while the next stage's is written. A block's buffer is freed only when the next block finishes decoding, or when the archive is closed. Stable 1.22.2, measured 2026-10-20 with `7z l -slt` on range-fetched header bytes:

- The Linux `RetroArch.7z` (179,361,448 B) is one block of one coder (`LZMA:26`): packed 179,180,498 B, unpacked 410,409,118 B, holding the 10,390,008 B AppImage.
- The Win64 `RetroArch.7z` (202,509,078 B) has two blocks. Block 0 is one coder (`LZMA2:26`): packed 166,881,476 B, unpacked 380,484,561 B. Block 1 is four coders (`BCJ2`, one `LZMA2`, two `LZMA`): packed 35,474,948 B, unpacked 161,415,485 B, holding `retroarch.exe` (18,641,482 B) and the DLLs. The program set has members in both.

The apply task decodes the needed blocks one at a time. Between blocks it closes the archive and reopens it on the same buffer; `r7z_archive_close` frees the decoded block. Before decoding each block, it reads available memory, and the block's sizes and coder count from `r7z_archive_folder_info` (§4.2). It stops unless available memory is at least the sum of:

- the archive size;
- the block's packed size;
- the block's unpacked size, counted twice when the block has more than one coder;
- the largest needed member in the block;
- 64 MiB.

For stable 1.22.2 that sum, less the 64 MiB, is 779,341,072 B on Linux. On Win64 it is 749,876,939 B for block 0 and 579,456,478 B for block 1. Available memory is `MemAvailable` from `/proc/meminfo` on Linux. On Windows it is the smaller of `ullAvailPhys` and `ullAvailVirtual` from `GlobalMemoryStatusEx`, since a 32-bit process can run out of address space before memory. Where available memory cannot be read, it proceeds. Reading the archive comes before the check; its size is the download size the user saw before agreeing (D8).

### 4.6 Applying the update

**Windows portable.** Staging is `<exe dir>\.retroarch-update\`, on the same volume so renames are atomic.

1. Download to staging; verify per §4.5.
2. Extract the **program set**: let `T` be the archive's single top-level folder; the set is every `T/*.exe`, `T/*.dll` and everything under `T/filters/` and `T/platforms/` — the set upstream's nightly `RetroArch_update.7z` carries. Nothing else is extracted, so `retroarch.cfg`, `retroarch.default.cfg`, saves, states, shaders, overlays, assets and databases are never written (D7).
3. Before the first rename, make the **rollback record**, the folder `<exe dir>\.retroarch-rollback`. It marks a replace in progress, so a crash part-way is repaired at the next start too. It is a folder because the file-operation table can make a folder but not write a file. If the record already exists, an earlier replace is unfinished: `self_update_replace` changes nothing and returns its incomplete-rollback result. Otherwise it first deletes every `*.retroarch-old` in `<exe dir>`, `filters\` and `platforms\`, so every one present under a record belongs to this replace; if one cannot be deleted, it changes nothing and fails as a clean rollback does, and a restart frees it. If the record cannot be made, it changes nothing and fails as a clean rollback does. Then, for each extracted file `p`: if `<exe dir>\p` exists, rename it to `p.retroarch-old`; then move the new file into place. Windows allows renaming a running executable or loaded DLL and writing a new file under its old name, but not deleting it (measured 2026-10-03).
4. If any step fails, undo every rename already made, in reverse, and report the failure. When the new file cannot be moved back to staging, the undo leaves it under `p` and does not rename the old file back. When the old file cannot be renamed back under `p`, the new file just moved to staging goes back to `p`. After a full success or a clean rollback, `self_update_replace` removes the record. If an undo rename itself fails, the record stays and `self_update_replace` returns a result apart from a clean rollback; the report says the update was only partly undone and that restarting RetroArch finishes the undo (INV-16).
5. Write the pending marker (§4.7), remove staging, and offer *Restart now*. The restart uses the `retroarch.exe` path captured **before** step 3: after the rename, the process's reported path still names the original file (measured: `Get-Process` reported the original name after a rename), so the captured path is the one that now holds the new build.

`*.retroarch-old` files in `<exe dir>`, `filters\` and `platforms\`, a leftover `.retroarch-update\` and a leftover `%TEMP%\retroarch-update\` (the installer route's staging, below) are deleted at start-up; a file still in use is left for the next start. When the rollback record exists, start-up repairs instead of deleting: for each `p.retroarch-old` it renames `p`, if present, to `p.retroarch-new`, then renames `p.retroarch-old` back to `p`. If that second rename fails, `p.retroarch-new` goes back to `p`. When `p` is present it first removes any `p.retroarch-new` an earlier repair left, since the Windows rename never replaces an existing file (`su_fs_rename` calls `MoveFileW` with no flags); the old file is still in `p.retroarch-old`. It removes the record once no `.retroarch-old` is left; a failed rename leaves the record for the next start. `self_update_cleanup` reports whether it put any file back. When it did, start-up removes the pending marker without its message and says instead that the update did not finish, the previous version was put back, and RetroArch must be restarted, since the running process may be the build just moved aside. So a record that a successful replace could not remove ends as a reported undo: every replaced file holds its old bytes, and a file the release added stays until the next update replaces it. `*.retroarch-new` files are deleted like `*.retroarch-old` ones, but only at a start that finds no record, since a half-done repair can leave one as the only copy (INV-16). Cleanup removes a staging folder only when its name is `.retroarch-update` or `retroarch-update`, so a wrong path cannot empty another folder.

**Windows installer.** RetroArch cannot run the installer itself and stay open: the installer skips a `retroarch.exe` it cannot open and still exits 0 (measured 2026-10-03). So:

1. Download the installer to `%TEMP%\retroarch-update\`; verify per §4.5.
2. Write `%TEMP%\retroarch-update\run.cmd`, which waits until RetroArch's process ID is gone, then runs `start "" /wait "<setup>"` with `/S` for `WIN_INSTALLER` and without it for `WIN_INSTALLER_VISIBLE`, then starts `<exe dir>\retroarch.exe`.
3. Start it hidden (`CreateProcess` on `cmd.exe /c`). Only once it has started, write the pending marker, so an installer that never started is not reported as an unfinished update. Then show *RetroArch will close to install the update*, and quit through `CMD_EVENT_QUIT` so content and config save as on any quit.

`start` goes through the shell, so Windows shows its usual admin prompt for the installer. For `WIN_INSTALLER_VISIBLE` the message before quitting names the folder the user must pick: `<exe dir>`.

**AppImage.** Staging is `<dir of $APPIMAGE>/.retroarch-update/`, so the final rename is on one filesystem.

1. Download to staging; verify per §4.5. If the directory is not writable, stop before downloading and say so.
2. Extract the `.AppImage` member (or take the bare file) as `staging/new.AppImage`; give it the old file's permission bits; flush it to disk.
3. `rename()` it over `$APPIMAGE`. The running copy keeps its already-mounted image (measured 2026-10-03 with stable 1.22.2: the process kept running and its mounted `usr/bin/` stayed readable after the rename).
4. Write the pending marker, remove staging, offer *Restart now*. Restart executes `$APPIMAGE`.

A leftover `.retroarch-update/` beside `$APPIMAGE` is deleted at start-up, as on Windows. The existing restart in `frontend_unix_set_fork` takes its target from `fill_pathname_application_path`, which reads `/proc/<pid>/exe` — inside an AppImage that is the binary in the old mounted image, so it would restart the old version.

### 4.7 Confirming the result

The pending marker is `self_update.pending` in the config directory, two lines: the target version and the install kind. At start-up, when it exists: if `PACKAGE_VERSION` ≥ the target, show *RetroArch was updated to X*; otherwise show *The update to X did not finish; RetroArch is still Y*. Then delete it. A start whose cleanup put files back shows its own message instead (§4.6). This is what turns the installer's silent skip of a locked file into something the user is told.

### 4.8 TLS

In every build outside `samples/`, every request the updater issues is built from `self_update_endpoints_default`: `https://api.github.com/` or `https://buildbot.libretro.com/`. A redirect from either to another HTTPS host is followed, and that host's certificate is verified like any other. None is made unless `settings->uints.tls_verify_mode` is `TLS_VERIFY_REQUIRED` (`network/tls_config.h`); otherwise the updater says certificate checking must be on and stops (D5). A redirect that leaves HTTPS ends the transfer with an error: `net_http.c` has that guard for every caller, upstream already (§2 item 4).

### 4.9 Menu and settings

- **Online Updater → Update RetroArch** (`MENU_ENUM_LABEL_SELF_UPDATE`). Shown only when the kind is not `NONE`. The Online Updater entry itself is already hidden when `menu_show_online_updater` is off or `kiosk_mode_enable` is on (`menu/menu_displaylist.c`), so this entry inherits both. Selecting it runs the check. Up to date → a notification naming the running version. Newer → a sub-list: an info line *RetroArch X is available (N MB)* and an **Update Now** entry, the same sub-list-of-actions shape as the cloud-sync conflict choice. Leaving the list is "not now".
- **Settings → Network → Updater → Check for RetroArch Updates at Start-up**, key `self_update_check_on_startup`, default `false` (`DEFAULT_SELF_UPDATE_CHECK_ON_STARTUP` in `config.def.h`). When on, the check runs once per start, quietly; a newer release produces one notification pointing at Online Updater → Update RetroArch. It never downloads.
- Strings: new `msg_hash` entries for the above. The three unused `MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_*` strings are removed in the same change (§2 item 2).

## 5. Invariants

The *Test:* paths below are created by this work; none runs yet. Each names the rule its fixture isolates.

- **INV-1** — The kind is `NONE` unless the positive evidence of §4.3 is present; with `NONE` there is no menu entry, no start-up check and no network request.
  *Test:* `samples/tasks/self_update/self_update_test.c` lane `kind`: a table of injected environments — `APPIMAGE` unset; set but relative; set to a directory; set with `FLATPAK_ID` also set; set with `SNAP` also set; set and valid. Only the last yields `APPIMAGE`. Windows rows: no `uninstall.exe`; `uninstall.exe` with no key; key naming another folder; key matching in `C:\RetroArch-Win64` on x86_64; matching elsewhere; matching on x86. Fixture isolates the kind decision only. Network lane `none`: the check for kind `NONE` is refused and makes no request.
  *Breaks when:* a Flatpak or Snap whose environment carries `APPIMAGE` is classed `APPIMAGE`, or a copied installer folder is classed `WIN_INSTALLER` from `uninstall.exe` alone.

- **INV-2** — A Steam build, and any target outside §4.1's list, compiles the feature out.
  *Test:* manual recipe: `./configure --enable-steam` then `grep HAVE_SELF_UPDATER config.mk` shows it off; so do `./configure --disable-7zip` and `./configure --disable-ssl`; a plain Linux x86_64 configure shows it on.
  *Breaks when:* `qb/config.libs.sh` derives `HAVE_SELF_UPDATER` before the Steam block turns `ONLINE_UPDATER` off.

- **INV-3** — Versions compare numerically, part by part, up to four parts, missing parts counting as 0.
  *Test:* lane `version`: `1.9.0 < 1.22.2`; `1.22.2 = 1.22.2.0`; `1.16.0 < 1.16.0.3`; `v1.22.2` parses; `1.22`, `1.2.3.4` parse; `1`, `vv1.2`, `1.2.3.4.5`, `1..2`, `1.2a`, `-1.2`, `""`, `1.70000` are rejected. Fixture isolates the parser and comparator.
  *Breaks when:* strings are compared as text (`1.9.0` > `1.22.2`), or a fourth part is dropped (`1.16.0.3` = `1.16.0`).

- **INV-4** — A release is offered only when it is not a draft, not a prerelease, has a valid tag, and is newer than `PACKAGE_VERSION`.
  *Test:* lane `release`: JSON fixtures for each failing condition and one passing one, each differing from the passing one in exactly that field. Fixture isolates the offer rule; JSON that fails to parse is its own fixture.
  *Breaks when:* `prerelease: true` is offered, or an equal or older tag is offered.

- **INV-5** — With `self_update_endpoints_default`, no update request is made unless `tls_verify_mode` is `TLS_VERIFY_REQUIRED`, every URL the updater builds starts with one of the two hosts of §4.8, and a redirect to a non-HTTPS URL fails the transfer.
  *Test:* lane `tls`, using `self_update_endpoints_default` and never the test endpoints, asserts every URL built for each install kind starts with one of those two prefixes and that the check refuses to start under `TLS_VERIFY_OPTIONAL` and `TLS_VERIFY_DISABLED`; network lane `tlsgate` pushes the check with `self_update_endpoints_default` under both modes and asserts it is refused before any request; `samples/tasks/http`'s `http_limits_test` hands the redirect step an HTTPS request and an `http://` `Location` and asserts the transfer ends in error, while the same request with an `https://` `Location` is followed — so the fixture isolates the scheme rule, not redirects as such. Each fixture isolates one of the three clauses.
  *Breaks when:* the mode gate is skipped for the start-up check, or `net_http_redirect` follows an `https`→`http` redirect.

- **INV-6** — Nothing is downloaded or changed on disk before the user selects **Update Now**; the start-up check only notifies.
  *Test:* lane `startup`: run the start-up check, through test endpoints (§4.2), against a local server offering a newer release; assert one notification and no download task in the queue.
  *Breaks when:* the start-up check chains into the download task.

- **INV-7** — When `<url>.sha256` answers 200, a file whose SHA-256 differs is never installed; only a 404 counts as "no checksum"; any other answer stops the update.
  *Test:* lane `checksum` with a local server, through test endpoints (§4.2): matching hash → proceeds; wrong hash → stops, staging removed; 404 → proceeds; 500 → stops. The 500 row isolates the "only 404 means absent" rule.
  *Breaks when:* a failed checksum fetch is treated as "no checksum published".

- **INV-8** — A Windows portable update writes only the program set of §4.6.
  *Test:* lane `program_set`: the filter applied to the member list of a real stable `RetroArch.7z` listing (checked into the test as a text fixture) keeps exactly the `.exe`, `.dll`, `filters/` and `platforms/` members of the top folder and nothing else.
  *Breaks when:* `retroarch.default.cfg`, `retroarch.cfg`, or any member under `assets/`, `shaders/` or `saves/` passes the filter.

- **INV-9** — A Windows portable replacement is all or nothing.
  *Test:* lane `replace`: the step-3 loop over a temporary folder with an injected rename that fails on the Nth file, for N = first, middle, last; after each, every original file is back under its own name and no `.retroarch-old` remains. Fixture isolates the rollback.
  *Breaks when:* a failure part-way leaves a mix of old and new DLLs.

- **INV-10** — The installer starts only after the RetroArch process has exited; `/S` is passed only for `WIN_INSTALLER`.
  *Test:* manual recipe on Windows (`wintest`): start the update from a running RetroArch; confirm `retroarch.exe`'s file time changes and the marker check reports success. Then repeat with RetroArch installed outside `C:\RetroArch-Win64` and confirm the installer opens visibly. Lane `kind` covers which route each install takes.
  *Breaks when:* the installer starts while `retroarch.exe` is still open — it then skips the file and exits 0.

- **INV-11** — After any applied update, the next start tells the user whether the running version reached the target, and removes the marker.
  *Test:* lane `marker`: marker targeting a version below, equal to and above a stubbed `PACKAGE_VERSION`; assert the success, success and failure messages and that the marker is gone each time.
  *Breaks when:* an installer that skipped the locked executable leaves the user believing the update succeeded.

- **INV-12** — An AppImage update replaces `$APPIMAGE` by a rename in its own directory, keeps the old file's permission bits, and a restart executes `$APPIMAGE`.
  *Test:* lane `appimage` in a temporary directory with a stand-in file of mode 0750; after the apply step the path holds the new bytes with mode 0750, and the restart target function returns the `APPIMAGE` value rather than `/proc/<pid>/exe`. Manual recipe: run a stable AppImage renamed to an older version string, update it, restart, and read the version in Information.
  *Breaks when:* the restart uses `fill_pathname_application_path` and comes back as the old build.

- **INV-13** — A failed or abandoned update leaves the installed program as it was and removes its staging folder.
  *Test:* lane `checksum` asserts staging is gone after each failure row; lane `replace` asserts that after each failed rename every original is back and no `.retroarch-old` is left; lane `cleanup` plants `*.retroarch-old` files beside their originals, and each kind's staging folder (the two Windows ones and the AppImage one) and asserts the start-up cleanup removes them all.
  *Breaks when:* a staging folder or `.retroarch-old` file survives a failure and is never removed.

- **INV-14** — No block of a `.7z` is decoded unless available memory, where the platform reports it, covers the §4.5 sum for that block: the archive size, the block's packed size, its unpacked size (twice for more than one coder), its largest needed member, and 64 MiB. The blocks are decoded one at a time, with the archive reopened between them.
  *Test:* lane `memory`: `self_update_memory_ok` for a one-coder block with available memory one byte below the sum → refused, and equal to it → allowed; for a four-coder block with the same sizes, the one-coder sum → refused, and the doubled sum → allowed; not reported (0) → allowed; sizes whose sum overflows 64 bits → refused. The four-coder rows isolate the doubling; the one-coder rows the rest of the sum. Reading the figure on each platform, and the reopen between blocks, are not covered.
  *Breaks when:* the check leaves out the packed size or the doubling, or a sum that wraps reads as small.

- **INV-15** — `r7z_archive_folder_info` reports each block's unpacked size, packed size and coder count as the archive header records them.
  *Test:* a new test in `libretro-common/samples/formats/r7z/`, over a checked-in fixture made by `7zz a -mf=BCJ2` of an `.exe` and then `7zz a -mf=off` of a text file: two blocks, block 0 one `LZMA2` coder, block 1 four coders under `BCJ2`. The expected sizes are written from `7zz l -slt` of the fixture. An index past the last block returns `R7Z_ERROR_PARAM`. The BCJ2 block isolates the packed-stream sum and the coder count; two blocks of different sizes isolate the index.
  *Breaks when:* a BCJ2 block reports one packed stream or one coder, or a block reports another block's sizes.

- **INV-16** — A failed undo never costs the old file. `self_update_replace` makes the rollback record before its first rename and removes it only after a full success or a clean rollback; it changes nothing when the record already exists or cannot be made, or when a `.retroarch-old` from before it cannot be deleted. When an undo rename fails, the record stays and the result is apart from a clean rollback. While the record exists, the next start's cleanup puts every `p.retroarch-old` back under `p`, moving a present `p` to `p.retroarch-new`, removes the record only when no `.retroarch-old` is left, and reports that it put files back. An undo or repair whose old-file rename fails moves the file it just moved away back to `p`; an undo that cannot move the new file out of `p` leaves the old file in `p.retroarch-old`.
  *Test:* lane `replace`: after a success and after each clean rollback, the record is gone. With an injected rename that fails during the undo, once on the move back to staging and once on the old file's rename back: the result is the incomplete-rollback one, the record exists, `p` exists, and `p.retroarch-old` still holds the old bytes; a second call returns the same result, renames nothing and deletes nothing, so `p.retroarch-old` still holds the old bytes after it. With the record's folder creation failing: the call fails, renames nothing, and every original is in place. With a stale `old.dll.retroarch-old` planted outside the set and removes failing: the call fails, renames nothing, and no record exists; with removes working, the stale file is gone after the call. Lane `cleanup` with the record: plants `retroarch.exe` holding new bytes beside `retroarch.exe.retroarch-old` holding old bytes; afterwards `retroarch.exe` holds the old bytes, the record is gone and cleanup reports a repair, also when a stale `retroarch.exe.retroarch-new` is planted beside them and the fake rename refuses an existing target, as `MoveFileW` does, and with an injected failure of the old file's rename back the record stays and `retroarch.exe` holds the new bytes. Lane `cleanup` without the record: the same planted files are deleted as INV-13 says and cleanup reports no repair. The record alone decides between repair and delete, so the two cleanup runs differ only in it.
  *Breaks when:* cleanup deletes the only copy of a program file or leaves a replaced file holding new bytes, a retry before restart deletes a set-aside old file, or the user is told nothing changed when an undo failed.

## 6. Failure modes

| Assumption broken | What happens |
|---|---|
| No network, GitHub API rate-limited (403/429), or JSON unreadable | Check reports *Could not check for updates* with the HTTP status; nothing else happens. |
| GitHub names a release whose buildbot files are not uploaded yet (HEAD 404) | Reported as *not available yet*; no download. |
| Certificate checking is not `REQUIRED` | The check does not start; the message names the TLS verification setting (`MENU_ENUM_LABEL_TLS_VERIFY_MODE`). |
| Download interrupted | `task_push_http_download_file` resumes where it can; on final failure, staging is removed and nothing is installed. |
| Not enough disk space, or the exe / AppImage folder is read-only | Detected when the staging file cannot be created or written; reported; nothing installed. A Windows portable copy under `Program Files` is the common read-only case. |
| A rename fails part-way (another program holds a file) | INV-9 rollback; reported. If the undo fails too, reported as partly undone; the next start puts every old file back (INV-16). |
| User declines the Windows admin prompt, or the installer fails | `run.cmd` still starts RetroArch; the marker check reports the update did not finish. |
| Installer reaches `retroarch.exe` before the process is gone | Prevented by the wait in `run.cmd`; caught by the marker if it happens anyway. |
| Some other caller relied on an `https`→`http` redirect | It now fails where it used to succeed over plain HTTP. The guard is in `net_http.c` for every caller (§4.8), so this is a deliberate behaviour change, stated in its own upstream PR. |
| Restart fails | The update is installed; the next manual start runs it and the marker check confirms it. |
| Not enough free memory to unpack (INV-14) | Stops before decoding the block; staging removed; the message names the memory needed. No installed file has been touched: extraction finishes before any is replaced (§4.6). |
| A nightly build whose `PACKAGE_VERSION` equals the last stable | Told it is up to date; a nightly is never replaced by an equal or older stable. |
| A user renamed their AppImage to include a version | The file keeps the user's name and now holds the new build. |

## 7. Tests

- `samples/tasks/self_update/` — new, built like `samples/tasks/core_updater/`: a standalone `Makefile` with `check` and `SANITIZER=address|undefined`. The policy lanes compile `self_update.c` alone: it reads no settings, and the TLS gate takes the `tls_verify_mode` value as an argument. The network lanes (`startup`, `tlsgate`, `none`, `checksum`; `self_update_net_test.c`, built by `Makefile.net`) link the real `tasks/task_self_update.c`, `tasks/task_http.c`, `task_queue.c` and `net_http.c`, stub the frontend symbols, and serve from a local HTTP server thread — the shape of `samples/tasks/core_updater/Makefile.refresh_flags` and its `get_list_refresh_flags_test.c`. Lanes: `kind` (INV-1, INV-10's routing), `version` (INV-3), `release` (INV-4), `tls` and `tlsgate` (INV-5), `none` (INV-1), `startup` (INV-6), `checksum` (INV-7, INV-13), `program_set` (INV-8), `replace` (INV-9, INV-13, INV-16), `marker` (INV-11), `appimage` (INV-12), `cleanup` (INV-13, INV-16), `memory` (INV-14). Wired into `samples/Makefile`'s `check`.
- `samples/tasks/http/` — `http_limits_test` covers the HTTPS-to-HTTP redirect guard (INV-5).
- `libretro-common/samples/formats/r7z/` — a new test of `r7z_archive_folder_info` over its two-block fixture (INV-15), wired into that directory's `Makefile` beside `r7z_lzma_props_test.c`.
- Each lane is run once against a deliberately broken build of the rule it tests (the fixture's named rule removed) and must fail there before it is trusted.
- Manual, on `wintest` (Windows 10): INV-10 both routes, a portable update, and a portable update while a DLL is held open by another process (INV-9's real-world case). Manual, on Linux: INV-12's AppImage recipe. INV-2's configure recipe.
- Not covered by any test: that GitHub keeps `releases/latest` meaning "newest non-prerelease, non-draft". The parser re-checks both flags so a change there is refused rather than trusted.

## 8. Alternatives considered (and rejected)

- **Read the latest version from the buildbot's `/stable/` listing.** It is an HTML directory page with no machine-readable index (`.index`, `.index-extended`, `latest` all 404) and sorts names as text, putting `1.9.x` after `1.22.x`. Parsing HTML for a security-relevant decision was worse than one JSON call.
- **AppImageUpdate / zsync.** The stable AppImage's `.upd_info` section is empty, so there is nothing to follow.
- **Use the nightly `RetroArch_update.7z` (58 MB) for its size.** Stable only (D4). It is used for stable if upstream ever publishes one (D8).
- **Always run the installer silently.** `/D=` is ignored, so an install outside `C:\RetroArch-Win64` would gain a second copy (D9).
- **Replace files in an installer install directly.** Often needs admin rights, and Windows' installed-programs entry would keep the old version (D6).
- **Overwrite `retroarch.exe` in place on Windows.** A running executable cannot be opened for writing or deleted; it can only be renamed (measured).
- **A separate updater helper executable.** A new build target to ship in every package. The portable route needs no helper (rename), and the installer route needs only a wait, which `cmd.exe` provides.
- **A low-memory 7z unpacker, or updating only from small files.** Rejected by the user on 2026-10-20 (D10). The first is a large new decoder to save a few seconds' peak. The second never updates today, since no small stable file is published.
- **Estimate block sizes from the entry list, with the archive size as the packed size.** Chosen and then rejected by the user on 2026-10-20 (D10): the entry list does not say which blocks have more than one coder, so the doubling would rest on a guess. `r7z_archive_folder_info` reads the real figures from the header instead.
- **Revive the removed Qt updater.** Qt-only, Windows-only, nightly-only, plain HTTP and no restart; upstream removed it wanting a non-Qt method, which this is.

## 9. Out of scope

- Flathub, Snap, Steam, app stores, distro packages, macOS, consoles and mobile (D2) — deferred; not queued, by decision.
- Nightly builds (D4) — deferred; not queued, by decision.
- Updating assets, shaders, overlays and databases — the existing Online Updater items do this.
- Asking upstream to publish stable checksums, a small stable update package and a bare AppImage (D5, D8) — deferred; not yet queued.
- Measuring the x86 installer's silent folder (§14) — deferred; not yet queued.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | `samples/tasks/self_update` lanes `kind`, `none` |
| INV-2 | **nothing** automatic — a manual configure recipe |
| INV-3 | lane `version` |
| INV-4 | lane `release` |
| INV-5 | lanes `tls`, `tlsgate`; `samples/tasks/http` `http_limits_test` |
| INV-6 | lane `startup` |
| INV-7 | lane `checksum` |
| INV-8 | lane `program_set` |
| INV-9 | lane `replace` |
| INV-10 | **Partial:** lane `kind` covers the route chosen; the wait before the installer runs is manual on `wintest` |
| INV-11 | lane `marker` |
| INV-12 | **Partial:** lane `appimage` covers the rename, mode and restart target; a real AppImage restart is manual |
| INV-13 | lanes `checksum`, `replace`, `cleanup` |
| INV-14 | **Partial:** lane `memory` covers the sum; reading available memory and the reopen between blocks are not covered |
| INV-15 | `libretro-common/samples/formats/r7z/` folder-info test |
| INV-16 | **Partial:** lanes `replace`, `cleanup`; the start-up message after a repair is manual on `wintest` |
| D7 (never writes config, saves or assets) | lane `program_set` (Windows); the AppImage route writes one file by construction |

## 11. Cross-doc impact

- `CHANGES.md` `# Future`: one user-visible entry when the feature lands on the PR branch.
- `docs/private/ROADMAP.md`: RETR-0021 progress notes.
- `docs/private/upstream-prs/README.md`: the self-updater PR, when opened; the redirect-guard PR is recorded there.
- `libretro-common/` is vendored (project `CLAUDE.md`): `r7z_archive_folder_info` and its test go upstream in the self-updater PR, and the fork carries them only on that PR's way there.
- No other spec binds the code this changes. `docs/private/specs/2026-04-27-tls-verification-opt-in-design.md` defines `tls_verify_mode`, which this reads and does not change.

## 12. Cold-eyes loop log

Rows live in `../reviews/2026-10-03-self-updater-loop-log.md`.

## 13. Resource cost

- Disk, temporary: one downloaded file in staging (stable 1.22.2: Win64 `RetroArch.7z` 202,509,078 B; Win64 setup 209,037,907 B; Linux `RetroArch.7z` 179,361,448 B — buildbot listing, 2026-10-03) plus the extracted program set. Staging is removed on success and on failure.
- Memory: downloads stream to disk (`task_push_http_download_file`), hashing reads the staging file back in chunks (`sha256_stream_*`). Unpacking a `.7z` holds the whole archive, a copy of one block's packed bytes and that whole block in memory at once, the block twice over when it has more than one coder. For stable 1.22.2 the peak is about 780 MB on Linux and 750 MB on Win64 (§4.5, D10).
- Network: one small JSON request per check, one HEAD per candidate file, then one download. The start-up check, when on, runs once per start; GitHub allows 60 unauthenticated requests an hour per address. Source: https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api
- No new dependencies.

## 14. Open questions

- **The x86 (32-bit) installer's silent folder.** Not measured; x86 installer installs take the visible route until it is. Measuring it on `wintest` would let them take the silent route.
- **Command-line arguments on restart.** The existing restarts differ: `frontend_win32_respawn` passes `GetCommandLine()`, so Windows keeps the original arguments; `frontend_unix_exec` passes only the path, so Linux drops them. The AppImage restart follows the Linux behaviour; the installer route's `run.cmd` starts RetroArch with no arguments. Whether to carry arguments across is undecided.
