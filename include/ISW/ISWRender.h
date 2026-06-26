/*
 * ISWRender.h - Rendering abstraction API for ISW widgets
 *
 * Copyright (c) 2026 ISW Project
 *
 * This file provides a backend-agnostic rendering API for ISW widgets.
 * Supports multiple rendering backends:
 *   - Cairo-XCB: Cairo with XCB surface (software rendering with anti-aliasing)
 *   - EGL: Pure EGL/OpenGL ES via NanoVG (hardware accelerated, no Cairo)
 *
 * Cairo is a mandatory dependency.
 * CRITICAL: All backends use pure XCB - NO XLIB DEPENDENCIES.
 */

#ifndef _ISWRender_h
#define _ISWRender_h

#include <ISW/Intrinsic.h>

/*
 * Relief types for 3D shadows (shared with ThreeD.h)
 */
#ifndef _IswRelief_defined
#define _IswRelief_defined
typedef enum {
    IswReliefNone,
    IswReliefRaised,
    IswReliefSunken,
    IswReliefRidge,
    IswReliefGroove
} IswRelief;
#endif

/*
 * Opaque rendering context handle
 */
typedef struct _ISWRenderContext ISWRenderContext;

/*
 * Available rendering backends
 */
typedef enum {
    ISW_RENDER_BACKEND_AUTO = 0,      /* Auto-detect best available */
    ISW_RENDER_BACKEND_CAIRO_XCB,     /* Cairo with XCB surface */
    ISW_RENDER_BACKEND_EGL            /* Pure EGL/OpenGL ES via NanoVG (no Cairo) */
} ISWRenderBackend;

/*
 * Rendering capability flags
 */
typedef enum {
    ISW_RENDER_CAP_BASIC         = (1 << 0),  /* Basic shapes */
    ISW_RENDER_CAP_ANTIALIASING  = (1 << 1),  /* Anti-aliased drawing */
    ISW_RENDER_CAP_GRADIENTS     = (1 << 2),  /* Linear/radial gradients */
    ISW_RENDER_CAP_ALPHA         = (1 << 3),  /* Alpha blending */
    ISW_RENDER_CAP_TRANSFORMS    = (1 << 4),  /* Affine transformations */
    ISW_RENDER_CAP_TEXT_ADVANCED = (1 << 5),  /* Advanced text layout */
    ISW_RENDER_CAP_HW_ACCEL      = (1 << 6)   /* Hardware acceleration */
} ISWRenderCaps;

/*
 * Fill rule for ISWRenderFill/ISWRenderClip when a path self-overlaps or has
 * multiple sub-paths (e.g. even-odd masking of corner slivers or text holes).
 */
typedef enum {
    ISW_FILL_RULE_WINDING = 0,   /* Non-zero winding (default) */
    ISW_FILL_RULE_EVEN_ODD       /* Even-odd */
} ISWFillRule;

/*
 * Compositing operator for subsequent draws.  ISW_OPERATOR_OVER is the normal
 * source-over blend; ISW_OPERATOR_DIFFERENCE gives an invert-style result used
 * for rubber-band overlays that toggle cleanly when drawn twice.
 */
typedef enum {
    ISW_OPERATOR_OVER = 0,       /* Normal source-over (default) */
    ISW_OPERATOR_DIFFERENCE      /* Difference (rubber-band / XOR-like) */
} ISWOperator;

/*
 * =================================================================
 * Context Creation and Destruction
 * =================================================================
 */

/*
 * ISWRenderCreate - Create rendering context for a widget
 *
 * Parameters:
 *   widget    - Widget to create context for
 *   preferred - Preferred backend (ISW_RENDER_BACKEND_AUTO for auto-detect)
 *
 * Returns: Rendering context, or NULL on failure
 *
 * Notes:
 *   - If preferred backend is unavailable, falls back to best available
 *   - Context must be destroyed with ISWRenderDestroy()
 */
ISWRenderContext* ISWRenderCreate(
    Widget widget,
    ISWRenderBackend preferred
);

