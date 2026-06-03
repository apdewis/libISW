/*
 * ISWRender.c - Core rendering abstraction implementation
 *
 * Copyright (c) 2026 ISW Project
 *
 * This file implements the backend-agnostic rendering API.
 * Handles backend detection, context lifecycle, and dispatching
 * to backend-specific implementations.
 *
 * CRITICAL: All backends use pure XCB - NO XLIB DEPENDENCIES.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ISWRenderPrivate.h"
#include "ISWXcbDraw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <cairo/cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* Defined in Initialize.c — avoids pulling in InitialI.h */
extern double _IswGetScaleFactor(xcb_connection_t *dpy);

/* Widget-tree access for the surface-tree composite pass. */
#include <ISW/IntrinsicP.h>
#include <ISW/CompositeP.h>
#include <ISW/SimpleP.h>

/*
 * =================================================================
 * Backend Availability Checks
 * =================================================================
 */

Boolean
ISWRenderBackendAvailable(ISWRenderBackend backend)
{
    switch (backend) {
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            return True;

#ifdef HAVE_CAIRO_EGL
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            return ISWRenderEGLAvailable();
#endif

        case ISW_RENDER_BACKEND_AUTO:
        default:
            return False;
    }

    return False;
}

/*
 * =================================================================
 * Backend Detection and Selection
 * =================================================================
 */

static ISWRenderBackend
ISWRenderDetectBackend(ISWRenderBackend preferred)
{
    /* Honor explicit request if available */
    if (preferred != ISW_RENDER_BACKEND_AUTO) {
        if (ISWRenderBackendAvailable(preferred)) {
            return preferred;
        }
        /* Fall through to auto-detection if unavailable */
    }

    /* Check environment variable */
    const char *env = getenv("ISW_RENDER_BACKEND");
    if (env) {
        if (strcmp(env, "cairo-egl") == 0) {
#ifdef HAVE_CAIRO_EGL
            if (ISWRenderBackendAvailable(ISW_RENDER_BACKEND_CAIRO_EGL)) {
                return ISW_RENDER_BACKEND_CAIRO_EGL;
            }
#endif
        } else if (strcmp(env, "cairo") == 0) {
            return ISW_RENDER_BACKEND_CAIRO_XCB;
        }
    }

    /* Auto-detect: prefer best available */
#ifdef HAVE_CAIRO_EGL
    if (ISWRenderBackendAvailable(ISW_RENDER_BACKEND_CAIRO_EGL)) {
        return ISW_RENDER_BACKEND_CAIRO_EGL;
    }
#endif

    return ISW_RENDER_BACKEND_CAIRO_XCB;
}

/*
 * =================================================================
 * Widget -> context registry
 * =================================================================
 *
 * Surface-per-widget compositing needs to find any widget's render context
 * (hence its surface) generically — including a parent's, during the composite
 * pass — without knowing the widget-specific instance-field offset where the
 * widget caches its context.  Render contexts are created/destroyed through the
 * two choke points below, so a simple Widget->context map kept in sync there
 * gives the composite walk what it needs.
 *
 * A widget may legitimately have several contexts over its life (create/destroy
 * cycles) but at most one live at a time; the map holds the most recent live
 * one.  Lookups tolerate absence (widget never painted -> nothing to compose).
 */
typedef struct _CtxMapEntry {
    Widget widget;
    ISWRenderContext *ctx;
    /* Composite clip persisted per-widget (survives context create/destroy), so
       a container can set it before the child's context exists. */
    Boolean has_clip;
    int clip_x, clip_y, clip_w, clip_h;
    struct _CtxMapEntry *next;
} CtxMapEntry;

static CtxMapEntry *ctx_map_head = NULL;

void _ISWRenderForgetDirtyRoot(Widget w);

/* True while ISWRenderCompositeSubtree is running, so the auto-composite in
   ISWRenderEnd does not re-enter the pass for each child's end(). */
static Boolean _isw_in_composite = False;

/* Composite coalescing: during an event dispatch, many widgets may repaint
   themselves (e.g. several buttons un/highlighting as the pointer crosses
   them).  Each repaint's ISWRenderEnd would otherwise re-fold the ENTIRE tree
   onto the windowed root and re-blit the whole window — once per widget.
   Instead, while deferral is enabled we record each windowed root that needs a
   composite and fold it exactly ONCE when the dispatch finishes
   (ISWRenderFlushComposites). */
#define ISW_MAX_DIRTY_ROOTS 16
static Widget   _isw_dirty_roots[ISW_MAX_DIRTY_ROOTS];
static int      _isw_dirty_count = 0;
static int      _isw_defer_depth = 0;   /* >0 = coalesce instead of compositing */

static void
_isw_mark_dirty_root(Widget root)
{
    int i;
    if (root == NULL)
        return;
    for (i = 0; i < _isw_dirty_count; i++)
        if (_isw_dirty_roots[i] == root)
            return;                      /* already pending */
    if (_isw_dirty_count < ISW_MAX_DIRTY_ROOTS)
        _isw_dirty_roots[_isw_dirty_count++] = root;
    else
        /* Overflow: composite immediately rather than dropping the update. */
        ISWRenderCompositeSubtree(root);
}

void
ISWRenderBeginDeferComposite(void)
{
    _isw_defer_depth++;
}

/* Request a composite of a windowed root: deferred (coalesced) if a dispatch
   is in progress, otherwise immediate.  Callers that previously invoked
   ISWRenderCompositeSubtree directly for a repaint should use this so the work
   collapses into the per-dispatch flush. */
void
ISWRenderRequestComposite(Widget windowed_root)
{
    if (windowed_root == NULL)
        return;
    if (_isw_defer_depth > 0)
        _isw_mark_dirty_root(windowed_root);
    else
        ISWRenderCompositeSubtree(windowed_root);
}

