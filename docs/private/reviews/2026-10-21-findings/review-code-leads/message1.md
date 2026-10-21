## Message 1 — the lanes' reports

Relayed by severity: each lane's CRITICAL, HIGH and MEDIUM sections are below verbatim, with each lead's verdict line. LOW and INFO findings, each lead's full evidence, and the lanes' open questions are held back here and are whole in the lane files named under each lane. I dropped only the lanes' preambles (line counts, what was in context on arrival).

Contents:
1. lane-01-su-core — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-01-su-core.result
2. lane-02-su-task — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-02-su-task.result
3. lane-03-r7z — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-03-r7z.result
4. lane-04-menu-drivers — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-04-menu-drivers.result
5. lane-05-menu-glue-frontend — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-05-menu-glue-frontend.result
6. lane-06-cloud-sync-s3build — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-06-cloud-sync-s3build.result
7. lane-07-replay-runahead — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-07-replay-runahead.result
8. lane-08-leaf-modules — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-08-leaf-modules.result
9. lane-09-build-tooling — /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-09-build-tooling.result

### lane-01-su-core
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-01-su-core.result

Lead verdicts:
- A1: VERDICT: not a defect. No build can reach it.
- A2: VERDICT: real, LOW.
- A3: VERDICT: real, LOW.
- A4: VERDICT: not a defect.
- A5: VERDICT: real, LOW (it breaks a project rule; no behaviour changes).
- A6: VERDICT: not a defect.

## Critical (0)

## High (0)

## Medium (1)
- [dim 9] `self_update.c:575-578` — `ops->rename(ops->ud, old, cur);` — `su_undo` ignores both rename results, so `self_update_replace` returns the same `-1` whether the rollback worked or not. If restoring `p.retroarch-old` → `p` fails:
  - the caller reports a "file in use" error, as if nothing had changed;
  - the caller then deletes the staging copy (`task_self_update.c:777`);
  - at the next start, `self_update_cleanup` (`:735-737`) deletes every `*.retroarch-old`, which is now the **only copy** of the original file.
  
  The result is a missing program file, against INV-9 ("all or nothing"). Fix: `su_undo` reports failure, and `replace` returns a separate "rollback incomplete" code. On that code the caller warns the user and keeps the `.retroarch-old` files (for example, by writing a note that start-up checks before cleaning).


### lane-02-su-task
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-02-su-task.result

Lead verdicts:
- B1: VERDICT: real (LOW)
- B2: VERDICT: real (LOW)
- B3: VERDICT: real (LOW). Latent with the current `version.all`.
- B4: VERDICT: not a defect
- C1: VERDICT: real (LOW). The return value is ignored, and most of the false paths report themselves; three do not.
- G1: VERDICT: not a defect. The code follows its spec. The project rule's blanket wording disagrees with the spec, and that is a document-side matter.

## Critical (0)

## High (0)

## Medium (0)


### lane-03-r7z
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-03-r7z.result

Lead verdicts:
- D1: VERDICT: real. The defect is in the document, not the code. The wrong part is older than the fork, but the fork made it worse.
- D2: VERDICT: not a defect.
- D3: VERDICT: not a defect. The short-size path cannot be reached.

## Critical (0)

## High (0)

## Medium (0)


### lane-04-menu-drivers
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-04-menu-drivers.result

Lead verdicts:
- M1: VERDICT: upstream code, real — but not as the lead describes it. There is no read outside the list. The real defect is an endless loop.
- M2: VERDICT: upstream code, not a defect.
- M3: VERDICT: not a defect. That state cannot be seen by other code.

## Critical (0)

## High (0)

## Medium (1)
- [dim 16] [lead M1] `materialui.c:11058`, `rgui.c:9171`, `ozone.c:9115`, `xmb.c:6938` — `while (new_selection == selection || entry_new.type != FILE_TYPE_PLAYLIST_COLLECTION)` — the Scan (random) action on the playlists tab never ends when there are no collections, or when the only collection is the one already selected. The menu freezes. Fix: count the collections other than the current one first; if there are none, return `MENU_ACTION_NOOP`. Otherwise pick from that set rather than drawing again and again.


### lane-05-menu-glue-frontend
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-05-menu-glue-frontend.result

