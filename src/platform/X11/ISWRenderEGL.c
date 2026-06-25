/*
 * ISWRenderEGL.c - Pure EGL / OpenGL ES rendering backend (NanoVG-based)
 *
 * Copyright (c) 2026 ISW Project
 *
 * A Cairo-free render backend.  Each widget surface is a GL framebuffer object
 * (FBO) with a colour texture; NanoVG (vendored, src/nanovg/) draws the vector
 * content into the bound FBO, and the composite pass folds child FBOs into
 * parent FBOs as textured quads.  The windowed root owns an EGL window surface
 * and presents with eglSwapBuffers.
 *
 * CRITICAL: Uses EGL, NOT GLX - pure XCB, NO XLIB DEPENDENCIES.  The xcb
 * connection / window / visual are resolved through the platform ops exactly as
 * the Cairo-XCB backend does (_IswXcbConn / _IswXcbScreen / _IswXcbWindow /
 * _IswPlatformWidgetWindow / _IswXcbFindVisual).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_EGL

#include "ISWRenderEGL.h"
#include "ISWRenderCairoXCB.h"   /* for _IswXcbFindVisual (shared backend helper) */
#include "ISWPlatformPrivate.h"
#include <ISW/IntrinsicP.h>
#include <ISW/CoreP.h>
#include <ISW/CompositeP.h>
#include <ISW/SimpleP.h>
#include <ISW/ShellP.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>   /* GL_DEPTH24_STENCIL8_OES */
#include <fontconfig/fontconfig.h>

#include "../../nanovg/nanovg.h"
#define NANOVG_GLES2 1
#include "../../nanovg/nanovg_gl.h"
#include "../../nanovg/nanovg_gl_utils.h"

/* Defined in Initialize.c */
extern double _IswGetScaleFactor(IswDisplay dpy);

/* Tight glyph bounding box (ISWNanoVGImpl.c) — like nvgTextBounds but
   preserves the actual glyph y extents instead of substituting line bounds. */
extern float isw_nvgTextGlyphBounds(NVGcontext *ctx, float x, float y,
                                    const char *string, const char *end,
                                    float *bounds);

/*
 * =================================================================
 * Shared EGL/NanoVG state
 * =================================================================
 *
 * One EGL display + context + NanoVG context per process (all ISW surfaces
 * share a single GL context; per-widget surfaces differ only by their FBO).
 * The pbuffer surface keeps a context current when no window surface is bound,
 * so off-screen (windowless) FBO rendering works before any window is mapped.
 */
typedef struct {
    Boolean      initialized;
    Boolean      usable;
    EGLDisplay   egl_dpy;
    EGLConfig    config;
    EGLContext   ctx;
    EGLSurface   pbuffer;        /* 1x1 placeholder for off-screen rendering */
    NVGcontext  *vg;
    int          font_default;   /* NanoVG font handle, -1 until loaded */

    /* Images created during the current frame are queued here and deleted only
       after nvgEndFrame flushes the batch.  NanoVG batches draws until
       nvgEndFrame, and nvgDeleteImage deletes the GL texture immediately, so a
       per-draw delete would destroy the texture before the batched draw that
       samples it (icons would render as nothing). */
    int          pending_imgs[64];
    int          n_pending;
} EglShared;

static EglShared g_egl = { False, False, EGL_NO_DISPLAY, NULL,
                           EGL_NO_CONTEXT, EGL_NO_SURFACE, NULL, -1,
                           { 0 }, 0 };

/* Queue an image for deletion after the current frame's nvgEndFrame. */
static void
egl_defer_delete_image(int img)
{
    if (img <= 0) return;
    if (g_egl.n_pending < (int)(sizeof(g_egl.pending_imgs)/sizeof(int)))
        g_egl.pending_imgs[g_egl.n_pending++] = img;
    else
        nvgDeleteImage(g_egl.vg, img);  /* overflow: delete now (rare) */
}

/* Delete all images queued during the frame.  Called after nvgEndFrame. */
static void
egl_flush_pending_images(void)
{
    for (int i = 0; i < g_egl.n_pending; i++)
        nvgDeleteImage(g_egl.vg, g_egl.pending_imgs[i]);
    g_egl.n_pending = 0;
}

/* Backend-local save stack for the context state the draw ops read
   (current_color / line_width / current_font).  NanoVG's own save/restore does
   NOT cover these — they live on ISWRenderContext — so a widget that does
   Save / SetColor(bg) / FillRect / Restore / DrawString (e.g. the Text sink
   painting a line background then the glyphs) would otherwise draw the string in
   the un-restored colour (the background) and the text would be invisible. */
typedef struct { Pixel color; NVGcolor nvg_color; double line_width; IswFontStruct *font; } EglSaved;
static EglSaved g_save_stack[64];
static int      g_save_top = 0;
static NVGcolor g_nvg_color;

/* ---- Font cache --------------------------------------------------------
   Maps (family, weight, slant) → NanoVG font handle.  Entries are populated
   lazily on first use via fontconfig + nvgCreateFont.  The cache is small and
   lives for the process lifetime (same as the NanoVG context). */
#define EGL_FONT_CACHE_MAX 32
typedef struct {
    char *family;
    int   weight;
    int   slant;
    int   nvg_id;
} EglFontEntry;
static EglFontEntry g_font_cache[EGL_FONT_CACHE_MAX];
static int          g_font_cache_n = 0;

static int
egl_resolve_font(const char *family, int weight, int slant)
{
    if (!g_egl.vg) return g_egl.font_default;

    for (int i = 0; i < g_font_cache_n; i++) {
        EglFontEntry *e = &g_font_cache[i];
        if (e->weight == weight && e->slant == slant
            && strcmp(e->family, family) == 0)
            return e->nvg_id;
    }

    int nvg_id = -1;
    FcPattern *pat = FcPatternCreate();
    if (pat) {
        FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *) family);
        FcPatternAddInteger(pat, FC_WEIGHT, weight);
        FcPatternAddInteger(pat, FC_SLANT, slant);
        FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
        FcConfigSubstitute(NULL, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);
        FcResult res;
        FcPattern *m = FcFontMatch(NULL, pat, &res);
        if (m) {
            FcChar8 *file = NULL;
            if (FcPatternGetString(m, FC_FILE, 0, &file) == FcResultMatch
                && file) {
                char name[128];
                snprintf(name, sizeof(name), "fc_%d", g_font_cache_n);
                nvg_id = nvgCreateFont(g_egl.vg, name, (const char *) file);
            }
            FcPatternDestroy(m);
        }
        FcPatternDestroy(pat);
    }

    if (nvg_id < 0)
        nvg_id = g_egl.font_default;

    if (g_font_cache_n < EGL_FONT_CACHE_MAX) {
        EglFontEntry *e = &g_font_cache[g_font_cache_n++];
        e->family = strdup(family);
        e->weight = weight;
        e->slant  = slant;
        e->nvg_id = nvg_id;
    }

    return nvg_id;
}

/* Bounding box of the rectangles added to the current path, so egl_clip_path can
   apply a real rectangular nvgScissor (NanoVG's only clip).  Reset at path_begin;
   updated by egl_path_rectangle. */
