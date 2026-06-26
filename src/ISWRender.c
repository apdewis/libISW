/*
 * ISWRender.c - Core rendering abstraction implementation
 *
 * Copyright (c) 2026 ISW Project
 *
 * This file implements the backend-agnostic rendering API.
 * Handles backend detection, context lifecycle, and dispatching
 * to backend-specific implementations.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ISWRenderOps.h"
#include <ISW/ISWPlatform.h>
#include <ISW/ISWFontMetricCache.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Defined in Initialize.c — avoids pulling in InitialI.h */
extern double _IswGetScaleFactor(IswDisplay dpy);

/* Widget-tree access for the surface-tree composite pass. */
#include <ISW/IntrinsicP.h>
#include <ISW/CompositeP.h>
#include <ISW/SimpleP.h>

/*
 * =================================================================
 * Surface tree access
 * =================================================================
 *
 * Surface-per-widget compositing finds any widget's surface generically through
 * core.surface (IswSurfaceOf) — the surface-tree analogue of core.window — so
 * the composite walk operates on surfaces and the per-widget composite state
 * that lives on Core (composite_dirty / composite_clip*), with no separate
 * Widget->context registry.
 */

void _ISWRenderForgetDirtyRoot(Widget w);

/* The active surface backend ops, published at the first ISWRenderCreate and
   used by the context-less composite walk (defined below). */
static const IswSurfaceOps *_isw_surface_ops;

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

/* Public: cancel a pending composite for a root whose window is being unmapped
   (e.g. a shell popdown), so a composite queued earlier in this dispatch does
   not re-present to the now-unmapped window. */
void
ISWRenderForgetRoot(Widget windowed_root)
{
    _ISWRenderForgetDirtyRoot(windowed_root);
}

/* Mark `w` and its windowless ancestors (up to and including the windowed
   root) composite_dirty, so the next fold re-runs their expose proc instead of
   reusing the persisted surface.  Called whenever something changes a widget's
   contribution to its window: a self-initiated paint (ISWRenderEnd) or a
   structural change — un/map, move, resize, destroy — that vacates pixels in
   the ancestor surfaces (ISWRenderRequestComposite, the single funnel for
   those). */
void
_ISWRenderMarkDirtyChain(Widget w)
{
    Widget a = w;
    while (a != NULL && IswIsWidget(a)) {
        a->core.composite_dirty = True;
        a = a->core.parent;
    }
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

    /* Start dirty so the first composite pass paints the container.  The
       per-widget composite state lives on Core (survives context lifecycle). */
    widget->core.composite_dirty = True;

    /* The neutral context holds no native display handle: the backend resolves
       the connection/window/screen/visual it needs from the display ops when it
       creates the widget's surface. */
    ctx->widget = widget;

    /* Detect and select the backend through the platform render ops. */
    {
        const IswPlatformRenderOps *rops = _IswPlatformRenderOpsActive();
        if (!rops) {
            fprintf(stderr, "ISWRender: no render ops in active platform backend\n");
            free(ctx);
            return NULL;
        }
        ctx->backend = rops->detect(preferred);
        ctx->ops = rops->draw_ops(ctx->backend);
        ctx->surface_ops = rops->surface_ops(ctx->backend);
        if (ctx->ops == NULL || ctx->surface_ops == NULL) {
            fprintf(stderr, "ISWRender: Unknown backend %d\n", ctx->backend);
            free(ctx);
            return NULL;
        }
    }

    /* Create the widget's surface (the draw target) and publish it on Core, so
       the composite walk reaches it generically via IswSurfaceOf. */
    ctx->surface = ctx->surface_ops->create(widget);
    if (ctx->surface == NULL) {
        fprintf(stderr, "ISWRender: Backend %d surface create failed\n",
                ctx->backend);
        free(ctx);
        return NULL;
    }
    widget->core.surface = ctx->surface;

    /* Publish the surface backend for the composite walk (which sees only
       widgets + their surfaces, not contexts). */
    _isw_surface_ops = ctx->surface_ops;

    ctx->capabilities = ISW_RENDER_CAP_BASIC |
                        ISW_RENDER_CAP_ANTIALIASING |
                        ISW_RENDER_CAP_GRADIENTS |
                        ISW_RENDER_CAP_ALPHA |
                        ISW_RENDER_CAP_TRANSFORMS |
                        ISW_RENDER_CAP_TEXT_ADVANCED;

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

    return ctx;
}

