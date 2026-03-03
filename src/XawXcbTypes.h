/*
 * XawXcbTypes.h - XCB type compatibility layer for Xaw3d
 * 
 * Provides helper definitions and macros for XCB compatibility.
 * This uses standard X11 types from Xlib/Xt headers, adding only
 * XCB-specific helpers and compatibility macros.
 *
 * Copyright (c) 2026 Xaw3d Project
 */

#ifndef _XawXcbTypes_h
#define _XawXcbTypes_h

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <stdint.h>

/*
 * XCB type aliases for compatibility
 * The custom libXt provides xcb_connection_t*, xcb_window_t, etc.
 * We add GC as an alias for xcb_gcontext_t for widget code.
 */
typedef xcb_gcontext_t GC;

/*
 * Event-related helper macros (using standard X11 types)
 * These maintain compatibility while providing cleaner access patterns
 */

/*
 * Font metrics compatibility
 * Standard XFontStruct from Xlib headers is used.
 */

/*
 * Xt types - these should come from Xt headers
 * Only define if missing
 */

/* XtOrientation - horizontal/vertical orientation for widgets */
#ifndef XtOrientation
typedef unsigned char XtOrientation;
#define XtorientHorizontal 0
#define XtorientVertical 1
#endif

/* XtJustify is already defined in Xaw3d LabelP.h, so don't redefine it */

/*
 * Region type - Phase 4: Custom Region implementation for XCB compatibility
 * Full implementation in XawRegion.h and XawRegion.c
 */
#include "XawRegion.h"

#endif /* _XawXcbTypes_h */