/*
 * ISWRenderDestroy - Destroy rendering context
 *
 * Parameters:
 *   ctx - Context to destroy
 *
 * Notes:
 *   - Frees all backend-specific resources
 *   - Safe to call with NULL context
 */
void ISWRenderDestroy(ISWRenderContext *ctx);

/*
 * ISWRenderGetBackend - Get active backend
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Returns: Active backend type
 */
ISWRenderBackend ISWRenderGetBackend(ISWRenderContext *ctx);

/*
 * ISWRenderGetCapabilities - Query backend capabilities
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Returns: Capability flags (bitwise OR of ISWRenderCaps)
 */
ISWRenderCaps ISWRenderGetCapabilities(ISWRenderContext *ctx);

/*
 * ISWRenderGetBackendName - Get human-readable backend name
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Returns: Backend name string (e.g., "Cairo-XCB", "Cairo-EGL")
 *          Returns "Unknown" if context is NULL
 */
const char* ISWRenderGetBackendName(ISWRenderContext *ctx);

/*
 * ISWRenderPrintBackendInfo - Print rendering backend information
 *
 * Notes:
 *   - Prints which backend will be used and its capabilities
 *   - Useful for debugging and informational purposes
 *   - Can be called at application startup
 */
void ISWRenderPrintBackendInfo(void);

/*
 * =================================================================
 * State Management
 * =================================================================
 */

/*
 * ISWRenderBegin - Begin rendering frame
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Notes:
 *   - Must be called before any drawing operations
 *   - Pairs with ISWRenderEnd()
 */
void ISWRenderBegin(ISWRenderContext *ctx);

/*
 * ISWRenderEnd - End rendering frame and flush to display
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Notes:
 *   - Flushes pending operations to X server
 *   - Swaps buffers if using EGL backend
 */
void ISWRenderEnd(ISWRenderContext *ctx);

/*
 * ISWRenderCompositeSubtree - Composite a windowless subtree onto a window
 *
 * Parameters:
 *   windowed_root - a windowed widget whose windowless descendants have just
 *                   repainted into their own per-widget surfaces
 *
 * Notes:
 *   - Surface-per-widget model: each windowless widget owns a back surface and
 *     paints into it at local (0,0).  This pass folds those surfaces up the
 *     tree (child onto parent, in stacking order) and blits the root once.
 *   - Call after the paint walk at a windowed widget's expose.
 *   - No-op if the root has no windowless children with live surfaces.
 */
void ISWRenderCompositeSubtree(Widget windowed_root);

/*
 * ISWRenderBeginCompositeBatch / ISWRenderEndCompositeBatch
 *
 * Bracket a paint walk that ends with an explicit ISWRenderCompositeSubtree.
 * Between these calls, the per-widget auto-composite that ISWRenderEnd performs
 * for self-initiated windowless repaints is suppressed, so the subtree is
 * folded and blitted once at the end rather than once per child paint.
 */
void ISWRenderBeginCompositeBatch(void);
void ISWRenderEndCompositeBatch(void);

/*
 * ISWRenderBeginDeferComposite / ISWRenderFlushComposites
 *
 * Coalesce composites across an event dispatch.  Between Begin and Flush, a
 * windowless widget's self-initiated repaint (ISWRenderEnd) records its
 * windowed root as "dirty" instead of immediately folding and blitting the
 * whole tree.  Flush folds each dirty root exactly once.  Nestable: only the
 * outermost Flush performs the composites.  The event dispatcher brackets each
 * dispatch with these so a burst of widget repaints produces one frame.
 */
void ISWRenderBeginDeferComposite(void);
void ISWRenderFlushComposites(void);

/*
 * ISWRenderRequestComposite - composite a windowed root, coalesced if possible
 *
 * If an event dispatch is in progress (between BeginDeferComposite and Flush),
 * records the root as dirty for the single end-of-dispatch flush.  Otherwise
 * composites immediately.  Use instead of ISWRenderCompositeSubtree for
 * repaint requests so repeated requests in one dispatch collapse to one frame.
 */
void ISWRenderRequestComposite(Widget windowed_root);