static double  g_rect_minx, g_rect_miny, g_rect_maxx, g_rect_maxy;
static Boolean g_rect_seen = False;

/* Current point set by egl_path_move_to, so egl_show_text draws at it (it honours
   the active transform).  ProgressBar centres its text by PathMoveTo(tx,ty) then
   ShowText — without this the text would draw at (0,0) and be mis-placed. */
static double g_cur_x = 0, g_cur_y = 0;

/*
 * Per-widget surface — the concrete struct _IswSurface for this backend.
 */
struct _IswSurface {
    xcb_connection_t *connection;
    xcb_screen_t     *screen;
    xcb_window_t      window;       /* target window (windowed widget) or 0 */
    xcb_visualtype_t *visual;

    EGLSurface        egl_window;   /* EGL window surface (windowed root) or
                                       EGL_NO_SURFACE for windowless widgets */

    GLuint            fbo;          /* back framebuffer */
    GLuint            tex;          /* colour texture backing the FBO */
    GLuint            rbo_stencil;  /* depth/stencil renderbuffer (NanoVG needs
                                       a stencil buffer) */

    int               back_w, back_h;   /* physical-pixel FBO extent */
    Boolean           deferred;         /* created unsized; build on first begin */
    Boolean           back_needs_clear; /* FBO freshly (re)allocated: clear to
                                           transparent on next begin, ONCE.  Not
                                           every begin — widgets like Command
                                           paint in two begin/end passes into the
                                           same FBO and clearing each pass would
                                           wipe the first pass's content. */
    int               frame_depth;      /* nested begin/end guard */
    double            scale;            /* HiDPI device scale */

    int               save_count;

    /* Deferred border ring (drawn after widget content so its AA isn't
       overwritten by the background fill's outward fringe at corners). */
    Boolean           has_border_ring;
    NVGcolor          border_color;
    float             border_width;
    float             border_half;
    float             border_fw, border_fh;
    float             border_radius;
};

typedef struct _IswSurface ISWRenderEGLData;

/* ===================================================================
 * EGL / NanoVG shared init
 * =================================================================== */

static Boolean
egl_shared_init(IswDisplay dpy)
{
    if (g_egl.initialized)
        return g_egl.usable;

    g_egl.initialized = True;
    g_egl.usable = False;

    g_egl.egl_dpy = eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY);
    if (g_egl.egl_dpy == EGL_NO_DISPLAY)
        return False;

    EGLint major, minor;
    if (!eglInitialize(g_egl.egl_dpy, &major, &minor))
        return False;

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        return False;

    /* A stencil buffer is mandatory for NanoVG's fills. */
    const EGLint cfg_attrs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_STENCIL_SIZE,    8,
        EGL_NONE
    };
    EGLint num_cfg = 0;
    if (!eglChooseConfig(g_egl.egl_dpy, cfg_attrs, &g_egl.config, 1, &num_cfg)
        || num_cfg < 1)
        return False;

    const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    g_egl.ctx = eglCreateContext(g_egl.egl_dpy, g_egl.config,
                                 EGL_NO_CONTEXT, ctx_attrs);
    if (g_egl.ctx == EGL_NO_CONTEXT)
        return False;

    const EGLint pb_attrs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    g_egl.pbuffer = eglCreatePbufferSurface(g_egl.egl_dpy, g_egl.config,
                                            pb_attrs);
    if (g_egl.pbuffer == EGL_NO_SURFACE)
        return False;

    if (!eglMakeCurrent(g_egl.egl_dpy, g_egl.pbuffer, g_egl.pbuffer,
                        g_egl.ctx))
        return False;

    g_egl.vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (g_egl.vg == NULL)
        return False;

    /* Load a default font so nvgText renders.  Resolve a concrete font file via
       fontconfig (the same discovery the Cairo backend uses), then hand the path
       to NanoVG/FontStash. */
    {
        FcPattern *pat = FcPatternCreate();
        if (pat) {
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *) "Sans");
            FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
            FcConfigSubstitute(NULL, pat, FcMatchPattern);
            FcDefaultSubstitute(pat);
            FcResult res;
            FcPattern *m = FcFontMatch(NULL, pat, &res);
            if (m) {
                FcChar8 *file = NULL;
                if (FcPatternGetString(m, FC_FILE, 0, &file) == FcResultMatch
                    && file)
                    g_egl.font_default =
                        nvgCreateFont(g_egl.vg, "sans", (const char *) file);
                FcPatternDestroy(m);
            }
            FcPatternDestroy(pat);
        }
        if (g_egl.font_default < 0)
            fprintf(stderr, "ISWRenderEGL: no default font loaded; "
                            "text will not render\n");
    }

    (void) dpy;
    g_egl.usable = True;
    return True;
}

/* Make the shared context current, bound to `draw`/`read` (a window surface or
   the pbuffer placeholder). */
static void
egl_make_current(EGLSurface draw)
{
    EGLSurface s = (draw != EGL_NO_SURFACE) ? draw : g_egl.pbuffer;
    eglMakeCurrent(g_egl.egl_dpy, s, s, g_egl.ctx);
}

/* (Re)allocate the FBO + colour texture + stencil renderbuffer at the given
   physical size.  Returns True on success. */
static Boolean
egl_ensure_fbo(IswSurface s, int pw, int ph)
{
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;
    if (s->fbo && s->back_w == pw && s->back_h == ph)
        return True;

    if (s->fbo)        { glDeleteFramebuffers(1, &s->fbo);  s->fbo = 0; }
    if (s->tex)        { glDeleteTextures(1, &s->tex);      s->tex = 0; }
    if (s->rbo_stencil){ glDeleteRenderbuffers(1, &s->rbo_stencil);
                         s->rbo_stencil = 0; }

    glGenTextures(1, &s->tex);
    glBindTexture(GL_TEXTURE_2D, s->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &s->rbo_stencil);
    glBindRenderbuffer(GL_RENDERBUFFER, s->rbo_stencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, pw, ph);

    glGenFramebuffers(1, &s->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s->tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, s->rbo_stencil);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, s->rbo_stencil);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "ISWRenderEGL: incomplete framebuffer %dx%d\n", pw, ph);
        return False;
    }
    s->back_w = pw;
    s->back_h = ph;
    s->back_needs_clear = True;   /* fresh buffer: begin() clears it once */
    return True;
}

/* ===================================================================
 * Backend context state (mirrors the active surface for the draw ops)
 *
 * The neutral ISWRenderContext carries no GL handles.  The draw ops read the
 * shared NanoVG context (g_egl.vg) plus per-context colour/line/font state.
 * NanoVG itself is the current-state machine; we keep just enough here to
 * service set_color / set_line_width / set_font and pixel decoding.
 * =================================================================== */

static NVGcolor
egl_pixel_to_nvg(IswSurface s, Pixel pixel)
{
    /* Decode against the backend visual (24/32-bit TrueColor: packed 0xRRGGBB,
       alpha opaque).  Mirrors the Cairo backend's packed-pixel decode. */
    (void) s;
    return nvgRGBA((unsigned char) ((pixel >> 16) & 0xff),
                   (unsigned char) ((pixel >>  8) & 0xff),
                   (unsigned char) ((pixel      ) & 0xff),
                   (unsigned char) ((pixel >> 24) & 0xff));
}

