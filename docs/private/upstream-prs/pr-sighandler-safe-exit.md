# PR draft — platform_unix: async-signal-safe exit on the second quit signal

Branch `pr/sighandler-safe-exit` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit on upstream/master `6bf58823c6`. Fork commit `c2a527778c`
(RETR-0017). Opened 2026-10-01 as PR #19664.

## Title

platform_unix: use _exit on the second quit signal, not exit

## Body

`frontend_unix_sighandler` force-quits on a second SIGINT/SIGTERM by
calling `exit(1)`. `exit()` is not async-signal-safe: it runs atexit
handlers and frees memory. If the second signal lands while the main
thread is inside `free()`, as it does during a slow shutdown, the
handler blocks on the allocator's lock forever. So the force-quit hangs
instead of quitting, which is the opposite of what a second Ctrl+C
asks for.

The fix is `_exit(1)`, which is async-signal-safe. The third-signal
`abort()` path is unchanged.

### How it was found

Two menu profiling runs hung at shutdown (the profiler sends SIGINT,
then SIGTERM). The core dumps of both show the same main-thread stack:

```
__lll_lock_wait_private
_int_free_chunk
__run_exit_handlers
exit
frontend_unix_sighandler
<signal handler called>
_int_free_chunk
string_list_free
core_info_free
core_info_deinit_list
driver_uninit
```

### Testing

Linux, null video driver, XMB menu. A script starts RetroArch, sends
SIGINT, and sends a second SIGINT when the log prints
`Unloading core...`, then checks whether the process exits within 10 s:

| Build | Hangs |
|---|---|
| our fork before the fix (same handler code as master) | 8 of 20 |
| our fork with the fix | 0 of 20 |
| this branch | 0 of 20 |

Builds clean with `make` and with `C89_BUILD=1`.

Made with Claude Code, reviewed and build-tested on our fork.
