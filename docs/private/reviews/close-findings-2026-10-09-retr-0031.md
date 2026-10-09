# close-findings ledger — RETR-0031, 2026-10-09

Findings: the scanner classes the 2026-10-02 whole-tree check-code run
reported and RETR-0025 did not read (raw output in
`/mnt/Games/claude-scratch/audit-1002/`, scanned at 73c7a9c0e3; per-class
row lists in its `r31/` folder). Rows were de-duplicated by file, line and
message, giving 1,129 rows in 31 classes.

Four reading lanes read every row on `local/fixes-2026-09` at e2918feb97,
each judging REAL, UNSURE or NOISE with a reason. Every REAL and UNSURE
verdict was then re-read by hand on that commit and on upstream master
5221eea01f before being recorded here. Line numbers below are at
e2918feb97.

## Dispositions

| finding | tool / class | verified | disposition |
|---|---|---|---|
| F1 `tasks/task_screenshot.c` `screenshot_rotate` 486-510: indexes a `uint32_t` buffer by pixel number, bounded by `> size / bpp` | cppcheck unsignedLessThanZero 507, 508 | yes, fork and upstream. A 2-byte-per-pixel frame writes up to 4N bytes into a 2N-byte calloc; a 4-byte frame writes element N, one past the end. Reached by the raw screenshot path (driver without viewport read) with a core that sets a rotation | queued RETR-0034 |
| F2 `gfx/video_filters/ntsc.c` 113: `x + t < 0` with `x` unsigned never fires | cppcheck unsignedLessThanZero | yes, fork and upstream. Left-edge taps wrap and read the line's last pixel | RETR-0034 |
| F3 `gfx/modeline/modeline_core.c` 593-609 `modeline_parse`: a user modeline with `htotal` 0 divides by zero | clang-tidy unchecked-string-to-number-conversion | yes, fork and upstream. Only `e != 9` is checked; integer division by `htotal`, double by `vtotal` | RETR-0034 |
| F4 `gfx/drivers_shader/shader_gl3.c` 1595-1600 with 2332-2340: push constant buffer calloc'd at its declared size, flattened `glUniform4fv` reads it rounded up to 16 | found next door by the integer-division read | yes, fork and upstream. The comment at 1604-1608 says a std430 block need not be a multiple of 16; over-read up to 12 bytes. The UBO case is std140 and is not affected | RETR-0034 |
| F5 `midi/drivers/winmm_midi.c` 290: unwind loop `while (--i <= 0)` on an unsigned | cppcheck unsignedLessThanZero | yes, fork and upstream. A failure on buffer 2 unprepares nothing | RETR-0034 |
| F6 `menu/drivers/xmb.c` 3302 and 3912: switches on two animation settings with no default leave `entry.duration` and `entry.easing_enum` uninitialised | clang-tidy unhandled-code-paths | yes, fork and upstream. The settings are not range-checked on config load; the sibling at 2602 is guarded at 2409 | RETR-0034 |
| F7 `frontend/drivers/platform_unix.c` 1776-1778, 1787-1790: `endptr` tested before `strtol` sets it | cppcheck knownConditionTrueFalse; also the tidy lane | yes, fork and upstream. The legacy `/proc/acpi` battery percentage is never computed. Coverity fix 72dc03a1c6 added the `endptr &&` | RETR-0034 |
| F8 `ui/drivers/ui_qt_widgets.cpp` 1043 `FloatSlider::paintEvent`: `value() / m_precision` is unsigned integer division compared with a float | clang-tidy integer-division | yes, fork and upstream. True on every paint for a fractional value, so `setValue` re-truncates each time | RETR-0034 |
| F9 `gfx/drivers/drm_gfx.c` 593-638 `modeset_create_dumbfb`: returns 0 on mmap failure and ignores its three ioctls | cppcheck knownConditionTrueFalse 282, 723 | yes, fork and upstream. Both callers only log, so a correct return value alone would not stop the write into `MAP_FAILED` | queued RETR-0035: needs the failure carried out of `drm_surface_setup`, which returns void |
| F10 `gfx/modeline/modeline_core.c` 546-547: GTF sync width truncated by integer division before `modeline_round_near` | clang-tidy integer-division | yes, fork and upstream. GTF rounds to nearest; this gives a sync up to 8 px narrower | queued RETR-0035: the fix changes generated modelines, unchecked on CRT hardware |
| vita_pib `shacccgpatch.c` `&shader + 0x30` writes | found next door by the void-pointer read | yes | dismissed: already RETR-0032 |
| `menu/cbs/menu_cbs_get_value.c` 300: `type - offset` not checked against the parameter count | clang-tidy assignment-in-selection-statement (UNSURE) | yes | dismissed: the menu type range ends at `MENU_SETTINGS_SHADER_PARAMETER_LAST`, which is `GFX_MAX_PARAMETERS - 1` (`menu/menu_driver.h`) |
| `ui/drivers/ui_win32_companion.c` 2935 `'i' < 0` | cppcheck unsignedLessThanZero (UNSURE) | yes | dismissed: no comparison of `i` against 0 exists there at the scan commit or HEAD |
| every other row | 31 classes | yes, per lane; grouped below | dismissed: tool noise, reasons below |