/*
 * ISWRenderForgetRoot - cancel any pending composite for a windowed root
 *
 * Drops the root from the coalesced dirty-root set so a queued composite does
 * not blit to it.  Call when a windowed root's window is unmapped (e.g. a shell
 * popdown): otherwise a composite requested earlier in the same event dispatch
 * would re-present the surface to the now-unmapped window, leaving it visible.
 */
void ISWRenderForgetRoot(Widget windowed_root);

/*
 * _ISWRenderMarkDirtyChain - force re-expose of a widget's container chain
 *
 * Marks `w` and its windowless ancestors (up to the windowed root) so the next
 * composite fold re-runs their expose proc rather than reusing their persisted
 * surface.  ISWRenderEnd does this automatically for self-initiated paints; call
 * it explicitly at structural changes that vacate ancestor pixels WITHOUT a
 * repaint — chiefly unmapping a windowless child (pass the child's parent), so
 * the container repaints its background over the vacated region.
 */
void _ISWRenderMarkDirtyChain(Widget w);

/*
 * ISWRenderSetCompositeClip - confine a widget to a rectangle when composited
 *
 * Sets a clip rectangle (in the widget's PARENT content coordinates) applied
 * when this widget is folded into its parent's surface.  Used by scrolling
 * containers (Viewport) to keep a scrolled child within the clip region.
 * Pass w<=0 to clear.  No-op if the widget has no render context yet.
 */
void ISWRenderSetCompositeClip(Widget widget, int x, int y, int w, int h);

/*
 * ISWRenderGetCompositeClip - read a widget's composite clip (parent coords)
 *
 * Returns True and fills *x,*y,*w,*h if the widget has a composite clip set
 * (see ISWRenderSetCompositeClip); False otherwise.  Used by hit-testing to
 * confine a scrolled child to the same region it is painted in.
 */
Boolean ISWRenderGetCompositeClip(Widget widget, int *x, int *y, int *w, int *h);

/*
 * ISWRenderSetVirtualOrigin - restrict a widget's back surface to a sub-region
 *
 * Instead of allocating a back surface the full size of the widget, the
 * backend allocates (w x h) and translates the drawing origin so widget-local
 * coordinate (x, y) maps to surface pixel (0, 0).  The widget's expose proc
 * draws at its normal local coordinates; only the visible tile is rasterised.
 * Pass w<=0 to clear (reverts to full-size surface).
 */
void ISWRenderSetVirtualOrigin(Widget widget, int x, int y, int w, int h);

/*
 * ISWRenderGetVirtualOrigin - read a widget's virtual origin
 *
 * Returns True and fills *x,*y,*w,*h if the widget has a virtual origin set;
 * False otherwise.
 */
Boolean ISWRenderGetVirtualOrigin(Widget widget, int *x, int *y, int *w, int *h);

/*
 * ISWRenderSave - Save current graphics state
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Notes:
 *   - Saves color, line width, etc.
 *   - Must be paired with ISWRenderRestore()
 */
void ISWRenderSave(ISWRenderContext *ctx);

/*
 * ISWRenderRestore - Restore previous graphics state
 *
 * Parameters:
 *   ctx - Rendering context
 */
void ISWRenderRestore(ISWRenderContext *ctx);

/*
 * =================================================================
 * Color and Line Management
 * =================================================================
 */

/*
 * ISWRenderSetColor - Set current drawing color
 *
 * Parameters:
 *   ctx   - Rendering context
 *   pixel - Pixel value (from widget's colormap)
 */
void ISWRenderSetColor(ISWRenderContext *ctx, Pixel pixel);

/*
 * ISWRenderSetColorRGBA - Set color with alpha (if supported)
 *
 * Parameters:
 *   ctx - Rendering context
 *   r   - Red component (0.0-1.0)
 *   g   - Green component (0.0-1.0)
 *   b   - Blue component (0.0-1.0)
 *   a   - Alpha component (0.0-1.0)
 *
 * Notes:
 *   - Works with all Cairo backends
 */
void ISWRenderSetColorRGBA(ISWRenderContext *ctx,
                          double r, double g, double b, double a);

/*
 * ISWRenderSetLineWidth - Set line width for stroking
 *
 * Parameters:
 *   ctx   - Rendering context
 *   width - Line width in pixels
 */