void
ISWRenderDestroy(ISWRenderContext *ctx)
{
    if (!ctx) {
        return;
    }

    /* Drop any pending coalesced composite for this widget so the
       end-of-dispatch flush doesn't touch a destroyed widget. */
    if (ctx->widget && IswIsWidget(ctx->widget))
        _ISWRenderForgetDirtyRoot(ctx->widget);

    /* Destroy the surface and clear it from Core. */
    if (ctx->surface_ops && ctx->surface_ops->destroy) {
        ctx->surface_ops->destroy(ctx->surface);
    }
    if (ctx->widget && IswIsWidget(ctx->widget) &&
        ctx->widget->core.surface == ctx->surface)
        ctx->widget->core.surface = NULL;

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
        case ISW_RENDER_BACKEND_EGL:
            return "EGL (NanoVG/OpenGL ES)";
        case ISW_RENDER_BACKEND_AUTO:
            return "Auto-detect";
        default:
            return "Unknown";
    }
}

void
ISWRenderPrintBackendInfo(void)
{
    const IswPlatformRenderOps *rops = _IswPlatformRenderOpsActive();
    ISWRenderBackend backend = rops ? rops->detect(ISW_RENDER_BACKEND_AUTO)
                                    : ISW_RENDER_BACKEND_CAIRO_XCB;
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
        case ISW_RENDER_BACKEND_EGL:
            backend_name = "EGL (NanoVG/OpenGL ES)";
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

    while (w != NULL && IswIsWidget(w) && !IswIsShell(w)) {
        IswBorderSides bs = _IswGetBorderSides(w);
        x += w->core.x + bs.left;
        y += w->core.y + bs.top;
        w = w->core.parent;
    }
    *ox = x;
    *oy = y;
}

void
ISWRenderBegin(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->surface_ops || !ctx->surface_ops->begin) {
        return;
    }

    /* Refresh the windowless drawing origin each frame: the widget's
       position within its windowed ancestor may have changed since the
       context was created (geometry updates don't recreate the context). */
    if (ctx->widget && IswIsWidget(ctx->widget) && !IswIsShell(ctx->widget))
        _ISWRenderComputeOrigin(ctx->widget, &ctx->origin_x, &ctx->origin_y);
    else {
        ctx->origin_x = 0;
        ctx->origin_y = 0;
    }

    ctx->surface_ops->begin(ctx->surface, ctx->widget);
}

void
ISWRenderEnd(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->surface_ops || !ctx->surface_ops->end) {
        return;
    }

    ctx->surface_ops->end(ctx->surface, ctx->widget, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(ctx->widget)), (Widget)(ctx->widget)));

    /* This widget painted into its own surface this frame, so the fold must
       re-expose it (if it is a container) and re-fold it onto its ancestors.
       Mark the chain composite_dirty so the next fold does not skip their
       re-expose.  Skip while inside a composite pass: the fold's own internal
       re-exposes go through ISWRenderEnd too, and re-flagging there would keep
       every container perpetually dirty and defeat the gate. */
    if (!_isw_in_composite && ctx->widget && IswIsWidget(ctx->widget))
        _ISWRenderMarkDirtyChain(ctx->widget);

    /* Surface-per-widget: a windowless widget's end() paints into its own
       surface but does not reach the screen.  When this is a top-level frame
       (not nested inside a parent's begin/end) AND not already running inside a
       composite pass, fold this widget's subtree up to its windowed ancestor
       and blit.  This handles self-initiated repaints (button highlight, text
       scroll) that bypass the expose walk. */
    if (!_isw_in_composite && ctx->widget && IswIsWidget(ctx->widget) &&
        !IswIsShell(ctx->widget)) {
        /* The composite root is the nearest enclosing shell — the widget that
           owns the persisted root surface the platform blits to its window.
           Must stop at a popup shell, not walk into the main tree above it. */
        Widget root = _IswWidgetAncestor(ctx->widget);
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
    if (!IswIsWidget(child) || IswIsShell(child))
        return False;
    if (!child->core.windowless_realized && !IswIsRealized(child))
        return False;
    /* windowless_mapped is the live "the window is mapped" equivalent for
       windowless widgets — driven by Isw{Map,Unmap}Widget / manage / unmanage
       exactly as map/unmap drive a real window.  A widget composites only while
       mapped, just as a windowed widget is on screen only while its window is
       mapped. */
    if (!child->core.windowless_mapped)
        return False;
    return True;
}

