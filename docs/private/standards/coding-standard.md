# Coding standard (fork)

**Authoritative sources — read these first. This doc consolidates and
deliberately echoes only the highest-cost rules; it does not exhaustively
restate them:**

- `CODING-GUIDELINES` (repo root) — struct-member ordering table, brace style.
- `CONTRIBUTING.md` §Coding style (repo root) — the C89/ISO-C++ subset,
  warning discipline, Allman braces, getter/setter avoidance, struct
  alignment, console stack limits, `HAVE_*` gating.
- `CLAUDE.md` (repo root) §Coding rules — the same rules with fork rationale.
- `https://docs.libretro.com/development/coding-standards/` — upstream.

This document adds only what those don't cover: **the fork's operational
rules** (branch model, verification discipline, secure-coding cross-link).

## 1. Portability floor (non-negotiable)

The single most-violated rule, restated because it costs the most when
missed (see ROADMAP Bundle 88 — four C89 regressions that pass local
desktop builds but fail `make C89_BUILD=1`, the `linux-c89` job; uncaught
until fixed because `local/*` branches skip CI):

- **C89 + ISO C++ compatible.** No declaration-after-statement, no
  `for (int i = …)`, no VLAs, no `//` comments, no compound literals in
  the C build. Declare locals at the top of a function or block.
- Enum values must be representable as a C89 `int`. A bare `1u << 31` is
  outside `int` range — write `(int)(1u << 31)` (as Bundle 88 fixed on
  `local/fixes-2026-04`).
- Build clean under `-Wall -Wsign-compare`; where practical
  `MISSING_DECLS=1` for `-Werror=missing-declarations`.

Rationale: the Xbox 360 / older-MSVC targets and the `linux-c89` CI job
enforce this. It is the explicit exception to "use current language
idioms" — do **not** reach for C99/C11 here even though it compiles on
desktop.

## 2. Style (see `CODING-GUIDELINES` for the full table)

- Allman braces; no braces for single-statement blocks unless the body is
  a multi-line macro. `for (;;)` over `while (true)`.
- Sort struct members by descending alignment; interleave a pointer with
  its matching `_len` field.
- No one-line getter/setter functions in hot paths — read/write the struct
  field directly (function-call overhead is measurable on PSP/3DS/Wii).
- Every new feature gated behind a `HAVE_*` macro from `config.h`.
- Update the copyright header in any file you substantially modify; add
  yourself to `AUTHORS.h` for significant work.

## 3. Memory & stack

- Console stacks are as small as 128 KB. Do not place
  `char buf[PATH_MAX_LENGTH]` (or similar large arrays) as locals in deep
  call stacks — use the caller's buffer or a single heap allocation.
- Every allocation whose failure is reachable is NULL-checked before the
  first deref. This is a recurring audit class (see
  [`security-standard.md`](security-standard.md) §Secure-coding rules).

## 4. Surgical edits (fork discipline)

- **No workarounds without a root-cause fix.** Silencing a warning,
  `#if 0`, commenting out broken code — last resort, and when unavoidable,
  leave a comment naming the underlying constraint.
- **Stay in your lane.** Every changed line traces to the task. No
  drive-by reformatting, no rewriting working code in a preferred idiom,
  no deleting pre-existing dead code without asking — surface it instead.
  Do clean up orphans your own change creates.
- **Shortest correct implementation.** Reuse before rewriting; extract a
  helper on the third call-site, not the first.

## 5. Verification discipline

- Source fixes are built before they are claimed done. The minimum gate is
  the affected object (`make obj-unix/release/<path>.o`); a
  behaviour-changing fix links `retroarch` and boots `./retroarch
  --version`. State the command run and its result — never assert
  "compiles" without having compiled.
- Reproduce-before-fix for bugs: where a `.ratst` replay or a
  `libretro-common` libcheck test can capture the symptom, write it first
  (see the root `CLAUDE.md` §Tests).
- Before pushing `local/*` source branches, run `local-CI.sh` — it lives at
  the root of the `local/fixes-2026-04` fixes worktree (e.g. `/tmp/ra-fixes`),
  not on the audit branch. `local/*` branches do not trigger GitHub CI.

## 6. Griffin (console unity build) — the file-addition trap

Adding or renaming a `.c` file: Linux/Windows builds pick it up via
`Makefile.common`, but **griffin/console builds silently miss it** unless
you also add it to `griffin/griffin.c` (or `griffin_cpp.cpp` /
`griffin_objc.m`). Verify any new `.c` appears in both. See
[`file-naming-standard.md`](file-naming-standard.md) §Drivers.
