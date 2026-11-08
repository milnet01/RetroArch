#!/usr/bin/env bash
#
# local-CI.sh — reproduce the project's Linux GitHub-Actions CI jobs locally,
#               before pushing, so a red X is caught on this machine first.
#
# WHY THIS EXISTS
#   Most of the fork's workflow files target platforms (3DS, PS2, PSP, Android,
#   MSVC, Wii, webOS, …) whose SDKs/cross-toolchains are not present on a
#   desktop Linux host, so they cannot be reproduced here. This script runs the
#   Linux jobs that gate this fork's Linux-desktop source work on every push,
#   and through act the jobs tools/ci_local.py says cover the commits not yet
#   pushed: those naming a changed file, and the tree-wide checks. The other
#   Linux workflows run on request through act (--all). --list names every
#   workflow, and every job of a workflow run here, in one of those sets or the
#   GitHub-only skip list; one in none fails the run.
#
# FIDELITY (what "matches the GitHub CI run" means here)
#   Most jobs run the workflow's own steps: the job's `run:` blocks are read
#   from the workflow file at the ref under test and run in order, each in its
#   own shell, stopping at the first that fails (run_workflow_job). Nothing is
#   copied, so a step added to the workflow runs here too. Left out: `uses:`
#   steps (checkout, cache) and the steps that install packages with sudo —
#   this host's packages stand in for the runner's. A default run also leaves
#   out the wall-clock steps in TIMING_STEPS unless the push changes a file
#   their test builds, and runs them after waiting for a quiet machine. The
#   steps get ccache, as the workflows' "Enable ccache" step gives them on
#   GitHub, from a cache of their own under the trees' directory (below). A
#   job using a workflow feature the runner does not honour (a template
#   expression, GITHUB_ENV, an `if:` or matrix outside the forms
#   workflow_tool names) is refused rather than run differently.
#
#   linux-i686 runs Linux.yml's steps in the same container image CI uses.
#   The rest run natively with this host's toolchain, so `./configure` can
#   detect a different HAVE_* feature set from CI's ubuntu runner. Two jobs
#   are hand-written, each for a stated reason at its body: c89 (host headers
#   newer than CI's) and headless-i686 (borrows the i686 image).
#
#   Each job builds from `git archive` of the target ref into a throwaway directory
#   (or, for container jobs, piped straight into the container). Your working tree
#   and its incremental build are never touched — no `make clean` runs in-place.
#
#   Jobs run one at a time. Inside a job, make runs $LOCAL_CI_JOBS jobs at
#   once (default 4, to bound RAM on this machine): `nproc` and `getconf
#   _NPROCESSORS_ONLN` report that number to every step, native, container
#   or act, so a workflow's `make -j$(nproc)` keeps to it.
#
#   Build trees and logs go under $LOCAL_CI_TMPDIR, else the git config
#   ants.localci.tmpdir, else ~/.cache/local-ci; on disk, since /tmp is tmpfs
#   here and a build tree there is held in RAM. Prefer the git config: cc-job
#   starts a job with a clean environment, so the variable set on its command
#   line does not arrive. The build trees are removed when the run ends; a
#   failed run keeps its logs. Container images and act's volumes stay in
#   podman's own storage, wherever this points.
#
# USAGE
#   ./local-CI.sh                 # run every runnable Linux job against HEAD,
#                                 # and the act jobs covering unpushed commits
#   ./local-CI.sh --list-changed  # print those act jobs, without running them
#   ./local-CI.sh c89 linux-i686  # run only the named jobs
#   ./local-CI.sh --worktree      # validate uncommitted (tracked) changes, not HEAD
#   ./local-CI.sh --all           # also run every on-request Linux workflow via act
#   ./local-CI.sh act:Linux-asan-ubsan.yml  # run one workflow file via act
#   ./local-CI.sh --list          # list jobs, the act set, the GitHub-only set, and
#                                 # any workflow in none of them (exits 1 if so)
#   ./local-CI.sh --pull          # force-refresh the i686 container image first
#   ./local-CI.sh --no-container  # skip the podman jobs (native jobs only)
#
# Exit status is non-zero if any run job fails.

set -uo pipefail

# ---- config ---------------------------------------------------------------

# The image CI's Linux.yml (CI Linux (i686)) job runs in via container.image.
# Only that ONE workflow uses it; the headless job runs natively on ubuntu-latest
# (we reuse this image for headless locally — see job_headless_i686). Docker Hub,
# addressed for podman.
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
#     --disable-sdl3     : retroarch.yml installs libsdl2-dev, not SDL3, so CI
#                          never compiles input/drivers_joypad/sdl3_joypad.c.
#                          (openSUSE ships SDL3, and that upstream file is not
#                          C89-clean.)
C89_ALIGN=(--disable-pipewire --disable-sdl3)

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
    "CI PS2 runtime, Play! AutoTest (PS2-runtime.yml)"
    "CI PS4/ORBIS (PS4-ORBIS.yml)"             "CI PSP (PSP.yml)"
    "CI PSVita (PSVita.yml)"                   "CI RetroFW MIPS32 (RetroFW.yml)"
    "CI RS90 (RS90.yml)"                       "CI Switch/libnx (Switch-libnx.yml)"
    "CI webOS (webOS.yml)"                     "CI WiiU (WiiU.yml)"
    "CI Wii (Wii.yml)"                         "CI Windows ARM64 (MSVC) (Windows-ARM64.yml)"
    "CI Windows i686 (MXE) (Windows-i686-MXE.yml)" "CI Windows x64 (MXE) (Windows-x64-MXE.yml)"
    "Source Release (SourceRelease.yml)"       "Crowdin sync (crowdin*.yml)"
    "CI Apple (Apple.yml)"                     "CI macOS PPC cross (MacOS-PPC-Cross.yml)"
    "CI Windows memmap area (Windows-memmap-area.yml)"
    "CI FreeBSD (FreeBSD.yml)"
    "CI iOS legacy (iOS-Legacy.yml)"            "CI Linux aarch64, Arm runner (Linux-aarch64.yml)"
    "CI macOS task queue (MacOS-task-queue-samples.yml)"
    "CI macOS VideoToolbox (MacOS-videotoolbox-encode.yml)"
    "CI macOS VideoToolbox HEVC (MacOS-videotoolbox-hevc.yml)"
    "Actions cache upkeep (Prune-caches.yml)"
)

