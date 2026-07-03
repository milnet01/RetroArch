# Dependency version policy

Standing rule for this RetroArch fork: **every dependency we control rides the
latest stable release** — for features *and* for security fixes — unless a
newer version demonstrably breaks something we rely on. This file is the single
place that records (a) the rule, (b) the allowed exceptions, and (c) the
register of things that broke us. A later session uses it to re-test and un-pin
once a fix ships upstream.

Layered under `~/.claude/CLAUDE.md` rule 5 (latest-external-library-version) —
this file is the RetroArch-specific application of that rule, plus the breakage
ledger the global rule asks for.

---

## 1. The standing rule

Default to the **latest stable release** of every dependency in scope (§2).
Applies on two triggers:

- **Sweep posture (check, don't wait):** at the start of a release cycle, and
  whenever you touch a manifest / lockfile / CI workflow for any other reason,
  glance at the version pins on the way past and surface anything behind.
- **Security:** a published advisory against a dependency is reason enough to
  bump immediately, even mid-cycle.

Bumping a dependency means **updating the code that calls it in the same
change** (global rule 5b) — don't bump and leave a stale idiom or a comment
describing the old behaviour. If the bump is API-neutral, say so explicitly in
the commit message ("no caller changes — patch only") rather than skipping the
check.

## 2. What counts as a "dependency in scope"

| Class | Examples in this tree | How to check latest |
|-------|-----------------------|---------------------|
| Vendored libraries | `deps/` (7zip, glslang, rcheevos, stb, libz, zstd…), `libretro-common/` | Compare vendored version marker against the upstream tag |
| CI actions | `.github/workflows/*.yml` — `actions/checkout`, `taiki-e/checkout-action`, `actions/setup-*`, third-party actions | `gh api repos/<owner>/<action>/releases/latest` |
| CI runner images | `runs-on:` values, `container.image:` (e.g. `reallibretroretroarch/libretro-build-i386-ubuntu:xenial-gcc9`) | Registry tag list / upstream build-image repo |
| Container base images | any `FROM` in build Dockerfiles | Registry tag list |
| Toolchain / runtime pins | compiler images, SDK versions in `Makefile.<platform>` and CI | Upstream release notes |
| Lockfiles | none currently in core scope (vendored-by-tarball) | n/a until one appears |

**Out of scope — the dev host's own system libraries.** Packages installed on a
contributor's machine (pipewire, libxkbcommon, SDL2, …) are the host's concern,
not a project dependency. Where a *newer host library than CI's* changes a build
result, that is handled in `local-CI.sh` (its `C89_ALIGN` flags and
host-header-divergence reporting), **not** here. Do not pin a system library to
"match CI"; align the local build instead.

## 3. The only allowed exception — pin + document

Keep an older version **only** when a newer one explicitly breaks a feature we
rely on **and** there is no reasonable code-side fix. When you do:

1. Pin the version at the point of use (vendored marker / workflow / Makefile),
   with an inline one-line comment naming the constraint so the pin reads as
   deliberate, not neglect.
2. Add a row to the **Known-Breakage Register** (§4). No register row → the pin
   is treated as drift and a future sweep will bump it.

A pin without a register row is a bug in this process.

## 4. Known-Breakage Register

Each row is a version that broke us and the newer version we must re-test
against once it appears. **Re-test procedure:** when a release *newer than
"First-broken"* is available, build/run the affected feature against it; if the
break is gone, un-pin (bump to latest), delete the row, and note the un-pin in
the commit. If it still breaks, update the "First-broken" cell to the newest
version you tried so the next session doesn't repeat the test.

| Dependency | Last-known-good | First-broken | Symptom (what breaks) | Pinned at | Re-test when | Recorded |
|------------|-----------------|--------------|-----------------------|-----------|--------------|----------|
| _(none yet)_ | | | | | | |

> No dependency is currently pinned to an *older* version for a breakage
> reason. Vendored `deps/` are pinned *by the vendoring mechanism* (tarball
> snapshot), which is a re-sync cadence question (§6), not a breakage pin — do
> not record those here unless a specific upstream version broke a feature.
> Unpinned CI image tags are a separate concern — see §4a.

## 4a. Unpinned / mutable CI image tags

A container image referenced by a **mutable tag** (`name:tag`, not
`name@sha256:…`) is a dependency whose contents can change under the same tag
between pulls. Two machines pulling `name:tag` need not get identical
toolchains. Prefer a **digest pin** (`image@sha256:…`) on CI images we depend
on, so a build is reproducible and any content change is an explicit, reviewed
bump rather than a silent surprise.

Concrete example in this tree — the CI i686 image
`reallibretroretroarch/libretro-build-i386-ubuntu:xenial-gcc9`
(`.github/workflows/Linux.yml:19`) bundles a Qt5 whose moc does not link
RetroArch's Qt frontend: `master` itself fails that link in this image
(verified 2026-07-03 by building it). This never surfaces on CI — `Linux.yml`
passes `./configure --disable-qt …`, and the headless job (`Linux-Headless.yml`)
runs on `ubuntu-latest` with no Qt dev installed, so `HAVE_QT=0` in both. But
`local-CI.sh` reuses this image for its `headless-i686` job (to isolate from the
host toolchain), which *would* enable Qt5, so that job passes `--disable-qt` to
reproduce CI's Qt-off configuration. A digest pin on the image would make its
exact contents (Qt5 and all) explicit rather than implied by a floating tag.

## 5. Deliberate project-level constraints (intentional — not "behind")

These are *chosen* constraints, not stale pins. A sweep must **not** try to
"modernise" them. They are the standing project constraint global rule 5 carves
out for this project (distinct from §3's breakage-pin exception):

- **C89 / ISO-C++ source compatibility.** No C99+ idioms in RA source (declared
  in `CLAUDE.md` / `CODING-GUIDELINES`); required for Xbox 360 / old-MSVC /
  console targets. Enforced by the `C89_BUILD=1` make flag in `retroarch.yml`'s
  CI build.
- **Console SDK / toolchain versions** pinned in `Makefile.<platform>` to what
  the platform actually ships.

If one of these ever *becomes* changeable (e.g. a target is dropped), that is a
deliberate decision recorded elsewhere — not a dependency bump.

## 6. Vendored-dependency re-sync (fork-specific)

`deps/` and `libretro-common/` are vendored and shared with the wider libretro
ecosystem. Per `CLAUDE.md`, **do not make local-only edits** to them — patch
upstream and re-vendor, or the next sync clobbers the change. "Bump to latest"
for these means **re-vendoring the current upstream snapshot**, on a cadence
matched to upstream's release pace, not hand-editing the vendored copy.

---

*Created in-session 2026-07-03. Update the register (§4) whenever a version is
pinned for breakage, and clear rows when a newer version tests clean.*
