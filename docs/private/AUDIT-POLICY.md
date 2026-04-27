# Audit policy — canonical patterns the static analysers must learn to ignore

This file documents project-wide coding patterns that produce recurring false
positives in static analysis. Each pattern is correct as written; the
analyser cannot resolve it without project-side intent. Once a pattern lands
here, the corresponding pre-triage drop rule in `docs/private/audit/aggregate.py`
or per-rule entry in `.cppcheck-suppress.txt` becomes permanent.

The rule for adding a pattern: it must recur across at least three sites in
unrelated code paths, and refactoring those sites to a "single cleanup block"
or equivalent must touch otherwise stable, well-tested code.

---

## free-on-fail-label / ownership-move-on-success

Where one site:

```c
char *p = malloc(...);
if (!p)
   goto error;
ret = function_that_takes_ownership(p);
if (ret) {
   p = NULL;            /* ownership transferred — caller must not free */
   return success;
}
goto error;             /* on failure: free at the cleanup label */

error:
   free(p);             /* NULL-safe; the success path nulled p out */
   return failure;
```

**What semgrep sees** (rule `c.lang.security.double-free.double-free`): the
analyser walks both the success branch and the cleanup label and reports a
double-free at `error:` because it loses the success-path null-out.

**Why it's correct**: the success path returns before the cleanup label can
be reached, and on the error path `p` is the only owner. `free(NULL)` is a
no-op so the cleanup label is safe even when ownership has transferred.

**Where it appears in tree** (audit S9, 29 sites):
- `network/cloud_sync/s3.c` — multipart upload state hand-off
- `tasks/task_database_cue.c` — file-handle ownership in disc-image scan
- `gfx/drivers/d3d9cg.c` — D3D9 resource ownership at init
- `gfx/drivers_font/bitmapfont_*.c` — atlas-buffer ownership
- `tasks/task_save.c` — savestate/replay buffer hand-off
- plus stb / video_shader_parse vendored / inline pieces

**Why we don't refactor**: refactoring 29 sites to a single-cleanup-block
idiom (`if (!ret) goto error; ... error: free(p); return ret;`) would touch
well-tested code paths for no behavioural change. The analyser is wrong; the
code is correct.

**Suppression**: `aggregate.py` `KNOWN_FP_RULES` drops semgrep findings whose
rule id contains `double-free` and whose file matches the cluster prefixes.
The drop is permanent — a regression that introduces a real double-free in
one of these files would still surface in cppcheck's `doubleFree` and in
clang-analyzer's flow analysis (which both have stronger ownership tracking
than the semgrep regex match), so we don't lose detection coverage.

---

## When to add a new pattern here

1. The same finding-class fires in three or more unrelated files.
2. Each instance is verified correct by walking the ownership / control-flow
   chain manually (or by an analyser with stronger semantics — clang-analyzer's
   path-sensitive mode will usually disagree with semgrep on these classes).
3. The refactor that would silence the analyser would touch otherwise stable
   code with no behavioural improvement.

If any of those three is missing, prefer one of:

- per-line `// cppcheck-suppress <id>` / `// nosemgrep: <rule>` markers
- per-file entry in `.cppcheck-suppress.txt` (anchored to a line so a rewrite
  forces re-triage)
- per-site `docs/private/audit/suppressions.md` entry with a "verified
  resolved-stale" note if the issue does not exist in the current code

Project-wide policy is the heaviest hammer. Reach for it last.
