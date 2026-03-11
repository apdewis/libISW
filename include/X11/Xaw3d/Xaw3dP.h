/*
 * Xaw3dP.h
 *
 * Global definitions and declarations. Not for public consumption.
 */

/*********************************************************************
Copyright (C) 1992 Kaleb Keithley
Copyright (C) 2000, 2003 David J. Hawkey Jr.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and
that both that copyright notice and this permission notice appear in
supporting documentation, and that the names of the copyright holders
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*********************************************************************/

#ifndef _Xaw3dP_h
#define _Xaw3dP_h

#include <X11/IntrinsicP.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>

/* Forward declare Region type - full definition in src/XawRegion.h */
/* We define it here to avoid include path issues */
#ifndef Region
typedef struct _XawRegion* Region;
#endif

/* XCB type compatibility */
#ifndef GC
typedef xcb_gcontext_t GC;
#endif

#ifndef XEvent
typedef xcb_generic_event_t XEvent;
#endif

/* Region is defined in XawRegion.h as a pointer to XawRegion struct */

#ifndef Screen
typedef xcb_screen_t Screen;
#endif

/* These are set during the build to reflect capability and options. */
/* I18n support */
/* XPM support */
/* gray stipples */
/* arrow scrollbars */

/* Min/Max utility macros */
#ifndef XawMin
#define XawMin(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef XawMax
#define XawMax(a, b) ((a) > (b) ? (a) : (b))
#endif
/* Also provide lowercase versions for compatibility */
#ifndef Min
#define Min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef Max
#define Max(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Assign-and-clamp macros */
#ifndef AssignMax
#define AssignMax(x, y) do { if ((y) > (x)) (x) = (y); } while (0)
#endif
#ifndef AssignMin
#define AssignMin(x, y) do { if ((y) < (x)) (x) = (y); } while (0)
#endif

#ifndef XtX
#define XtX(w)			(((RectObj)w)->rectangle.x)
#endif
#ifndef XtY
#define XtY(w)			(((RectObj)w)->rectangle.y)
#endif
#ifndef XtWidth
#define XtWidth(w)		(((RectObj)w)->rectangle.width)
#endif
#ifndef XtHeight
#define XtHeight(w)		(((RectObj)w)->rectangle.height)
#endif
#ifndef XtBorderWidth
#define XtBorderWidth(w)	(((RectObj)w)->rectangle.border_width)
#endif

/* Text justification types (missing from XCB-based libXt) */
#ifndef _XawXtJustify_defined
#define _XawXtJustify_defined
typedef enum {
    XtJustifyLeft,
    XtJustifyCenter,
    XtJustifyRight
} XtJustify;
#endif

/* Widget edge types for Form layout are defined in Form.h (XawEdgeType) */
/* XawXcbDraw.h provides XtEdgeType for converters */

/* Orientation type for Box, Paned, Scrollbar (missing from XCB-based libXt) */
#ifndef _XawXtOrientation_defined
#define _XawXtOrientation_defined
typedef enum {
    XtorientHorizontal = 0,
    XtorientVertical = 1
} XtOrientation;
#endif

/* Widget gravity type for Tree layout (missing from XCB-based libXt) */
#ifndef _XawXtGravity_defined
#define _XawXtGravity_defined
typedef unsigned int XtGravity;
#endif

#ifdef XAW_GRAY_BLKWHT_STIPPLES
extern unsigned long
grayPixel(
    unsigned long,
    xcb_connection_t *,  /* XCB connection (was Display*) */
    xcb_screen_t *
);
#else
#define grayPixel(p, dpy, scn)	(p)
#endif

#ifdef XAW_MULTIPLANE_PIXMAPS
extern Pixmap
stipplePixmap(
    Widget,
    Pixmap,
    Colormap,
    Pixel,
    unsigned int
);
#endif

#endif	/* _Xaw3dP_h */