void
ISWRenderFlushComposites(void)
{
    int i, n;
    Widget roots[ISW_MAX_DIRTY_ROOTS];

    if (_isw_defer_depth > 0)
        _isw_defer_depth--;
    if (_isw_defer_depth > 0)
        return;                          /* still nested; flush at outermost */

    /* Snapshot and clear first: compositing can itself trigger paints that
       re-mark roots dirty, and we must not lose or infinitely chase them. */
    n = _isw_dirty_count;
    for (i = 0; i < n; i++)
        roots[i] = _isw_dirty_roots[i];
    _isw_dirty_count = 0;

    for (i = 0; i < n; i++)
        ISWRenderCompositeSubtree(roots[i]);
}

/* If a widget that's pending composite is being destroyed, drop it. */
void
_ISWRenderForgetDirtyRoot(Widget w)
{
    int i;
    for (i = 0; i < _isw_dirty_count; i++) {
        if (_isw_dirty_roots[i] == w) {
            _isw_dirty_roots[i] = _isw_dirty_roots[--_isw_dirty_count];
            return;
        }
    }
}

static void
_ISWRenderRegister(Widget w, ISWRenderContext *ctx)
{
    CtxMapEntry *e;
    for (e = ctx_map_head; e != NULL; e = e->next) {
        if (e->widget == w) {           /* replace stale entry for this widget */
            e->ctx = ctx;
            return;
        }
    }
    e = (CtxMapEntry *) calloc(1, sizeof(*e));
    if (!e) return;
    e->widget = w;
    e->ctx = ctx;
    e->next = ctx_map_head;
    ctx_map_head = e;
}

static void
_ISWRenderUnregister(ISWRenderContext *ctx)
{
    CtxMapEntry **pp = &ctx_map_head;
    while (*pp != NULL) {
        if ((*pp)->ctx == ctx) {
            CtxMapEntry *e = *pp;
            /* Drop any pending coalesced composite for this widget so the
               end-of-dispatch flush doesn't touch a destroyed widget. */
            _ISWRenderForgetDirtyRoot(e->widget);
            if (e->has_clip) {
                /* Keep the entry for its persisted composite clip; just drop
                   the (now-destroyed) context pointer. */
                e->ctx = NULL;
                return;
            }
            *pp = e->next;
            free(e);
            return;
        }
        pp = &(*pp)->next;
    }
}

static CtxMapEntry*
_ISWRenderEntry(Widget w)
{
    CtxMapEntry *e;
    for (e = ctx_map_head; e != NULL; e = e->next)
        if (e->widget == w)
            return e;
    return NULL;
}

static ISWRenderContext*
_ISWRenderLookup(Widget w)
{
    CtxMapEntry *e = _ISWRenderEntry(w);
    return e ? e->ctx : NULL;
}

/*
 * =================================================================
 * Context Lifecycle
 * =================================================================
 */

ISWRenderContext*
ISWRenderCreate(Widget widget, ISWRenderBackend preferred)
{
    ISWRenderContext *ctx;
    static Boolean backend_logged = False;  /* Log backend info once */
    
    if (!widget) {
        fprintf(stderr, "ISWRender: NULL widget passed to ISWRenderCreate\n");
        return NULL;
    }
    
    /* Allocate context */
    ctx = (ISWRenderContext*)calloc(1, sizeof(ISWRenderContext));
    if (!ctx) {
        fprintf(stderr, "ISWRender: Failed to allocate context\n");
        return NULL;
    }
    
    /* Get widget display and window info */
    ctx->widget = widget;
    ctx->connection = (xcb_connection_t*)IswDisplay(widget);
    ctx->window = IswWindow(widget);
    ctx->screen = (xcb_screen_t*)IswScreen(widget);
    
    /* Get colormap from screen - we'll use the screen's default colormap */
    ctx->colormap = ctx->screen ? ctx->screen->default_colormap : 0;
    
    /* Detect and select backend */
    ctx->backend = ISWRenderDetectBackend(preferred);
    
    /* Initialize backend */
    switch (ctx->backend) {
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            ctx->ops = &isw_render_cairo_xcb_ops;
            break;

#ifdef HAVE_CAIRO_EGL
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            ctx->ops = &isw_render_cairo_egl_ops;
            break;
#endif

        default:
            fprintf(stderr, "ISWRender: Unknown backend %d\n", ctx->backend);
            free(ctx);
            return NULL;
    }

    /* Call backend init */
    if (!ctx->ops->init(ctx)) {
        fprintf(stderr, "ISWRender: Backend %d init failed\n", ctx->backend);
        free(ctx);
        return NULL;
    }
    
    /* Initialize state */
    ctx->current_color = 0;
    ctx->line_width = 1.0;
    ctx->current_font = NULL;
    
    /* Log backend selection once */
    if (!backend_logged) {
        unsigned int caps = ctx->capabilities;
        fprintf(stderr, "ISWRender: Using '%s' backend", 
               ISWRenderGetBackendName(ctx));
        
        /* Show capabilities */
        if (caps != ISW_RENDER_CAP_BASIC) {
            fprintf(stderr, " (");
            if (caps & ISW_RENDER_CAP_ANTIALIASING) fprintf(stderr, "antialiasing ");
            if (caps & ISW_RENDER_CAP_GRADIENTS) fprintf(stderr, "gradients ");
            if (caps & ISW_RENDER_CAP_ALPHA) fprintf(stderr, "alpha ");
            if (caps & ISW_RENDER_CAP_TRANSFORMS) fprintf(stderr, "transforms ");
            if (caps & ISW_RENDER_CAP_HW_ACCEL) fprintf(stderr, "hw-accel ");
            fprintf(stderr, ")");
        }
        fprintf(stderr, "\n");
        backend_logged = True;
    }

    _ISWRenderRegister(widget, ctx);
    return ctx;
}

void
ISWRenderDestroy(ISWRenderContext *ctx)
{
    if (!ctx) {
        return;
    }

    _ISWRenderUnregister(ctx);

    /* Call backend destroy */
    if (ctx->ops && ctx->ops->destroy) {
        ctx->ops->destroy(ctx);
    }

    /* Free context */
    free(ctx);
}

ISWRenderBackend
ISWRenderGetBackend(ISWRenderContext *ctx)
{
    if (!ctx) {
        return ISW_RENDER_BACKEND_CAIRO_XCB;  /* Safe default */
    }

    return ctx->backend;
}

