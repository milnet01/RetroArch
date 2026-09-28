# PR H — netplay: OS CSPRNG salt and constant-time password compare

Branch `pr/netplay-password-hardening`, one commit on upstream/master
`41caa78885`. Fork source: `c6d9f37dc7` (April 2026), adapted for
upstream's platform range. Approved by the user 2026-09-28.

**Adapted from the fork, which should take this version back:**
- Desktop Windows uses `CryptGenRandom`, not `BCryptGenRandom`, which
  needs Vista and a `bcrypt` link the mingw Makefiles lack. RetroArch
  still ships MSVC 6/2003/2005 projects.
- macOS uses `/dev/urandom`, not `arc4random_buf`, which PowerPC builds
  (10.5) lack.
- The platform headers moved below the socket headers, so `<windows.h>`
  arrives through winsock2.

**Title:** netplay: draw the password salt from the OS CSPRNG, compare in constant time

**Body:**

## Problem

The netplay password challenge has two weaknesses.

- **A predictable salt.** `netplay_handshake_init_send` takes the salt
  from `simple_rand`, an LCG seeded with `time(NULL)` on first use.
  Anyone who sees one salt can recover the LCG state and predict every
  later one, so a captured hash can be attacked offline ahead of the next
  connection.
- **A timing leak.** `netplay_handshake_pre_password` checks the
  client's hash with `memcmp`, whose early exit reveals how many leading
  bytes matched.

## Fix

- `netplay_secure_random_bytes()` fills the salt from the platform
  source:
  - desktop Windows: `CryptGenRandom`, the call
    `deps/mbedtls/entropy_poll.c` already makes, so it works with the
    XP-era toolchains;
  - UWP: `BCryptGenRandom`, since CryptoAPI is unavailable there;
  - glibc 2.25+: `getrandom`;
  - elsewhere: `/dev/urandom`, which covers macOS (including PowerPC),
    the BSDs and Android.
- Where none is available (consoles), the LCG stays as the fallback,
  re-seeded from microsecond time mixed with the connection's address
  rather than the wall-clock second.
- `netplay_constant_time_eq()` replaces both `memcmp` calls in the
  password check.

The salt travels in the same header field as before, so the protocol and
older clients are unaffected.

## Testing

- Linux: full build. `netplay_frontend.c` compiles with `C89_BUILD=1`,
  no warnings.
- Windows: the file compiles with mingw-w64 (`-std=gnu89 -Wall`, no
  warnings). The two new functions, extracted verbatim, ran on
  Windows 10 22H2: five salts, all distinct and all reported as
  successful; equal buffers compare equal; a one-bit change in the first
  or last byte compares unequal.
- A host with a password set starts, issues the challenge and waits for
  the reply (fceumm, local host and client).
- Not tested: a complete password handshake. The client asks for the
  password through the on-screen keyboard, which I could not drive from a
  script. Also untested: macOS, the BSDs, UWP, and consoles.

This change was found and written with the help of Claude Code (an AI
assistant), then reviewed and build-tested on a fork. Happy to adjust
anything.
