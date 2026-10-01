# PR draft — gl: upload per-draw coords with GL_STREAM_DRAW

Branch `pr/gl-stream-draw` in `/mnt/Games/Scripts/Linux/ra-pr`, one
commit on upstream/master `6bf58823c6`. Fork commit `734682a8f6`
(RETR-0016). Opened 2026-10-01 as PR #19661.

## Title

gl: upload per-draw coords with GL_STREAM_DRAW

## Body

`gl_glsl_set_vbo` re-specifies the GLSL backend's vertex buffer with
`glBufferData` on every draw whose coordinates changed. That is each
font flush and each menu quad, many times a frame. It declared the
buffer `GL_STATIC_DRAW`, the hint for data written once and drawn many
times. `GL_STREAM_DRAW` is the hint for this pattern, and the glcore
driver's scratch VBOs already use it.

The hint changes no results, only where the driver places the buffer.

### Measurements

- `perf` on Ozone's idle menu with the gl driver (radeonsi): 2.7% of
  the process's time is in this upload, via
  `gl2_raster_font_flush_block`.
- A windowless EGL benchmark on an RX 6600 (radeonsi) of the font
  path's pattern, twelve ~6 KB uploads and draws a frame, two runs:

  | Hint | Wall per frame | CPU per frame |
  |---|---|---|
  | `GL_STATIC_DRAW` (before) | 278 us | 99 us |
  | `GL_STREAM_DRAW` (after) | 228-236 us | 84-87 us |
  | `GL_DYNAMIC_DRAW` | 262-281 us | 98-99 us |

  Not measured on other GPUs. Slower and integrated GPUs, such as a
  Raspberry Pi's, are where the per-frame cost matters most.

### Testing

The Ozone menu runs on the gl driver with no GL errors.

Made with Claude Code, reviewed and build-tested on our fork.