ISWRenderCaps
ISWRenderGetCapabilities(ISWRenderContext *ctx)
{
    if (!ctx) {
        return ISW_RENDER_CAP_BASIC;  /* Safe default */
    }
    
    return ctx->capabilities;
}

const char*
ISWRenderGetBackendName(ISWRenderContext *ctx)
{
    if (!ctx) {
        return "None";
    }
    
    switch (ctx->backend) {
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            return "Cairo-XCB";
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            return "Cairo-EGL (Hardware Accelerated)";
        case ISW_RENDER_BACKEND_AUTO:
            return "Auto-detect";
        default:
            return "Unknown";
    }
}

void
ISWRenderPrintBackendInfo(void)
{
    ISWRenderBackend backend = ISWRenderDetectBackend(ISW_RENDER_BACKEND_AUTO);
    ISWRenderCaps caps;
    const char *backend_name;
    
    /* Get backend name */
    switch (backend) {
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            backend_name = "Cairo-XCB";
            caps = ISW_RENDER_CAP_BASIC | ISW_RENDER_CAP_ANTIALIASING |
                   ISW_RENDER_CAP_GRADIENTS | ISW_RENDER_CAP_ALPHA |
                   ISW_RENDER_CAP_TRANSFORMS | ISW_RENDER_CAP_TEXT_ADVANCED;
            break;
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            backend_name = "Cairo-EGL (Hardware Accelerated)";
            caps = ISW_RENDER_CAP_BASIC | ISW_RENDER_CAP_ANTIALIASING |
                   ISW_RENDER_CAP_GRADIENTS | ISW_RENDER_CAP_ALPHA |
                   ISW_RENDER_CAP_TRANSFORMS | ISW_RENDER_CAP_TEXT_ADVANCED |
                   ISW_RENDER_CAP_HW_ACCEL;
            break;
        default:
            backend_name = "Unknown";
            caps = ISW_RENDER_CAP_BASIC;
            break;
    }
    
    fprintf(stderr, "ISWRender: Using '%s' backend", backend_name);
    
    /* Show capabilities */
    if (caps != ISW_RENDER_CAP_BASIC) {
        fprintf(stderr, " (");
        if (caps & ISW_RENDER_CAP_ANTIALIASING) fprintf(stderr, "antialiasing ");
        if (caps & ISW_RENDER_CAP_GRADIENTS) fprintf(stderr, "gradients ");
        if (caps & ISW_RENDER_CAP_ALPHA) fprintf(stderr, "alpha ");
        if (caps & ISW_RENDER_CAP_TRANSFORMS) fprintf(stderr, "transforms ");
        if (caps & ISW_RENDER_CAP_HW_ACCEL) fprintf(stderr, "hw-accel ");
        fprintf(stderr, "\b)");  /* Remove trailing space */
    }
    fprintf(stderr, "\n");
}

/*
 * =================================================================
 * State Management
 * =================================================================
 */

/* Compute a windowless widget's content drawing origin: its position relative
   to the nearest windowed ancestor, summed across windowless parents.  Each
   level contributes its position plus its border width, because the widget's
   content area sits inside its border ring (matching X's window+border model).
   Returns (0,0) for windowed widgets. */
static void
_ISWRenderComputeOrigin(Widget w, int *ox, int *oy)
{
    int x = 0, y = 0;

    while (w != NULL && IswIsWidget(w) && w->core.windowless) {
        x += w->core.x + (int) w->core.border_width;
        y += w->core.y + (int) w->core.border_width;
        w = w->core.parent;
    }
    *ox = x;
    *oy = y;
}

void
ISWRenderBegin(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->begin) {
        return;
    }

    /* Refresh the windowless drawing origin each frame: the widget's
       position within its windowed ancestor may have changed since the
       context was created (geometry updates don't recreate the context). */
    if (ctx->widget && IswIsWidget(ctx->widget) && ctx->widget->core.windowless)
        _ISWRenderComputeOrigin(ctx->widget, &ctx->origin_x, &ctx->origin_y);
    else {
        ctx->origin_x = 0;
        ctx->origin_y = 0;
    }

    ctx->ops->begin(ctx);
}

void
ISWRenderEnd(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->end) {
        return;
    }
    
    ctx->ops->end(ctx);

    /* Surface-per-widget: a windowless widget's end() paints into its own
       surface but does not reach the screen.  When this is a top-level frame
       (not nested inside a parent's begin/end) AND not already running inside a
       composite pass, fold this widget's subtree up to its windowed ancestor
       and blit.  This handles self-initiated repaints (button highlight, text
       scroll) that bypass the expose walk. */
    if (!_isw_in_composite && ctx->widget && IswIsWidget(ctx->widget) &&
        ctx->widget->core.windowless) {
        Widget root = ctx->widget;
        while (root != NULL && IswIsWidget(root) && root->core.windowless)
            root = root->core.parent;
        if (root != NULL) {
            /* During an event dispatch, coalesce: record the root and fold it
               once when the dispatch unwinds.  Outside a dispatch (deferral
               disabled), composite immediately as before. */
            if (_isw_defer_depth > 0)
                _isw_mark_dirty_root(root);
            else
                ISWRenderCompositeSubtree(root);
        }
    }
}

/* Same "shown" gate the paint walk and hit-test use. */
static Boolean
_isw_composite_shown(Widget child)
{
    if (!IswIsWidget(child) || !child->core.windowless)
        return False;
    if (!child->core.windowless_realized && !IswIsRealized(child))
        return False;
    if (!child->core.managed && !child->core.mapped_when_managed)
        return False;
    return True;
}

static void _isw_composite_children_into(Widget parent,
                                         ISWRenderContext *parent_ctx);
static void _isw_composite_children_into_at(Widget parent,
                                            ISWRenderContext *dst_ctx,
                                            int ox, int oy);

/* Fold one windowless child (and its descendants) into a destination surface
   (dst_ctx) at accumulated logical offset (ox, oy) within dst's content. */