static void _isw_composite_children_into(Widget parent,
                                         IswSurface parent_surface);
static void _isw_composite_children_into_at(Widget parent, IswSurface dst,
                                            Widget dst_widget, int ox, int oy);

/* Number of child surfaces folded by composite passes, total.  Always
   maintained (not trace-gated) so ISWRenderCompositeSubtree can detect a
   no-op pass and skip the redundant present() blit. */
static long _isw_fold_count = 0;

/* True if any descendant of `parent` is a composite-shown windowless widget,
   i.e. the fold walk would touch at least one surface.  Mirrors the descent in
   _isw_composite_children_into_at without painting.  Used to short-circuit a
   composite pass that would fold nothing: during startup, every windowless
   widget's ISWRenderEnd triggers a composite of its windowed root before any
   child is mapped, producing hundreds of passes that fill+blit a full-window
   background with no content on it. */
static Boolean
_isw_subtree_has_shown_child(Widget parent)
{
    if (IswIsComposite(parent)) {
        CompositeWidget cw = (CompositeWidget) parent;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++) {
            Widget c = cw->composite.children[i];
            if (_isw_composite_shown(c))
                return True;
            /* A context-less container is not itself shown but may hold shown
               descendants. */
            if (IswIsWidget(c) && !IswIsShell(c) &&
                _isw_subtree_has_shown_child(c))
                return True;
        }
    }
    if (IswIsSubclass(parent, simpleWidgetClass)) {
        SimpleWidgetClass sc = (SimpleWidgetClass) parent->core.widget_class;
        if (sc->simple_class.nth_windowless_child != NULL) {
            int i = 0;
            Widget child;
            while ((child = (*sc->simple_class.nth_windowless_child)(parent, i++))
                   != NULL)
                if (_isw_composite_shown(child))
                    return True;
        }
    }
    return False;
}

/* Fold one windowless child (and its descendants) into a destination surface
   (dst at dst_widget) at accumulated logical offset (ox, oy) within dst's
   content. */
static void
_isw_composite_one(Widget child, IswSurface dst, Widget dst_widget,
                   int ox, int oy)
{
    IswSurface child_surface;

    if (!_isw_composite_shown(child))
        return;

    child_surface = IswSurfaceOf(child);
    if (child_surface == NULL) {
        /* Surface-less windowless container (e.g. a pure layout container with
           no expose proc, so it never created a surface).  It contributes no
           pixels of its own, but its descendants still must be composited —
           fold them directly onto dst, accumulating this child's offset. */
        {
            IswBorderSides cbs = _IswGetBorderSides(child);
            _isw_composite_children_into_at(child, dst, dst_widget,
                                            ox + child->core.x + cbs.left,
                                            oy + child->core.y + cbs.top);
        }
        return;
    }

    /* Re-establish a CONTAINER's OWN surface before folding descendants.
       A container surface persists between composites and accumulates the
       pixels of children folded onto it on previous passes.  If a child has
       since been unmapped, the composite below correctly skips it, but its old
       pixels would remain on this surface.  Re-running the container's expose
       proc repaints its background (Form fills, Viewport redraws its gutter,
       etc.), erasing the vacated region; the still-mapped descendants are then
       folded back on top.  Only composites accumulate child pixels — a leaf
       widget's surface holds only its own content, blended fresh each pass, so
       skip them (re-running a leaf's expose every composite is wasteful and
       some leaves don't accept a NULL full-repaint event).  Safe during
       composite: ISWRenderEnd suppresses its auto-composite while
       _isw_in_composite.

       Gated on composite_dirty (on Core): a container whose surface is
       unchanged since the last fold (nothing under it repainted, no child
       un/mapped or moved) already holds the correct background + folded
       children, so regenerating identical pixels is pure cost — the dominant
       cost of compositing a large window for a localized change (a scroll, a
       hover).  A dirty descendant re-exposes itself in its own
       _isw_composite_one and is re-folded onto this (clean) surface below by
       composite_onto, so skipping the parent's re-expose does not stale the
       descendant. */
    if (child->core.widget_class->core_class.expose != NULL &&
        child->core.composite_dirty &&
        (IswIsComposite(child) || child->core.virtual_origin)) {
        (*child->core.widget_class->core_class.expose)(child, NULL, 0);
        child->core.composite_dirty = False;
    }

    /* Fold this child's own descendants into its surface first (bottom-up). */
    _isw_composite_children_into(child, child_surface);

    /* Then fold the child's surface into dst at the child's position. */
    if (_isw_surface_ops && _isw_surface_ops->composite_onto) {
        _isw_fold_count++;
        _isw_surface_ops->composite_onto(dst, dst_widget, child_surface, child,
                                         ox + child->core.x, oy + child->core.y);
    }
}