# Every runnable Linux job, in CI-cost order (native first, containers last).
ALL_JOBS=(c89 samples-tasks samples-tasks-http common-samples-codecs
          common-samples-formats common-samples-core config-fuzz
          linux-i686 headless-i686)

# The workflow job each of ALL_JOBS runs, as <workflow>:<job>.
declare -A JOB_SOURCE=(
    [c89]=retroarch.yml:linux-c89
    [samples-tasks]=Linux-samples-tasks.yml:samples-tasks
    [samples-tasks-http]=Linux-samples-tasks.yml:samples-tasks-http
    [common-samples-codecs]=Linux-libretro-common-samples.yml:samples
    [common-samples-formats]=Linux-libretro-common-samples.yml:samples
    [common-samples-core]=Linux-libretro-common-samples.yml:samples
    [config-fuzz]=Linux-libretro-common-samples.yml:config-file-fuzz-smoke
    [linux-i686]=Linux.yml:build
    [headless-i686]=Linux-Headless.yml:linux-nomenu
)

# The matrix value a job of ALL_JOBS runs its workflow job with.
declare -A JOB_MATRIX=(
    [common-samples-codecs]=group=codecs
    [common-samples-formats]=group=formats
    [common-samples-core]=group=core
)

# Jobs in those same workflows that need a toolchain this host lacks.
GITHUB_ONLY_JOBS=(
    "Linux-libretro-common-samples.yml:emscripten-memmap (emsdk)"
    "Linux-libretro-common-samples.yml:retro-atomic-cross (cross compilers, qemu-user)"
    "Linux-libretro-common-samples.yml:crc32-cross (cross compilers, qemu-user)"
    "Linux-libretro-common-samples.yml:task-queue-mingw (mingw, wine)"
)

# Linux workflows run through act, whole, in a container. Too slow and
# RAM-hungry for every push (the sanitizer builds, the Vulkan checks), so
# they run on request: `--all`, or by name as act:<file>. A workflow with a
# Windows or macOS job as well runs its ubuntu jobs only; act skips the
# other platforms.
ACT_WORKFLOWS=(
    Compile-Matrix.yml Cross-libc-vfs-samples.yml Ctx-Presentable-Check.yml
    Extern-C-Prototype-Check.yml
    Linux-asan-ubsan.yml Linux-audio-transport.yml Linux-autoconf-tests.yml
    Linux-driver-vtables.yml Linux-filter-plugins.yml Linux-frontend-overhead.yml
    Linux-gl1-upload.yml Linux-library-net-backends.yml
    Linux-libretro-common-tests.yml Linux-libretro-db-samples.yml
    Linux-menu-harness.yml Linux-network-clients.yml Linux-no-pkg-config.yml
    Linux-overlay-on-screen.yml Linux-pace-shadow.yml Linux-qt-companion.yml
    Linux-samples-audio.yml Linux-samples-cores.yml Linux-samples-formats.yml
    Linux-samples-gfx.yml Linux-samples-input.yml Linux-samples-network.yml
    Linux-samples-record.yml Linux-samples-settings.yml Linux-samples-unclaimed.yml
    Linux-state-runahead-tests.yml Linux-tsan-harness.yml Linux-tsan-threaded-video.yml
    Linux-vulkan-validation.yml Msg-Hash-Packed-Check.yml Overlap-Copy-Check.yml
    procbarrier-tiers.yml Removed-Member-Check.yml RGUI-Pixel-Format-Check.yml
    Auto-state-load.yml CI-Local-Runner.yml Linux-cdfs-io.yml Linux-dvd-blocks.yml
    Linux-egl-hw-core.yml Linux-griffin.yml Linux-mcp-stdio.yml Linux-menu-media.yml
    Linux-msvc6-standin.yml Linux-netplay-e2e.yml Linux-openxr-input.yml
    Linux-qnx-standin.yml Linux-tsan-samples.yml Linux-vulkan-resize.yml
    Linux-wayland-input.yml Linux-webm-player.yml Samples-Objdir-Check.yml
    Windows-wine-input.yml Windows-wine-menu.yml Windows-wine-views.yml
)
# Most act jobs a default run adds for unpushed commits, Windows-wine-* never
# among them (see the selection loop). A common source can
# select dozens, about 10 minutes each at worst; those over the limit show as
# SKIP in the summary. 0 means no limit. The figure is a judgement.
CHANGED_MAX="${LOCAL_CI_CHANGED_MAX:-20}"
ACT_BIN="${ACT_BIN:-$HOME/.local/bin/act}"
# ubuntu-latest is 24.04; this is the image act's own picker offers for it.
ACT_IMAGE="catthehacker/ubuntu:act-24.04"
JOBS="${LOCAL_CI_JOBS:-4}"
# act runs ACT_IMAGE with three changes, built once as a local image whose
# tag is a hash of its recipe (act_image), so a changed recipe rebuilds it:
#   /var/run is a real directory. In ACT_IMAGE it links to /run, and podman
#   refuses to copy an action into /var/run/act through the link ("statat
#   var/run/act: path escapes from parent"), so every `uses:` step failed.
#   nproc and getconf are the CPU shims below, so steps build at -j$JOBS.
#   git trusts the checkout, whose owner differs inside the container, as
#   actions/checkout arranges on GitHub; a step's `git diff` needs it.
#   The package lists are filled and ccache installed, as on GitHub's
#   runner image: the workflows' "Enable ccache" step installs it with no
#   `apt-get update` first. libjson-perl too: vkd3d's configure needs it in
#   the Windows-wine workflows, which do not install it and rely on the runner.
#   dpkg never unpacks samba-tool, so local_ad_dc_test.sh takes its own "skip: no
#   samba AD DC". Samba gives domain files IDs from 3000000 and a rootless
#   container maps 65536 (/etc/subuid), so the chown fails and provisioning
#   panics. That Kerberos lane is covered on GitHub only. dpkg never unpacks
#   ganesha.nfsd either, so local_ganesha_test.sh takes its "skip: nfs-ganesha
#   not installed": overlayfs gives it no file handles ("Could not get handle
#   ... Operation not supported"), and on a tmpfs, even with SYS_ADMIN and
#   DAC_READ_SEARCH, a client's create is refused. NFSv4 is GitHub only too.
#   clang and its sanitizer libraries, which GitHub's runner image has and
#   some steps call by name.
#   dpkg skips its syncs: on this overlay a workflow's 10-minute install step
#   otherwise runs out of time. libLLVM's own leak, reached through the Vulkan
#   validation layer, is suppressed (see LSAN_OPTIONS in job_act).
ACT_RUN_IMAGE=""