static void
_isw_composite_one(Widget child, ISWRenderContext *dst_ctx, int ox, int oy)
{
    ISWRenderContext *child_ctx;

    if (!_isw_composite_shown(child))
        return;

    {
        CtxMapEntry *e = _ISWRenderEntry(child);
        child_ctx = e ? e->ctx : NULL;
        if (child_ctx != NULL && e->has_clip) {
            /* Apply the persisted composite clip to the live context. */
            child_ctx->clip_x = e->clip_x;
            child_ctx->clip_y = e->clip_y;
            child_ctx->clip_w = e->clip_w;
            child_ctx->clip_h = e->clip_h;
        } else if (child_ctx != NULL) {
            child_ctx->clip_w = 0;
        }
    }
    if (child_ctx == NULL) {
        /* Context-less windowless container (e.g. a pure layout container with
           no expose proc, so it never created a surface).  It contributes no
           pixels of its own, but its descendants still must be composited —
           fold them directly onto dst, accumulating this child's offset. */
        _isw_composite_children_into_at(child, dst_ctx,
                                        ox + child->core.x + child->core.border_width,
                                        oy + child->core.y + child->core.border_width);
        return;
    }

    /* Fold this child's own descendants into its surface first (bottom-up). */
    _isw_composite_children_into(child, child_ctx);

    /* Then fold the child's surface into dst at the child's position. */
    if (dst_ctx->ops && dst_ctx->ops->composite_onto)
        dst_ctx->ops->composite_onto(dst_ctx, child_ctx,
                                     ox + child->core.x, oy + child->core.y);
}

/* Recursively fold a parent's windowless children into a destination surface
   at logical offset (ox, oy).  Depth-first, in stacking order: composite
   children plus non-composite-tracked sub-widgets via the Simple hook. */
static void
_isw_composite_children_into_at(Widget parent, ISWRenderContext *dst_ctx,
                                int ox, int oy)
{
    if (dst_ctx == NULL)
        return;

    if (IswIsComposite(parent)) {
        CompositeWidget cw = (CompositeWidget) parent;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++)
            _isw_composite_one(cw->composite.children[i], dst_ctx, ox, oy);
    }

    if (IswIsSubclass(parent, simpleWidgetClass)) {
        SimpleWidgetClass sc = (SimpleWidgetClass) parent->core.widget_class;
        if (sc->simple_class.nth_windowless_child != NULL) {
            int i = 0;
            Widget child;
            while ((child = (*sc->simple_class.nth_windowless_child)(parent, i++))
                   != NULL)
                _isw_composite_one(child, dst_ctx, ox, oy);
        }
    }
}

/* Fold a parent's windowless children into the parent's OWN surface. */
static void
_isw_composite_children_into(Widget parent, ISWRenderContext *parent_ctx)
{
    _isw_composite_children_into_at(parent, parent_ctx, 0, 0);
}

void
ISWRenderCompositeSubtree(Widget windowed_root)
{
    ISWRenderContext *root_ctx;
    Boolean prev = _isw_in_composite;

    if (windowed_root == NULL || !IswIsWidget(windowed_root))
        return;

    root_ctx = _ISWRenderLookup(windowed_root);
    if (root_ctx == NULL) {
        /* Bare windowed root (Box/Form/Shell with no expose proc that paints
           its own content).  Create a context so children have a surface to
           composite onto, and mark it so we background-fill it each pass —
           nothing else paints this window's background. */
        root_ctx = ISWRenderCreate(windowed_root, ISW_RENDER_BACKEND_AUTO);
        if (root_ctx == NULL)
            return;
        root_ctx->lazy_composite_root = True;
    }

    _isw_in_composite = True;

    /* Fill the background ONLY for a lazily-created root.  A widget-owned root
       (SimpleMenu, IconView, ...) painted its own content via its expose proc
       this frame; filling would wipe it.  The composite then folds the
       windowless children on top of whatever the root drew. */
    if (root_ctx->lazy_composite_root &&
        root_ctx->ops && root_ctx->ops->fill_background)
        root_ctx->ops->fill_background(root_ctx);
    _isw_composite_children_into(windowed_root, root_ctx);

    /* Blit the fully-composited root surface to its window once. */
    if (root_ctx->ops && root_ctx->ops->present)
        root_ctx->ops->present(root_ctx);
    _isw_in_composite = prev;
}

/* Batch guard: while a caller is driving a paint walk that ends with an
   explicit ISWRenderCompositeSubtree, suppress the per-end() auto-composite so
   the subtree is folded once, not once per child. */
void
ISWRenderBeginCompositeBatch(void)
{
    _isw_in_composite = True;
}

void
ISWRenderEndCompositeBatch(void)
{
    _isw_in_composite = False;
}

void
ISWRenderSetCompositeClip(Widget widget, int x, int y, int w, int h)
{
    /* Store on the persistent registry entry (creating one if the widget has no
       context yet), so the clip survives until the child paints and is applied
       in the composite pass regardless of paint/layout ordering. */
    CtxMapEntry *e = _ISWRenderEntry(widget);
    if (e == NULL) {
        e = (CtxMapEntry *) calloc(1, sizeof(*e));
        if (!e) return;
        e->widget = widget;
        e->next = ctx_map_head;
        ctx_map_head = e;
    }
    e->has_clip = (w > 0 && h > 0);
    e->clip_x = x;
    e->clip_y = y;
    e->clip_w = (w > 0) ? w : 0;
    e->clip_h = (h > 0) ? h : 0;
}

Boolean
ISWRenderGetCompositeClip(Widget widget, int *x, int *y, int *w, int *h)
{
    CtxMapEntry *e = _ISWRenderEntry(widget);
    if (e == NULL || !e->has_clip)
        return False;
    if (x) *x = e->clip_x;
    if (y) *y = e->clip_y;
    if (w) *w = e->clip_w;
    if (h) *h = e->clip_h;
    return True;
}

void
ISWRenderSave(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->save) {
        return;
    }
    
    ctx->ops->save(ctx);
}

void
ISWRenderRestore(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->restore) {
        return;
    }
    
    ctx->ops->restore(ctx);
}

/*
 * =================================================================
 * Color and Line Management
 * =================================================================
 */

void
ISWRenderSetColor(ISWRenderContext *ctx, Pixel pixel)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_color) {
        return;
    }
    
    ctx->ops->set_color(ctx, pixel);
}