Lead verdicts:
- G1: VERDICT: upstream code, not a defect (latent).
- G2: VERDICT: upstream code, not a defect.
- G3: VERDICT: upstream code, not a defect.
- F1: VERDICT: real, but as a weak guard rather than a current overflow.
- F2: VERDICT: upstream code, real.
- F3: VERDICT: real, and wider than the lead says.
- F4: VERDICT: not a defect (both copies are fork-era code; the fork's claim is correct).

## Critical (0)

## High (0)

## Medium (1)
- [dim 9] [lead F3] `configuration.c:9780`: `(void)chmod(path, S_IRUSR | S_IWUSR);`. Every save writes the credentials to a fresh 0644 `.tmp` file and renames it over the 0600 config before this `chmod` runs. A descriptor opened in that window stays readable afterwards.
  - Fix: create the temporary with mode 0600, before any data is written. Either `open(tmp, O_CREAT|O_WRONLY|O_TRUNC, 0600)` and `fdopen`, or an `fchmod` before the dump.
  - That change is in vendored `libretro-common`, so it needs to go upstream or behind a flag. A fork-local alternative is to bracket the call with `umask(077)`, but that is process-wide and races other threads creating files.


### lane-06-cloud-sync-s3build
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-06-cloud-sync-s3build.result

Lead verdicts:
- L1: VERDICT: real, but no current caller is affected (latent).
- L2: VERDICT: real (it already existed in upstream; the fork made it worse).
- L3: VERDICT: not a defect in `cloud_sync_http_body_is_framed`. The RFC gap is in upstream `net_http`, and it is real but low.
- L4: VERDICT: not a defect (it fails safe). The header comment overstates what the helper accepts.
- S5: VERDICT: not a defect (no link failure).

## Critical (0)

## High (0)

## Medium (1)
- [dim 3] [lead L2] `network/cloud_sync/webdav.c:332` — `if (!webdav_st->qop_auth) return false;` (and the other bare `return false` exits in the parse loop) — after `webdav_cleanup_digest()`, these exits leave `cnonce`/`nonce`/`realm` NULL while `basic` stays false. The next request then calls `strlen(webdav_st->cnonce)` in `webdav_create_digest_auth_header`, a NULL dereference: a server-triggered crash. — fix: send every exit to `parse_fail`, and make `webdav_create_digest_auth_header` return NULL when `ha1hash`, `nonce`, `realm` or `cnonce` is NULL.


### lane-07-replay-runahead
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-07-replay-runahead.result

Lead verdicts:
- R1: VERDICT: real (fork code, LOW)
- R2: VERDICT: upstream code, real (MEDIUM)
- R3: VERDICT: not a defect (the only effect is less deduplication)
- R4: VERDICT: not a defect as framed; a nearby upstream issue is real (LOW, below)
- R5: VERDICT: upstream code, real (LOW)
- R6: VERDICT: not a defect (cannot be reached)
- R7: VERDICT: not a defect; keep the check
- R8: VERDICT: not a defect (all four causes refuse the directory)

## Critical (0)

## High (0)

## Medium (1)
- [dim 3][lead R2] `input/input_driver.c:7961` — `RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[i])[turbo_btn_id])`: an `input_turbo_bind` from a config file or override outside -1..`RARCH_BIND_LIST_END-1` reads out of bounds on every poll, even with turbo off. Fix: clamp at load in `configuration.c:10505` to the menu's range, or apply the same range check as line 2140 before 7948.


### lane-08-leaf-modules
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-08-leaf-modules.result

Lead verdicts:
- P1: VERDICT: real. Fork code, but the cause is not the one the lead gives.
- P2: VERDICT: real. Same shape as P1, in vendored code.
- T1: VERDICT: real (rendering defect), not out of bounds. The 54-byte offset and the no-padding assumption are upstream; the fork's size check and stride are consistent with them.
- T2: VERDICT: upstream code, real but latent.
- T3: VERDICT: real, LOW.
- V1: VERDICT: not a defect in the code. The comment is wrong.
- V2: VERDICT: not a defect (debug output only).
- V3: VERDICT: not a defect.

## Critical (0)

## High (0)

## Medium (2)
- [dim 9] [lead P1] `tasks/task_save.c:430` — `filestream_delete(path);` — The rename it falls back from already replaces the destination on every platform. So this delete only runs after a failure that deleting won't fix, and it destroys the last good save. The data is then left in `.tmp`, which nothing loads and the next save truncates. — fix: drop the delete-and-retry and leave both files in place on failure. Also correct the comment about Win32.
- [dim 9] [lead P2] `libretro-common/streams/file_stream.c:2272` — `filestream_delete(path);` — Same defect, reached from SRAM saves, the disk index and cloud sync. — fix: the same, and send it upstream so a re-sync does not undo it.


### lane-09-build-tooling
File: /home/ants/.local/share/claude-scratch/review-code-2026-10-21-leads/lane-09-build-tooling.result

Lead verdicts:
- B1: VERDICT: real. It is wider than the lead says.
- B2: VERDICT: real. Yes, the script claims otherwise.
- B3: VERDICT: real.
- B4: VERDICT: real, narrow. Whether it is upstream code is unsettled: I have no patch to check it against.

## Critical (0)

## High (3)
- [dim 2] [lead B1] `local-CI.sh:366` — `if ! ( cd "$d" && make clean all ); then` — common-samples has no sanitizers, about 19 of the workflow's 80 run-targets, no classification gate, and none of the TSan or extra jobs. The comment at `:345-347` claiming an "exact block … single source of truth" is false. **Fix:** extract the `run:` block from the workflow at run time (for example with `yq`) or run the file through act. Until then, move it out of `NATIVE_WORKFLOWS`.
- [dim 2] [lead B2] `local-CI.sh:288` — `job_samples_tasks()` — 6 of about 37 steps, and none of the `samples-tasks-http` job. **Fix:** run `Linux-samples-tasks.yml` through act at `-j4` and drop the hand mirror, or rename the job "smoke subset" and take the file out of `NATIVE_WORKFLOWS`.
- [dim 2] [lead B3] `local-CI.sh:388-391` — `./configure --disable-qt --enable-xdelta … make -j"$JOBS" info all` — skips `include_order_check.sh` and the stack-frame sweep. **Fix:** add both steps to the container command.

## Medium (4)
- [dim 16] `local-CI.sh:350` — `( cd "$w/libretro-common/samples"` with no `&&` and no `set -e`.
  - **Trigger:** `mktemp -d` or `export_tree` fails, for example on a full disk. Note that `export_tree`'s status is never checked.
  - **What happens:** the `cd` fails, and the subshell carries on in the caller's directory, which is the repo root under the pre-push hook. `find . -name Makefile` then runs `make clean all` in every Makefile directory of the **working tree**, including the top level.
  - That breaks the promise at `:28-29` that the working tree is never touched.
  - `job_c89` `:260` has the same shape. With `w=""`, `cd ""` succeeds and `make clean` runs in the working tree.
  - **Fix:** `cd … || exit 1`, and check `mktemp` and `export_tree`.
- [dim 7] `local-CI.sh:262-270` — `make $JPAR -k C89_BUILD=1` — the exit status of make is ignored. Only `error:` lines decide the result, so a failure that prints no `error:` makes the job PASS. Examples: `make: *** No rule to make target`, a Makefile syntax error, a killed compiler. **Fix:** record make's status, and fail when it is non-zero and no `error:` line is under `/usr/`.
- [dim 11] `local-CI.sh:440-442` — `"$ACT_BIN" -W ".github/workflows/$wf" … --concurrent-jobs 1` — the act workflows build at `-j$(nproc)`. Confirmed in `Linux-asan-ubsan.yml`, `Linux-vulkan-validation.yml`, `Linux-samples-audio.yml` (58 hits), `Linux-menu-harness.yml` and others. So `--all` breaks this machine's `-j4` ceiling, and `LOCAL_CI_JOBS` does not reach them. **Fix:** inject a cap, for example `--env MAKEFLAGS=-j4` plus a `nproc` shim, or `podman --cpus=4` through act's `--container-options`.
- [dim 2] `local-CI.sh:18-24` — "Two of the five Linux jobs run inside a fixed container image on CI; those are reproduced bit-exactly". This is contradicted by `:57-59` and `Linux-Headless.yml:24`: headless runs on `ubuntu-latest`. Locally it runs in a xenial gcc9 image, so warnings and errors that only a newer gcc raises are missed. **The document is the wrong side here.** **Fix:** reword it so headless is "different compiler from CI".


