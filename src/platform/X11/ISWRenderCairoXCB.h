#ifndef _ISWRenderPrivate_h
#define _ISWRenderPrivate_h

#include "ISWRenderOps.h"     /* neutral dispatcher structs (no xcb/cairo) */
#include <xcb/xcb.h>

/* Cairo is a mandatory dependency */
#include <cairo.h>
#include <cairo-xcb.h>



extern const ISWRenderOps isw_render_cairo_xcb_ops;
extern const IswSurfaceOps isw_surface_cairo_xcb_ops;

/* Find the XCB visual for a depth on a screen (backend-internal). */
xcb_visualtype_t *_IswXcbFindVisual(xcb_screen_t *screen, uint8_t depth);

/* Widget-keyed font measurement (cairo/FreeType/fontconfig), backend-private.
   Published on the platform render ops; the neutral ISWScaled* wrappers
   forward to these. */
int cairo_xcb_scaled_text_width(Widget widget, IswFontStruct *font,
                                const char *text, int len);
int cairo_xcb_scaled_font_height(Widget widget, IswFontStruct *font);
int cairo_xcb_scaled_font_ascent(Widget widget, IswFontStruct *font);
int cairo_xcb_scaled_font_cap_height(Widget widget, IswFontStruct *font);

/*
 * Present source accessor — the inputs a platform present_root needs to blit a
 * finished composite surface to its window, without reaching into the private
 * struct _IswSurface.  Fills the out params from `surface`'s back buffer and
 * (for the Present path) bumps the present serial.  Returns False if the
 * surface has no back buffer yet (nothing to present).
 *
 *   back_cairo  — the back buffer as a cairo surface (the blit source).
 *   window_cr   — the surface's cached cairo context on the window-target
 *                 surface (the cairo blit destination); NULL if unavailable.
 *   back_pixmap — the server pixmap for the Present path, or 0 when Present is
 *                 unusable / the back buffer is a client image (use the cairo
 *                 path instead).
 *   present_serial — next Present serial (only meaningful when back_pixmap != 0).
 *   copy_pixmap — the back buffer's server pixmap for a straight xcb_copy_area
 *                 blit to the window (any time the back buffer is a server pixmap,
 *                 independent of Present); 0 for client image surfaces.
 *   copy_w / copy_h — physical-pixel extent to copy (device scale folded in).
 *
 * Defined in ISWRenderCairoXCB.c.
 */
Boolean _ISWRenderSurfacePresentSource(IswSurface surface,
                                       cairo_surface_t **back_cairo,
                                       void **window_cr,
                                       xcb_pixmap_t *back_pixmap,
                                       uint32_t *present_serial,
                                       xcb_pixmap_t *copy_pixmap,
                                       unsigned int *copy_w,
                                       unsigned int *copy_h);

/*
 * Configure a cairo context with the TTF face+size from an IswFontStruct.
 * Defined in the cairo-XCB backend; used by its own text draw path.
 */
void _ISWSetCairoFontFromXFont(cairo_t *cr, IswFontStruct *font, double scale);

#endif /* _ISWRenderPrivate_h */
