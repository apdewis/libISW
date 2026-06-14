/*
 * ISWNanoVGImpl.c - Single translation unit that compiles the vendored NanoVG.
 *
 * Copyright (c) 2026 ISW Project
 *
 * NanoVG ships as header-with-implementation: the core and the GL backend are
 * compiled by defining the implementation macros in exactly ONE source file and
 * including the vendored sources.  This is that file; the rest of the EGL
 * backend (ISWRenderEGL.c) includes only the public NanoVG headers.
 *
 * The vendored copy lives in src/nanovg/ (zlib licensed, see its README.ISW.md).
 * This file does not modify NanoVG; it only selects its build configuration.
 *
 * Profile: OpenGL ES 2.0 (widest EGL/embedded coverage).
 * Fonts:   FreeType when available (metric parity with the rest of ISW), else
 *          the bundled stb_truetype fallback.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_EGL

/* Use FreeType for glyph rasterization so text metrics match the rest of the
   toolkit; FontStash picks this up via FONS_USE_FREETYPE. */
#ifdef HAVE_FREETYPE
#define FONS_USE_FREETYPE 1
#endif

#include <GLES2/gl2.h>

#define NANOVG_GLES2_IMPLEMENTATION
#include "../../nanovg/nanovg.c"
#include "../../nanovg/nanovg_gl.h"
#include "../../nanovg/nanovg_gl_utils.h"

/* Return tight glyph bounding box (y bounds from the actual glyphs, not the
   line metrics).  nvgTextBounds overwrites bounds[1]/[3] with fonsLineBounds;
   this version preserves the glyph-level y extents from fonsTextBounds. */
float isw_nvgTextGlyphBounds(NVGcontext* ctx, float x, float y,
                             const char* string, const char* end,
                             float* bounds)
{
    NVGstate* state = nvg__getState(ctx);
    float scale = nvg__getFontScale(state) * ctx->devicePxRatio;
    float invscale = 1.0f / scale;

    if (state->fontId == FONS_INVALID) return 0;

    fonsSetSize(ctx->fs, state->fontSize * scale);
    fonsSetSpacing(ctx->fs, state->letterSpacing * scale);
    fonsSetBlur(ctx->fs, state->fontBlur * scale);
    fonsSetAlign(ctx->fs, state->textAlign);
    fonsSetFont(ctx->fs, state->fontId);

    float width = fonsTextBounds(ctx->fs, x * scale, y * scale,
                                 string, end, bounds);
    if (bounds != NULL) {
        bounds[0] *= invscale;
        bounds[1] *= invscale;
        bounds[2] *= invscale;
        bounds[3] *= invscale;
    }
    return width * invscale;
}

#endif /* HAVE_EGL */
