#!/usr/bin/env bash
#
# local-CI.sh — reproduce the project's Linux GitHub-Actions CI jobs locally,
#               before pushing, so a red X is caught on this machine first.
#
# WHY THIS EXISTS
#   The fork carries ~30 workflow files under .github/workflows/. All but the
#   Linux ones target platforms (3DS, PS2, PSP, Android, MSVC, Wii, webOS, …)
#   whose SDKs/cross-toolchains are not present on a desktop Linux host, so they
#   cannot be reproduced here. This script mirrors the five *Linux* jobs — the
#   ones that actually gate this fork's Linux-desktop source work — as faithfully
#   as a single Linux box allows, and prints the rest as an explicit skip list so
#   nothing looks silently covered.
#
# FIDELITY (what "matches the GitHub CI run" means here)
#   Two of the five Linux jobs run inside a fixed container image on CI; those are
#   reproduced bit-exactly via podman using that same image. The three ubuntu-runner
#   jobs are reproduced natively with this host's toolchain. Native builds run the
#   *identical* configure/make commands the workflow runs, but `./configure` detects
#   this host's installed libraries, so the resulting HAVE_* feature set can differ
#   from CI's ubuntu runner. The two i686 container jobs are therefore the
#   bit-for-bit anchor; the native jobs catch compile / C89 / regression-test
#   breakage fast without an image pull.
#
#   Each job builds from `git archive` of the target ref into a throwaway directory
#   (or, for container jobs, piped straight into the container). Your working tree
#   and its incremental build are never touched — no `make clean` runs in-place.
#
#   Deviation from the workflows, documented deliberately: builds add -j$(nproc).
#   The workflows build serially; parallelism changes only wall-clock, never the
#   pass/fail result.
#
# USAGE
#   ./local-CI.sh                 # run every runnable Linux job against HEAD
#   ./local-CI.sh c89 linux-i686  # run only the named jobs
#   ./local-CI.sh --worktree      # validate uncommitted (tracked) changes, not HEAD
#   ./local-CI.sh --list          # list jobs + the GitHub-only (un-runnable) set
#   ./local-CI.sh --pull          # force-refresh the i686 container image first
#   ./local-CI.sh --no-container  # skip the podman jobs (native jobs only)
#
# Exit status is non-zero if any run job fails.

set -uo pipefail

# ---- config ---------------------------------------------------------------

# The exact image CI's i686 jobs run in (.github/workflows/Linux.yml,
# Linux-Headless.yml -> container.image). Docker Hub, addressed for podman.
IMG_I686="docker.io/reallibretroretroarch/libretro-build-i386-ubuntu:xenial-gcc9"

# Environment alignment for the NATIVE c89 job.
#   CI's retroarch.yml runner installs a FIXED apt set; a lib this host has but
#   that set lacks would make ./configure enable a feature CI never compiles,
#   producing errors CI never sees (false failures). Each flag here forces the
#   same effective feature set as CI's runner, so the c89 job reproduces CI —
#   not this host's superset. Revisit when CI's apt list changes.
#     --disable-pipewire : retroarch.yml installs no libpipewire-0.3-dev, so CI
#                          builds with HAVE_PIPEWIRE=0. (openSUSE ships it, and
#                          the pipewire/spa system headers are not C89-clean.)
C89_ALIGN=(--disable-pipewire)

ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "local-CI.sh: not inside a git repository" >&2; exit 2; }

# Jobs that exist on CI but need an SDK/toolchain not installable on a Linux
# desktop — printed by --list so their absence is explicit, never silent.
GITHUB_ONLY=(
    "CI 3DS (3DS.yml)"                         "CI Android (Android.yml)"
    "CI DOS/DJGPP (DOS-DJGPP.yml)"             "CI Emscripten (Emscripten.yml)"
    "CI GameCube (GameCube.yml)"               "CI iOS (iOS.yml)"
    "CI macOS (MacOS.yml)"                     "CI Miyoo (Miyoo.yml)"
    "CI Windows (MSVC) (MSVC.yml)"             "CI MSYS2 (MSYS2.yml)"
    "CI PS2 (PS2.yml)"                         "CI PS3/PSL1GHT (PS3-PSL1GHT.yml)"
    "CI PS4/ORBIS (PS4-ORBIS.yml)"             "CI PSP (PSP.yml)"
    "CI PSVita (PSVita.yml)"                   "CI RetroFW MIPS32 (RetroFW.yml)"
    "CI RS90 (RS90.yml)"                       "CI Switch/libnx (Switch-libnx.yml)"
    "CI webOS (webOS.yml)"                     "CI WiiU (WiiU.yml)"
    "CI Wii (Wii.yml)"                         "CI Windows ARM64 (MSVC) (Windows-ARM64.yml)"
    "CI Windows i686 (MXE) (Windows-i686-MXE.yml)" "CI Windows x64 (MXE) (Windows-x64-MXE.yml)"
    "Source Release (SourceRelease.yml)"       "Crowdin sync (crowdin*.yml)"
)