# workflow_tool jobs <workflow>
#   Print the workflow's job ids, one per line.
# workflow_tool steps <workflow> <job> <outdir> [<container image>]
#   Write the job's runnable steps to <outdir> as NN.sh, in order: each
#   enters the step's working directory, exports its env and runs its `run:`
#   block (NN.body) in the shell a runner would use. `uses:` steps are left
#   out; so are steps that install packages with sudo, and "Enable ccache",
#   whose `ccache -M` would set this machine's real cache size; both named on
#   stdout. WF_MATRIX=<key>=<value> picks one value of a job's matrix.
#   WF_SKIP_STEPS lists step names to leave out, one per line, each named on
#   stdout; the push gate fills it (timing_skips). A job
#   or step using a feature this does not honour is refused with a reason,
#   so the job fails instead of running something other than the workflow. A
#   job with a container is refused unless its image is the one passed.
workflow_tool() {
    python3 -I - "$@" <<'PY'
import os, re, shlex, sys, yaml

cmd, wf = sys.argv[1], sys.argv[2]
with open(wf) as f:
    d = yaml.safe_load(f)
if cmd == "jobs":
    print("\n".join(d["jobs"]))
    sys.exit(0)

job, out = sys.argv[3], sys.argv[4]
image = sys.argv[5] if len(sys.argv) > 5 else None
j = d["jobs"][job]

def refuse(why):
    sys.exit("%s:%s: %s, which the step runner does not honour"
             % (os.path.basename(wf), job, why))

for k in ("defaults", "if", "services", "continue-on-error"):
    if j.get(k) or (k == "defaults" and d.get(k)):
        refuse("it uses " + k)
matrix = {}
if j.get("strategy"):
    m = os.environ.get("WF_MATRIX", "").partition("=")
    if m[2] not in (j["strategy"].get("matrix") or {}).get(m[0], []):
        refuse("it uses strategy with no matching WF_MATRIX")
    matrix[m[0]] = m[2]
def expand(v):                       # ${{ matrix.<key> }} in a job's env
    return re.sub(r"\$\{\{ *matrix\.(\w+) *\}\}",
                  lambda g: matrix.get(g.group(1), g.group(0)), str(v))
def runs(cond):                      # a step's if:, in the forms workflows use
    if re.fullmatch(r"\$\{\{ *!cancelled\(\) *\}\}", cond):
        return True                  # nothing here cancels a job
    g = re.fullmatch(r"matrix\.(\w+) == '([^']*)'", cond)
    if not g or g.group(1) not in matrix:
        refuse("a step uses if: " + cond)
    return matrix[g.group(1)] == g.group(2)
def hub(i):                          # Docker Hub's name with or without docker.io/
    return i[len("docker.io/"):] if i and i.startswith("docker.io/") else i
c = j.get("container")
c = c.get("image") if isinstance(c, dict) else c
if hub(c) != hub(image):
    refuse("its container is %s where the caller expects %s"
           % (c or "none", image or "none"))

env0 = dict(d.get("env") or {})
env0.update({k: expand(v) for k, v in (j.get("env") or {}).items()})
known = {"name", "id", "if", "run", "shell", "working-directory", "env", "timeout-minutes"}
os.makedirs(out)
n = 0
for s in j["steps"]:
    if "uses" in s:
        continue
    if "if" in s and not runs(str(s["if"])):
        continue
    name = s.get("name") or s["run"].splitlines()[0]
    if name == "Enable ccache":
        print("skipped, sets the machine's ccache size:", name)
        continue
    # CI's own upkeep: Ubuntu packages, its linker and cache statistics
    if name in ("Apt cache key", "Install dependencies", "Link with lld",
                "ccache statistics"):
        continue
    if "sudo " in s["run"]:
        print("skipped, installs packages:", name)
        continue
    if name in os.environ.get("WF_SKIP_STEPS", "").split("\n"):
        print("SKIPPED, wall-clock check, the push changes nothing it builds:", name)
        continue
    env = dict(env0)
    env.update(s.get("env") or {})
    wd = s.get("working-directory", ".")
    if set(s) - known:
        refuse("step '%s' uses %s" % (name, ", ".join(sorted(set(s) - known))))
    if s.get("shell", "bash") != "bash":
        refuse("step '%s' uses shell %s" % (name, s["shell"]))
    if "${{" in s["run"] + wd + str(env):
        refuse("step '%s' uses a template expression" % name)
    for v in ("GITHUB_ENV", "GITHUB_PATH", "GITHUB_OUTPUT"):
        if v in s["run"]:
            refuse("step '%s' writes %s" % (name, v))
    shell = ("bash --noprofile --norc -eo pipefail" if s.get("shell") == "bash"
             else "bash -e")
    if s.get("timeout-minutes"):
        shell = "timeout %sm %s" % (s["timeout-minutes"], shell)
    n += 1
    with open(os.path.join(out, "%02d.body" % n), "w") as f:
        f.write(s["run"])
    with open(os.path.join(out, "%02d.sh" % n), "w") as f:
        f.write("# %s\n" % name)
        f.write("cd %s || exit 1\n" % shlex.quote(wd))
        for k, v in env.items():
            f.write("export %s=%s\n" % (k, shlex.quote(str(v))))
        f.write('exec %s "${0%%/*}/%02d.body"\n' % (shell, n))
if n == 0:
    refuse("it has no runnable step")
PY
}

