/*
 * ISWRenderOps.h - Neutral render dispatcher structures (no xcb/cairo)
 *
 * Copyright (c) 2026 ISW Project
 *
 * The backend-agnostic dispatcher (ISWRender.c) needs the render context, the
 * drawing/surface op vtables, and the platform render-ops struct — but NONE of
 * the native (xcb) or cairo types.  Those vtable members are expressed in
 * neutral terms (IswPoint, void* draw handles), so this header pulls in no
 * xcb/cairo headers and the neutral translation unit stays free of them.
 *
 * The X11/cairo backend includes ISWRenderPrivate.h, which includes THIS header
 * plus the xcb/cairo headers and the backend-private declarations.
 */

#ifndef _ISWRenderOps_h
#define _ISWRenderOps_h

#include <ISW/ISWRender.h>   /* public render API + IswPoint, IswFontStruct, Pixel */

/*
 * =================================================================
 * Surface Backend Operations Vtable
 * =================================================================
 *
 * An IswSurface (declared opaque in ISW/Intrinsic.h) is the per-widget render
 * target — the surface-tree analogue of a window.  Every widget owns one
 * (core.surface); a windowless widget paints into its own surface at local
 * (0,0), and the composite pass folds child surfaces into parent surfaces up to
 * the windowed root, which presents to its window.
 *
 * Keyed on IswSurface handles (and the owning Widget for geometry/background),
 * NOT on a render context, so the composite walk operates on surfaces reached
 * generically via IswSurfaceOf — no Widget->context registry.
 *
 * The concrete struct _IswSurface is defined by the backend.
 */
typedef struct _IswSurfaceOps {
    /* Create the per-widget surface for `widget` (windowed or windowless),
       sized from the widget's current geometry.  Returns NULL on failure or if
       the widget is not yet sized (caller retries at first paint). */
    IswSurface (*create)(Widget widget);
    /* Destroy a surface and free its buffers. */
    void (*destroy)(IswSurface surface);

    /* Begin a frame: ensure the back buffer matches the widget's current size,
       target it, and return the render-layer drawing handle (an opaque void*,
       a cairo_t* in the cairo backend) the ISWRenderOps primitives draw with —
       or NULL if not ready this frame. */
    void *(*begin)(IswSurface surface, Widget widget);
    /* End a frame: flush the back buffer.  For a windowed widget, blit it to
       `window`; for a windowless widget this is just a flush (the composite pass
       presents).  `window` is the widget's own window (windowed) or 0. */
    void (*end)(IswSurface surface, Widget widget, IswWindow window);

    /* Fold src's surface onto dst's surface at logical (x,y) in dst's content
       frame, clipped to dst's bounds.  `dst_widget`/`src_widget` supply
       geometry/border/clip for the fold. */
    void (*composite_onto)(IswSurface dst, Widget dst_widget,
                           IswSurface src, Widget src_widget, int x, int y);
    /* Fill `surface` with `widget`'s background pixel, sizing it to the widget's
       current window size.  Called on a windowed composite root before folding
       children so uncovered gaps show the background, not a bare window. */
    void (*fill_background)(IswSurface surface, Widget widget);
} IswSurfaceOps;

/*
 * =================================================================
 * Backend Drawing Operations Vtable
 * =================================================================
 *
 * Each backend implements this interface.  The neutral dispatcher dispatches to
 * the active backend via these function pointers.  All members are expressed in
 * neutral types: points are IswPoint, the cairo handle is an opaque void*.
 */
typedef struct _ISWRenderContext ISWRenderContextStruct; /* fwd for members */