void
ISWRenderSetColorRGBA(ISWRenderContext *ctx, double r, double g, double b, double a)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_color_rgba) {
        /* Fallback to solid color if RGBA not supported */
        if (ctx->ops && ctx->ops->set_color) {
            /* Best effort: just use pixel color, ignore alpha */
            return;
        }
        return;
    }
    
    ctx->ops->set_color_rgba(ctx, r, g, b, a);
}

void
ISWRenderSetLineWidth(ISWRenderContext *ctx, double width)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_line_width) {
        return;
    }
    
    ctx->ops->set_line_width(ctx, width);
}

/*
 * =================================================================
 * Shape Drawing Primitives
 * =================================================================
 */

void
ISWRenderStrokeRectangle(ISWRenderContext *ctx, int x, int y, int width, int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_rectangle) {
        return;
    }
    
    ctx->ops->stroke_rectangle(ctx, x, y, width, height);
}

void
ISWRenderFillRectangle(ISWRenderContext *ctx, int x, int y, int width, int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_rectangle) {
        return;
    }
    
    ctx->ops->fill_rectangle(ctx, x, y, width, height);
}

void
ISWRenderFillRoundedRectangle(ISWRenderContext *ctx,
                              int x, int y, int width, int height,
                              double radius)
{
    if (!ctx || !ctx->ops || !ctx->ops->get_cairo_context) {
        /* Fall back to plain rectangle */
        ISWRenderFillRectangle(ctx, x, y, width, height);
        return;
    }

    cairo_t *cr = (cairo_t *)ctx->ops->get_cairo_context(ctx);
    if (!cr) {
        ISWRenderFillRectangle(ctx, x, y, width, height);
        return;
    }

    /* Clamp radius to half the smallest dimension */
    double max_r = (width < height ? width : height) / 2.0;
    if (radius > max_r) radius = max_r;

    double x0 = x, y0 = y, w = width, h = height, r = radius;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x0 + w - r, y0 + r,     r, -M_PI/2, 0);
    cairo_arc(cr, x0 + w - r, y0 + h - r, r, 0,        M_PI/2);
    cairo_arc(cr, x0 + r,     y0 + h - r, r, M_PI/2,   M_PI);
    cairo_arc(cr, x0 + r,     y0 + r,     r, M_PI,      3*M_PI/2);
    cairo_close_path(cr);
    cairo_fill(cr);
}

void
ISWRenderStrokeRoundedRectangle(ISWRenderContext *ctx,
                                int x, int y, int width, int height,
                                double radius,
                                double stroke_width)
{
    if (!ctx || !ctx->ops || !ctx->ops->get_cairo_context) {
        ISWRenderStrokeRectangle(ctx, x, y, width, height);
        return;
    }

    cairo_t *cr = (cairo_t *)ctx->ops->get_cairo_context(ctx);
    if (!cr) {
        ISWRenderStrokeRectangle(ctx, x, y, width, height);
        return;
    }

    double max_r = (width < height ? width : height) / 2.0;
    if (radius > max_r) radius = max_r;

    double x0 = x, y0 = y, w = width, h = height, rad = radius;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x0 + w - rad, y0 + rad,     rad, -M_PI/2, 0);
    cairo_arc(cr, x0 + w - rad, y0 + h - rad, rad, 0,        M_PI/2);
    cairo_arc(cr, x0 + rad,     y0 + h - rad, rad, M_PI/2,   M_PI);
    cairo_arc(cr, x0 + rad,     y0 + rad,     rad, M_PI,      3*M_PI/2);
    cairo_close_path(cr);

    cairo_set_line_width(cr, stroke_width);
    cairo_stroke(cr);
}

void
ISWRenderFillStrokeRoundedRectangle(ISWRenderContext *ctx,
                                    int x, int y, int width, int height,
                                    double radius,
                                    double fill_alpha,
                                    double stroke_width)
{
    if (!ctx || !ctx->ops || !ctx->ops->get_cairo_context) {
        ISWRenderFillRectangle(ctx, x, y, width, height);
        return;
    }

    cairo_t *cr = (cairo_t *)ctx->ops->get_cairo_context(ctx);
    if (!cr) {
        ISWRenderFillRectangle(ctx, x, y, width, height);
        return;
    }

    double max_r = (width < height ? width : height) / 2.0;
    if (radius > max_r) radius = max_r;

    double r, g, b;
    ISWRenderPixelToRGB(ctx, ctx->current_color, &r, &g, &b);

    double x0 = x, y0 = y, w = width, h = height, rad = radius;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x0 + w - rad, y0 + rad,     rad, -M_PI/2, 0);
    cairo_arc(cr, x0 + w - rad, y0 + h - rad, rad, 0,        M_PI/2);
    cairo_arc(cr, x0 + rad,     y0 + h - rad, rad, M_PI/2,   M_PI);
    cairo_arc(cr, x0 + rad,     y0 + rad,     rad, M_PI,      3*M_PI/2);
    cairo_close_path(cr);

    cairo_set_source_rgba(cr, r, g, b, fill_alpha);
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, r, g, b);
    cairo_set_line_width(cr, stroke_width);
    cairo_stroke(cr);
}

void
ISWRenderStrokePolygon(ISWRenderContext *ctx, xcb_point_t *points, int num_points)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_polygon) {
        return;
    }
    
    ctx->ops->stroke_polygon(ctx, points, num_points);
}

void
ISWRenderFillPolygon(ISWRenderContext *ctx, xcb_point_t *points, int num_points)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_polygon) {
        return;
    }
    
    ctx->ops->fill_polygon(ctx, points, num_points);
}

void
ISWRenderDrawLine(ISWRenderContext *ctx, int x1, int y1, int x2, int y2)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_line) {
        return;
    }
    
    ctx->ops->draw_line(ctx, x1, y1, x2, y2);
}

void
ISWRenderDrawArc(ISWRenderContext *ctx, int x, int y, int width, int height,
                double angle1, double angle2)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_arc) {
        return;
    }
    
    ctx->ops->draw_arc(ctx, x, y, width, height, angle1, angle2);
}

/*
 * =================================================================
 * Text Rendering
 * =================================================================
 */