/* The currently-bound surface for draw ops (set in surface begin). */
static IswSurface g_active_surface = NULL;

/* ===================================================================
 * Surface ops
 * =================================================================== */

/* Nearest windowed ancestor of a (possibly windowless) widget. */
static Widget
egl_windowed_widget(Widget w)
{
    while (w != NULL && IswIsWidget(w) && !IswIsShell(w) &&
           w->core.parent != NULL)
        w = w->core.parent;
    return w;
}

static IswSurface
egl_surface_create(Widget widget)
{
    if (!egl_shared_init(IswDisplayOf(widget)))
        return NULL;

    IswSurface s = (IswSurface) calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->connection = _IswXcbConn(IswDisplayOf(widget));
    s->screen     = _IswXcbScreen(IswScreenOf(widget));
    s->window     = _IswXcbWindow(
        _IswPlatformWidgetWindow(IswDisplayOf(widget), widget));
    s->egl_window = EGL_NO_SURFACE;
    s->scale      = _IswGetScaleFactor(IswDisplayOf(widget));

    uint8_t depth = (widget->core.depth != 0)
                  ? widget->core.depth : s->screen->root_depth;
    s->visual = _IswXcbFindVisual(s->screen, depth);
    if (!s->visual && depth != s->screen->root_depth)
        s->visual = _IswXcbFindVisual(s->screen, s->screen->root_depth);

    /* Defer until the widget is sized and (for windowed widgets) has a window. */
    if (widget->core.width == 0 || widget->core.height == 0) {
        s->deferred = True;
        return s;
    }
    s->deferred = False;
    return s;
}

static void
egl_surface_destroy(IswSurface s)
{
    if (!s) return;
    if (g_active_surface == s)
        g_active_surface = NULL;

    if (g_egl.usable) {
        egl_make_current(EGL_NO_SURFACE);
        if (s->fbo)         glDeleteFramebuffers(1, &s->fbo);
        if (s->tex)         glDeleteTextures(1, &s->tex);
        if (s->rbo_stencil) glDeleteRenderbuffers(1, &s->rbo_stencil);
        if (s->egl_window != EGL_NO_SURFACE)
            eglDestroySurface(g_egl.egl_dpy, s->egl_window);
    }
    free(s);
}

/* Create the EGL window surface for a windowed widget on first present. */
static void
egl_ensure_window_surface(IswSurface s)
{
    if (s->egl_window != EGL_NO_SURFACE || s->window == 0)
        return;
    s->egl_window = eglCreateWindowSurface(g_egl.egl_dpy, g_egl.config,
                                           (EGLNativeWindowType) s->window,
                                           NULL);
}

static void *
egl_surface_begin(IswSurface s, Widget widget)
{
    if (!s) return NULL;

    /* Nested begin: a parent frame is already active. */
    if (s->frame_depth > 0) {
        s->frame_depth++;
        return g_egl.vg;
    }

    /* Late surface build for a deferred widget. */
    if (s->deferred) {
        if (widget->core.width == 0 || widget->core.height == 0)
            return NULL;
        if (s->window == 0)
            s->window = _IswXcbWindow(
                _IswPlatformWidgetWindow(IswDisplayOf(widget), widget));
        s->deferred = False;
    }

    s->scale = _IswGetScaleFactor(IswDisplayOf(widget));

    /* Surface-per-widget model (mirrors the Cairo backend): a windowless widget
       owns an FBO sized to its OWN footprint (content + border ring), drawing at
       local (0,0); content is offset by the border width and the composite pass
       folds this footprint into the parent at the child's slot.  A windowed
       widget (shell) covers its whole window. */
    Boolean windowless = (widget && !IswIsShell(widget));
    IswBorderSides bs = {0, 0, 0, 0};
    if (windowless)
        bs = _IswGetBorderSides(widget);
    int fw = windowless ? (widget->core.width  + _IswBorderHoriz(bs)) : widget->core.width;
    int fh = windowless ? (widget->core.height + _IswBorderVert(bs))  : widget->core.height;
    int pw = (int) (fw * s->scale + 0.5);
    int ph = (int) (fh * s->scale + 0.5);

    egl_make_current(EGL_NO_SURFACE);   /* off-screen: render into our FBO */
    if (!egl_ensure_fbo(s, pw, ph))
        return NULL;

    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
    glViewport(0, 0, s->back_w, s->back_h);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    s->back_needs_clear = False;
    glClear(GL_STENCIL_BUFFER_BIT);   /* stencil must be fresh each frame */
    

    /* NanoVG works in logical pixels; pass the device-pixel-ratio so its AA and
       line widths match the FBO's physical resolution. */
    nvgBeginFrame(g_egl.vg,
                  (float) (s->back_w / s->scale),
                  (float) (s->back_h / s->scale),
                  (float) s->scale);

    /* Border ring: stroke or fill the border edges.  Skipped for widgets that
       paint their own border (self_border, e.g. ProgressBar).  When all four
       sides are equal, stroke a centered rect; otherwise fill four rects. */
    Dimension ring_r = (IswIsSubclass(widget, simpleWidgetClass))
                       ? ((SimpleWidget) widget)->simple.corner_radius : 0;
    Boolean has_border = (bs.top || bs.right || bs.bottom || bs.left) &&
        !(IswIsSubclass(widget, simpleWidgetClass) &&
          ((SimpleWidget) widget)->simple.self_border);

    s->has_border_ring = False;
    if (has_border) {
        Pixel bp = (IswIsSubclass(widget, simpleWidgetClass) &&
                    ((SimpleWidget) widget)->simple.use_border_color)
                   ? ((SimpleWidget) widget)->simple.border_color
                   : widget->core.border_pixel;
        NVGcolor col = nvgRGBA((bp >> 16) & 0xff, (bp >> 8) & 0xff,
                               bp & 0xff, 255);

        if (_IswBorderIsUniform(bs)) {
            s->has_border_ring = True;
            s->border_color = col;
            s->border_width = (float) bs.top;
            s->border_half = (float) bs.top / 2.0f;
            s->border_fw = (float) (widget->core.width + bs.top);
            s->border_fh = (float) (widget->core.height + bs.top);
            s->border_radius = ring_r > 0 ? (float) ring_r + s->border_half : 0;
        } else {
            float ffw = (float) (widget->core.width + _IswBorderHoriz(bs));
            float ffh = (float) (widget->core.height + _IswBorderVert(bs));
            nvgBeginPath(g_egl.vg);
            if (bs.top > 0)
                nvgRect(g_egl.vg, 0, 0, ffw, (float) bs.top);
            if (bs.bottom > 0)
                nvgRect(g_egl.vg, 0, ffh - (float) bs.bottom,
                        ffw, (float) bs.bottom);
            if (bs.left > 0)
                nvgRect(g_egl.vg, 0, (float) bs.top,
                        (float) bs.left, ffh - (float) bs.top - (float) bs.bottom);
            if (bs.right > 0)
                nvgRect(g_egl.vg, ffw - (float) bs.right, (float) bs.top,
                        (float) bs.right, ffh - (float) bs.top - (float) bs.bottom);
            nvgFillColor(g_egl.vg, col);
            nvgFill(g_egl.vg);
        }
    }

    /* Content draws at local (0,0) = inside the border ring. */
    if (bs.left > 0 || bs.top > 0)
        nvgTranslate(g_egl.vg, (float) bs.left, (float) bs.top);
    nvgScissor(g_egl.vg, 0, 0, (float) widget->core.width,
               (float) widget->core.height);

    g_active_surface = s;
    s->frame_depth = 1;
    s->save_count = 0;
    g_save_top = 0;   /* reset the colour/line/font save stack for this frame */
    return g_egl.vg;
}

