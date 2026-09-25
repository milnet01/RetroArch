Title: net/tls: verify server certificates by default

## Problem

`libretro-common/net/net_socket_ssl_mbed.c` sets `MBEDTLS_SSL_VERIFY_OPTIONAL` on every connection and never acts on the verification result. A TLS handshake therefore succeeds against any certificate. Anyone who can intercept the traffic can impersonate the Online Updater, RetroAchievements and Cloud Sync servers, and serve cores or read credentials.

## Fix

- New setting `tls_verify_mode`: Required (default), Optional, Disabled.
- Required: a bad certificate fails the handshake and the reason is logged.
- Optional: the old behaviour, now with a logged warning.
- Disabled: no verification, with a warning on every connection.
- Applied at startup and when the setting changes; takes effect on the next connection.
- Logging uses two hooks the backend defines as weak no-ops, so libretro-common still builds standalone. `network/tls_log.c` routes them to the RetroArch log. Griffin skips the weak versions because `tls_log.c` is in the same unit.
- BearSSL already fails closed; it gets a no-op setter.

## Testing

- Linux, bundled mbedTLS: full `make` succeeds; touched files compile with `C89_BUILD=1`, no new warnings.
- A small test program linked against the built objects, in Required mode: buildbot.libretro.com, thumbnails.libretro.com, retroachievements.org and the Google APIs hosts connect; self-signed.badssl.com and expired.badssl.com are refused with the reason logged. Optional and Disabled connect to the self-signed host and log warnings.
- Not tested: griffin/console builds, MSVC, BearSSL.

Note for review: MSVC has no weak symbols, so a non-griffin MSVC build of libretro-common alone would need `tls_log.c` or its own definitions of the two hooks.

This change was found and written with the help of Claude Code (an AI assistant), then reviewed and build-tested on a fork. Happy to adjust anything.