# unclassified_workflows : print every workflow file in none of the lists
# above, and every job of a workflow JOB_SOURCE runs from that is neither
# run nor in GITHUB_ONLY_JOBS. An upstream sync that adds a workflow or a
# job shows up here instead of being silently uncovered.
unclassified_workflows() {
    local f b g known jobs j src
    for f in "$ROOT"/.github/workflows/*.yml "$ROOT"/.github/workflows/*.yaml; do
        [ -e "$f" ] || continue          # an unmatched glob stays literal
        b="${f##*/}"; known=0
        for src in "${JOB_SOURCE[@]}"; do
            [ "$b" = "${src%%:*}" ] && { known=2; break; }
        done
        if [ "$known" -eq 0 ]; then
            for g in "${ACT_WORKFLOWS[@]}"; do
                [ "$b" = "$g" ] && { known=1; break; }
            done
        fi
        if [ "$known" -eq 0 ]; then
            for g in "${GITHUB_ONLY[@]}"; do
                g="${g##*(}"; g="${g%)}"
                # shellcheck disable=SC2053  # $g may be a glob (crowdin*.yml)
                [[ "$b" == $g ]] && { known=1; break; }
            done
        fi
        [ "$known" -eq 0 ] && echo "$b"
        [ "$known" -eq 2 ] || continue
        jobs="$(workflow_tool jobs "$f")" || { echo "$b (jobs unreadable)"; continue; }
        for j in $jobs; do
            for src in "${JOB_SOURCE[@]}" "${GITHUB_ONLY_JOBS[@]%% *}"; do
                [ "$src" = "$b:$j" ] && continue 2
            done
            echo "$b:$j"
        done
    done
}

# push_base : print the git log exclusion marking what is already pushed.
# The pre-push hook gates in a detached worktree, where @{upstream} does not
# resolve, so its ANTS_PUSH_REFS line for this commit comes first. A new branch
# has an all-zero old value there, and that or a by-hand detached run falls
# through to --remotes: everything on no remote is unpushed.
push_base() {
    local head sha old
    head="$(git -C "$ROOT" rev-parse HEAD)"
    while read -r _ sha _ old; do
        [ "$(git -C "$ROOT" rev-parse -q --verify "$sha^{commit}")" = "$head" ] || continue
        git -C "$ROOT" cat-file -e "$old^{commit}" 2>/dev/null && { echo "$old"; return; }
    done <<<"${ANTS_PUSH_REFS:-}"
    git -C "$ROOT" rev-parse -q --verify '@{upstream}' || echo "--remotes"
}

# unpushed_files : print each file the commits not yet pushed change, and
# under --worktree the uncommitted edits too, one per line. Commits upstream
# already has do not count: what a merge brings has had upstream's CI, and
# ANTS_PUSH_CHANGED is not used because it lists those files.
unpushed_files() {
    { git -C "$ROOT" log --no-merges --format= --name-only HEAD --not "$(push_base)" --remotes=upstream
      [ "$REF" = worktree ] && git -C "$ROOT" diff --name-only HEAD "$ARCHIVE_REF"
    } | sort -u | sed '/^$/d'
}

# changed_jobs : print act:<workflow>:<job> for each job tools/ci_local.py
# says covers the commits not yet pushed, naming a changed file first, then
# the tree-wide checks; its full-tree builds are not taken, as the default jobs
# stand in for them. A job not runnable here goes to stderr.
changed_jobs() {
    local files out wf job j
    files="$(unpushed_files)"
    [ -n "$files" ] || return 0
    # shellcheck disable=SC2086  # one path per word; none holds a space that
    # matters here, since a path ci_local.py cannot match selects nothing
    out="$(python3 "$ROOT/tools/ci_local.py" which $files)" || {
        echo "tools/ci_local.py which failed" >&2; return 1; }
    while read -r wf job; do
        for j in "${JOB_SOURCE[@]}"; do
            [ "$j" = "$wf:$job" ] && continue 2      # a default job already
        done
        for j in "${ACT_WORKFLOWS[@]}"; do
            [ "$j" = "$wf" ] && { echo "act:$wf:$job"; continue 2; }
        done
        echo "not runnable here: $wf:$job" >&2
    done < <(printf '%s\n' "$out" | awk '
        /^Jobs that name|^Tree-wide/ { take = 1; next }
        /^Full-tree|^Run one with/   { take = 0; next }
        take && $2 == "--job" && $NF == "Linux" { print $1, $3 }' | awk '!seen[$0]++')
}

# ---- arg parsing ----------------------------------------------------------

REF="HEAD"
DO_PULL=0
NO_CONTAINER=0
LIST_CHANGED=0
declare -a SELECTED=()

while [ $# -gt 0 ]; do
    case "$1" in
        --list)
            echo "Runnable Linux jobs (this host), and the workflow job each runs:"
            for j in "${ALL_JOBS[@]}"; do printf '  %-20s %s\n' "$j" "${JOB_SOURCE[$j]}"; done
            echo
            echo "On-request Linux workflows (act; --all, or by name — not run by default):"
            for a in "${ACT_WORKFLOWS[@]}"; do echo "  act:$a"; done
            echo
            echo "GitHub-only (SDK/toolchain not available locally — not run):"
            for g in "${GITHUB_ONLY[@]}" "${GITHUB_ONLY_JOBS[@]}"; do echo "  $g"; done
            u="$(unclassified_workflows)"
            if [ -n "$u" ]; then
                echo
                echo "UNCLASSIFIED workflows and jobs (in no list above — classify them in local-CI.sh):"
                printf '%s\n' "$u" | sed 's/^/  /'
                exit 1
            fi
            exit 0 ;;
        --list-changed) LIST_CHANGED=1; shift ;;
        --all)        SELECTED+=("${ALL_JOBS[@]}"); for a in "${ACT_WORKFLOWS[@]}"; do SELECTED+=("act:$a"); done; shift ;;
        --worktree)   REF="worktree"; shift ;;
        --pull)       DO_PULL=1; shift ;;
        --no-container) NO_CONTAINER=1; shift ;;
        -h|--help)
            sed -n '2,/^set -uo/p' "$0" | sed '$d'; exit 0 ;;
        --*) echo "local-CI.sh: unknown flag $1" >&2; exit 2 ;;
        *)   SELECTED+=("$1"); shift ;;
    esac
done