void
ISWRenderDrawString(ISWRenderContext *ctx, const char *text, int length,
                   int x, int y)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_string) {
        return;
    }
    
    ctx->ops->draw_string(ctx, text, length, x, y);
}

int
ISWRenderTextWidth(ISWRenderContext *ctx, const char *text, int length)
{
    if (!ctx || !ctx->ops || !ctx->ops->text_width) {
        /* Rough estimate: 8 pixels per character */
        return length * 8;
    }
    
    return ctx->ops->text_width(ctx, text, length);
}

int
ISWRenderTextHeight(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->text_height) {
        /* Rough estimate */
        return 12;
    }
    
    return ctx->ops->text_height(ctx);
}

void
ISWRenderSetFont(ISWRenderContext *ctx, IswFontStruct *font)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_font) {
        return;
    }
    
    ctx->current_font = font;
    ctx->ops->set_font(ctx, font);
}

/*
 * =================================================================
 * Clipping
 * =================================================================
 */

void
ISWRenderSetClipRectangle(ISWRenderContext *ctx, int x, int y, int width, int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_clip_rectangle) {
        return;
    }

    ctx->ops->set_clip_rectangle(ctx, x, y, width, height);
}

void
ISWRenderClearClip(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->clear_clip) {
        return;
    }
    
    ctx->ops->clear_clip(ctx);
}

/*
 * =================================================================
 * Pixmap/Bitmap Rendering
 * =================================================================
 */

void
ISWRenderCopyArea(ISWRenderContext *ctx,
                  int src_x, int src_y,
                  int dst_x, int dst_y,
                  unsigned int width, unsigned int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->copy_area) {
        return;
    }

    ctx->ops->copy_area(ctx, src_x, src_y, dst_x, dst_y, width, height);
}

void
ISWRenderDrawPixmap(ISWRenderContext *ctx,
                    xcb_pixmap_t pixmap,
                    int src_x, int src_y,
                    int dst_x, int dst_y,
                    unsigned int width, unsigned int height,
                    unsigned int depth)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_pixmap || !pixmap) {
        return;
    }

    ctx->ops->draw_pixmap(ctx, pixmap, src_x, src_y, dst_x, dst_y,
                          width, height, depth);
}

/*
 * =================================================================
 * RGBA Image Rendering
 * =================================================================
 */

void
ISWRenderDrawImageRGBA(ISWRenderContext *ctx,
                       const unsigned char *rgba,
                       unsigned int img_width, unsigned int img_height,
                       int dst_x, int dst_y,
                       unsigned int dst_w, unsigned int dst_h)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_image_rgba || !rgba)
        return;

    ctx->ops->draw_image_rgba(ctx, rgba, img_width, img_height,
                              dst_x, dst_y, dst_w, dst_h);
}

void
ISWRenderDrawImageMasked(ISWRenderContext *ctx, Pixel foreground,
                         const unsigned char *rgba,
                         unsigned int img_w, unsigned int img_h,
                         int dst_x, int dst_y,
                         unsigned int dst_w, unsigned int dst_h)
{
    cairo_t *cr;
    unsigned int stride, i;
    unsigned char *a8_buf;
    cairo_surface_t *mask_surface;

    cr = (cairo_t *)ISWRenderGetCairoContext(ctx);
    if (!cr || !rgba || img_w == 0 || img_h == 0)
        return;

    /* Build an A8 surface from the RGBA alpha channel */
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_A8, (int)img_w);
    a8_buf = (unsigned char *)calloc(stride * img_h, 1);
    if (!a8_buf)
        return;

    for (i = 0; i < img_w * img_h; i++) {
        unsigned int row = i / img_w;
        unsigned int col = i % img_w;
        a8_buf[row * stride + col] = rgba[i * 4 + 3];
    }

    mask_surface = cairo_image_surface_create_for_data(
        a8_buf, CAIRO_FORMAT_A8, (int)img_w, (int)img_h, (int)stride);

    if (cairo_surface_status(mask_surface) == CAIRO_STATUS_SUCCESS) {
        ISWRenderSetColor(ctx, foreground);
        cairo_save(cr);
        if (dst_w != img_w || dst_h != img_h) {
            cairo_translate(cr, dst_x, dst_y);
            cairo_scale(cr,
                        (double)dst_w / (double)img_w,
                        (double)dst_h / (double)img_h);
            cairo_mask_surface(cr, mask_surface, 0, 0);
        } else {
            cairo_mask_surface(cr, mask_surface, dst_x, dst_y);
        }
        cairo_restore(cr);
    }

    cairo_surface_destroy(mask_surface);
    free(a8_buf);
}

/*
 * =================================================================
 * Advanced Features (Cairo-only)
 * =================================================================
 */

Boolean
ISWRenderSetGradient(ISWRenderContext *ctx, double x1, double y1, double x2, double y2,
                    Pixel color1, Pixel color2)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_gradient) {
        return False;
    }
    
    return ctx->ops->set_gradient(ctx, x1, y1, x2, y2, color1, color2);
}

void*
ISWRenderGetCairoContext(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->get_cairo_context) {
        return NULL;
    }
    
    return ctx->ops->get_cairo_context(ctx);
}

/*
 * =================================================================
 * Helper Functions
 * =================================================================
 */

void
ISWRenderPixelToRGB(ISWRenderContext *ctx, Pixel pixel,
                   double *r, double *g, double *b)
{
    IswColor color;
    
    if (!ctx || !r || !g || !b) {
        return;
    }
    
    /* Query color from X server using the context's colormap */
    color.pixel = pixel;
    
    if (ctx->colormap && ISWQueryColor(ctx->connection, ctx->colormap, &color)) {
        /* Convert from 16-bit to 0.0-1.0 range */
        *r = color.red / 65535.0;
        *g = color.green / 65535.0;
        *b = color.blue / 65535.0;
    } else {
        /* Fallback: extract RGB from pixel assuming XCB_VISUAL_CLASS_TRUE_COLOR format */
        *r = ((pixel >> 16) & 0xFF) / 255.0;
        *g = ((pixel >> 8) & 0xFF) / 255.0;
        *b = (pixel & 0xFF) / 255.0;
    }
}