/* Blit a surface's FBO texture to its EGL window surface and swap.  Shared by
   the windowed-root branch of end() (self-painting roots) and present_root
   (composite-pass driven roots).  `window` is the neutral handle, used to
   resolve the xcb id if the surface has not cached one yet. */
static void
egl_present_to_window(IswSurface s, IswWindow window)
{
    if (!s || s->tex == 0)
        return;

    if (s->window == 0 && window != NULL)
        s->window = _IswXcbWindow(window);
    if (s->window == 0)
        return;

    egl_ensure_window_surface(s);
    if (s->egl_window == EGL_NO_SURFACE)
        return;

    egl_make_current(s->egl_window);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, s->back_w, s->back_h);

    /* Present the back texture with NanoVG: a single full-extent image quad. */
    int img = nvglCreateImageFromHandleGLES2(g_egl.vg, s->tex,
                                             s->back_w, s->back_h,
                                             NVG_IMAGE_FLIPY | NVG_IMAGE_NODELETE);
    nvgBeginFrame(g_egl.vg,
                  (float) (s->back_w / s->scale),
                  (float) (s->back_h / s->scale),
                  (float) s->scale);
    NVGpaint p = nvgImagePattern(g_egl.vg, 0, 0,
                                 (float) (s->back_w / s->scale),
                                 (float) (s->back_h / s->scale),
                                 0.0f, img, 1.0f);
    nvgBeginPath(g_egl.vg);
    nvgRect(g_egl.vg, 0, 0,
            (float) (s->back_w / s->scale),
            (float) (s->back_h / s->scale));
    nvgFillPaint(g_egl.vg, p);
    nvgFill(g_egl.vg);
    nvgEndFrame(g_egl.vg);
    nvgDeleteImage(g_egl.vg, img);

    eglSwapBuffers(g_egl.egl_dpy, s->egl_window);
}

static void
egl_surface_end(IswSurface s, Widget widget, IswWindow window)
{
    if (!s) return;

    if (s->frame_depth > 1) {
        s->frame_depth--;
        return;
    }
    s->frame_depth = 0;

    /* Draw deferred border ring on top of widget content so the background
       fill's AA fringe at rounded corners doesn't overwrite the border. */
    if (g_active_surface == s && s->has_border_ring) {
        nvgResetTransform(g_egl.vg);
        nvgResetScissor(g_egl.vg);
        nvgBeginPath(g_egl.vg);
        if (s->border_radius > 0)
            nvgRoundedRect(g_egl.vg, s->border_half, s->border_half,
                           s->border_fw, s->border_fh, s->border_radius);
        else
            nvgRect(g_egl.vg, s->border_half, s->border_half,
                    s->border_fw, s->border_fh);
        nvgStrokeColor(g_egl.vg, s->border_color);
        nvgStrokeWidth(g_egl.vg, s->border_width);
        nvgStroke(g_egl.vg);
        s->has_border_ring = False;
    }

    /* Finish NanoVG drawing into the bound FBO. */
    if (g_active_surface == s) {
        nvgEndFrame(g_egl.vg);
        /* Now that the batch has flushed, it is safe to delete images created
           during this frame's draws (icons, etc.). */
        egl_flush_pending_images();
    }

    /* Windowless: leave the painted FBO for the composite pass to fold. */
    if (widget && !IswIsShell(widget))
        return;

    /* Windowed root that painted its own content (self-paint path): present it
       now.  The composite-pass path presents via present_root instead. */
    egl_present_to_window(s, window);
}

/* present_root: present a folded composite root surface to its window.  Called
   by the composite pass after folding all children into the root's FBO. */
static void
egl_surface_present_root(IswSurface s, Widget widget, IswWindow window,
                         int width, int height)
{
    (void) widget; (void) width; (void) height;
    egl_present_to_window(s, window);
}

