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

`fontstash.h`, `nanovg.h`, `nanovg.c` and `nanovg_gl.h` are modified so the
EGL backend can honour the system font rendering settings (antialias, hinting,
hint style, subpixel order, LCD filter) and render subpixel (LCD) text. Each
modified file carries an "ISW-modified from upstream" note in its header and
`ISW:` comments at the changed lines. The remaining files are unmodified
verbatim copies of the pinned upstream commit.

### Modification policy (zlib clause 2)

If any file in this directory is ever altered from its upstream form, the change
**must**:

1. be recorded in the list below (date, file, and a one-line description), and
2. be marked inline in that file — add an "ISW-modified from upstream" note to
   the file's header so the altered source is plainly marked as such and not
   misrepresented as the original NanoVG.

| Date | File | Change |
|------|------|--------|
| 2026-11-05 | `fontstash.h` | Glyph atlas holds RGBA per-channel coverage (4 bytes/texel); FreeType fonts take per-font load flags, render mode, LCD filter, BGR order and hinted advances (`fonsSetFontRender`) instead of the fixed FORCE_AUTOHINT/TARGET_LIGHT rendering and unscaled advances; mono and LCD/LCD_V bitmaps expanded into the atlas; blur applied per channel. |
| 2026-11-05 | `nanovg.h`   | `NVG_IMAGE_COVERAGE` image flag for the coverage atlas. |
| 2026-11-05 | `nanovg.c`   | Font atlas textures created `NVG_TEXTURE_RGBA` with `NVG_IMAGE_COVERAGE`. |
| 2026-11-05 | `nanovg_gl.h` | Coverage textures: texType 3 (subpixel text, per-channel coverage blended against the text colour via `glBlendColor` + `GL_CONSTANT_COLOR`/`GL_ONE_MINUS_SRC_COLOR`) under source-over, texType 4 (coverage as alpha) otherwise. |