/* Recursively fold a parent's windowless children into a destination surface
   at logical offset (ox, oy).  Depth-first, in stacking order: composite
   children plus non-composite-tracked sub-widgets via the Simple hook. */
static void
_isw_composite_children_into_at(Widget parent, IswSurface dst,
                                Widget dst_widget, int ox, int oy)
{
    if (dst == NULL)
        return;

    if (IswIsComposite(parent)) {
        CompositeWidget cw = (CompositeWidget) parent;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++)
            _isw_composite_one(cw->composite.children[i], dst, dst_widget, ox, oy);
    }

    if (IswIsSubclass(parent, simpleWidgetClass)) {
        SimpleWidgetClass sc = (SimpleWidgetClass) parent->core.widget_class;
        if (sc->simple_class.nth_windowless_child != NULL) {
            int i = 0;
            Widget child;
            while ((child = (*sc->simple_class.nth_windowless_child)(parent, i++))
                   != NULL)
                _isw_composite_one(child, dst, dst_widget, ox, oy);
        }
    }
}

/* Fold a parent's windowless children into the parent's OWN surface. */
static void
_isw_composite_children_into(Widget parent, IswSurface parent_surface)
{
    _isw_composite_children_into_at(parent, parent_surface, parent, 0, 0);
}

void
ISWRenderCompositeSubtree(Widget windowed_root)
{
    IswSurface root_surface;
    Boolean prev = _isw_in_composite;

    if (windowed_root == NULL || !IswIsWidget(windowed_root))
        return;

    root_surface = IswSurfaceOf(windowed_root);
    if (root_surface == NULL) {
        /* Bare windowed root (Box/Form/Shell with no expose proc that paints
           its own content).  Create a surface so children have somewhere to
           composite onto, and mark it so we background-fill it each pass —
           nothing else paints this window's background. */
        ISWRenderContext *root_ctx =
            ISWRenderCreate(windowed_root, ISW_RENDER_BACKEND_AUTO);
        if (root_ctx == NULL)
            return;
        windowed_root->core.composite_lazy_root = True;
        root_surface = IswSurfaceOf(windowed_root);
        if (root_surface == NULL)
            return;
    }

    /* Short-circuit a pass that would fold nothing onto a lazy root that has
       never shown content.  During startup each windowless widget's
       ISWRenderEnd composites the windowed root before any child is mapped;
       without this guard that produces hundreds of full-window background
       fill+blit passes with no content on them. */
    if (windowed_root->core.composite_lazy_root &&
        !windowed_root->core.composite_presented &&
        !_isw_subtree_has_shown_child(windowed_root))
        return;

    _isw_in_composite = True;

    long fold0 = _isw_fold_count;
    Boolean filled_bg = False;

    /* Fill the background ONLY for a lazily-created root.  A widget-owned root
       (SimpleMenu, IconView, ...) painted its own content via its expose proc
       this frame; filling would wipe it.  The composite then folds the
       windowless children on top of whatever the root drew. */
    if (windowed_root->core.composite_lazy_root &&
        _isw_surface_ops && _isw_surface_ops->fill_background) {
        _isw_surface_ops->fill_background(root_surface, windowed_root);
        filled_bg = True;
    }
    _isw_composite_children_into(windowed_root, root_surface);

    /* Present the composited root surface to its window — but only if this pass
       actually changed the surface.  A pass that folded no children and did no
       background fill (common during startup, when widgets paint before being
       mapped) leaves the surface identical to what is already on screen, so the
       present is a pure-overhead full-window blit.  Skipping those collapses a
       startup storm of hundreds of redundant blits.  The window blit lives in
       the platform layer now (present_root), so the render layer hands it the
       opaque root window + surface and never names the native window. */
    Boolean folded_now = (_isw_fold_count != fold0);
    /* A widget-owned (non-lazy) root painted its own content into root_surface
       via its expose proc before this composite — present it even though the
       fold itself touched nothing, otherwise self-painting roots (SimpleMenu,
       IconView, ...) never reach the screen.  Lazy roots only present when they
       filled or folded something. */
    Boolean self_painted = !windowed_root->core.composite_lazy_root;
    if (filled_bg || folded_now || self_painted) {
        double sf = _IswGetScaleFactor(IswDisplayOf(windowed_root));
        int pw = (int)(windowed_root->core.width * sf + 0.5);
        int ph = (int)(windowed_root->core.height * sf + 0.5);
        IswWindow win = _IswPlatformWidgetWindow(
            IswDisplayOf((Widget)(windowed_root)), (Widget)(windowed_root));
        /* Presentation of the folded root is backend-specific (Cairo blits its
           back buffer; EGL blits its FBO and swaps), so it goes through the
           active surface ops, not a Cairo-specific platform op. */
        if (_isw_surface_ops && _isw_surface_ops->present_root)
            _isw_surface_ops->present_root(root_surface, windowed_root,
                                           win, pw, ph);
        else
            _IswPlatformPresentRoot(IswDisplayOf(windowed_root), win,
                                    root_surface, pw, ph);
    }
    if (folded_now)
        windowed_root->core.composite_presented = True;
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
    /* Store on Core, where it survives the surface's create/destroy and is
       applied in the composite pass (composite_onto) regardless of
       paint/layout ordering. */
    if (widget == NULL || !IswIsWidget(widget))
        return;
    widget->core.composite_clip = (w > 0 && h > 0);
    widget->core.composite_clip_x = x;
    widget->core.composite_clip_y = y;
    widget->core.composite_clip_w = (w > 0) ? w : 0;
    widget->core.composite_clip_h = (h > 0) ? h : 0;
}