DEFAULT_RUN=0
if [ ${#SELECTED[@]} -eq 0 ]; then
    DEFAULT_RUN=1
    SELECTED=("${ALL_JOBS[@]}")
fi

# Resolve the ref to archive. --worktree captures uncommitted *tracked* edits
# (untracked files are excluded, exactly like CI's checkout of a pushed commit).
ARCHIVE_REF="$REF"
if [ "$REF" = "worktree" ]; then
    ARCHIVE_REF="$(git -C "$ROOT" stash create)"
    [ -z "$ARCHIVE_REF" ] && ARCHIVE_REF="HEAD"   # tree clean -> HEAD is identical
fi

if [ "$LIST_CHANGED" -eq 1 ]; then
    changed_jobs
    exit $?
fi

# Default run: add the act jobs covering the unpushed commits, up to CHANGED_MAX.
declare -a CHANGED_RESULTS=()
if [ "$DEFAULT_RUN" -eq 1 ]; then
    if changed="$(changed_jobs)"; then
        n=0
        while read -r j; do
            [ -n "$j" ] || continue
            # Half an hour each under Wine, and this desktop slows meanwhile;
            # the user tests Windows on a real Windows machine instead.
            case "$j" in act:Windows-wine-*)
                CHANGED_RESULTS+=("SKIP  $j (Windows machine instead; by name to run it)")
                continue ;;
            esac
            n=$((n + 1))
            if [ "$CHANGED_MAX" -gt 0 ] && [ "$n" -gt "$CHANGED_MAX" ]; then
                CHANGED_RESULTS+=("SKIP  $j (over LOCAL_CI_CHANGED_MAX=$CHANGED_MAX)")
            else
                SELECTED+=("$j")
            fi
        done <<<"$changed"
    else
        CHANGED_RESULTS+=("FAIL  could not work out which jobs cover the unpushed commits")
    fi
fi

CI_TMP="${LOCAL_CI_TMPDIR:-$(git -C "$ROOT" config --get ants.localci.tmpdir)}"
CI_TMP="${CI_TMP:-${XDG_CACHE_HOME:-$HOME/.cache}/local-ci}"
mkdir -p "$CI_TMP"
# ccache for the native workflow steps, kept between runs beside the trees:
# the machine-wide cache is full, and its default home, ~/.cache, is on a
# nearly full system disk here. Its config file replaces the user's.
CI_CCACHE="$CI_TMP/ccache"
mkdir -p "$CI_CCACHE"
[ -f "$CI_CCACHE/ccache.conf" ] || echo "max_size = 8G" >"$CI_CCACHE/ccache.conf"
LOGDIR="$(mktemp -d "$CI_TMP/local-ci.XXXXXX")" || {
    echo "local-CI.sh: cannot create a log directory under $CI_TMP" >&2; exit 2; }
JPAR="-j$JOBS"

# nproc and getconf reporting $JOBS CPUs, first on PATH for every workflow
# step, so a step's `make -j$(nproc)` keeps to this machine's limit.
CPU_SHIM="$LOGDIR/cpu-shim"
mkdir -p "$CPU_SHIM"
printf '#!/bin/sh\necho %s\n' "$JOBS" >"$CPU_SHIM/nproc"
printf '#!/bin/sh\ncase "$1" in _NPROCESSORS_ONLN|_NPROCESSORS_CONF) echo %s ;;\n*) exec /usr/bin/getconf "$@" ;; esac\n' \
    "$JOBS" >"$CPU_SHIM/getconf"
chmod +x "$CPU_SHIM/nproc" "$CPU_SHIM/getconf"

echo "== local-CI.sh =="
echo "repo:   $ROOT"
echo "ref:    $REF  ($(git -C "$ROOT" rev-parse --short "$ARCHIVE_REF" 2>/dev/null || echo '?'))"
echo "branch: $(git -C "$ROOT" rev-parse --abbrev-ref HEAD)"
echo "logs:   $LOGDIR"
echo "jobs:   ${SELECTED[*]}"
UNCLASSIFIED="$(unclassified_workflows)"
[ -n "$UNCLASSIFIED" ] && echo "FAIL:   unclassified workflows or jobs, run nowhere:" \
    "$(printf '%s\n' "$UNCLASSIFIED" | paste -sd' ')"
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

# Wall-clock checks: upstream steps asserting a time or throughput floor. On
# this shared desktop they fail when other work loads the machine, with
# nothing wrong (RETR-0067: at load 24 the scan throughput factor read 0.98
# against 0.80, and the HTTP yield 246724 bytes per update against 262144). A
# gate that fails at random teaches pushing past it, so the push gate runs one
# only when the push changes a file it builds, and then on a quiet machine.
# Each name maps to the <directory>:<makefile> that builds its test.
declare -A TIMING_STEPS=(
    ["Build and run scan_begin_budget_test (plain, ASan + UBSan, TSan)"]="samples/tasks/database:Makefile"
    ["Build and run http_transfer_stress_test (ASan + UBSan, TSan)"]="samples/tasks/http:Makefile.stress"
)
QUIET_LOAD="${LOCAL_CI_QUIET_LOAD:-6}"     # one-minute load average
QUIET_WAIT="${LOCAL_CI_QUIET_WAIT:-3600}"  # seconds, then run regardless

# timing_skips <tree> : print the TIMING_STEPS a default run leaves out, one
# per line: those whose test sources (make -n in <tree>) and directory the
# unpushed commits do not touch. A header counts when it sits beside one of
# the sources or under libretro-common/include. A run naming its jobs skips
# nothing, and a step whose sources cannot be listed is never skipped.
timing_skips() {
    local name dir mf srcs srcdirs files f hit
    [ "$DEFAULT_RUN" -eq 1 ] || return 0
    files="$(unpushed_files)" || return 0
    for name in "${!TIMING_STEPS[@]}"; do
        dir="${TIMING_STEPS[$name]%%:*}"; mf="${TIMING_STEPS[$name]#*:}"
        srcs="$(cd "$1/$dir" && make -n -B -f "$mf" all 2>/dev/null \
            | grep -oE '[^ ]+\.c\b' | sort -u \
            | while read -r f; do realpath -m --relative-to="$1" "$f"; done)" || continue
        [ -n "$srcs" ] || continue
        srcdirs="$(printf '%s\n' "$srcs" | sed 's#/[^/]*$##' | sort -u)"
        hit=0
        while read -r f; do
            case "$f" in
                "") ;;
                "$dir"/*|libretro-common/include/*) hit=1 ;;
                *.h) grep -qxF "${f%/*}" <<<"$srcdirs" && hit=1 ;;
                *)   grep -qxF "$f" <<<"$srcs" && hit=1 ;;
            esac
        done <<<"$files"
        [ "$hit" -eq 0 ] && printf '%s\n' "$name"
    done
}

# wait_quiet : before a wall-clock step, wait until the one-minute load
# average is below QUIET_LOAD, checking every 30 s for up to QUIET_WAIT
# seconds, then run regardless and say so.
wait_quiet() {
    local waited=0 load
    while :; do
        read -r load _ </proc/loadavg
        awk -v l="$load" -v q="$QUIET_LOAD" 'BEGIN { exit !(l < q) }' && return 0
        if [ "$waited" -ge "$QUIET_WAIT" ]; then
            echo "== load still $load after ${QUIET_WAIT}s; running the wall-clock step anyway"
            return 0
        fi
        [ "$waited" -eq 0 ] && echo "== load $load; waiting for it to fall below $QUIET_LOAD before a wall-clock step"
        sleep 30
        waited=$((waited + 30))
    done
}

# The workflows' "Enable ccache" step is skipped here (workflow_tool), so the
# steps find ccache's compiler links first on PATH instead. Every run builds
# in a new directory, which BASEDIR and NOHASHDIR keep out of the hash.
CCACHE_WRAP=""
for d in /usr/lib64/ccache /usr/lib/ccache; do
    [ -x "$d/cc" ] && { CCACHE_WRAP="$d"; break; }
done

# run_workflow_job <job> : run the steps of the workflow job JOB_SOURCE
# names for <job>, natively, from a fresh export of the ref, stopping at the
# first step that fails, as a runner does.
run_workflow_job() {
    local wf="${JOB_SOURCE[$1]%%:*}" wj="${JOB_SOURCE[$1]#*:}" w st name
    local WF_MATRIX="${JOB_MATRIX[$1]:-}" WF_SKIP_STEPS
    export WF_MATRIX WF_SKIP_STEPS
    w="$(mktemp -d "$LOGDIR/$1.XXXX")" && export_tree "$w" && mkdir "$w.tmp" || return 1
    WF_SKIP_STEPS="$(timing_skips "$w")"
    workflow_tool steps "$w/.github/workflows/$wf" "$wj" "$w.steps" || return 1
    for st in "$w.steps"/*.sh; do
        name="$(sed -n '1s/^# //p' "$st")"
        echo "== step: $name"
        [ -n "${TIMING_STEPS[$name]:-}" ] && wait_quiet
        # A private Wine prefix, not the user's; no core dump pop-up when a
        # test crashes.
        ( ulimit -c 0; cd "$w" && PATH="$CPU_SHIM${CCACHE_WRAP:+:$CCACHE_WRAP}:$PATH" \
            CCACHE_DIR="$CI_CCACHE" CCACHE_CONFIGPATH="$CI_CCACHE/ccache.conf" \
            CCACHE_BASEDIR="$w" CCACHE_NOHASHDIR=1 RUNNER_TEMP="$w.tmp" \
            GITHUB_WORKSPACE="$w" WINEPREFIX="$w.tmp/wine" bash "$st" ) || { echo "== step FAILED: $name"; return 1; }
    done
}

# ---- job bodies -----------------------------------------------------------
# The two hand-written jobs; every other job runs its workflow's own steps.

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
# The job FAILS if a C89 error originates in a repo-tracked file, or if make
# fails with no host-header error to account for it (a missing rule, a
# killed compiler). Linking is NOT asserted here (a skipped host-divergent
# TU makes the local link unreliable); linux-i686 is the full build+link.
job_c89() {
    local w raw repo_err sys_err n make_rc
    w="$(mktemp -d "$LOGDIR/c89.XXXX")" && export_tree "$w" || return 1
    raw="$LOGDIR/c89.build.log"
    ( cd "$w" || exit 1
      ./configure "${C89_ALIGN[@]}"
      rc=0
      make $JPAR -k C89_BUILD=1 || rc=1
      make $JPAR clean
      make $JPAR -k DEBUG=1 GL_DEBUG=1 C89_BUILD=1 info all || rc=1
      exit "$rc"
    ) >"$raw" 2>&1
    make_rc=$?
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
    if [ "$make_rc" -ne 0 ] && [ -z "$sys_err" ]; then
        echo "make failed with no C89 error to account for it — see $raw"; return 1
    fi
    echo "[pass] no repo-source C89 errors across the full tree (C89_BUILD + DEBUG)"
    return 0
}

# Linux.yml :: build  (i686 container — the image CI uses)
#   The job's steps run inside the container, from the workflow file at the
#   ref under test; workflow_tool refuses them if Linux.yml's image is not
#   IMG_I686.
job_linux_i686() {
    local s
    ensure_image || return 1
    s="$(mktemp -d "$LOGDIR/linux-i686.XXXX")" \
      && git -C "$ROOT" show "$ARCHIVE_REF:.github/workflows/Linux.yml" >"$s/Linux.yml" \
      && workflow_tool steps "$s/Linux.yml" build "$s/steps" "$IMG_I686" || return 1
    git -C "$ROOT" archive --format=tar "$ARCHIVE_REF" | podman run --rm -i --user root \
      -v "$s/steps:/steps:ro,Z" -v "$CPU_SHIM:/shim:ro,Z" "$IMG_I686" \
      bash -c 'set -e; mkdir -p /b && cd /b && tar x
               for st in /steps/*.sh; do
                   echo "== step: $(sed -n "1s/^# //p" "$st")"
                   PATH="/shim:$PATH" bash "$st"
               done'
}

# Linux-Headless.yml :: linux-nomenu  ("(i686)" name, but CI runs it on ubuntu-latest)
#   We reuse the xenial-gcc9 i686 image (isolation) and add --disable-qt — a
#   deviation from the workflow's bare --disable-menu. CI's headless job runs
#   Qt-off (ubuntu-latest, no Qt dev, so HAVE_QT=0); --disable-qt reproduces that,
#   and this image's bundled Qt5 doesn't link RA's Qt frontend anyway. Full
#   rationale + the Qt-link detail: docs/private/DEPENDENCY-POLICY.md §4a, on
#   the local/audit-2026-04 branch. A different OS and compiler from CI's,
#   so a warning or error only CI's newer gcc raises is not caught here.
job_headless_i686() {
    ensure_image || return 1
    git -C "$ROOT" archive --format=tar "$ARCHIVE_REF" | podman run --rm -i --user root -e JOBS="$JOBS" "$IMG_I686" \
      bash -c 'set -e; mkdir -p /b && cd /b && tar x
               ./configure --disable-menu --disable-qt
               make -j"$JOBS"'
}

# act:<workflow>  (ubuntu-latest, the real workflow file through act)
#   act needs a Docker API socket and a git checkout. A podman service is
#   started for the run and stopped on exit; the checkout is a depth-2 fetch
#   of ARCHIVE_REF under $LOGDIR, made once and reused by every act job, deep
#   enough for Removed-Member-Check's HEAD^. It holds its own objects: a clone sharing this
#   repository's has none inside the container. It is on a branch, not detached:
#   act copies files and no empty directories, so with no ref in .git/refs the
#   job's copy lacks that directory and git says "Not a git repository" there.
#   Jobs inside one workflow run one at a time, to bound RAM; the legs of one
#   job's matrix do not, as act ignores --concurrent-jobs for them.
ACT_SRC=""
ACT_SOCK=""
ACT_PID=""
#   A failed setup is not retried: a second try would meet the first's
#   half-made checkout and start a second podman service, so every later
#   act job fails at once, pointing at the first job's log.
ACT_SETUP_FAILED=0
act_setup() {
    [ -n "$ACT_SRC" ] && return 0
    [ "$ACT_SETUP_FAILED" -eq 0 ] \
        || { echo "act setup failed for an earlier job; its log says why"; return 1; }
    act_setup_once || { ACT_SETUP_FAILED=1; return 1; }
}
act_setup_once() {
    [ -x "$ACT_BIN" ] || { echo "act not found at $ACT_BIN (set ACT_BIN)"; return 1; }
    ACT_SOCK="$LOGDIR/podman.sock"
    podman system service --time=0 "unix://$ACT_SOCK" >"$LOGDIR/podman-service.log" 2>&1 &
    ACT_PID=$!
    # The clone and the socket go too, so a kept log directory holds logs only.
    trap '[ -n "$ACT_PID" ] && kill "$ACT_PID" 2>/dev/null; rm -rf "$LOGDIR/act-src" "$ACT_SOCK"' EXIT
    local _; for _ in 1 2 3 4 5 6 7 8 9 10; do [ -S "$ACT_SOCK" ] && break; sleep 1; done
    [ -S "$ACT_SOCK" ] || { echo "podman service did not start"; return 1; }
    git init -q "$LOGDIR/act-src" \
      && git -C "$LOGDIR/act-src" fetch -q \
           --upload-pack 'git -c uploadpack.allowAnySHA1InWant=true upload-pack' \
           --depth 2 \
           "file://$ROOT" "$(git -C "$ROOT" rev-parse "$ARCHIVE_REF")" \
      && git -C "$LOGDIR/act-src" checkout -q -b local-ci FETCH_HEAD \
      || return 1
    act_image || return 1
    ACT_SRC="$LOGDIR/act-src"
}

# act_image : set ACT_RUN_IMAGE, building it from ACT_IMAGE (see
# ACT_RUN_IMAGE for why) when it is missing or --pull asked for a fresh base.
act_image() {
    printf '%s\n' "FROM docker.io/$ACT_IMAGE" \
        'RUN rm /var/run && mkdir /var/run && ln -s /run/lock /var/run/lock' \
        'RUN apt-get update && apt-get install -y --no-install-recommends ccache libjson-perl clang libclang-rt-18-dev' \
        'RUN printf "path-exclude=/usr/bin/%s\n" samba-tool ganesha.nfsd >/etc/dpkg/dpkg.cfg.d/local-ci-exclude' \
        'RUN echo force-unsafe-io >/etc/dpkg/dpkg.cfg.d/local-ci-unsafe-io' \
        'RUN echo leak:libLLVM >/etc/local-ci-lsan.supp' \
        "RUN git config --system --add safe.directory '*'" \
        'COPY nproc getconf /usr/local/bin/' >"$CPU_SHIM/Containerfile"
    ACT_RUN_IMAGE="localhost/local-ci-act:$(cat "$CPU_SHIM"/Containerfile "$CPU_SHIM"/nproc \
        "$CPU_SHIM"/getconf | sha256sum | cut -c1-12)"
    if [ "$DO_PULL" -eq 0 ] && podman image exists "$ACT_RUN_IMAGE"; then return 0; fi
    if [ "$DO_PULL" -eq 1 ] || ! podman image exists "docker.io/$ACT_IMAGE"; then
        podman pull "docker.io/$ACT_IMAGE" || return 1
    fi
    podman build -q -t "$ACT_RUN_IMAGE" -f "$CPU_SHIM/Containerfile" "$CPU_SHIM"
}

# job_act <workflow>[:<job>] : the whole workflow, or one of its jobs.
# act exits 0 when every job is skipped or if-gated off, so a zero exit alone
# is not a pass: at least one "Job succeeded" line must show a job ran.
# --rm: without it act leaves a failed job's container running.
# seccomp=unconfined: a GitHub runner is a VM with no seccomp filter, and
# podman's default one refuses the personality call in the workflows'
# `setarch -R` ("failed to set personality"), so their TSan lanes fail here.
# --pids-limit: act's --container-options parser otherwise leaves the container
# a limit too low to fork a step's first child ("fork: retry: Resource
# temporarily unavailable"). 4096 is twice podman's default.
# --matrix <key>:ubuntu-latest, for a job whose runs-on is a matrix value: act
# resolves the image of such legs racily, and the ubuntu leg beside a macOS or
# Windows one sometimes starts with no image at all ("invalid reference
# format"). Dropping the other legs, which act would skip anyway, removes it.
# -P ubuntu-24.04: the image is 24.04, and act skips a job on a runner it has no
# image for.
# --network podman: act's default shares this machine's network, so a test's
# client reached the real Samba server on port 445. A GitHub runner has its own.
# --init: act's process 1 is `tail -f /dev/null`, which reaps nothing. A test
# that kills its Xvfb without waiting leaves a zombie whose pid keeps the lock
# file "live", so the next Xvfb on that display exits ("Server is already active
# for display 91"), as "xwd: unable to open display" or "Xvfb did not become
# ready". On a runner systemd reaps it.
# NET_RAW, NET_ADMIN: tshark captures on the container's own lo, as root on a
# runner can; without them it exits and local_read_plus_test.sh's kill fails.
# --action-offline-mode: matrix legs run at once and each otherwise updates
# act's shared copy of an action, so one fails ("remove .../.gitignore: no such
# file or directory") and takes the job's other legs with it.
# LSAN_OPTIONS: only stacks through libLLVM are suppressed; a step setting its
# own LSAN_OPTIONS replaces this.
# --cache-server-external-url: act tells a job to reach its cache server at
# this machine's LAN address, which under rootless podman's pasta network is
# the container's own copy of that address, so every actions/cache restore and
# save failed ("connect ECONNREFUSED") and each run downloaded every package
# again. host.containers.internal reaches the host. The URL needs the port, so
# a free one is picked per run; the server still binds the LAN address, as
# before.
job_act() {
    local wf="${1%%:*}" out rc key port
    local -a only=() runner=()
    [ "$wf" != "$1" ] && only=(-j "${1#*:}")
    act_setup || return 1
    port="$(python3 -Ic 'import socket; s = socket.socket(); s.bind(("", 0)); print(s.getsockname()[1])')" \
        || { echo "no free port for act's cache server"; return 1; }
    [ -f "$ACT_SRC/.github/workflows/$wf" ] || { echo "no such workflow at this ref: $wf"; return 1; }
    for key in $(sed -nE 's/^ *runs-on: *\$\{\{ *matrix\.([A-Za-z_]+) *\}\}.*/\1/p' \
            "$ACT_SRC/.github/workflows/$wf" | sort -u); do
        runner+=(--matrix "$key:ubuntu-latest")
    done
    out="$LOGDIR/$1.act.out"
    ( cd "$ACT_SRC" && DOCKER_HOST="unix://$ACT_SOCK" "$ACT_BIN" \
        -W ".github/workflows/$wf" -P "ubuntu-latest=$ACT_RUN_IMAGE" \
        -P "ubuntu-24.04=$ACT_RUN_IMAGE" --network podman --action-offline-mode \
        --cache-server-port "$port" \
        --cache-server-external-url "http://host.containers.internal:$port" \
        --env LSAN_OPTIONS=suppressions=/etc/local-ci-lsan.supp \
        --pull=false --rm --container-daemon-socket - --concurrent-jobs 1 \
        --container-options "--security-opt seccomp=unconfined --pids-limit 4096 --init --cap-add NET_RAW --cap-add NET_ADMIN" \
        "${runner[@]}" "${only[@]}" ) 2>&1 | tee "$out"
    rc=${PIPESTATUS[0]}
    if [ "$rc" -eq 0 ] && ! grep -q 'Job succeeded' "$out"; then
        echo "act ran no job in $wf (every job skipped or if-gated off) — not a pass"
        rc=1
    fi
    rm -f "$out"
    return "$rc"
}