typedef struct _ISWRenderOps {
    /* State management */
    void (*save)(struct _ISWRenderContext *ctx);
    void (*restore)(struct _ISWRenderContext *ctx);

    /* Color */
    void (*set_color)(struct _ISWRenderContext *ctx, Pixel pixel);
    void (*set_color_rgba)(struct _ISWRenderContext *ctx,
                          double r, double g, double b, double a);
    void (*set_line_width)(struct _ISWRenderContext *ctx, double width);

    /* Drawing primitives */
    void (*fill_rectangle)(struct _ISWRenderContext *ctx,
                          int x, int y, int w, int h);
    void (*stroke_rectangle)(struct _ISWRenderContext *ctx,
                            int x, int y, int w, int h);
    void (*fill_polygon)(struct _ISWRenderContext *ctx,
                        IswPoint *pts, int num);
    void (*stroke_polygon)(struct _ISWRenderContext *ctx,
                          IswPoint *pts, int num);
    void (*draw_line)(struct _ISWRenderContext *ctx,
                     int x1, int y1, int x2, int y2);
    void (*draw_arc)(struct _ISWRenderContext *ctx,
                    int x, int y, int w, int h,
                    double angle1, double angle2);

    /* Rounded rectangles (backend draws the path; neutral layer no longer
       reaches for a cairo context to do it itself). */
    void (*fill_rounded_rect)(struct _ISWRenderContext *ctx,
                              int x, int y, int w, int h, double radius);
    void (*stroke_rounded_rect)(struct _ISWRenderContext *ctx,
                                int x, int y, int w, int h, double radius,
                                double stroke_width);
    void (*fill_stroke_rounded_rect)(struct _ISWRenderContext *ctx,
                                     int x, int y, int w, int h, double radius,
                                     double fill_alpha, double stroke_width);

    /* Text */
    void (*draw_string)(struct _ISWRenderContext *ctx,
                       const char *text, int len, int x, int y);
    int (*text_width)(struct _ISWRenderContext *ctx,
                     const char *text, int len);
    int (*text_height)(struct _ISWRenderContext *ctx);
    void (*set_font)(struct _ISWRenderContext *ctx, IswFontStruct *font);

    /* Clipping */
    void (*set_clip_rectangle)(struct _ISWRenderContext *ctx,
                              int x, int y, int w, int h);
    void (*clear_clip)(struct _ISWRenderContext *ctx);

    /* Pixmap/Bitmap rendering */
    void (*copy_area)(struct _ISWRenderContext *ctx,
                      int src_x, int src_y,
                      int dst_x, int dst_y,
                      unsigned int width, unsigned int height);

    /* RGBA image rendering */
    void (*draw_image_rgba)(struct _ISWRenderContext *ctx,
                            const unsigned char *rgba,
                            unsigned int img_w, unsigned int img_h,
                            int dst_x, int dst_y,
                            unsigned int dst_w, unsigned int dst_h);
    /* Paint an RGBA image's alpha as a mask in the current foreground colour
       (used for recoloured glyph/icon bitmaps). */
    void (*draw_image_masked)(struct _ISWRenderContext *ctx, Pixel foreground,
                              const unsigned char *rgba,
                              unsigned int img_w, unsigned int img_h,
                              int dst_x, int dst_y,
                              unsigned int dst_w, unsigned int dst_h);

    /* Advanced */
    Boolean (*set_gradient)(struct _ISWRenderContext *ctx,
                           double x1, double y1, double x2, double y2,
                           Pixel c1, Pixel c2);
    /* Opaque backend draw handle (cairo_t* in the cairo backend), for callers
       that still need direct access.  void* keeps cairo out of this header. */
    void* (*get_cairo_context)(struct _ISWRenderContext *ctx);

    /* Begin a compositing group: subsequent draws accumulate into an offscreen
       group instead of the surface.  Pair with pop_group_alpha. */
    void (*push_group)(struct _ISWRenderContext *ctx);
    /* End the group started by push_group and paint it onto the surface at the
       given opacity (0..1).  Used for insensitive/greyed-out compositing. */
    void (*pop_group_alpha)(struct _ISWRenderContext *ctx, double alpha);

    /* Path construction (see ISWRenderPath* in ISWRender.h). */
    void (*path_begin)(struct _ISWRenderContext *ctx);
    void (*path_new_sub_path)(struct _ISWRenderContext *ctx);
    void (*path_move_to)(struct _ISWRenderContext *ctx, double x, double y);
    void (*path_line_to)(struct _ISWRenderContext *ctx, double x, double y);
    void (*path_arc)(struct _ISWRenderContext *ctx, double cx, double cy,
                     double r, double angle1, double angle2);
    void (*path_rectangle)(struct _ISWRenderContext *ctx,
                           double x, double y, double w, double h);
    void (*path_close)(struct _ISWRenderContext *ctx);

    /* Paint / clip the current path. */
    void (*fill_path)(struct _ISWRenderContext *ctx, Boolean preserve);
    void (*stroke_path)(struct _ISWRenderContext *ctx, Boolean preserve);
    void (*clip_path)(struct _ISWRenderContext *ctx);
    /* Paint the current colour over the entire current clip region. */
    void (*paint)(struct _ISWRenderContext *ctx);

    /* Path / draw state. */
    void (*set_fill_rule)(struct _ISWRenderContext *ctx, ISWFillRule rule);
    void (*set_dash)(struct _ISWRenderContext *ctx, const double *dashes,
                     int num_dashes, double offset);
    void (*set_operator)(struct _ISWRenderContext *ctx, ISWOperator op);

    /* Affine transform of the coordinate system. */
    void (*translate)(struct _ISWRenderContext *ctx, double tx, double ty);
    void (*scale)(struct _ISWRenderContext *ctx, double sx, double sy);
    void (*rotate)(struct _ISWRenderContext *ctx, double radians);

    /* Draw text at the current point honouring the active transform. */
    void (*show_text)(struct _ISWRenderContext *ctx, const char *text);

    /* Decode a pixel to 0..1 RGB using the backend's own visual/colormap
       (the backend owns these privately; the neutral ctx holds no native
       display handle).  ISWRenderPixelToRGB forwards here. */
    void (*pixel_to_rgb)(struct _ISWRenderContext *ctx, Pixel pixel,
                         double *r, double *g, double *b);

} ISWRenderOps;