Boolean
ISWRenderGetCompositeClip(Widget widget, int *x, int *y, int *w, int *h)
{
    if (widget == NULL || !IswIsWidget(widget) || !widget->core.composite_clip)
        return False;
    if (x) *x = widget->core.composite_clip_x;
    if (y) *y = widget->core.composite_clip_y;
    if (w) *w = widget->core.composite_clip_w;
    if (h) *h = widget->core.composite_clip_h;
    return True;
}

void
ISWRenderSetVirtualOrigin(Widget widget, int x, int y, int w, int h)
{
    if (widget == NULL || !IswIsWidget(widget))
        return;
    widget->core.virtual_origin = (w > 0 && h > 0);
    widget->core.virtual_origin_x = x;
    widget->core.virtual_origin_y = y;
    widget->core.virtual_origin_w = (w > 0) ? w : 0;
    widget->core.virtual_origin_h = (h > 0) ? h : 0;
}

Boolean
ISWRenderGetVirtualOrigin(Widget widget, int *x, int *y, int *w, int *h)
{
    if (widget == NULL || !IswIsWidget(widget) || !widget->core.virtual_origin)
        return False;
    if (x) *x = widget->core.virtual_origin_x;
    if (y) *y = widget->core.virtual_origin_y;
    if (w) *w = widget->core.virtual_origin_w;
    if (h) *h = widget->core.virtual_origin_h;
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
 * Virtual-origin draw culling
 * =================================================================
 *
 * When a widget has a virtual origin (its back surface covers only a tile of
 * its logical extent), draw calls whose bounding rect falls entirely outside
 * the tile can be skipped before reaching the backend.  The backend's clip
 * would discard the pixels anyway, but skipping here avoids the text shaping,
 * image decode, and path construction that the backend would do first.
 */
static inline Boolean
_isw_outside_tile(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    Widget wgt;
    if (!ctx || !(wgt = ctx->widget) || !wgt->core.virtual_origin)
        return False;
    int tx = wgt->core.virtual_origin_x;
    int ty = wgt->core.virtual_origin_y;
    int tr = tx + wgt->core.virtual_origin_w;
    int tb = ty + wgt->core.virtual_origin_h;
    return (x + w <= tx || x >= tr || y + h <= ty || y >= tb);
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
    
    if (!_isw_outside_tile(ctx, x, y, width, height))
        ctx->ops->stroke_rectangle(ctx, x, y, width, height);
}

void
ISWRenderFillRectangle(ISWRenderContext *ctx, int x, int y, int width, int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_rectangle)
        return;
    if (_isw_outside_tile(ctx, x, y, width, height))
        return;
    ctx->ops->fill_rectangle(ctx, x, y, width, height);
}