## Noise, by class

objectIndex (117): every row is `gfx_ctx_drm_load_mode`'s array of
pointers to separate `timings` fields in `drm_ctx.c`, not indexing past an
object.

duplicateCondition (45): `GENERAL_SETTING` expansions in
`configuration.c` testing literal flags; `if (fullscreen)` re-tested in an
`else if` in the X context drivers; repeated same-flag blocks in ozone,
xmb and vita_pib hooks; `runloop.c` 9888 reassigns between the tests.

unsignedLessThanZero (29 noise): `VIDEO_SCALE_PACK` / `VIDEO_SCALE_PUT_W`
uses, a cppcheck artefact not reproduced on a minimal file; `<= 0` on an
unsigned meaning `== 0`; `video_driver.c` 7098 has no such test.

knownConditionTrueFalse (411 noise): a pointer to a global or an array
element tested for NULL (146 rows, judged by message pattern with about 15
read); a feature, platform or stub folded to a constant in the scan's
configuration (138, read); a redundant check after an earlier guard (112,
read); a write cppcheck cannot see through a macro, callback or loop (15,
read).

branch-clone (118): no copy-paste error. Empty or `#ifdef` cases, label
tables mapping several entries to one result, and either-way fallbacks.

unhandled-code-paths (74 noise): the value falls to a return or a
pre-initialised default.

unchecked-string-to-number-conversion (69 noise): a bad string gives 0 or
a value that is range-checked after.

assignment-in-selection-statement (67 noise): every one is a deliberate
`if ((x = f()) && ...)`.

integer-division (34 noise): pixel centring and layout, frame counts and
whole-hertz labels where truncation is intended.

invalidPointerCast (32), invalidPrintfArgType (18),
arithOperationsOnVoidPointer (16): malloc'd or mapped buffers used as one
type at a time; same-width sign mismatches with small values; void-pointer
arithmetic only in GCC-built console and Linux code.

suspicious-string-compare (27) and the small bugprone classes (57 rows,
18 classes): each read; deliberate or bounded.

## Run level

- cited_by: none; nothing was renamed.
- swept: none; this run fixed nothing. The sweep belongs to RETR-0034's
  fixes.
- collateral: none.
- surfaced: none.
- out_of_scope, found next door: F4 and the vita `&shader` writes, both
  above. Not filed: `menu/menu_displaylist.c` near 11796 keeps an
  unreachable `build_list[i].checked = ...` statement after a `break`,
  left when 3e97ef9e4a removed its case labels; dead code, harmless.
- falsified: none.

## Correction, 2026-10-09, while fixing RETR-0034

F4 is withdrawn: dismissed, not reachable. Its verification checked the
allocation and the read size, not whether the read runs. Every
filter-chain pass is compiled by `gl3_cross_compile_program` with
`flatten` false, which leaves `flat_push_vertex` and
`flat_push_fragment` at -1, so the `glUniform4fv` branch never runs for
a pass. The flattened programs are the quad and `gl3.c` pipelines,
which never touch a pass's push constant buffer. Upstream master passes
false as well. RETR-0034 closed with seven fixes.