xcb_visualtype_t*
ISWRenderFindVisual(xcb_screen_t *screen, uint8_t depth)
{
    xcb_depth_iterator_t depth_iter;
    xcb_visualtype_iterator_t visual_iter;
    
    if (!screen) {
        return NULL;
    }
    
    /* Iterate through screen depths */
    for (depth_iter = xcb_screen_allowed_depths_iterator(screen);
         depth_iter.rem;
         xcb_depth_next(&depth_iter)) {
        
        if (depth_iter.data->depth == depth) {
            /* Found matching depth, return first visual */
            visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
            if (visual_iter.rem) {
                return visual_iter.data;
            }
        }
    }
    
    return NULL;
}

/*
 * =================================================================
 * HiDPI Scaling
 * =================================================================
 */

double
ISWScaleFactor(Widget widget)
{
    if (!widget)
        return 1.0;
    return _IswGetScaleFactor(IswDisplayOfObject(widget));
}

Dimension
ISWScaleDim(Widget widget, int value)
{
    double scale = ISWScaleFactor(widget);
    if (value == 0)
        return 0;
    int result = (int)(value * scale + 0.5);
    return (Dimension)(result > 0 ? result : 1);
}

Dimension
ISWUnscaleDim(Widget widget, int value)
{
    double scale = ISWScaleFactor(widget);
    if (value == 0)
        return 0;
    int result = (int)(value / scale + 0.5);
    return (Dimension)(result > 0 ? result : 1);
}

Position
ISWScalePos(Widget widget, int value)
{
    double scale = ISWScaleFactor(widget);
    return (Position)lrint((double)value * scale);
}

Position
ISWUnscalePos(Widget widget, int value)
{
    double scale = ISWScaleFactor(widget);
    return (Position)lrint((double)value / scale);
}

#include <cairo.h>
#include <math.h>

/*
 * =================================================================
 * FreeType / Fontconfig Font Resolution
 *
 * Provides TTF/OTF font support via fontconfig (font discovery) +
 * FreeType (font loading) + cairo-ft (Cairo integration).
 * No Xlib dependencies in this path.
 * =================================================================
 */

static FT_Library _ft_library = NULL;

/* Cache for resolved font faces — avoids repeated fontconfig lookups */
typedef struct _ISWFontCacheEntry {
    struct _ISWFontCacheEntry *next;
    char *pattern_key;          /* "family:size:weight:slant" */
    cairo_font_face_t *cr_face;
    FT_Face ft_face;
} _ISWFontCacheEntry;

static _ISWFontCacheEntry *_font_cache = NULL;

static void
_ISWInitFreeType(void)
{
    if (!_ft_library) {
        FT_Init_FreeType(&_ft_library);
    }
}

/*
 * _ISWResolveFontFace - Resolve a font description to a Cairo font face.
 *
 * Uses fontconfig to find a matching font file, FreeType to load it,
 * and cairo-ft to create a Cairo font face. Results are cached.
 *
 * Parameters:
 *   family - font family name (e.g., "Sans", "Monospace", "Serif")
 *   weight - FC_WEIGHT_NORMAL, FC_WEIGHT_BOLD, etc.
 *   slant  - FC_SLANT_ROMAN, FC_SLANT_ITALIC, etc.
 *
 * Returns a cairo_font_face_t* (cached, do NOT destroy).
 */
cairo_font_face_t *
_ISWResolveFontFace(const char *family, int weight, int slant)
{
    char key[256];
    _ISWFontCacheEntry *entry;
    FcPattern *pattern = NULL, *match = NULL;
    FcResult result;
    FcChar8 *font_file = NULL;
    FT_Face ft_face = NULL;

    snprintf(key, sizeof(key), "%s:%d:%d", family ? family : "Sans",
             weight, slant);

    /* Check cache */
    for (entry = _font_cache; entry; entry = entry->next) {
        if (strcmp(entry->pattern_key, key) == 0)
            return entry->cr_face;
    }

    _ISWInitFreeType();

    /* Use fontconfig to find a matching font file.
     * Prefer scalable (outline) fonts — bitmap fonts like "fixed" become
     * fuzzy when scaled to non-native sizes under HiDPI. */
    pattern = FcPatternCreate();
    FcPatternAddString(pattern, FC_FAMILY,
                       (const FcChar8 *)(family ? family : "Sans"));
    FcPatternAddInteger(pattern, FC_WEIGHT, weight);
    FcPatternAddInteger(pattern, FC_SLANT, slant);
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    match = FcFontMatch(NULL, pattern, &result);
    if (!match) {
        FcPatternDestroy(pattern);
        return NULL;
    }

    if (FcPatternGetString(match, FC_FILE, 0, &font_file) != FcResultMatch) {
        FcPatternDestroy(match);
        FcPatternDestroy(pattern);
        return NULL;
    }

    /* Load with FreeType */
    if (FT_New_Face(_ft_library, (const char *)font_file, 0, &ft_face) != 0) {
        FcPatternDestroy(match);
        FcPatternDestroy(pattern);
        return NULL;
    }

    /* Create Cairo font face */
    cairo_font_face_t *cr_face =
        cairo_ft_font_face_create_for_ft_face(ft_face, 0);

    /* Cache it */
    entry = (_ISWFontCacheEntry *)malloc(sizeof(_ISWFontCacheEntry));
    entry->pattern_key = strdup(key);
    entry->cr_face = cr_face;
    entry->ft_face = ft_face;  /* kept alive — Cairo references it */
    entry->next = _font_cache;
    _font_cache = entry;

    FcPatternDestroy(match);
    FcPatternDestroy(pattern);

    return cr_face;
}

/*
 * _ISWSetCairoFontFromXFont - Configure a Cairo context with a proper
 * TTF font face resolved via fontconfig, sized from IswFontStruct metrics.
 */
void
_ISWSetCairoFontFromXFont(cairo_t *cr, IswFontStruct *font, double scale)
{
    cairo_font_face_t *face;
    double size;

    const char *family = (font && font->font_family) ? font->font_family : "Sans";
    int weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
    int slant  = font ? font->font_slant  : FC_SLANT_ROMAN;
    face = _ISWResolveFontFace(family, weight, slant);
    if (face)
        cairo_set_font_face(cr, face);

    if (font && font->pt_size > 0)
        size = font->pt_size * (96.0 / 72.0) * scale;
    else if (font)
        size = (double)(font->ascent + font->descent) * scale;
    else
        size = 12.0 * scale;

    if (size < 1.0)
        size = 12.0 * scale;

    cairo_set_font_size(cr, size);
}

