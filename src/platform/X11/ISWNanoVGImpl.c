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

/* Per-font glyph rendering (FreeType load flags incl. hinting target, render
   mode, LCD filter, BGR subpixel order, hinted advances).  See
   fonsSetFontRender. */
void isw_nvgSetFontRender(NVGcontext* ctx, int font, int loadFlags,
                          int renderMode, int lcdFilter, int bgr,
                          int hintedAdvance)
{
    fonsSetFontRender(ctx->fs, font, loadFlags, renderMode, lcdFilter, bgr,
                      hintedAdvance);
}

/* Axis-aligned bounds (frame coordinates: x0, y0, x1, y1) of the current
   scissor.  Returns 0 when no scissor is set. */
int isw_nvgScissorBounds(NVGcontext* ctx, float* bounds)
{
    NVGstate* state = nvg__getState(ctx);
    NVGscissor* sc = &state->scissor;
    float ex, ey;

    if (sc->extent[0] < -0.5f || sc->extent[1] < -0.5f)
        return 0;
    ex = fabsf(sc->xform[0]) * sc->extent[0] + fabsf(sc->xform[2]) * sc->extent[1];
    ey = fabsf(sc->xform[1]) * sc->extent[0] + fabsf(sc->xform[3]) * sc->extent[1];
    bounds[0] = sc->xform[4] - ex;
    bounds[1] = sc->xform[5] - ey;
    bounds[2] = sc->xform[4] + ex;
    bounds[3] = sc->xform[5] + ey;
    return 1;
}

/* Drop every cached glyph so fonts re-rasterize under changed rendering
   parameters.  Must be called outside a frame (no batched text pending). */
void isw_nvgResetFontAtlas(NVGcontext* ctx)
{
    int iw = 0, ih = 0;

    nvgImageSize(ctx, ctx->fontImages[ctx->fontImageIdx], &iw, &ih);
    if (iw > 0 && ih > 0)
        fonsResetAtlas(ctx->fs, iw, ih);
}

/* nvgTextMetrics with FontStash's span normalization corrected to em units.
   FontStash divides ascender/descender/lineh by (ascender - descender) font
   units, but the FreeType pixel scale is em-based (size / units_per_EM), so
   reported vertical metrics come out low by span/units_per_EM relative to
   what actually rasterizes.  Rescale so metrics match the rendered glyphs. */
void isw_nvgTextMetricsEm(NVGcontext* ctx, float* ascender, float* descender,
                          float* lineh)
{
    nvgTextMetrics(ctx, ascender, descender, lineh);
#ifdef FONS_USE_FREETYPE
    NVGstate* state = nvg__getState(ctx);

    if (state->fontId == FONS_INVALID) return;

    FONSfont* font = ctx->fs->fonts[state->fontId];
    FT_Face face = font->font.font;
    float span = (float)(face->ascender - face->descender);

    if (face->units_per_EM > 0 && span > 0.0f) {
        float factor = span / (float)face->units_per_EM;

        if (ascender) *ascender *= factor;
        if (descender) *descender *= factor;
        if (lineh) *lineh *= factor;
    }
#endif
}

#endif /* HAVE_EGL */
