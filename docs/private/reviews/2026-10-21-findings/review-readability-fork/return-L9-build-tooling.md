I found eight readability problems in the fork's build scripts. Three of them are rated HIGH, and they share one theme: local-CI.sh's comments say it repeats the real CI exactly, but in several places it runs a smaller hand-made copy that has drifted from the real one. Both tool candidates are confirmed.

## Files read
- `patch-L9-build-tooling.diff` — 593 / 593
- `local-CI.sh` — 510 / 510 (a new file, so read in full)
- `qb/qb.comp.sh` — 166 / 166
- `qb/qb.make.sh` — 26 / 26
- `Makefile` — lines 1–60, 150–175 and 300–345 (the hunk is at 332–339); total length not measured
- `Makefile.common` — lines 1030–1060 and 2460–2495; total not measured
- `qb/config.libs.sh` — lines 1–60 and 215–235; total not measured
- `qb/config.params.sh` — lines 18–30 and 226–238; total not measured

To check claims I also read: `configure` (lines 1–17); the workflows `Linux-libretro-common-samples.yml` (lines 1–330), `Linux.yml` (1–56), `Linux-Headless.yml` (1–60) and `retroarch.yml` (64–75); and a search of the build commands in `Linux-samples-tasks.yml`.

## What I started with
- The global `~/.claude/CLAUDE.md`.
- The review-lane agent instructions.
- An environment block: working directory `/home/ants/.cache/lane-neutral`, not a git repository.
- A start-up hook message about the Ants MCP verbs.
- No git snapshot, no memory index and no project `CLAUDE.md`.

## Summary
This patch changes how the fork is built and checked before a push.
- **Makefiles:** typing a bare object name like `make save.o` now builds the real object in the build folder. The new optional S3 cloud-sync backend is wired in.
- **Configure scripts:** they now detect the faster "mold" linker and use it when it works.
- **`local-CI.sh`:** a new script that tries to rerun the project's Linux GitHub checks on this machine before a push, so a failure is caught here first.

The scripts are mostly well commented. Several of those comments are now wrong about how closely the local run matches the real one.

## Findings

**[Q4][Q1] HIGH `local-CI.sh:345`** — `# The workflow inlines a ~90-line runner; invoke that exact block from the exported tree so the run-allowlist / build-only logic stays the single source of truth (extracted straight from the workflow's `run:` script).`
- The comment is false. Nothing is pulled from the workflow; lines 352–381 are a hand-made copy, and it has drifted a long way:
  - `RUN_TARGETS` has 19 entries here. The workflow has about 100 (`float_to_s32_test`, `crc32_test`, `pixconv_test`, `rac3_*` and more).
  - This copy drops the workflow's `MAKE_ARGS: "SANITIZER=address,undefined"`.
  - It has no `NORUN_TARGETS` list and no "built but in neither list is an error" rule.
  - Its `RUN_ENV` keeps a `config_file_test` leak exemption, and does not have the workflow's `fastjmp_test` and `faulthandler_test` entries. The workflow says that exemption is gone.
  - The workflow's run block is far longer than "~90 lines": the `RUN_TARGETS` list alone runs about 110 lines.
- What goes wrong: someone who adds a test to the workflow will trust this comment and expect the local run to pick it up. It never will.
- Fix: either really extract and run the workflow's `run:` block, or rewrite the comment to say this is a hand copy that must be kept in step.
- Confirms candidate `local-CI.sh:345:census_number:0`.
- Searched: read `Linux-libretro-common-samples.yml` lines 1–330.

**[Q4][Q1] HIGH `local-CI.sh:239`** — `# Command strings below mirror the referenced workflow step verbatim (modulo -j).` The FIDELITY note at line 21 makes the same promise: "*identical* configure/make commands".
- How each job differs from its workflow:
  - **samples-tasks** (`job_samples_tasks`): runs 9 test binaries. `Linux-samples-tasks.yml` builds and runs dozens more, including `video_shader_wildcard_test`, `cloudsync_savefile_owner_test`, `input_remap_bounds_test`, `bps_patch_bounds_test`, `patch_stream_test`, the TSan lanes and `wait_timeout_test`. It also runs `self_update_net` through `make -f Makefile.net check`, not by calling the binary directly.
  - **linux-i686**: leaves out `Linux.yml`'s "Include order" step (`tools/include_order_check.sh`) and its "Stack frame sweep" step.
  - **c89 and headless**: the departures from their workflows are explained right next to the code, so they don't mislead.
- What goes wrong: a stranger who sees a local PASS will believe the matching CI job passes too. Nothing near `job_samples_tasks` says otherwise.
- Fix: replace "verbatim" with a per-job list of the steps that are mirrored and the ones left out.
- Searched: grep of `make`/`timeout` lines in `Linux-samples-tasks.yml`; read `Linux.yml` lines 1–56.

**[Q4] MEDIUM `local-CI.sh:18`** — `#   Two of the five Linux jobs run inside a fixed container image on CI; those are reproduced bit-exactly via podman using that same image.` Line 23 repeats it: "The two i686 container jobs are therefore the bit-for-bit anchor".
- This contradicts the same file at lines 57–59 ("Only that ONE workflow uses it; the headless job runs natively on ubuntu-latest") and line 394.
- `Linux-Headless.yml` confirms the later comments: `runs-on: ubuntu-latest`, with no container.
- What goes wrong: a stranger treats a local headless-i686 result as an exact copy of CI's, when it runs a different OS image with an extra `--disable-qt` flag.
- Fix: say one CI job is containerised and the headless job only borrows that image locally.
- Searched: read `Linux-Headless.yml` lines 1–60.