/* fill_background: clear the composite root's FBO to the widget's background. */
static void
egl_surface_fill_background(IswSurface s, Widget widget)
{
    if (!s || widget == NULL) return;

    double sf = _IswGetScaleFactor(IswDisplayOf(widget));
    int pw = (int) (widget->core.width  * sf + 0.5);
    int ph = (int) (widget->core.height * sf + 0.5);

    egl_make_current(EGL_NO_SURFACE);
    if (!egl_ensure_fbo(s, pw, ph))
        return;
    s->scale = sf;

    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
    glViewport(0, 0, s->back_w, s->back_h);

    Pixel bg = widget->core.background_pixel;
    glClearColor(((bg >> 16) & 0xff) / 255.0f,
                 ((bg >>  8) & 0xff) / 255.0f,
                 ((bg      ) & 0xff) / 255.0f,
                 ((bg >> 24) & 0xff) / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

/* composite_onto: fold src's FBO texture onto dst's FBO at the child's position
   within dst's content area, clipped to dst's bounds. */
static void
egl_surface_composite_onto(IswSurface dd, Widget dst_widget,
                           IswSurface sd, Widget src_widget, int x, int y)
{
    if (!dd || !sd || sd->tex == 0 || dd->fbo == 0)
        return;

    double sf = dd->scale > 0 ? dd->scale : 1.0;
    int dst_off_x, dst_off_y;
    if (dst_widget && !IswIsShell(dst_widget)) {
        IswBorderSides dbs = _IswGetBorderSides(dst_widget);
        dst_off_x = dbs.left;
        dst_off_y = dbs.top;
    } else {
        dst_off_x = dst_off_y = 0;
    }

    egl_make_current(EGL_NO_SURFACE);
    glBindFramebuffer(GL_FRAMEBUFFER, dd->fbo);
    glViewport(0, 0, dd->back_w, dd->back_h);

    /* src footprint in logical pixels (its FBO covers its own extent). */
    float fw = (float) (sd->back_w / (sf > 0 ? sf : 1.0));
    float fh = (float) (sd->back_h / (sf > 0 ? sf : 1.0));

    int img = nvglCreateImageFromHandleGLES2(g_egl.vg, sd->tex,
                                             sd->back_w, sd->back_h,
                                             NVG_IMAGE_FLIPY | NVG_IMAGE_NODELETE);

    nvgBeginFrame(g_egl.vg,
                  (float) (dd->back_w / sf),
                  (float) (dd->back_h / sf),
                  (float) sf);

    /* Clip to dst's content rectangle (the clipping the X server used to
       enforce via child windows).  nvgScissor sets the base region; the further
       clips below intersect into it (nvgIntersectScissor), matching the Cairo
       backend's stacked cairo_clip calls. */
    if (dst_widget != NULL)
        nvgScissor(g_egl.vg, (float) dst_off_x, (float) dst_off_y,
                   (float) dst_widget->core.width,
                   (float) dst_widget->core.height);

    /* Composite clip the parent imposed on this child (Viewport confining its
       scrolled content to the clip region so it does not overflow the viewport
       and overdraw the scrollbars).  Given in the parent's content frame; the
       child folds at (x,y) = parent_origin + child.x/y, so the parent frame sits
       at (x - child.x, y - child.y).  Without this, scrolled content bleeds over
       the scrollbars (which then appear to scroll with the content). */
    if (IswIsWidget(src_widget) && src_widget->core.composite_clip &&
        src_widget->core.composite_clip_w > 0) {
        int frame_x = x - (int) src_widget->core.x;
        int frame_y = y - (int) src_widget->core.y;
        nvgIntersectScissor(g_egl.vg,
                            (float) (dst_off_x + frame_x + src_widget->core.composite_clip_x),
                            (float) (dst_off_y + frame_y + src_widget->core.composite_clip_y),
                            (float) src_widget->core.composite_clip_w,
                            (float) src_widget->core.composite_clip_h);
    }

    /* Confine the source to its own widget footprint at its composited position,
       so surface slack (scrollbars, AA bleed) beyond the widget rectangle does
       not overflow into adjacent siblings regardless of fold order. */
    if (IswIsWidget(src_widget)) {
        IswBorderSides sbs = _IswGetBorderSides(src_widget);
        nvgIntersectScissor(g_egl.vg,
                            (float) (dst_off_x + x), (float) (dst_off_y + y),
                            (float) (src_widget->core.width + _IswBorderHoriz(sbs)),
                            (float) (src_widget->core.height + _IswBorderVert(sbs)));
    }

    {
        float ox = (float) (dst_off_x + x);
        float oy = (float) (dst_off_y + y);
        Dimension cr = (IswIsWidget(src_widget) &&
                        IswIsSubclass(src_widget, simpleWidgetClass))
                       ? ((SimpleWidget) src_widget)->simple.corner_radius : 0;
        NVGpaint p = nvgImagePattern(g_egl.vg, ox, oy, fw, fh, 0.0f, img, 1.0f);
        nvgBeginPath(g_egl.vg);
        if (cr > 0) {
            IswBorderSides sbs2 = _IswGetBorderSides(src_widget);
            nvgShapeAntiAlias(g_egl.vg, 0);
            nvgRoundedRect(g_egl.vg, ox, oy, fw, fh, (float) cr + sbs2.left);
        } else {
            nvgRect(g_egl.vg, ox, oy, fw, fh);
        }
        nvgFillPaint(g_egl.vg, p);
        nvgFill(g_egl.vg);
        if (cr > 0)
            nvgShapeAntiAlias(g_egl.vg, 1);
    }

    nvgEndFrame(g_egl.vg);
    nvgDeleteImage(g_egl.vg, img);
    (void) src_widget;
}

/* ===================================================================
 * Draw ops (NanoVG adapters)
 *
 * All ops operate on the shared g_egl.vg, which surface begin() left in a
 * frame.  ctx->current_color / line_width / current_font hold the neutral state.
 * =================================================================== */

#define VG (g_egl.vg)

static void egl_save(ISWRenderContext *ctx)
{
    if (VG) nvgSave(VG);
    if (g_save_top < (int)(sizeof(g_save_stack)/sizeof(g_save_stack[0]))) {
        g_save_stack[g_save_top].color      = ctx->current_color;
        g_save_stack[g_save_top].nvg_color  = g_nvg_color;
        g_save_stack[g_save_top].line_width = ctx->line_width;
        g_save_stack[g_save_top].font       = ctx->current_font;
    }
    g_save_top++;
}

static void egl_restore(ISWRenderContext *ctx)
{
    if (VG) nvgRestore(VG);
    if (g_save_top > 0) g_save_top--;
    if (g_save_top < (int)(sizeof(g_save_stack)/sizeof(g_save_stack[0]))) {
        ctx->current_color = g_save_stack[g_save_top].color;
        g_nvg_color        = g_save_stack[g_save_top].nvg_color;
        ctx->line_width    = g_save_stack[g_save_top].line_width;
        ctx->current_font  = g_save_stack[g_save_top].font;
    }
}

static void egl_set_color(ISWRenderContext *ctx, Pixel pixel)
{
    ctx->current_color = pixel;
    NVGcolor c = egl_pixel_to_nvg(ctx->surface, pixel);
    g_nvg_color = c;
    if (!VG) return;
    nvgFillColor(VG, c);
    nvgStrokeColor(VG, c);
}

static void egl_set_color_rgba(ISWRenderContext *ctx,
                               double r, double g, double b, double a)
{
    (void) ctx;
    NVGcolor c = nvgRGBAf((float) r, (float) g, (float) b, (float) a);
    g_nvg_color = c;
    if (!VG) return;
    nvgFillColor(VG, c);
    nvgStrokeColor(VG, c);
}

static void egl_set_line_width(ISWRenderContext *ctx, double width)
{
    ctx->line_width = width;
    if (VG) nvgStrokeWidth(VG, (float) width);
}

static void egl_fill_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    (void) ctx;
    if (!VG) return;
    nvgBeginPath(VG);
    nvgRect(VG, (float) x, (float) y, (float) w, (float) h);
    nvgFill(VG);
}

static void egl_stroke_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    (void) ctx;
    if (!VG) return;
    nvgBeginPath(VG);
    nvgRect(VG, (float) x + 0.5f, (float) y + 0.5f,
            (float) w - 1.0f, (float) h - 1.0f);
    nvgStroke(VG);
}

static void egl_fill_polygon(ISWRenderContext *ctx, IswPoint *pts, int num)
{
    (void) ctx;
    if (!VG || num < 2) return;
    nvgBeginPath(VG);
    nvgMoveTo(VG, (float) pts[0].x, (float) pts[0].y);
    for (int i = 1; i < num; i++)
        nvgLineTo(VG, (float) pts[i].x, (float) pts[i].y);
    nvgClosePath(VG);
    nvgFill(VG);
}

static void egl_stroke_polygon(ISWRenderContext *ctx, IswPoint *pts, int num)
{
    (void) ctx;
    if (!VG || num < 2) return;
    nvgBeginPath(VG);
    nvgMoveTo(VG, (float) pts[0].x + 0.5f, (float) pts[0].y + 0.5f);
    for (int i = 1; i < num; i++)
        nvgLineTo(VG, (float) pts[i].x + 0.5f, (float) pts[i].y + 0.5f);
    nvgStroke(VG);
}

static void egl_draw_line(ISWRenderContext *ctx, int x1, int y1, int x2, int y2)
{
    (void) ctx;
    if (!VG) return;
    nvgBeginPath(VG);
    nvgMoveTo(VG, (float) x1 + 0.5f, (float) y1 + 0.5f);
    nvgLineTo(VG, (float) x2 + 0.5f, (float) y2 + 0.5f);
    nvgStroke(VG);
}