void ISWRenderSetLineWidth(ISWRenderContext *ctx, double width);

/*
 * =================================================================
 * Shape Drawing Primitives
 * =================================================================
 */

/*
 * ISWRenderStrokeRectangle - Draw rectangle outline
 *
 * Parameters:
 *   ctx    - Rendering context
 *   x, y   - Top-left corner
 *   width  - Rectangle width
 *   height - Rectangle height
 */
void ISWRenderStrokeRectangle(ISWRenderContext *ctx,
                             int x, int y,
                             int width, int height);

/*
 * ISWRenderFillRectangle - Fill rectangle
 *
 * Parameters:
 *   ctx    - Rendering context
 *   x, y   - Top-left corner
 *   width  - Rectangle width
 *   height - Rectangle height
 */
void ISWRenderFillRectangle(ISWRenderContext *ctx,
                           int x, int y,
                           int width, int height);

/*
 * ISWRenderFillRoundedRectangle - Fill rectangle with rounded corners
 *
 * Parameters:
 *   ctx    - Rendering context
 *   x, y   - Top-left corner
 *   width  - Rectangle width
 *   height - Rectangle height
 *   radius - Corner radius in pixels
 */
void ISWRenderFillRoundedRectangle(ISWRenderContext *ctx,
                                   int x, int y,
                                   int width, int height,
                                   double radius);

/*
 * ISWRenderStrokeRoundedRectangle - Stroke a rounded rectangle outline
 */
void ISWRenderStrokeRoundedRectangle(ISWRenderContext *ctx,
                                     int x, int y,
                                     int width, int height,
                                     double radius,
                                     double stroke_width);

/*
 * ISWRenderFillStrokeRoundedRectangle - Fill and stroke a rounded rectangle
 *
 * Fills with the current color at fill_alpha, then strokes the same path
 * with the current color at full opacity.  Falls back to ISWRenderFillRectangle
 * on backends without Cairo.
 */
void ISWRenderFillStrokeRoundedRectangle(ISWRenderContext *ctx,
                                         int x, int y,
                                         int width, int height,
                                         double radius,
                                         double fill_alpha,
                                         double stroke_width);

/*
 * ISWRenderStrokePolygon - Draw polygon outline
 *
 * Parameters:
 *   ctx        - Rendering context
 *   points     - Array of points
 *   num_points - Number of points
 */
void ISWRenderStrokePolygon(ISWRenderContext *ctx,
                           IswPoint *points,
                           int num_points);

/*
 * ISWRenderFillPolygon - Fill polygon
 *
 * Parameters:
 *   ctx        - Rendering context
 *   points     - Array of points
 *   num_points - Number of points
 */
void ISWRenderFillPolygon(ISWRenderContext *ctx,
                         IswPoint *points,
                         int num_points);

/*
 * ISWRenderDrawLine - Draw line
 *
 * Parameters:
 *   ctx      - Rendering context
 *   x1, y1   - Starting point
 *   x2, y2   - Ending point
 */
void ISWRenderDrawLine(ISWRenderContext *ctx,
                      int x1, int y1, int x2, int y2);

/*
 * ISWRenderDrawArc - Draw arc
 *
 * Parameters:
 *   ctx           - Rendering context
 *   x, y          - Bounding box top-left
 *   width, height - Bounding box size
 *   angle1        - Start angle (radians)
 *   angle2        - End angle (radians)
 */
void ISWRenderDrawArc(ISWRenderContext *ctx,
                     int x, int y,
                     int width, int height,
                     double angle1, double angle2);

/*
 * =================================================================
 * Text Rendering
 * =================================================================
 */

/*
 * ISWRenderDrawString - Draw text string
 *
 * Parameters:
 *   ctx    - Rendering context
 *   text   - Text to draw
 *   length - Length of text
 *   x, y   - Text baseline position
 *
 * Notes:
 *   - Font must be set with ISWRenderSetFont() first
 *   - Cairo backends may use Pango for advanced layout
 */
void ISWRenderDrawString(ISWRenderContext *ctx,
                        const char *text, int length,
                        int x, int y);