void
ISWRenderFillRoundedRectangle(ISWRenderContext *ctx,
                              int x, int y, int width, int height,
                              double radius)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_rounded_rect)
        return;
    if (_isw_outside_tile(ctx, x, y, width, height))
        return;
    ctx->ops->fill_rounded_rect(ctx, x, y, width, height, radius);
}

void
ISWRenderStrokeRoundedRectangle(ISWRenderContext *ctx,
                                int x, int y, int width, int height,
                                double radius,
                                double stroke_width)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_rounded_rect)
        return;
    if (_isw_outside_tile(ctx, x, y, width, height))
        return;
    ctx->ops->stroke_rounded_rect(ctx, x, y, width, height, radius,
                                  stroke_width);
}

void
ISWRenderFillStrokeRoundedRectangle(ISWRenderContext *ctx,
                                    int x, int y, int width, int height,
                                    double radius,
                                    double fill_alpha,
                                    double stroke_width)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_stroke_rounded_rect)
        return;
    if (_isw_outside_tile(ctx, x, y, width, height))
        return;
    ctx->ops->fill_stroke_rounded_rect(ctx, x, y, width, height, radius,
                                       fill_alpha, stroke_width);
}

void
ISWRenderStrokePolygon(ISWRenderContext *ctx, IswPoint *points, int num_points)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_polygon)
        return;
    ctx->ops->stroke_polygon(ctx, points, num_points);
}

void
ISWRenderFillPolygon(ISWRenderContext *ctx, IswPoint *points, int num_points)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_polygon)
        return;
    ctx->ops->fill_polygon(ctx, points, num_points);
}

void
ISWRenderDrawLine(ISWRenderContext *ctx, int x1, int y1, int x2, int y2)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_line)
        return;
    int lx = x1 < x2 ? x1 : x2;
    int ly = y1 < y2 ? y1 : y2;
    int lw = (x1 < x2 ? x2 - x1 : x1 - x2) + 1;
    int lh = (y1 < y2 ? y2 - y1 : y1 - y2) + 1;
    if (_isw_outside_tile(ctx, lx, ly, lw, lh))
        return;
    ctx->ops->draw_line(ctx, x1, y1, x2, y2);
}

void
ISWRenderDrawArc(ISWRenderContext *ctx, int x, int y, int width, int height,
                double angle1, double angle2)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_arc)
        return;
    if (_isw_outside_tile(ctx, x, y, width, height))
        return;
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
    if (!ctx || !ctx->ops || !ctx->ops->draw_string)
        return;
    /* y is the baseline; text extends roughly one line height above it and
       descenders below.  Use a generous vertical band (200px) to avoid calling
       text_height — the exact height doesn't matter for culling, only ensuring
       we never incorrectly skip a visible string. */
    if (ctx->widget && ctx->widget->core.virtual_origin) {
        int tt = ctx->widget->core.virtual_origin_y;
        int tb = tt + ctx->widget->core.virtual_origin_h;
        if (y + 200 < tt || y - 200 >= tb)
            return;
    }
    ctx->ops->draw_string(ctx, text, length, x, y);
}