/*
 * =================================================================
 * Base Rendering Context
 * =================================================================
 *
 * Shared by all backends.  Carries NO native (xcb) display handle: the backend
 * owns the connection/window/screen/visual privately (in its IswSurface) and
 * resolves them from the display ops.  The neutral dispatcher reaches platform
 * capabilities only through ops.
 */
typedef struct _ISWRenderContext {
    Widget widget;

    /* Backend information */
    ISWRenderBackend backend;
    ISWRenderCaps capabilities;
    const ISWRenderOps *ops;            /* stateless drawing primitives */
    const IswSurfaceOps *surface_ops;   /* surface lifecycle + compositing */

    /* The surface this context draws into — the widget's own (core.surface).
       Cached here so the drawing primitives reach it without a widget lookup.
       It also holds the active draw target (set by surface_ops->begin), which
       the ISWRenderOps primitives use. */
    IswSurface surface;

    /* Current state */
    Pixel current_color;
    double line_width;
    IswFontStruct *current_font;

    /* Windowless rendering: drawing origin within the target window.
       A windowless widget shares its windowed ancestor's window; this is
       the widget's position relative to that window, applied as a
       coordinate translation at frame Begin so the widget draws in its own
       local (0,0)-based coordinates.  Zero for windowed widgets. */
    int origin_x, origin_y;

    /* Composite root that the composite pass created itself, because the
       windowed root has no expose proc that paints its own content (bare
       Box/Form/Shell).  Such a root's window must be background-filled before
       folding children.  A root owned by a widget that paints its own content
       (SimpleMenu, IconView, ...) must NOT be filled — that would wipe it. */
    Boolean lazy_composite_root;

    /* Set once this root has presented a pass that actually folded child
       content.  Until then, a pass that would fold nothing is skipped entirely
       (no fill, no blit): a lazy root with no mapped children yet has nothing to
       show, and re-filling+blitting the full-window background is pure overhead.
       Once content has appeared, later empty passes still present so that
       un-mapping the last child correctly clears it. */
    Boolean presented_content;

    /* Composite re-expose gate.  A composite container's surface persists
       between composite passes and accumulates its folded children.  Set
       whenever the container (or something under it) actually changed since the
       last fold; the fold re-runs the container's expose proc ONLY when set,
       then clears it.  Starts True so the first composite always paints. */
    Boolean composite_dirty;

} ISWRenderContext;

/*
 * =================================================================
 * Platform render ops (member of IswPlatformOps.render)
 * =================================================================
 *
 * The render system's slot in the platform ops table.  The platform header
 * (ISW/ISWPlatform.h) forward-declares this struct opaquely and holds it by
 * pointer in IswPlatformOps so it pulls in no cairo/xcb render types; the
 * concrete layout is defined here, in render-internal (but still neutral) scope.
 *
 * Carries the per-backend drawing + surface sub-vtables, the backend-detection
 * hooks the dispatcher uses to pick a backend, and the widget-keyed font
 * measurement entry points.  ISWRender.c reaches these through the active
 * platform ops (IswDisplay -> ops -> render).
 */
struct _IswPlatformRenderOps {
    /* True if `backend` can be used on this platform. */
    Boolean (*available)(ISWRenderBackend backend);
    /* Resolve a (possibly AUTO) preference to a concrete usable backend. */
    ISWRenderBackend (*detect)(ISWRenderBackend preferred);
    /* Drawing + surface sub-vtables for a concrete backend (NULL if that
       backend is unavailable in this build). */
    const ISWRenderOps  *(*draw_ops)(ISWRenderBackend backend);
    const IswSurfaceOps *(*surface_ops)(ISWRenderBackend backend);

    /* Widget-keyed text/font measurement.  Backend-global (a process-wide
       measurement context), not per-surface, so they hang off the platform
       render ops rather than a draw context.  The neutral ISWScaled* wrappers
       reach them via _IswPlatformRenderOpsActive(). */
    int (*scaled_text_width)(Widget widget, IswFontStruct *font,
                             const char *text, int len);
    int (*scaled_font_height)(Widget widget, IswFontStruct *font);
    int (*scaled_font_ascent)(Widget widget, IswFontStruct *font);
    int (*scaled_font_cap_height)(Widget widget, IswFontStruct *font);
};

/* ISWRenderBackendAvailable - Check if a backend is available (neutral). */
Boolean ISWRenderBackendAvailable(ISWRenderBackend backend);

#endif /* _ISWRenderOps_h */