static void egl_draw_arc(ISWRenderContext *ctx, int x, int y, int w, int h,
                         double angle1, double angle2)
{
    (void) ctx;
    if (!VG) return;
    float cx = (float) x + (float) w / 2.0f;
    float cy = (float) y + (float) h / 2.0f;
    float r  = (float) ((w < h ? w : h) / 2.0);
    /* ISW arc angles are CCW-from-east; NanoVG y-down arc sweeps CW for
       positive deltas, so negate to match. */
    nvgBeginPath(VG);
    nvgArc(VG, cx, cy, r, (float) -angle1, (float) -(angle1 + angle2),
           NVG_CCW);
    nvgStroke(VG);
}

/* Build a rounded-rect sub-path (radius clamped). */
static void egl_rounded_path(int x, int y, int w, int h, double radius)
{
    float r = (float) radius;
    if (r > w / 2.0f) r = w / 2.0f;
    if (r > h / 2.0f) r = h / 2.0f;
    nvgBeginPath(VG);
    nvgRoundedRect(VG, (float) x, (float) y, (float) w, (float) h, r);
}

static void egl_fill_rounded_rect(ISWRenderContext *ctx,
                                  int x, int y, int w, int h, double radius)
{
    (void) ctx;
    if (!VG) return;
    egl_rounded_path(x, y, w, h, radius);
    nvgFill(VG);
}

static void egl_stroke_rounded_rect(ISWRenderContext *ctx,
                                    int x, int y, int w, int h, double radius,
                                    double stroke_width)
{
    if (!VG) return;
    nvgStrokeWidth(VG, (float) stroke_width);
    egl_rounded_path(x, y, w, h, radius);
    nvgStroke(VG);
    nvgStrokeWidth(VG, (float) ctx->line_width);
}

static void egl_fill_stroke_rounded_rect(ISWRenderContext *ctx,
                                         int x, int y, int w, int h,
                                         double radius, double fill_alpha,
                                         double stroke_width)
{
    if (!VG) return;
    NVGcolor c = g_nvg_color;
    egl_rounded_path(x, y, w, h, radius);
    NVGcolor fc = c; fc.a = (float) fill_alpha;
    nvgFillColor(VG, fc);
    nvgFill(VG);
    nvgStrokeColor(VG, c);
    nvgStrokeWidth(VG, (float) stroke_width);
    egl_rounded_path(x, y, w, h, radius);
    nvgStroke(VG);
    nvgStrokeWidth(VG, (float) ctx->line_width);
    nvgFillColor(VG, c);
}

/* ---- Text ---- */

/* Configure NanoVG's font/size from the context's current font. */
static void egl_apply_font(ISWRenderContext *ctx)
{
    if (!VG) return;
    IswFontStruct *font = ctx->current_font;
    const char *family = (font && font->font_family) ? font->font_family : "Sans";
    int weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
    int slant  = font ? font->font_slant  : FC_SLANT_ROMAN;
    int id = egl_resolve_font(family, weight, slant);
    if (id >= 0)
        nvgFontFaceId(VG, id);
    double pt = (font && font->pt_size > 0) ? font->pt_size : 11.0;
    nvgFontSize(VG, (float) (pt * 96.0 / 72.0));
}