/*
 * Persistent measurement context — avoids creating/destroying a Cairo
 * surface + context on every text measurement or font extents query.
 * Lazily created on first use, lives for the process lifetime.
 */
static cairo_surface_t *_measure_surf = NULL;
static cairo_t *_measure_cr = NULL;

/* Cached font state — re-queried only when font identity or size changes.
 * Font identity is tracked by properties, not pointer, because the resource
 * system can free and reallocate an IswFontStruct at the same address
 * during theme reload. */
static double _cached_font_size = -1.0;
static const char *_cached_font_family = NULL;
static int _cached_font_weight = -1;
static int _cached_font_slant = -1;
static cairo_font_extents_t _cached_font_extents;
static double _measure_device_scale = 0.0;

static cairo_t *
_ISWGetMeasureCR(double device_scale)
{
    if (!_measure_cr) {
        cairo_font_face_t *face;

        _measure_surf = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
        if (device_scale > 1.0)
            cairo_surface_set_device_scale(_measure_surf, device_scale, device_scale);
        _measure_device_scale = device_scale;
        _measure_cr = cairo_create(_measure_surf);

        face = _ISWResolveFontFace("Sans", FC_WEIGHT_NORMAL, FC_SLANT_ROMAN);
        if (face)
            cairo_set_font_face(_measure_cr, face);
        else
            cairo_select_font_face(_measure_cr, "Sans",
                                   CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_NORMAL);
    } else if (device_scale != _measure_device_scale) {
        cairo_surface_set_device_scale(_measure_surf,
            device_scale > 1.0 ? device_scale : 1.0,
            device_scale > 1.0 ? device_scale : 1.0);
        _measure_device_scale = device_scale;
        _cached_font_size = -1.0;  /* force re-query of font extents */
        _cached_font_family = NULL;
    }
    return _measure_cr;
}

static double
_ISWComputeFontSize(Widget widget, IswFontStruct *font)
{
    (void)widget;
    if (font) {
        if (font->pt_size > 0)
            return font->pt_size * (96.0 / 72.0);
        double s = (double)(font->ascent + font->descent);
        return s >= 1.0 ? s : 10.0;
    }
    return 10.0;
}

/*
 * Ensure the measurement context has the correct font face and size.
 * Only re-sets when the font identity or size actually changes.
 */
static int
_ISWMeasureFontChanged(IswFontStruct *font)
{
    const char *family = (font && font->font_family) ? font->font_family : "Sans";
    int weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
    int slant  = font ? font->font_slant  : FC_SLANT_ROMAN;

    if (weight != _cached_font_weight || slant != _cached_font_slant)
        return 1;
    if (!_cached_font_family || strcmp(family, _cached_font_family) != 0)
        return 1;
    return 0;
}

static void
_ISWSyncMeasureFont(cairo_t *cr, Widget widget, IswFontStruct *font)
{
    double size = _ISWComputeFontSize(widget, font);

    if (_ISWMeasureFontChanged(font)) {
        _ISWSetCairoFontFromXFont(cr, font, 1.0);
        _cached_font_family = (font && font->font_family) ? font->font_family : "Sans";
        _cached_font_weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
        _cached_font_slant  = font ? font->font_slant  : FC_SLANT_ROMAN;
        cairo_font_extents(cr, &_cached_font_extents);
        _cached_font_size = size;
    } else if (size != _cached_font_size) {
        cairo_set_font_size(cr, size);
        cairo_font_extents(cr, &_cached_font_extents);
        _cached_font_size = size;
    }
}

/*
 * Get cached Cairo font extents. Only re-queries Cairo when the
 * effective font size changes (different font or different scale).
 */
static void
_ISWGetCairoFontExtents(Widget widget, IswFontStruct *font, cairo_font_extents_t *extents)
{
    cairo_t *cr = _ISWGetMeasureCR(ISWScaleFactor(widget));

    _ISWSyncMeasureFont(cr, widget, font);
    *extents = _cached_font_extents;
}

/*
 * Measure text using the persistent Cairo context with the same font face
 * and size that the render path uses. This ensures layout matches rendering.
 */
int
ISWScaledTextWidth(Widget widget, IswFontStruct *font, const char *text, int len)
{
    cairo_text_extents_t extents;
    char *null_term;
    int width;
    cairo_t *cr;

    if (!text || len <= 0)
        return 0;

    cr = _ISWGetMeasureCR(ISWScaleFactor(widget));
    _ISWSyncMeasureFont(cr, widget, font);

    null_term = (char *)malloc(len + 1);
    if (!null_term)
        return len * 8;
    memcpy(null_term, text, len);
    null_term[len] = '\0';

    cairo_text_extents(cr, null_term, &extents);
    double adv = ceil(extents.x_advance);
    width = (int)adv;

    free(null_term);
    return width;
}

int
ISWScaledFontHeight(Widget widget, IswFontStruct *font)
{
    cairo_font_extents_t extents;
    _ISWGetCairoFontExtents(widget, font, &extents);
    double h = ceil(extents.ascent + extents.descent);
    return (int)h;
}

int
ISWScaledFontAscent(Widget widget, IswFontStruct *font)
{
    cairo_font_extents_t extents;
    _ISWGetCairoFontExtents(widget, font, &extents);
    double a = ceil(extents.ascent);
    return (int)a;
}

int
ISWScaledFontCapHeight(Widget widget, IswFontStruct *font)
{
    cairo_t *cr = _ISWGetMeasureCR(ISWScaleFactor(widget));
    cairo_text_extents_t text_ext;

    _ISWSyncMeasureFont(cr, widget, font);

    cairo_text_extents(cr, "X", &text_ext);
    double cap = ceil(-text_ext.y_bearing);
    return (int)cap;
}

/* Cairo is now a mandatory dependency — no non-Cairo fallback needed */
