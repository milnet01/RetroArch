# PR G — config: re-apply tls_verify_mode on every settings load

Branch `pr/tls-verify-on-reload`, one commit on upstream/master
`41caa78885`. Fork source: `9552e7e2f0` (RETR-0008). Approved by the user
2026-09-28.

**Title:** config: re-apply tls_verify_mode on every settings load

**Body:**

## Problem

`tls_verify_mode` (#19626) reaches the SSL backend through
`ssl_socket_set_verify_mode`, which has two callers: startup in
`retroarch.c` and the menu write handler in `menu/menu_setting.c`.

A per-core or per-game override, unloading one, and the reload after
saving one all change `settings->uints.tls_verify_mode` without calling
it. The backend keeps the previous mode until RetroArch restarts. That
mode can be looser than the one now configured, for example `Optional`
when an override asks for `Required`.

## Fix

`config_load_file` is the one path all of those take. It now hands the
mode to the backend at its end. The startup call stays, because it also
covers a start where no config file was loaded and defaults apply.

## Testing

- Linux: full build. `configuration.c` compiles with `C89_BUILD=1`, no
  warnings.
- Not tested at runtime: that needs a core, content, an override that
  changes the mode, and a live HTTPS connection.

This change was found and written with the help of Claude Code (an AI
assistant), then reviewed and build-tested on a fork. Happy to adjust
anything.
