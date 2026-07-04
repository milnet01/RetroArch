# Security standard (fork)

The fork's security **engineering** contract: the threat model it defends,
the secure-coding rules every change is held to, what has been hardened,
and the risks that are knowingly accepted (with their mitigation state).

## Relationship to the root `SECURITY.md`

The repo-root `SECURITY.md` is upstream RetroArch's **public vulnerability-
reporting policy** (report to `libretro@gmail.com`, supported delivery
channels, remediation posture). This fork does **not** modify it — that
would diverge from upstream and conflict on every re-sync. This document is
the separate, internal engineering standard. Report a vulnerability via the
upstream channel in `SECURITY.md`; write secure code per the rules here.

## 1. Threat model

RetroArch is a libretro frontend. Its trust boundaries:

- **Cores are unsandboxed and untrusted-by-design.** A core can read,
  write, and delete files, spawn processes, and use the network; RetroArch
  does not restrict it, binaries are unsigned, and core source is not
  necessarily under libretro control (root `SECURITY.md`). **This is
  accepted, not a bug** — do not file "a core can do X" as a finding.
- **Attacker-controlled *data*, by contrast, is in scope.** Everything RA
  parses from an untrusted source must be treated as hostile:
  - network peers — UPnP/IGD router responses, netplay packets, the
    localhost IPC command socket;
  - remote services — cloud-sync manifests + downloaded files (Google
    Drive / WebDAV / S3), RetroAchievements + updater HTTPS;
  - shared files — save states, BSV replays, playlists, thumbnails,
    config a user imported.
- **Secrets at rest** — OAuth refresh tokens, AWS keys, stream keys,
  WebDAV/SMB/netplay passwords in `retroarch.cfg`.

## 2. Secure-coding rules (mandatory)

These are the forward-looking form of the bug classes the audit has closed;
every change is held to them.

1. **Bounds-check every length/count read from the wire or a file before
   it drives an allocation, index, or copy.** Cap it against a sane maximum
   *before* any multiplication that could wrap. (natt UPnP parser
   `9953abc994`; `command.c` `command_write_ram` 4096-byte cap +
   `command_read_ram/_memory` overflow guard `8aedb937f1`; save-state /
   BSV parsers `1f59d9d31d`.)
2. **NULL-check every allocation whose failure is reachable, before the
   first deref** — including the OOM path. Free partial state on the error
   label. (Cloud-sync request builders `794a7a04bf`; many bundles.)
3. **Reject path traversal in any attacker-supplied path component**:
   treat NULL, empty, leading-slash, or `..`-containing keys as malformed
   and short-circuit. (Cloud-sync manifest `18b55ec71a`.)
4. **Write user data atomically**: write to `<dest>.tmp`, then
   delete-dest + rename. Never truncate the destination in place — power
   loss mid-write must not destroy the previous good file. (`403106af54`,
   `b510d9d654`.)
5. **TLS fails closed by default.** Certificate verification is `REQUIRED`
   unless the user has explicitly selected a weaker mode. (`net_socket_ssl_mbed.c`
   secure core, Bundle 87 `436928da68`. The opt-out consent dialog / on-screen
   warning UX is still deferred — see §4.)
6. **Secrets compared on the wire use a constant-time compare; security-
   critical nonces/salts come from a CSPRNG**, not `time()`/LCG. (Netplay
   salt + constant-time compare `d973d6cc0c`.) Where no CSPRNG is wrapped
   in-tree yet, a replay-defeating nonce is at minimum freshly derived per
   request — the WebDAV digest `cnonce` (`2386e898d4`) is MD5-mixed from
   `time`/`clock`/stack-address/nonce-count, is **explicitly not a CSPRNG**,
   and defeats wire replay only; upgrade it once libretro-common wraps a
   CSPRNG.
7. **Network listeners bind loopback by default.** No unauthenticated
   service on `0.0.0.0`; cross-host access is the user's explicit opt-in
   (or an SSH forward). (`command.c` `127.0.0.1` bind `8aedb937f1`.)
8. **No uninitialized reads on branch conditions** — initialize a stack
   buffer/struct before any path can read it. (`ab7f4d0f6e`; drm_gfx
   `connector` Bundle 89.)

A change that reintroduces one of these classes is a release blocker, not a
style nit.

## 3. Hardening status (summary)

Note on verification: the fix commits cited throughout this document live
on `local/fixes-2026-04` (worked in the `/tmp/ra-fixes` worktree), not on
the `local/audit-2026-04` docs branch — a checkout of the audit branch may
still show the pre-fix source. See `docs/private/ROADMAP.md` and the fork's
two-branch model.

Closed across the audit bundles (see `docs/private/ROADMAP.md` for the
per-finding record and commits): network-command socket bind + wire-driven
OOB read/write; UPnP parser NULL-deref/recursion; WebDAV digest parser +
cnonce; netplay CSPRNG + constant-time compare; cloud-sync OOM-null-deref
cluster, path traversal, and atomic downloaded-file writes; save-state /
BSV buffer-overflow class; TLS verification secure core (mbedtls
fail-closed; BearSSL fails closed natively — **secure core only; the
remaining phases are open, see §4**).

## 4. Accepted-risk register

Known, deliberately-not-fully-closed items. Each names its mitigation and
why the residual is accepted; revisit when the mitigation tier advances.

| Risk | State | Residual accepted because |
|---|---|---|
| Cores run unsandboxed | By design | Core of the libretro execution model; documented in root `SECURITY.md`. |
| Plaintext credentials in `retroarch.cfg` | Mitigated (Tier 1) | File `chmod 0600` after save (`8aedb937f1`). Secrets-file split + OS-keyring (Tiers 2–3) deferred; see ROADMAP Tier-1 + Strategic follow-ups. |
| TLS non-mbedtls/BearSSL backends & consent UX | Partial — consent/warning UX + other backends deferred | mbedtls fails closed; BearSSL fails closed natively. Consent dialog, on-screen warning, other backends, and the upstream PR are deferred (spec Phase 4–5). |
| `libretro-common/file/config_file.c` cfg save not atomic | Deferred | Vendored upstream — only fixable by upstreaming (ROADMAP cross-cutting item). |
| Localhost IPC command socket is unauthenticated | Accepted | Bound to `127.0.0.1`; cross-host use is an explicit SSH-forward opt-in. |

Any addition to this register requires a one-line rationale here — an
undocumented "we'll get to it" is not an accepted risk, it's an open bug.

## 5. Regression coverage (open gap)

The CRITICAL/HIGH wire- and file-parser fixes above (UPnP, command IPC,
cloud-sync, save-state/BSV) shipped **without** regression tests, so an
upstream re-sync or a nearby refactor could silently revert one. (The
`libretro-common` libcheck suite, made green in Bundle 76, covers utility
code — not these fixes.) Closing
this is the highest-value security follow-up — see ROADMAP §Strategic
follow-ups. New security fixes should land with a test (a `.ratst` replay
or a `libretro-common` libcheck case) that fails on the pre-fix code.