/*
 * ISWRenderTextWidth - Measure text width
 *
 * Parameters:
 *   ctx    - Rendering context
 *   text   - Text to measure
 *   length - Length of text
 *
 * Returns: Width in pixels
 */
int ISWRenderTextWidth(ISWRenderContext *ctx,
                      const char *text, int length);

/*
 * ISWRenderTextHeight - Measure text height
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Returns: Height in pixels (based on current font)
 */
int ISWRenderTextHeight(ISWRenderContext *ctx);

/*
 * ISWRenderSetFont - Set font for text rendering
 *
 * Parameters:
 *   ctx  - Rendering context
 *   font - IswFontStruct pointer
 *
 * Notes:
 *   - Converts IswFontStruct metrics to Cairo font sizing
 */
void ISWRenderSetFont(ISWRenderContext *ctx, IswFontStruct *font);

/*
 * =================================================================
 * Clipping
 * =================================================================
 */

/*
 * ISWRenderSetClipRectangle - Set rectangular clip region
 *
 * Parameters:
 *   ctx    - Rendering context
 *   x, y   - Top-left corner
 *   width  - Clip region width
 *   height - Clip region height
 */
void ISWRenderSetClipRectangle(ISWRenderContext *ctx,
                              int x, int y,
                              int width, int height);

/*
 * ISWRenderClearClip - Clear clip region
 *
 * Parameters:
 *   ctx - Rendering context
 */
void ISWRenderClearClip(ISWRenderContext *ctx);

/*
 * =================================================================
 * Pixmap/Bitmap Rendering
 * =================================================================
 */

/*
 * ISWRenderCopyArea - Copy area within the rendering surface
 *
 * Parameters:
 *   ctx          - Rendering context
 *   src_x, src_y - Source position
 *   dst_x, dst_y - Destination position
 *   width        - Region width
 *   height       - Region height
 *
 * Notes:
 *   - Copies pixels within the same window (used for scrolling)
 *   - Cairo backend flushes surface before copying
 */
void ISWRenderCopyArea(ISWRenderContext *ctx,
                       int src_x, int src_y,
                       int dst_x, int dst_y,
                       unsigned int width, unsigned int height);

/*
 * =================================================================
 * RGBA Image Rendering
 * =================================================================
 */

/*
 * ISWRenderDrawImageRGBA - Draw an RGBA pixel buffer onto the surface
 *
 * Parameters:
 *   ctx          - Rendering context
 *   rgba         - RGBA pixel data (4 bytes per pixel: R, G, B, A)
 *   img_width    - Width of the source image in pixels
 *   img_height   - Height of the source image in pixels
 *   dst_x, dst_y - Destination position on surface
 *   dst_w, dst_h - Destination size (image will be scaled to fit)
 *
 * Notes:
 *   - Used for rendering rasterized SVG images
 *   - Alpha channel is respected (transparent areas show through)
 *   - If dst_w/dst_h differ from img_width/img_height, the image is scaled
 */
void ISWRenderDrawImageRGBA(ISWRenderContext *ctx,
                            const unsigned char *rgba,
                            unsigned int img_width, unsigned int img_height,
                            int dst_x, int dst_y,
                            unsigned int dst_w, unsigned int dst_h);

/*
 * ISWRenderDrawImageMasked - Draw an RGBA image as an alpha mask painted
 *                            with the given foreground color.
 *
 * Extracts the alpha channel from the RGBA buffer and uses it as a Cairo
 * mask surface.  The visible pixels are painted in `foreground`, ignoring
 * the RGB channels of the source image.  This is the correct way to render
 * monochrome (currentColor) SVG icons so that color inversion (e.g. pressed
 * button state) works by simply changing the foreground pixel value.
 *
 * Parameters match ISWRenderDrawImageRGBA plus a foreground color.
 */
void ISWRenderDrawImageMasked(ISWRenderContext *ctx, Pixel foreground,
                              const unsigned char *rgba,
                              unsigned int img_width, unsigned int img_height,
                              int dst_x, int dst_y,
                              unsigned int dst_w, unsigned int dst_h);

