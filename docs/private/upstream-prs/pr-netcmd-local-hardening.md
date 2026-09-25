Title: command: bind the network command socket to loopback and bound reads

## Problem

With `network_cmd_enable` on, the UDP command interface binds to 0.0.0.0. Any host on the network can send `LOAD_CORE`, `WRITE_CORE_RAM` or `WRITE_CORE_MEMORY`.

The handlers also trust their length arguments. `READ_CORE_RAM` and `READ_CORE_MEMORY` compute `40 + nbytes * 3` / `64 + nbytes * 3` in `unsigned int`. With nbytes around 1431655745 this wraps to a few bytes, and the reply is written past the allocation. `WRITE_CORE_RAM` writes the whole payload with no limit.

## Fix

- Bind the command socket to 127.0.0.1. Remote use can go through a forwarded port.
- Reject read requests above 16 KiB (outside `HAVE_CHEEVOS`, since `READ_CORE_MEMORY` is not cheevos-only).
- Stop `WRITE_CORE_RAM` after 4096 bytes, with a warning.
- `rcheevos_filter_url_param`: `strcpy` on overlapping ranges replaced with `memmove`.
- `config_save_file`: chmod the config to 0600 on POSIX platforms, since it holds passwords and tokens.

The loopback bind changes behaviour for anyone driving RetroArch from another machine. If you would rather keep LAN access behind a setting, I can add one. The 16 KiB and 4096 limits are also open to adjustment.

## Testing

- Linux: full build; touched files compile with `C89_BUILD=1`, no warnings.
- `ss -ulnp` shows the socket on 127.0.0.1 only.
- A config saved on exit is mode 600 under umask 022.
- `READ_CORE_MEMORY 0 1431655745` is rejected and RetroArch keeps running.

This change was found and written with the help of Claude Code (an AI assistant), then reviewed and build-tested on a fork. Happy to adjust anything.
