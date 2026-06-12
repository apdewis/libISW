# NanoVG — vendored copy

This directory is a vendored, in-tree copy of **NanoVG**, a small antialiased
vector-graphics renderer for OpenGL / OpenGL ES.

- **Upstream:** https://github.com/memononen/nanovg
- **Pinned commit:** `ce3bf745eb2d2dbc14a50bf2446783f691ac4353` (branch `master`)
- **License:** zlib (see [`LICENSE.txt`](LICENSE.txt))

## Why it's here

NanoVG provides the parts of a Cairo-free EGL/OpenGL rendering backend that are
hardest to write by hand: path tessellation, built-in geometry anti-aliasing,
gradients, image patterns, and TrueType text rendering. The ISW pure-EGL render
backend builds on these primitives.

## Provenance and integrity

The files below were copied **verbatim** (byte-for-byte) from the pinned commit
above. Their original in-file copyright and license notices are preserved
unchanged (zlib clause 3 — the notice may not be removed from a source
distribution).

| File                | Origin / notice                          |
|---------------------|------------------------------------------|
| `nanovg.c`          | NanoVG — zlib, © 2013 Mikko Mononen       |
| `nanovg.h`          | NanoVG — zlib, © 2013 Mikko Mononen       |
| `nanovg_gl.h`       | NanoVG — zlib, © 2009–2013 Mikko Mononen  |
| `nanovg_gl_utils.h` | NanoVG — zlib, © 2009–2013 Mikko Mononen  |
| `fontstash.h`       | fontstash — zlib, © 2009–2013 Mikko Mononen |
| `stb_truetype.h`    | stb — public domain (Sean Barrett)        |
| `stb_image.h`       | stb — public domain (Sean Barrett)        |
| `LICENSE.txt`       | NanoVG zlib license (verbatim upstream)   |

`nanovg.c` includes `fontstash.h`, `stb_truetype.h`, and `stb_image.h`; the full
set above is vendored together so the library builds as shipped and every
embedded notice is retained.

## Local modifications

**None.** All files are currently unmodified verbatim copies of the pinned
upstream commit.

### Modification policy (zlib clause 2)

If any file in this directory is ever altered from its upstream form, the change
**must**:

1. be recorded in the list below (date, file, and a one-line description), and
2. be marked inline in that file — add an "ISW-modified from upstream" note to
   the file's header so the altered source is plainly marked as such and not
   misrepresented as the original NanoVG.

| Date | File | Change |
|------|------|--------|
| —    | —    | (no local modifications yet) |