int
ISWRenderTextWidth(ISWRenderContext *ctx, const char *text, int length)
{
    if (!ctx || !ctx->ops || !ctx->ops->text_width) {
        return length * 8;
    }

    ISWFontMetricCache *mc = ISWFontMetricCacheGet();
    int cached;
    if (ISWFontMetricCacheLookup(mc, ctx->current_font, text, length, &cached))
        return cached;

    int w = ctx->ops->text_width(ctx, text, length);
    ISWFontMetricCacheStore(mc, ctx->current_font, text, length, w);
    return w;
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
    if (_isw_outside_tile(ctx, dst_x, dst_y, (int)dst_w, (int)dst_h))
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
    if (!ctx || !ctx->ops || !ctx->ops->draw_image_masked || !rgba)
        return;
    if (_isw_outside_tile(ctx, dst_x, dst_y, (int)dst_w, (int)dst_h))
        return;
    ctx->ops->draw_image_masked(ctx, foreground, rgba, img_w, img_h,
                                dst_x, dst_y, dst_w, dst_h);
}

/*
 * =================================================================
 * Retained Image API
 * =================================================================
 */

int
ISWRenderImageUpload(ISWRenderContext *ctx,
                     const unsigned char *rgba,
                     unsigned int w, unsigned int h)
{
    if (!ctx || !ctx->ops || !ctx->ops->image_upload || !rgba)
        return 0;
    return ctx->ops->image_upload(rgba, w, h);
}

void
ISWRenderImageFree(ISWRenderContext *ctx, int handle)
{
    if (!ctx || !ctx->ops || !ctx->ops->image_free || handle <= 0)
        return;
    ctx->ops->image_free(handle);
}

void
ISWRenderDrawImageHandle(ISWRenderContext *ctx, int handle,
                         int dst_x, int dst_y,
                         unsigned int dst_w, unsigned int dst_h)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_image_handle || handle <= 0)
        return;
    if (_isw_outside_tile(ctx, dst_x, dst_y, (int)dst_w, (int)dst_h))
        return;
    ctx->ops->draw_image_handle(ctx, handle, dst_x, dst_y, dst_w, dst_h);
}

Boolean
ISWRenderSetGradient(ISWRenderContext *ctx, double x1, double y1, double x2, double y2,
                    Pixel color1, Pixel color2)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_gradient) {
        return False;
    }
    
    return ctx->ops->set_gradient(ctx, x1, y1, x2, y2, color1, color2);
}

void
ISWRenderPushGroup(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->push_group) {
        return;
    }

    ctx->ops->push_group(ctx);
}

void
ISWRenderPopGroupWithAlpha(ISWRenderContext *ctx, double alpha)
{
    if (!ctx || !ctx->ops || !ctx->ops->pop_group_alpha) {
        return;
    }

    ctx->ops->pop_group_alpha(ctx, alpha);
}

void
ISWRenderPathBegin(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->path_begin) return;
    ctx->ops->path_begin(ctx);
}

void
ISWRenderPathNewSubPath(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->path_new_sub_path) return;
    ctx->ops->path_new_sub_path(ctx);
}

void
ISWRenderPathMoveTo(ISWRenderContext *ctx, double x, double y)
{
    if (!ctx || !ctx->ops || !ctx->ops->path_move_to) return;
    ctx->ops->path_move_to(ctx, x, y);
}

void
ISWRenderPathLineTo(ISWRenderContext *ctx, double x, double y)
{
    if (!ctx || !ctx->ops || !ctx->ops->path_line_to) return;
    ctx->ops->path_line_to(ctx, x, y);
}

void
ISWRenderPathArc(ISWRenderContext *ctx, double cx, double cy, double r,
                 double angle1, double angle2)
{
    if (!ctx || !ctx->ops || !ctx->ops->path_arc) return;
    ctx->ops->path_arc(ctx, cx, cy, r, angle1, angle2);
}

void
ISWRenderPathRectangle(ISWRenderContext *ctx,
                       double x, double y, double w, double h)
{
    if (!ctx || !ctx->ops || !ctx->ops->path_rectangle) return;
    ctx->ops->path_rectangle(ctx, x, y, w, h);
}

void
ISWRenderPathClose(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->path_close) return;
    ctx->ops->path_close(ctx);
}

