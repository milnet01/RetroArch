# PR draft — webdav: a fresh client nonce for each digest challenge

Branch `pr/webdav-random-cnonce` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit (`c76212a34d`) on upstream/master `6bf58823c6`. The fork's own
version is `webdav_create_cnonce` on `local/fixes-2026-09`, written before
upstream had a crypto library; it mixes time, clock and a stack address
through MD5. This branch uses `crypto_random_bytes` instead (RETR-S0115).

## Title

webdav: a fresh client nonce for each digest challenge

## Body

WebDAV digest authentication sends `cnonce="1a2b3c4f"` on every login:
`webdav_create_digest_auth` sets it to that constant. RFC 7616 §5.9 says
a MITM or a malicious server can choose the nonce the client hashes, and
that the countermeasure is the client varying the input through the
cnonce. With the cnonce fixed, the attacker controls every input, which
opens the precomputed dictionary attack of §5.10.

This change makes a new 32-hex-digit cnonce for each challenge, from 16
bytes of `crypto_random_bytes`. Without `HAVE_CRYPTO`, or when no entropy
source answers, it falls back to an MD5 of the microsecond clock and a
counter. That still differs per challenge, but it is not unpredictable.
The cnonce now lives in a 33-byte buffer in the WebDAV state instead of a
`const char *`, so nothing new is allocated.

### Testing

Linux, null drivers, a fresh config with WebDAV cloud sync in automatic
mode, pointed at a local test server. The server sends a fresh digest
challenge and checks the response against
`MD5(HA1:nonce:nc:cnonce:qop:HA2)`:

| Build | cnonce sent | Server's check | Sync |
|---|---|---|---|
| master | `1a2b3c4f` | valid | completes |
| this branch, run 1 | `727146624ede69033f3f318766925114` | valid | completes |
| this branch, run 2 | `602782b46ad8ca21324f82efc33a8eb6` | valid | completes |

Builds clean with `make`. `webdav.c` compiles as gnu89 with
`-Werror=declaration-after-statement`, both with and without
`HAVE_CRYPTO`.

Made with Claude Code, reviewed and build-tested on our fork.