/*
 * ISWRenderPixelToRGB - Convert pixel value to RGB components
 *
 * Parameters:
 *   ctx     - Rendering context
 *   pixel   - Pixel value
 *   r, g, b - Output RGB components (0.0-1.0)
 */
void ISWRenderPixelToRGB(ISWRenderContext *ctx, Pixel pixel,
                         double *r, double *g, double *b);

/*
 * =================================================================
 * Advanced Features (Cairo-only)
 * =================================================================
 */

/*
 * ISWRenderSetGradient - Set linear gradient source
 *
 * Parameters:
 *   ctx           - Rendering context
 *   x1, y1        - Gradient start point
 *   x2, y2        - Gradient end point
 *   color1        - Start color
 *   color2        - End color
 *
 * Returns: True on success
 */
Boolean ISWRenderSetGradient(ISWRenderContext *ctx,
                             double x1, double y1,
                             double x2, double y2,
                             Pixel color1, Pixel color2);

/*
 * ISWRenderPushGroup - Begin an offscreen compositing group
 *
 * Subsequent draw calls accumulate into a group rather than painting
 * directly onto the surface.  Pair with ISWRenderPopGroupWithAlpha to
 * composite the group onto the surface at a chosen opacity (e.g. for
 * rendering insensitive/greyed-out widgets).  No-op if the backend does
 * not support grouping.
 *
 * Parameters:
 *   ctx - Rendering context
 */
void ISWRenderPushGroup(ISWRenderContext *ctx);

/*
 * ISWRenderPopGroupWithAlpha - End a group and paint it at the given opacity
 *
 * Parameters:
 *   ctx   - Rendering context
 *   alpha - Opacity to composite the group at (0.0-1.0)
 */
void ISWRenderPopGroupWithAlpha(ISWRenderContext *ctx, double alpha);

/*
 * =================================================================
 * Path Construction and Painting
 * =================================================================
 *
 * Build an arbitrary path with the ISWRenderPath* calls, then paint it with
 * ISWRenderFill / ISWRenderStroke (optionally *Preserve to keep the path for a
 * second paint, e.g. fill-then-stroke), or clip to it with ISWRenderClip.
 * Coordinates are logical pixels; the backend handles HiDPI scaling.  All are
 * no-ops on backends without path support.
 */

/* Start a fresh, empty path (discards any current path). */
void ISWRenderPathBegin(ISWRenderContext *ctx);
/* Begin a new sub-path with no current point (needed before a lone arc/circle
 * so it is not joined to the previous sub-path by a line). */
void ISWRenderPathNewSubPath(ISWRenderContext *ctx);
/* Set the current point without drawing. */
void ISWRenderPathMoveTo(ISWRenderContext *ctx, double x, double y);
/* Add a straight segment from the current point to (x, y). */
void ISWRenderPathLineTo(ISWRenderContext *ctx, double x, double y);
/* Add a circular arc centred at (cx, cy), radius r, sweeping angle1->angle2
 * (radians, positive = clockwise in the y-down surface). */
void ISWRenderPathArc(ISWRenderContext *ctx, double cx, double cy, double r,
                      double angle1, double angle2);
/* Add an axis-aligned rectangle as a closed sub-path. */
void ISWRenderPathRectangle(ISWRenderContext *ctx,
                            double x, double y, double w, double h);
/* Close the current sub-path back to its start point. */
void ISWRenderPathClose(ISWRenderContext *ctx);

/* Fill the current path with the current colour. */
void ISWRenderFill(ISWRenderContext *ctx);
/* Fill the current path but keep it for a subsequent paint. */
void ISWRenderFillPreserve(ISWRenderContext *ctx);
/* Stroke the current path with the current colour and line width. */
void ISWRenderStroke(ISWRenderContext *ctx);
/* Stroke the current path but keep it for a subsequent paint. */
void ISWRenderStrokePreserve(ISWRenderContext *ctx);
/* Intersect the clip region with the current path (consumes the path). */
void ISWRenderClip(ISWRenderContext *ctx);
/* Paint the current colour over the entire current clip region. */
void ISWRenderPaint(ISWRenderContext *ctx);