void
ISWRenderFill(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_path) return;
    ctx->ops->fill_path(ctx, False);
}

void
ISWRenderFillPreserve(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_path) return;
    ctx->ops->fill_path(ctx, True);
}

void
ISWRenderStroke(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_path) return;
    ctx->ops->stroke_path(ctx, False);
}

void
ISWRenderStrokePreserve(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_path) return;
    ctx->ops->stroke_path(ctx, True);
}

void
ISWRenderClip(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->clip_path) return;
    ctx->ops->clip_path(ctx);
}

void
ISWRenderPaint(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->paint) return;
    ctx->ops->paint(ctx);
}

void
ISWRenderSetFillRule(ISWRenderContext *ctx, ISWFillRule rule)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_fill_rule) return;
    ctx->ops->set_fill_rule(ctx, rule);
}

void
ISWRenderSetDash(ISWRenderContext *ctx, const double *dashes,
                 int num_dashes, double offset)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_dash) return;
    ctx->ops->set_dash(ctx, dashes, num_dashes, offset);
}

void
ISWRenderSetOperator(ISWRenderContext *ctx, ISWOperator op)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_operator) return;
    ctx->ops->set_operator(ctx, op);
}

void
ISWRenderTranslate(ISWRenderContext *ctx, double tx, double ty)
{
    if (!ctx || !ctx->ops || !ctx->ops->translate) return;
    ctx->ops->translate(ctx, tx, ty);
}

void
ISWRenderScale(ISWRenderContext *ctx, double sx, double sy)
{
    if (!ctx || !ctx->ops || !ctx->ops->scale) return;
    ctx->ops->scale(ctx, sx, sy);
}

void
ISWRenderRotate(ISWRenderContext *ctx, double radians)
{
    if (!ctx || !ctx->ops || !ctx->ops->rotate) return;
    ctx->ops->rotate(ctx, radians);
}

void
ISWRenderShowText(ISWRenderContext *ctx, const char *text)
{
    if (!ctx || !ctx->ops || !ctx->ops->show_text) return;
    ctx->ops->show_text(ctx, text);
}

/*
 * =================================================================
 * Helper Functions
 * =================================================================
 */

/*
 * Decode a pixel to 0..1 RGB.  The neutral context holds no visual/colormap;
 * forward to the backend's .pixel_to_rgb op, which decodes using the backend's
 * own visual.  Fallback to packed 0xRRGGBB if the backend can't decode.
 */
void
ISWRenderPixelToRGB(ISWRenderContext *ctx, Pixel pixel,
                   double *r, double *g, double *b)
{
    if (!ctx || !r || !g || !b)
        return;

    if (ctx->ops && ctx->ops->pixel_to_rgb) {
        ctx->ops->pixel_to_rgb(ctx, pixel, r, g, b);
        return;
    }

    *r = ((pixel >> 16) & 0xFF) / 255.0;
    *g = ((pixel >> 8) & 0xFF) / 255.0;
    *b = (pixel & 0xFF) / 255.0;
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

int
ISWScaledTextWidth(Widget widget, IswFontStruct *font, const char *text, int len)
{
    const IswPlatformRenderOps *rops = _IswPlatformRenderOpsActive();
    if (rops && rops->scaled_text_width)
        return rops->scaled_text_width(widget, font, text, len);
    return 0;
}

int
ISWScaledFontHeight(Widget widget, IswFontStruct *font)
{
    const IswPlatformRenderOps *rops = _IswPlatformRenderOpsActive();
    if (rops && rops->scaled_font_height)
        return rops->scaled_font_height(widget, font);
    return 0;
}

int
ISWScaledFontAscent(Widget widget, IswFontStruct *font)
{
    const IswPlatformRenderOps *rops = _IswPlatformRenderOpsActive();
    if (rops && rops->scaled_font_ascent)
        return rops->scaled_font_ascent(widget, font);
    return 0;
}

int
ISWScaledFontCapHeight(Widget widget, IswFontStruct *font)
{
    const IswPlatformRenderOps *rops = _IswPlatformRenderOpsActive();
    if (rops && rops->scaled_font_cap_height)
        return rops->scaled_font_cap_height(widget, font);
    return 0;
}