# ---- driver ---------------------------------------------------------------

declare -A DISPATCH=(
    [c89]=job_c89
    [samples-tasks]=run_workflow_job
    [samples-tasks-http]=run_workflow_job
    [common-samples-codecs]=run_workflow_job
    [common-samples-formats]=run_workflow_job
    [common-samples-core]=run_workflow_job
    [config-fuzz]=run_workflow_job
    [linux-i686]=job_linux_i686
    [headless-i686]=job_headless_i686
)

declare -a RESULTS=()
overall=0
if [ -n "$UNCLASSIFIED" ]; then
    RESULTS+=("FAIL  unclassified, run nowhere: $(printf '%s\n' "$UNCLASSIFIED" | paste -sd' ')")
    overall=1
fi
for r in "${CHANGED_RESULTS[@]}"; do
    RESULTS+=("$r")
    [ "${r%% *}" = FAIL ] && overall=1
done

for job in "${SELECTED[@]}"; do
    fn="${DISPATCH[$job]:-}"; arg=""
    [ "${job#act:}" != "$job" ] && { fn=job_act; arg="${job#act:}"; }
    if [ -z "$fn" ]; then
        echo "!! unknown job '$job' (see --list)"; overall=1; RESULTS+=("SKIP  $job (unknown)"); continue
    fi
    if [ "$NO_CONTAINER" -eq 1 ] && { [ "$job" = "linux-i686" ] || [ "$job" = "headless-i686" ] || [ -n "$arg" ]; }; then
        echo "-- $job: skipped (--no-container)"; RESULTS+=("SKIP  $job (--no-container)"); continue
    fi
    # An ACT_WORKFLOWS entry upstream has since deleted is stale, not failing.
    if [ -n "$arg" ] && ! git -C "$ROOT" cat-file -e "$ARCHIVE_REF:.github/workflows/${arg%%:*}" 2>/dev/null; then
        for a in "${ACT_WORKFLOWS[@]}"; do
            [ "$a" = "${arg%%:*}" ] || continue
            echo "-- $job: skipped (not at this ref — stale ACT_WORKFLOWS entry)"
            RESULTS+=("SKIP  $job (stale ACT_WORKFLOWS entry: ${arg%%:*} not at this ref)")
            continue 2
        done
    fi
    log="$LOGDIR/${arg:-$job}.log"
    printf '>> %-14s ... ' "$job"
    # Called outside an if: inside one, bash ignores set -e in the job and
    # everything it runs, so a job's `( set -e ...)` would stop on nothing
    # and only its last command could fail it.
    "$fn" "${arg:-$job}" >"$log" 2>&1
    rc=$?
    if [ "$rc" -eq 0 ]; then
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
# Build trees are directories; logs are files. A kept log directory holds
# logs only, so repeated failures do not fill the disk.
find "$LOGDIR" -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +
if [ "$overall" -eq 0 ]; then
    echo "All selected Linux CI jobs passed. (Cross-platform jobs are GitHub-only; see --list.)"
    rm -rf "$LOGDIR"
else
    echo "One or more jobs FAILED. Logs kept in: $LOGDIR"
fi
exit "$overall"