/* Set the fill rule used by path fills and ISWRenderClip. */
void ISWRenderSetFillRule(ISWRenderContext *ctx, ISWFillRule rule);
/* Set a dash pattern for stroking; num_dashes == 0 restores a solid line.
 * `dashes` are on/off lengths in logical pixels. */
void ISWRenderSetDash(ISWRenderContext *ctx, const double *dashes,
                      int num_dashes, double offset);
/* Set the compositing operator for subsequent draws. */
void ISWRenderSetOperator(ISWRenderContext *ctx, ISWOperator op);

/*
 * Affine transform of the coordinate system (cumulative until the enclosing
 * ISWRenderSave/Restore pair is restored).  Used for scaled/rotated text and
 * shapes (e.g. vertical progress-bar labels).
 */
void ISWRenderTranslate(ISWRenderContext *ctx, double tx, double ty);
void ISWRenderScale(ISWRenderContext *ctx, double sx, double sy);
void ISWRenderRotate(ISWRenderContext *ctx, double radians);

/*
 * Draw `text` at the current point using the current font and colour, advancing
 * the current point.  Honours the active transform (unlike ISWRenderDrawString,
 * which positions in surface coordinates).  Used for transformed label text.
 */
void ISWRenderShowText(ISWRenderContext *ctx, const char *text);

/*
 * =================================================================
 * HiDPI Scaling
 * =================================================================
 */

/*
 * ISWScaleFactor - Get HiDPI scale factor for a widget's display
 *
 * Parameters:
 *   widget - Widget to query
 *
 * Returns: Scale factor (1.0 = 96 DPI, 1.75 = 168 DPI, etc.)
 */
double ISWScaleFactor(Widget widget);

/*
 * ISWScaleDim - Scale a dimension value by the display's scale factor
 *
 * Parameters:
 *   widget - Widget to query
 *   value  - Dimension value to scale
 *
 * Returns: Scaled dimension (rounded up), minimum 1
 */
Dimension ISWScaleDim(Widget widget, int value);

/*
 * ISWUnscaleDim - Convert a physical dimension to logical pixels
 *
 * Parameters:
 *   widget - Widget to query
 *   value  - Physical dimension value to unscale
 *
 * Returns: Logical dimension (rounded half-up), minimum 1
 */
Dimension ISWUnscaleDim(Widget widget, int value);

/*
 * ISWScalePos - Scale a position value from logical to physical pixels
 *
 * Parameters:
 *   widget - Widget to query
 *   value  - Logical position value to scale
 *
 * Returns: Physical position (rounded to nearest)
 */
Position ISWScalePos(Widget widget, int value);

/*
 * ISWUnscalePos - Convert a physical position to logical pixels
 *
 * Parameters:
 *   widget - Widget to query
 *   value  - Physical position value to unscale
 *
 * Returns: Logical position (rounded to nearest)
 */
Position ISWUnscalePos(Widget widget, int value);

/*
 * ISWScaledTextWidth - Measure text width using Cairo at the scaled font size.
 * Returns the width Cairo will actually use to render this text, ensuring
 * layout matches rendering on HiDPI displays.
 *
 * Parameters:
 *   widget - Widget for display/scale context
 *   font   - IswFontStruct for base font metrics (may be NULL for default)
 *   text   - Text string to measure
 *   len    - Length of text
 *
 * Returns: Text width in pixels as Cairo would render it
 */
int ISWScaledTextWidth(Widget widget, IswFontStruct *font, const char *text, int len);

/*
 * ISWScaledFontHeight - Get the font line height as Cairo would render it.
 */
int ISWScaledFontHeight(Widget widget, IswFontStruct *font);

/*
 * ISWScaledFontAscent - Get the font ascent as Cairo would render it.
 */
int ISWScaledFontAscent(Widget widget, IswFontStruct *font);

/*
 * ISWScaledFontCapHeight - Get the cap height (height of a capital letter)
 * as Cairo would render it.
 */
int ISWScaledFontCapHeight(Widget widget, IswFontStruct *font);

#endif /* _ISWRender_h */