static void egl_draw_string(ISWRenderContext *ctx, const char *text, int len,
                            int x, int y)
{
    if (!VG || text == NULL) return;
    egl_apply_font(ctx);
    nvgTextAlign(VG, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgFillColor(VG, g_nvg_color);
    nvgText(VG, (float) x, (float) y, text,
            len >= 0 ? text + len : NULL);
}

static int egl_text_width(ISWRenderContext *ctx, const char *text, int len)
{
    if (!VG || text == NULL) return 0;
    egl_apply_font(ctx);
    float bounds[4];
    float adv = nvgTextBounds(VG, 0, 0, text,
                              len >= 0 ? text + len : NULL, bounds);
    return (int) (adv + 0.5f);
}

static int egl_text_height(ISWRenderContext *ctx)
{
    if (!VG) return 0;
    egl_apply_font(ctx);
    float asc, desc, lineh;
    nvgTextMetrics(VG, &asc, &desc, &lineh);
    return (int) (lineh + 0.5f);
}

static void egl_set_font(ISWRenderContext *ctx, IswFontStruct *font)
{
    ctx->current_font = font;
}

/* ---- Clipping ---- */

static void egl_set_clip_rectangle(ISWRenderContext *ctx,
                                   int x, int y, int w, int h)
{
    (void) ctx;
    if (VG) nvgScissor(VG, (float) x, (float) y, (float) w, (float) h);
}

static void egl_clear_clip(ISWRenderContext *ctx)
{
    (void) ctx;
    if (VG) nvgResetScissor(VG);
}

/* ---- copy_area (scroll blit within a surface) ---- */

static void egl_copy_area(ISWRenderContext *ctx,
                          int src_x, int src_y, int dst_x, int dst_y,
                          unsigned int width, unsigned int height)
{
    IswSurface s = ctx->surface;
    if (!VG || !s || s->tex == 0) return;

    /* Snapshot the current FBO into a temporary texture, then re-draw the
       requested region at the destination.  (GLES2 has no glBlitFramebuffer.) */
    double sf = s->scale > 0 ? s->scale : 1.0;
    int img = nvglCreateImageFromHandleGLES2(g_egl.vg, s->tex,
                                             s->back_w, s->back_h,
                                             NVG_IMAGE_FLIPY | NVG_IMAGE_NODELETE);
    float fw = (float) (s->back_w / sf);
    float fh = (float) (s->back_h / sf);
    nvgSave(VG);
    nvgScissor(VG, (float) dst_x, (float) dst_y, (float) width, (float) height);
    NVGpaint p = nvgImagePattern(VG,
                                 (float) (dst_x - src_x), (float) (dst_y - src_y),
                                 fw, fh, 0.0f, img, 1.0f);
    nvgBeginPath(VG);
    nvgRect(VG, (float) dst_x, (float) dst_y, (float) width, (float) height);
    nvgFillPaint(VG, p);
    nvgFill(VG);
    nvgRestore(VG);
    nvgDeleteImage(VG, img);
}

/* ---- RGBA images ---- */

static void egl_draw_image_rgba(ISWRenderContext *ctx,
                                const unsigned char *rgba,
                                unsigned int img_w, unsigned int img_h,
                                int dst_x, int dst_y,
                                unsigned int dst_w, unsigned int dst_h)
{
    (void) ctx;
    if (!VG || rgba == NULL || img_w == 0 || img_h == 0) return;
    int img = nvgCreateImageRGBA(VG, (int) img_w, (int) img_h, 0, rgba);
    if (img == 0) return;
    NVGpaint p = nvgImagePattern(VG, (float) dst_x, (float) dst_y,
                                 (float) dst_w, (float) dst_h, 0.0f, img, 1.0f);
    nvgBeginPath(VG);
    nvgRect(VG, (float) dst_x, (float) dst_y, (float) dst_w, (float) dst_h);
    nvgFillPaint(VG, p);
    nvgFill(VG);
    /* Delete only after the frame flushes — the draw above is batched. */
    egl_defer_delete_image(img);
}

static void egl_draw_image_masked(ISWRenderContext *ctx, Pixel foreground,
                                  const unsigned char *rgba,
                                  unsigned int img_w, unsigned int img_h,
                                  int dst_x, int dst_y,
                                  unsigned int dst_w, unsigned int dst_h)
{
    if (!VG || rgba == NULL || img_w == 0 || img_h == 0) return;

    /* Premultiply the source alpha into the foreground colour so the RGB of the
       source image is ignored and only its alpha shapes the painted glyph. */
    unsigned int n = img_w * img_h;
    unsigned char *buf = (unsigned char *) malloc((size_t) n * 4);
    if (!buf) return;
    unsigned char fr = (foreground >> 16) & 0xff;
    unsigned char fg = (foreground >>  8) & 0xff;
    unsigned char fb = (foreground      ) & 0xff;
    for (unsigned int i = 0; i < n; i++) {
        unsigned char a = rgba[i * 4 + 3];
        buf[i * 4 + 0] = fr;
        buf[i * 4 + 1] = fg;
        buf[i * 4 + 2] = fb;
        buf[i * 4 + 3] = a;
    }
    int img = nvgCreateImageRGBA(VG, (int) img_w, (int) img_h, 0, buf);
    free(buf);
    if (img == 0) return;
    NVGpaint p = nvgImagePattern(VG, (float) dst_x, (float) dst_y,
                                 (float) dst_w, (float) dst_h, 0.0f, img, 1.0f);
    nvgBeginPath(VG);
    nvgRect(VG, (float) dst_x, (float) dst_y, (float) dst_w, (float) dst_h);
    nvgFillPaint(VG, p);
    nvgFill(VG);
    egl_defer_delete_image(img);   /* delete after frame flush (batched draw) */
    (void) ctx;
}

/* ---- Gradients ---- */

static Boolean egl_set_gradient(ISWRenderContext *ctx,
                                double x1, double y1, double x2, double y2,
                                Pixel c1, Pixel c2)
{
    if (!VG) return False;
    NVGpaint p = nvgLinearGradient(VG, (float) x1, (float) y1,
                                   (float) x2, (float) y2,
                                   egl_pixel_to_nvg(ctx->surface, c1),
                                   egl_pixel_to_nvg(ctx->surface, c2));
    nvgFillPaint(VG, p);
    return True;
}

/* ---- Groups (insensitive/greyed compositing) ---- */

static void egl_push_group(ISWRenderContext *ctx)
{
    /* NanoVG has no offscreen group; approximate with a global-alpha save.
       pop_group_alpha sets the alpha at composite time. */
    (void) ctx;
    if (VG) nvgSave(VG);
}

static void egl_pop_group_alpha(ISWRenderContext *ctx, double alpha)
{
    (void) ctx; (void) alpha;
    if (VG) nvgRestore(VG);
    /* Note: true group-then-fade is not modelled here; see backend README. */
}

/* ---- Path construction ---- */

static void egl_path_begin(ISWRenderContext *ctx)
{ (void) ctx; g_rect_seen = False; if (VG) nvgBeginPath(VG); }

static void egl_path_new_sub_path(ISWRenderContext *ctx)
{ (void) ctx; /* NanoVG starts a new sub-path on the next moveTo */ }

static void egl_path_move_to(ISWRenderContext *ctx, double x, double y)
{ (void) ctx; g_cur_x = x; g_cur_y = y; if (VG) nvgMoveTo(VG, (float) x, (float) y); }

static void egl_path_line_to(ISWRenderContext *ctx, double x, double y)
{ (void) ctx; if (VG) nvgLineTo(VG, (float) x, (float) y); }

static void egl_path_arc(ISWRenderContext *ctx, double cx, double cy,
                         double r, double angle1, double angle2)
{
    (void) ctx;
    if (!VG) return;
    int dir = angle2 >= angle1 ? NVG_CW : NVG_CCW;
    nvgArc(VG, (float) cx, (float) cy, (float) r,
           (float) angle1, (float) angle2, dir);
}

static void egl_path_rectangle(ISWRenderContext *ctx,
                               double x, double y, double w, double h)
{
    (void) ctx;
    if (!g_rect_seen) {
        g_rect_minx = x; g_rect_miny = y; g_rect_maxx = x + w; g_rect_maxy = y + h;
        g_rect_seen = True;
    } else {
        if (x < g_rect_minx) g_rect_minx = x;
        if (y < g_rect_miny) g_rect_miny = y;
        if (x + w > g_rect_maxx) g_rect_maxx = x + w;
        if (y + h > g_rect_maxy) g_rect_maxy = y + h;
    }
    if (VG) nvgRect(VG, (float) x, (float) y, (float) w, (float) h);
}

static void egl_path_close(ISWRenderContext *ctx)
{ (void) ctx; if (VG) nvgClosePath(VG); }

static void egl_fill_path(ISWRenderContext *ctx, Boolean preserve)
{ (void) ctx; (void) preserve; if (VG) nvgFill(VG); }

static void egl_stroke_path(ISWRenderContext *ctx, Boolean preserve)
{ (void) ctx; (void) preserve; if (VG) nvgStroke(VG); }

static void egl_clip_path(ISWRenderContext *ctx)
{
    /* NanoVG clips only to rectangular scissors.  Apply the bounding rectangle
       of the rectangles added to the current path as a real scissor (intersected
       with any existing one), so a rectangular path-clip actually confines later
       draws — e.g. ProgressBar clipping its split-colour value text to the
       filled / unfilled halves.  Non-rectangular path clips degrade to this
       bounding scissor (documented limitation). */
    (void) ctx;
    if (VG && g_rect_seen)
        nvgIntersectScissor(VG, (float) g_rect_minx, (float) g_rect_miny,
                            (float) (g_rect_maxx - g_rect_minx),
                            (float) (g_rect_maxy - g_rect_miny));
}

static void egl_paint(ISWRenderContext *ctx)
{
    /* Paint the current colour over the current scissor/clip region. */
    if (!VG || !ctx->surface) return;
    IswSurface s = ctx->surface;
    nvgBeginPath(VG);
    nvgRect(VG, 0, 0,
            (float) (s->back_w / (s->scale > 0 ? s->scale : 1.0)),
            (float) (s->back_h / (s->scale > 0 ? s->scale : 1.0)));
    nvgFillColor(VG, g_nvg_color);
    nvgFill(VG);
}

static void egl_set_fill_rule(ISWRenderContext *ctx, ISWFillRule rule)
{
    (void) ctx;
    if (VG)
        nvgPathWinding(VG, rule == ISW_FILL_RULE_EVEN_ODD ? NVG_HOLE : NVG_SOLID);
}

static void egl_set_dash(ISWRenderContext *ctx, const double *dashes,
                         int num_dashes, double offset)
{
    /* NanoVG has no dashed strokes; no-op (documented gap vs Cairo backend). */
    (void) ctx; (void) dashes; (void) num_dashes; (void) offset;
}

static void egl_set_operator(ISWRenderContext *ctx, ISWOperator op)
{
    (void) ctx;
    if (!VG) return;
    if (op == ISW_OPERATOR_DIFFERENCE)
        nvgGlobalCompositeBlendFunc(VG, NVG_ONE_MINUS_DST_COLOR, NVG_ZERO);
    else
        nvgGlobalCompositeOperation(VG, NVG_SOURCE_OVER);
}

static void egl_translate(ISWRenderContext *ctx, double tx, double ty)
{ (void) ctx; if (VG) nvgTranslate(VG, (float) tx, (float) ty); }

static void egl_scale(ISWRenderContext *ctx, double sx, double sy)
{ (void) ctx; if (VG) nvgScale(VG, (float) sx, (float) sy); }

static void egl_rotate(ISWRenderContext *ctx, double radians)
{ (void) ctx; if (VG) nvgRotate(VG, (float) radians); }

static void egl_show_text(ISWRenderContext *ctx, const char *text)
{
    if (!VG || text == NULL) return;
    egl_apply_font(ctx);
    nvgTextAlign(VG, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgFillColor(VG, g_nvg_color);
    /* Draw at the path's current point (set by ISWRenderPathMoveTo); honours the
       active transform.  ProgressBar positions its centred value text this way. */
    nvgText(VG, (float) g_cur_x, (float) g_cur_y, text, NULL);
}

static void egl_pixel_to_rgb(ISWRenderContext *ctx, Pixel pixel,
                             double *r, double *g, double *b)
{
    (void) ctx;
    if (r) *r = ((pixel >> 16) & 0xff) / 255.0;
    if (g) *g = ((pixel >>  8) & 0xff) / 255.0;
    if (b) *b = ((pixel      ) & 0xff) / 255.0;
}

#undef VG

/* ===================================================================
 * Font measurement (platform render ops; widget-keyed, backend-global)
 * =================================================================== */

static void egl_measure_font(IswFontStruct *font)
{
    if (!g_egl.vg) return;
    const char *family = (font && font->font_family) ? font->font_family : "Sans";
    int weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
    int slant  = font ? font->font_slant  : FC_SLANT_ROMAN;
    int id = egl_resolve_font(family, weight, slant);
    if (id >= 0)
        nvgFontFaceId(g_egl.vg, id);
    double pt = (font && font->pt_size > 0) ? font->pt_size : 11.0;
    nvgFontSize(g_egl.vg, (float) (pt * 96.0 / 72.0));
}

int egl_scaled_text_width(Widget widget, IswFontStruct *font,
                          const char *text, int len)
{
    (void) widget;
    if (!g_egl.vg || text == NULL) return 0;
    egl_measure_font(font);
    float bounds[4];
    float adv = nvgTextBounds(g_egl.vg, 0, 0, text,
                              len >= 0 ? text + len : NULL, bounds);
    return (int) (adv + 0.5f);
}

int egl_scaled_font_height(Widget widget, IswFontStruct *font)
{
    (void) widget;
    if (!g_egl.vg) return 0;
    egl_measure_font(font);
    float asc, desc, lineh;
    nvgTextMetrics(g_egl.vg, &asc, &desc, &lineh);
    return (int) (lineh + 0.5f);
}

int egl_scaled_font_ascent(Widget widget, IswFontStruct *font)
{
    (void) widget;
    if (!g_egl.vg) return 0;
    egl_measure_font(font);
    float asc, desc, lineh;
    nvgTextMetrics(g_egl.vg, &asc, &desc, &lineh);
    return (int) (asc + 0.5f);
}

int egl_scaled_font_cap_height(Widget widget, IswFontStruct *font)
{
    (void) widget;
    if (!g_egl.vg) return 0;
    egl_measure_font(font);
    float bounds[4];
    isw_nvgTextGlyphBounds(g_egl.vg, 0, 0, "X", NULL, bounds);
    return (int) (-bounds[1] + 0.5f);
}

/* ===================================================================
 * Availability + vtables
 * =================================================================== */

Boolean
ISWRenderGLAvailable(void)
{
    /* Probe once; egl_shared_init needs a display, so defer the real probe to
       the first surface create.  Here, just confirm EGL links and a default
       display can be opened. */
    EGLDisplay d = eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY);
    if (d == EGL_NO_DISPLAY)
        return False;
    EGLint maj, min;
    if (!eglInitialize(d, &maj, &min))
        return False;
    return True;
}

const ISWRenderOps isw_render_egl_ops = {
    .save                     = egl_save,
    .restore                  = egl_restore,
    .set_color                = egl_set_color,
    .set_color_rgba           = egl_set_color_rgba,
    .set_line_width           = egl_set_line_width,
    .fill_rectangle           = egl_fill_rectangle,
    .stroke_rectangle         = egl_stroke_rectangle,
    .fill_polygon             = egl_fill_polygon,
    .stroke_polygon           = egl_stroke_polygon,
    .draw_line                = egl_draw_line,
    .draw_arc                 = egl_draw_arc,
    .fill_rounded_rect        = egl_fill_rounded_rect,
    .stroke_rounded_rect      = egl_stroke_rounded_rect,
    .fill_stroke_rounded_rect = egl_fill_stroke_rounded_rect,
    .draw_string              = egl_draw_string,
    .text_width               = egl_text_width,
    .text_height              = egl_text_height,
    .set_font                 = egl_set_font,
    .set_clip_rectangle       = egl_set_clip_rectangle,
    .clear_clip               = egl_clear_clip,
    .copy_area                = egl_copy_area,
    .draw_image_rgba          = egl_draw_image_rgba,
    .draw_image_masked        = egl_draw_image_masked,
    .set_gradient             = egl_set_gradient,
    .push_group               = egl_push_group,
    .pop_group_alpha          = egl_pop_group_alpha,
    .path_begin               = egl_path_begin,
    .path_new_sub_path        = egl_path_new_sub_path,
    .path_move_to             = egl_path_move_to,
    .path_line_to             = egl_path_line_to,
    .path_arc                 = egl_path_arc,
    .path_rectangle           = egl_path_rectangle,
    .path_close               = egl_path_close,
    .fill_path                = egl_fill_path,
    .stroke_path              = egl_stroke_path,
    .clip_path                = egl_clip_path,
    .paint                    = egl_paint,
    .set_fill_rule            = egl_set_fill_rule,
    .set_dash                 = egl_set_dash,
    .set_operator             = egl_set_operator,
    .translate                = egl_translate,
    .scale                    = egl_scale,
    .rotate                   = egl_rotate,
    .show_text                = egl_show_text,
    .pixel_to_rgb             = egl_pixel_to_rgb,
};

const IswSurfaceOps isw_surface_egl_ops = {
    .create          = egl_surface_create,
    .destroy         = egl_surface_destroy,
    .begin           = egl_surface_begin,
    .end             = egl_surface_end,
    .composite_onto  = egl_surface_composite_onto,
    .fill_background = egl_surface_fill_background,
    .present_root    = egl_surface_present_root,
};

#endif /* HAVE_EGL */
