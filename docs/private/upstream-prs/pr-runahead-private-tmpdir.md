# PR draft — runahead: copy the core only into a private temp dir

Branch `pr/runahead-private-tmpdir` in `/mnt/Games/Scripts/Linux/ra-pr`,
one commit on upstream/master `6bf58823c6`. Not opened: the user decides.
Fork commit `30b7dc942f` (RETR-0006). Fixes issue #19632.

## Title

runahead: copy the core only into a private temp dir

## Body

Fixes #19632.

Run-ahead's second instance copies the core to
`<tmp>/retroarch_temp/<core>`. On Linux, macOS and the BSDs `<tmp>` is
usually the shared `/tmp`, and the copy opens its destination
create-or-truncate, following links. Another local user who creates
`retroarch_temp` first can plant a link there for the copy to overwrite,
or replace the copied core before it is loaded and run code as the
player.

This adds `runahead_tmp_dir_private()`. It accepts the directory only if
it is a real directory, not a link, owned by the current user. If others
can reach it, it is tightened to 0700. Otherwise run-ahead logs a warning
and falls back to the single-instance method, as it does for any other
copy failure. Windows and Android temp dirs are per-user, so they are
not checked.

With the directory private, the predictable file names no longer expose
anything, so the name code is unchanged.

### Testing

- `samples/runahead`: a new case, `tmp_dir_not_private`, fails before
  the change (a copy is opened and reported ready) and passes after.
  The whole harness passes, plain and under ASan+UBSan.
- The helper against real directories: a 0777 directory we own is
  tightened and accepted; a link, a root-owned directory and a missing
  one are refused.
- Live, gambatte with run-ahead's second instance and `TMPDIR` set: a
  0777 `retroarch_temp` became 0700 and was used; a linked one was
  refused, nothing was written through the link, and run-ahead fell
  back to one instance.
- `runahead.o` builds clean, normally and with `C89_BUILD=1`.

Made with Claude Code, reviewed and build-tested on our fork.