**[Q4] MEDIUM `local-CI.sh:7`** — `#   The fork carries ~30 workflow files under .github/workflows/.`
- There are 74. Some of them (`FreeBSD.yml`, `Extern-C-Prototype-Check.yml`, `Linux-network-clients.yml`, `Linux-library-net-backends.yml`, `Linux-frontend-overhead.yml`) are in none of the script's three lists.
- What goes wrong: the stale number understates how much is left uncovered, and it keeps going stale with every upstream sync.
- Fix: drop the count and point to `--list`.
- Confirms candidate `local-CI.sh:7:census_number:0`.
- Searched: counted `^name:` across `.github/workflows` (74 files), then checked those 5 names against the three lists.

**[Q4] MEDIUM `local-CI.sh:31`** — `# Deviation from the workflows ... builds run in parallel, $LOCAL_CI_JOBS jobs at once (default 4 ...). The workflows build serially`
- The workflows do not build serially: `Linux.yml` uses `make -j$(getconf _NPROCESSORS_ONLN)`, and `Linux-samples-tasks.yml` uses `-j$(nproc)`.
- "jobs at once" also suggests the CI jobs themselves run at the same time. In fact the driver loop (lines 465–498) runs them one after another; only `make -j` is parallel.
- What goes wrong: someone chasing a parallel-only build failure will be misled about where the parallelism is.
- Fix: say "`make -j$LOCAL_CI_JOBS`; the CI jobs run one at a time".
- Searched: as above.

**[Q4] MEDIUM `local-CI.sh:109`** — `# are upstream's, so local-gate.md § 3's inversion is unavailable and act`
- No `local-gate.md` exists in this repository, so a stranger cannot learn what "the inversion" means. The sentence is the only reason given for the act route.
- Fix: say the rule in one clause, or link a path the repository actually has.
- Searched: Ants `workspace_search` for `local-gate` (only this line matched).

**[Q4] LOW `local-CI.sh:235`** — `# run_native <job> <logfile> : body reads the exported tree from $PWD.`
- No `run_native` function exists. The comment describes a helper that isn't there.
- Fix: delete the comment, or turn it into a note on how the native jobs work.
- Searched: Ants `workspace_search` for `run_native` (only this line matched).

**[Q4] LOW `local-CI.sh:399`** — `#   rationale + the Qt-link detail: docs/private/DEPENDENCY-POLICY.md §4a.`
- `docs/private/` does not exist in this checkout, so the "full rationale" cannot be reached. It may be a private directory kept out of the repository.
- Fix: put the rationale inline, or name where the document lives.
- Searched: Grep on `docs/private` (the path does not exist).

**[Q5] LOW `qb/config.params.sh:234`** — `HAVE_S3=no                 # S3 cloud sync backend (experimental, requires networking)`
- The new `HAVE_S3` block (`Makefile.common:2481`) sits inside `ifeq ($(HAVE_CLOUDSYNC), 1)`, and `HAVE_CLOUDSYNC` is only set for Linux and Windows.
- `config.libs.sh:227` only ties S3 to networking.
- What goes wrong: someone enabling S3 on BSD or macOS gets no S3 backend, and no warning from configure.
- Fix: name the cloud-sync and OS dependency in the comment, or check it in configure.

No findings on the Makefile hunk, the mold probe (`qb.comp.sh:74–102`) or `qb.make.sh:1–12`.
- I checked the `qb.make.sh` claim that `NEED_GOLD_LINKER=1` still wins against `Makefile:42`.
- I checked the "after library detection" claim against the source order in `configure`: `qb.comp.sh` runs, then `qb.libs.sh`, then `qb.make.sh`.

## Candidates dismissed
None. Both candidates are confirmed above; `:345` is folded into the first HIGH finding.

## Correctness leads
- `local-CI.sh:366` — common-samples builds without `SANITIZER=address,undefined`. It also keeps `detect_leaks=0` for `config_file_test`. So ASan and LeakSanitizer failures that CI catches can pass locally. — Searched: the workflow, lines 90–330.
- `local-CI.sh:288` — samples-tasks runs a small subset of `Linux-samples-tasks.yml`. A local PASS does not mean that workflow passes. — Searched: grep of the workflow.
- `local-CI.sh:385` — linux-i686 skips `include_order_check.sh` and the stack-frame sweep that `Linux.yml` runs. — Searched: `Linux.yml` lines 1–56.
- `qb/config.libs.sh:227` with `Makefile.common:2481` — `--enable-s3` on a system without `HAVE_CLOUDSYNC` would put `HAVE_S3` in `config.h` without building `s3.o`. I did not check whether `configuration.c:1959` and the `menu_setting.c` uses sit under `HAVE_CLOUDSYNC`; if any does not, the link fails. — Searched: grep for `HAVE_S3` in `*.c`/`*.h`.
- `qb/qb.comp.sh:90` — the mold probe links without `$CFLAGS`/`$LDFLAGS`. So for example an `X86=1` (`-m32`) build could pass the probe and still fail at the final link. — Searched: none.

## Not checked
- I couldn't confirm which pre-push hook runs `local-CI.sh`: there is no `.githooks/`, and I did not search further.
- The total line counts of `Makefile`, `Makefile.common`, `config.libs.sh` and `config.params.sh` (only the windows listed above were read).
- I did not check that `OBJ` is complete before `Makefile:337`, where the new static rule reads it.
- The `Linux-libretro-common-samples.yml` workflow past line 330.
- One search run without the repository's ignore rules (Ants `workspace_search` with `respect_gitignore:false`) returned zero matches, even for `local-CI.sh` itself, so I did not use it. Any claim about ignored files rests on the Grep path checks instead.
