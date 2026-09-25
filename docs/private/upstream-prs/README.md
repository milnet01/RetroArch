# Upstream contributions — drafts (RETR-S0115)

Drafted 2026-09-26 for libretro/RetroArch and libretro/libretro-common.
**Nothing here has been pushed or opened yet.** The user has pre-approved
opening them and will not review; the next session reviews each branch
against the checklist below, then pushes and opens.

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
