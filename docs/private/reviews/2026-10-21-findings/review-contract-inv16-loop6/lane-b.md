**Subject line count:** `2026-10-03-self-updater.md` has 337 lines, read in full with native `Read` from the scrubbed copy, which the brief says has the same line numbers as the original.

**Already in my context when I arrived:**
- The global `~/.claude/CLAUDE.md`.
- The user's email address, attached automatically by Claude Code.
- A SessionStart hook message about the Ants MCP verbs.
- Environment details: working directory `/home/ants/.cache/lane-neutral`, Linux, and the date.

There was no project `CLAUDE.md`, no memory index and no git snapshot. My brief (`shared-context.md`, which includes the context packet) was read in full.

**Disagreements between the brief and my agent file:** none came up.

**Pass type:** the packet carries no `BREADTH PASS` marker, so I treated this as a depth pass.

## Findings

- **[Q2]** `docs/private/specs/2026-10-03-self-updater.md:241` — "A failed undo never costs the old file." This conflicts with the fallback the spec gives at `:144`: "Without the record, a `p.retroarch-old` whose `p` is missing is still renamed back rather than deleted, in case the record could not be made."
  - That fallback only covers the case where `p` is missing. Take an undo where `cur→src` fails: the new file stays at `p` and the old one sits at `p.retroarch-old`.
  - If the record cannot be made in that state (`mkdir_p` fails, perhaps for the same reason the rename failed), the next start finds no record. The `:144` rule then deletes `*.retroarch-old` beside a present `p`, so the only old copy is lost and a mixed install is left.
  - A retry before restart loses it the same way. The replace loop removes a stale `p.retroarch-old` whenever `p` exists (`self_update.c:449-450` in the packet window, file 767 lines). With no record, nothing blocks that.
  - INV-16's own `:243` lists both outcomes as breaches: "cleanup deletes the only copy of a program file or leaves a mixed install in place, a retry before restart deletes a set-aside old file". Built as written, the design satisfies the INV-16 tests and still breaks the invariant whenever the record cannot be made. The tests always succeed in making the record, so they cannot see this.
  - → An implementer would make record creation best-effort. They would treat "record could not be made" like any other failure, and the old-file deletion paths would fire.
  - → fix: either have the caller (`tasks/task_self_update.c`, which can write files) create the record when `self_update_replace` reports incomplete rollback and the record folder is missing, or state INV-16's limit ("…when the record can be made") and add a test row where `mkdir_p` fails during the undo.

## Confirmed

- I checked the `fileops` table in `self_update.h:175-232` (the window shows `struct self_update_fileops`). It has `mkdir_p` and `read` but no write operation, so `:141`'s "can make a folder but not write a file" is true.
- I read `self_update_replace` and `su_undo` in the packet window. When an undo fails, the new file stays at `p` and the old one at `p.retroarch-old`, which is the state the finding depends on. A stale `.retroarch-old` is removed only when `p` exists.
- I read the existing `lane_cleanup` (`self_update_test.c:728-776`). It plants `.retroarch-old` files with no originals and asserts they are gone. Under the new rule those files get renamed back, so `olds_in == 0` still passes but the test no longer separates the two rules. INV-13's "beside their originals" (`:230`) already requires the fix, so this is a test change, not a spec defect.
- I ran no searches, so no exclusion glob was needed. I opened nothing outside the packet and the subject.

## Open questions

- **A stale `p.retroarch-new` during repair.** The repair at `:144` renames `p` to `p.retroarch-new` and never says what happens when one already exists. That can happen if an earlier start left one "in use", and `*.retroarch-new` is not deleted while a record exists. With `MoveFileW` and no flags (packet fact), the repair rename could then fail on every start: the record would never clear and `self_update_replace` would stay blocked. The Linux test `rename` overwrites, so it would not show this. Is this left for the builder to settle, or should the spec say "remove a stale `p.retroarch-new` first unless it is the only copy"?
- **Whether a restart can run the repair at all.** "Restarting RetroArch finishes the undo" (`:141`) assumes the half-undone build can start and reach cleanup. Suppose the undo restored the old `retroarch.exe` but left a new DLL in place. Whether that build loads is Windows runtime behaviour, which the packet puts out of scope, so I could not settle it.

## NEEDS MEASUREMENT

- Whether `MoveFileW` with no flags fails when the target name exists. This decides whether the first open question is real. — On `wintest`: `cmd /c "echo a>x & echo b>y & move /-Y x y"` is only a rough stand-in. The real check is a minimal C call of `MoveFileW(L"x", L"y")` with `y` already present, and a look at its return value.