# Every runnable Linux job, in CI-cost order (native first, containers last).
ALL_JOBS=(c89 samples-tasks common-samples linux-i686 headless-i686)

# ---- arg parsing ----------------------------------------------------------

REF="HEAD"
DO_PULL=0
NO_CONTAINER=0
declare -a SELECTED=()

while [ $# -gt 0 ]; do
    case "$1" in
        --list)
            echo "Runnable Linux jobs (this host):"
            for j in "${ALL_JOBS[@]}"; do echo "  $j"; done
            echo
            echo "GitHub-only jobs (SDK/toolchain not available locally — not run):"
            for g in "${GITHUB_ONLY[@]}"; do echo "  $g"; done
            exit 0 ;;
        --worktree)   REF="worktree"; shift ;;
        --pull)       DO_PULL=1; shift ;;
        --no-container) NO_CONTAINER=1; shift ;;
        -h|--help)
            sed -n '2,40p' "$0"; exit 0 ;;
        --*) echo "local-CI.sh: unknown flag $1" >&2; exit 2 ;;
        *)   SELECTED+=("$1"); shift ;;
    esac
done

[ ${#SELECTED[@]} -eq 0 ] && SELECTED=("${ALL_JOBS[@]}")

# Resolve the ref to archive. --worktree captures uncommitted *tracked* edits
# (untracked files are excluded, exactly like CI's checkout of a pushed commit).
ARCHIVE_REF="$REF"
if [ "$REF" = "worktree" ]; then
    ARCHIVE_REF="$(git -C "$ROOT" stash create)"
    [ -z "$ARCHIVE_REF" ] && ARCHIVE_REF="HEAD"   # tree clean -> HEAD is identical
fi

LOGDIR="$(mktemp -d "${TMPDIR:-/tmp}/local-ci.XXXXXX")"
JPAR="-j$(nproc 2>/dev/null || echo 2)"

echo "== local-CI.sh =="
echo "repo:   $ROOT"
echo "ref:    $REF  ($(git -C "$ROOT" rev-parse --short "$ARCHIVE_REF" 2>/dev/null || echo '?'))"
echo "branch: $(git -C "$ROOT" rev-parse --abbrev-ref HEAD)"
echo "logs:   $LOGDIR"
echo "jobs:   ${SELECTED[*]}"
echo

# ---- helpers --------------------------------------------------------------

# export_tree <destdir> : lay down a clean copy of ARCHIVE_REF (no .git, no
# working-tree pollution, no untracked files — a faithful CI checkout).
export_tree() {
    git -C "$ROOT" archive --format=tar "$ARCHIVE_REF" | tar -x -C "$1"
}

have_image() { podman image exists "$IMG_I686"; }

ensure_image() {
    if [ "$DO_PULL" -eq 1 ] || ! have_image; then
        echo ">> pulling $IMG_I686 (first run only; ~GB) ..."
        podman pull "$IMG_I686" || return 1
    fi
}

# run_native <job> <logfile> : body reads the exported tree from $PWD.
# Each native job cd's into its own fresh export dir.

# ---- job bodies -----------------------------------------------------------
# Command strings below mirror the referenced workflow step verbatim (modulo -j).

# retroarch.yml :: linux-c89  (ubuntu-latest, native)
#
# This is a COMPILE validation of the whole repo source under the strict C89
# compiler. Two host-vs-CI realities are handled so the verdict matches CI:
#   1. Host system libraries can be NEWER than CI's pinned apt set, so their
#      headers (e.g. libxkbcommon) may use post-C89 constructs that trip
#      -pedantic where CI's older headers don't. Such errors originate under
#      /usr/** — they are reported as informational divergences, never a fail,
#      because CI compiles those same headers clean.
#   2. -k (keep-going) compiles the ENTIRE tree in one pass, so every
#      repo-source C89 error surfaces at once and a host-header divergence in
#      one TU doesn't hide our errors in the others.
# The job FAILS iff a C89 error originates in a repo-tracked file. Linking is
# NOT asserted here (a skipped host-divergent TU makes the local link
# unreliable); the i686 container jobs are the authoritative full build+link.
job_c89() {
    local w raw repo_err sys_err n
    w="$(mktemp -d "$LOGDIR/c89.XXXX")"; export_tree "$w"
    raw="$LOGDIR/c89.build.log"
    ( cd "$w"
      ./configure "${C89_ALIGN[@]}"
      make $JPAR -k C89_BUILD=1
      make $JPAR clean
      make $JPAR -k DEBUG=1 GL_DEBUG=1 C89_BUILD=1 info all
    ) >"$raw" 2>&1
    if [ ! -f "$w/config.h" ]; then
        echo "configure failed (no config.h produced) — see $raw"; return 1
    fi
    sys_err="$(grep -E 'error:' "$raw" | grep -E '/usr/' | sort -u)"
    repo_err="$(grep -E 'error:' "$raw" | grep -vE '/usr/' | sort -u)"
    if [ -n "$sys_err" ]; then
        n=$(printf '%s\n' "$sys_err" | grep -c .)
        echo "NOTE: $n host system-header divergence(s) (newer local libs than CI's"
        echo "      pinned apt set — CI compiles these headers clean; NOT a CI failure):"
        printf '%s\n' "$sys_err" | head -8 | sed 's/^/   /'
        [ "$n" -gt 8 ] && echo "   ... ($n total; full list in $raw)"
    fi
    if [ -n "$repo_err" ]; then
        echo "REPO-SOURCE C89 errors (these WOULD fail CI):"
        printf '%s\n' "$repo_err" | sed 's/^/   /'
        return 1
    fi
    echo "[pass] no repo-source C89 errors across the full tree (C89_BUILD + DEBUG)"
    return 0
}

# Linux-samples-tasks.yml :: samples-tasks  (ubuntu-latest, native)
job_samples_tasks() {
    local w; w="$(mktemp -d "$LOGDIR/stasks.XXXX")"; export_tree "$w"
    ( set -e
      cd "$w/samples/tasks/database"
      make clean all && test -x database_task
      rc=0; ./database_task >/dev/null 2>&1 || rc=$?
      [ "$rc" -ne 0 ] || { echo "database_task no-args expected non-zero, got 0"; exit 1; }
      echo "[pass] database_task no-args exit=$rc"

      cd "$w/samples/tasks/decompress"
      make clean all && test -x archive_name_safety_test
      timeout 60 ./archive_name_safety_test && echo "[pass] archive_name_safety_test"

      cd "$w/samples/tasks/http"
      make clean all SANITIZER=address && test -x http_method_match_test
      timeout 60 ./http_method_match_test && echo "[pass] http_method_match_test" )
}

# Linux-libretro-common-samples.yml :: samples  (ubuntu-latest, native)
# The workflow inlines a ~90-line runner; invoke that exact block from the
# exported tree so the run-allowlist / build-only logic stays the single source
# of truth (extracted straight from the workflow's `run:` script).
job_common_samples() {
    local w; w="$(mktemp -d "$LOGDIR/csam.XXXX")"; export_tree "$w"
    ( cd "$w/libretro-common/samples"
      set -u; set -o pipefail
      declare -a RUN_TARGETS=(
        compat_fnmatch_test snprintf unbase64_test archive_zip_test archive_zstd_test
        config_file_test path_resolve_realpath_test nbio_test rpng rzip_chunk_size_test
        net_ifinfo vfs_read_overflow_test cdrom_cuesheet_overflow_test http_parse_test
        rjson_test rtga_test rbmp_test rpng_chunk_overflow_test rpng_roundtrip_test )
      declare -A RUN_ENV=( [config_file_test]="ASAN_OPTIONS=detect_leaks=0" )
      declare -a BUILD_ONLY_DIRS=( formats/xml )
      declare -a SKIP_DIRS=( )
      is_in(){ local n=$1; shift; local h; for h in "$@"; do [ "$h" = "$n" ] && return 0; done; return 1; }
      fails=0; builds=0; runs=0
      mapfile -t MKDIRS < <(find . -name Makefile -printf '%h\n' | sort)
      for d in "${MKDIRS[@]}"; do
        rel=${d#./}
        is_in "$rel" "${SKIP_DIRS[@]}" && { echo "[skip] $rel"; continue; }
        if ! ( cd "$d" && make clean all ); then
          echo "::error:: $rel failed to build"; fails=$((fails+1)); continue; fi
        builds=$((builds+1))
        is_in "$rel" "${BUILD_ONLY_DIRS[@]}" && { echo "[skip-run] $rel"; continue; }
        mapfile -t targets < <(grep -hE '^(TARGET|TARGETS|TARGET_TEST[0-9]*)[[:space:]]*[:?]?=' "$d/Makefile" \
          | sed -E 's/^[^=]*=[[:space:]]*//' | tr -s ' \t' '\n' | grep -v '^$' | sort -u)
        for t in "${targets[@]}"; do
          is_in "$t" "${RUN_TARGETS[@]}" || { echo "[skip-run] $rel/$t"; continue; }
          [ -x "$d/$t" ] || { echo "::error:: $t missing after build"; fails=$((fails+1)); continue; }
          if ( cd "$d" && env ${RUN_ENV[$t]:-} timeout 60 "./$t" ); then
            echo "[pass] $t"; runs=$((runs+1))
          else echo "::error:: $t exit $?"; fails=$((fails+1)); fi
        done
      done
      echo "Built:$builds Ran:$runs Failed:$fails"
      [ "$fails" -eq 0 ] )
}

# Linux.yml :: build  (i686 container — bit-exact)
job_linux_i686() {
    ensure_image || return 1
    git -C "$ROOT" archive --format=tar "$ARCHIVE_REF" | podman run --rm -i --user root "$IMG_I686" \
      bash -c 'set -e; mkdir -p /b && cd /b && tar x
               ./configure --disable-qt --enable-xdelta
               make -j"$(getconf _NPROCESSORS_ONLN)" clean
               make -j"$(getconf _NPROCESSORS_ONLN)" info all'
}

# Linux-Headless.yml :: linux-nomenu  (i686 container)
#   Aligned deviation from the workflow's bare `./configure --disable-menu`:
#   we add --disable-qt. The `xenial-gcc9` image is a MUTABLE tag whose bundled
#   Qt5/moc no longer links RetroArch's Qt frontend — `master` itself fails the
#   Qt link in this image, independent of any branch change (verified 2026-07-03).
#   CI's headless job stays green because its effective runtime env links Qt5;
#   ours (a drifted image pull) does not. The job's PURPOSE is the no-menu build
#   path, orthogonal to the Qt frontend, so disabling Qt reproduces that intent
#   without the image-drift false failure. See docs/private/DEPENDENCY-POLICY.md
#   (unpinned CI image tags). Drop --disable-qt once the image (or its digest
#   pin) links Qt again.
job_headless_i686() {
    ensure_image || return 1
    git -C "$ROOT" archive --format=tar "$ARCHIVE_REF" | podman run --rm -i --user root "$IMG_I686" \
      bash -c 'set -e; mkdir -p /b && cd /b && tar x
               ./configure --disable-menu --disable-qt
               make -j"$(getconf _NPROCESSORS_ONLN)"'
}

# ---- driver ---------------------------------------------------------------

declare -A DISPATCH=(
    [c89]=job_c89
    [samples-tasks]=job_samples_tasks
    [common-samples]=job_common_samples
    [linux-i686]=job_linux_i686
    [headless-i686]=job_headless_i686
)

declare -a RESULTS=()
overall=0

for job in "${SELECTED[@]}"; do
    fn="${DISPATCH[$job]:-}"
    if [ -z "$fn" ]; then
        echo "!! unknown job '$job' (see --list)"; overall=1; RESULTS+=("SKIP  $job (unknown)"); continue
    fi
    if [ "$NO_CONTAINER" -eq 1 ] && { [ "$job" = "linux-i686" ] || [ "$job" = "headless-i686" ]; }; then
        echo "-- $job: skipped (--no-container)"; RESULTS+=("SKIP  $job (--no-container)"); continue
    fi
    log="$LOGDIR/$job.log"
    printf '>> %-14s ... ' "$job"
    if "$fn" >"$log" 2>&1; then
        echo "PASS"; RESULTS+=("PASS  $job")
    else
        echo "FAIL  (see $log)"; RESULTS+=("FAIL  $job -> $log")
        echo "   --- last 20 log lines ---"
        tail -n 20 "$log" | sed 's/^/   /'
        overall=1
    fi
done

echo
echo "== summary =="
for r in "${RESULTS[@]}"; do echo "  $r"; done
echo
if [ "$overall" -eq 0 ]; then
    echo "All selected Linux CI jobs passed. (Cross-platform jobs are GitHub-only; see --list.)"
    rm -rf "$LOGDIR"
else
    echo "One or more jobs FAILED. Logs kept in: $LOGDIR"
fi
exit "$overall"
