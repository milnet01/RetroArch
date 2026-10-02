# PR draft — metainfo: add releases 1.20.0 to 1.22.2

Branch `pr/metainfo-releases` in `/mnt/Games/Scripts/Linux/ra-pr`,
one commit on upstream/master `bfb6bb0a58`: `8d88874b47` (RETR-0020). Opened
2026-10-02 as libretro/RetroArch#19689.
Fork commits `cfb745a736` and `13602e0689`.

## Title

metainfo: add releases 1.20.0 to 1.22.2

## Body

`com.libretro.RetroArch.metainfo.xml` lists releases only up to 1.9.11
(October 2021). Software centres such as GNOME Software and KDE
Discover show that as RetroArch's latest release.

This adds entries for 1.20.0, 1.21.0, 1.22.0, 1.22.1 and 1.22.2:

- Each date is the release tag's date, and matches the GitHub release.
- Each feature list summarises that release's section in `CHANGES.md`.
  1.22.1 and 1.22.2 get a one-line entry pointing to the GitHub
  release notes.
- `CHANGES.md` has no 1.22.0 heading. The list under `# 1.22.1`
  describes code already in the v1.22.0 tag, so this file puts it
  under 1.22.0.
- 1.10.x to 1.19.x are not filled in.

### Testing

`appstreamcli validate --no-net` passes. It gives the same two info
messages as the current file.

Made with Claude Code, reviewed and build-tested on our fork.
