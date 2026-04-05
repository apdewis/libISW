/*
 * ISWRender.h - Rendering abstraction API for ISW widgets
 *
 * Copyright (c) 2026 ISW Project
 *
 * This file provides a backend-agnostic rendering API for ISW widgets.
 * Supports multiple rendering backends:
 *   - Cairo-XCB: Cairo with XCB surface (software rendering with anti-aliasing)
 *   - Cairo-EGL: Cairo with EGL/OpenGL (hardware accelerated)
 *
 * Cairo is a mandatory dependency.
 * CRITICAL: All backends use pure XCB - NO XLIB DEPENDENCIES.
 */

#ifndef _ISWRender_h
#define _ISWRender_h

#include <X11/Intrinsic.h>
#include <xcb/xcb.h>

/*
 * Relief types for 3D shadows (shared with ThreeD.h)
 */
#ifndef _XtRelief_defined
#define _XtRelief_defined
typedef enum {
    XtReliefNone,
    XtReliefRaised,
    XtReliefSunken,
    XtReliefRidge,
    XtReliefGroove
} XtRelief;
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
    ISW_RENDER_BACKEND_CAIRO_EGL      /* Cairo with EGL (NOT GLX!) */
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
 * ISWRenderStrokePolygon - Draw polygon outline
 *
 * Parameters:
 *   ctx        - Rendering context
 *   points     - Array of points
 *   num_points - Number of points
 */
void ISWRenderStrokePolygon(ISWRenderContext *ctx,
                           xcb_point_t *points,
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
                         xcb_point_t *points,
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
 *   font - XFontStruct pointer
 *
 * Notes:
 *   - Converts XFontStruct metrics to Cairo font sizing
 */
void ISWRenderSetFont(ISWRenderContext *ctx, XFontStruct *font);

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
 * ISWRenderDrawPixmap - Draw a pixmap onto the rendering surface
 *
 * Parameters:
 *   ctx          - Rendering context
 *   pixmap       - Source pixmap
 *   src_x, src_y - Source position within pixmap
 *   dst_x, dst_y - Destination position on surface
 *   width        - Region width
 *   height       - Region height
 *   depth        - Depth of the source pixmap
 *
 * Notes:
 *   - For depth == 1 (bitmaps): uses current foreground color for set bits,
 *     draws transparently (only foreground bits) in Cairo mode
 *   - For depth > 1: copies pixel values directly
 *   - Set foreground color with ISWRenderSetColor() before calling
 *     for depth == 1 bitmaps
 */
void ISWRenderDrawPixmap(ISWRenderContext *ctx,
                         xcb_pixmap_t pixmap,
                         int src_x, int src_y,
                         int dst_x, int dst_y,
                         unsigned int width, unsigned int height,
                         unsigned int depth);

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
 * ISWRenderGetCairoContext - Get direct Cairo context
 *
 * Parameters:
 *   ctx - Rendering context
 *
 * Returns: cairo_t* (cast from void*)
 *
 * Notes:
 *   - For advanced Cairo operations beyond the ISWRender API
 */
void* ISWRenderGetCairoContext(ISWRenderContext *ctx);

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
 * ISWScaledTextWidth - Measure text width using Cairo at the scaled font size.
 * Returns the width Cairo will actually use to render this text, ensuring
 * layout matches rendering on HiDPI displays.
 *
 * Parameters:
 *   widget - Widget for display/scale context
 *   font   - XFontStruct for base font metrics (may be NULL for default)
 *   text   - Text string to measure
 *   len    - Length of text
 *
 * Returns: Text width in pixels as Cairo would render it
 */
int ISWScaledTextWidth(Widget widget, XFontStruct *font, const char *text, int len);

/*
 * ISWScaledFontHeight - Get the font line height as Cairo would render it.
 */
int ISWScaledFontHeight(Widget widget, XFontStruct *font);

/*
 * ISWScaledFontAscent - Get the font ascent as Cairo would render it.
 */
int ISWScaledFontAscent(Widget widget, XFontStruct *font);

/*
 * ISWScaledFontCapHeight - Get the cap height (height of a capital letter)
 * as Cairo would render it.
 */
int ISWScaledFontCapHeight(Widget widget, XFontStruct *font);

/*
 * ISWRenderDrawBorder - Draw per-edge widget borders
 *
 * Draws up to four filled trapezoids (mitered at 45-degree corners)
 * for the top, right, bottom, and left edges of a widget. Edges with
 * width 0 are skipped. Each edge can have its own color.
 *
 * The border is drawn inside the widget's area, from the outer edge
 * inward. Caller must bracket with ISWRenderBegin/ISWRenderEnd.
 *
 * Parameters:
 *   ctx          - Rendering context (must be in a begin/end frame)
 *   width_top    - Top edge border width (0 to skip)
 *   width_right  - Right edge border width
 *   width_bottom - Bottom edge border width
 *   width_left   - Left edge border width
 *   color_top    - Top edge pixel color
 *   color_right  - Right edge pixel color
 *   color_bottom - Bottom edge pixel color
 *   color_left   - Left edge pixel color
 *   widget_width - Widget width in pixels
 *   widget_height - Widget height in pixels
 */
void ISWRenderDrawBorder(ISWRenderContext *ctx,
                         Dimension width_top, Dimension width_right,
                         Dimension width_bottom, Dimension width_left,
                         Pixel color_top, Pixel color_right,
                         Pixel color_bottom, Pixel color_left,
                         Dimension widget_width, Dimension widget_height);

#endif /* _ISWRender_h */
