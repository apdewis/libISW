/*
 * XawContext.h - Context management for XCB-based Xaw3d
 * 
 * Provides XContext functionality for XCB, replacing Xlib's XSaveContext/
 * XFindContext/XDeleteContext which are not available in XCB.
 *
 * Copyright (c) 2026 Xaw3d Project
 */

#ifndef _XawContext_h
#define _XawContext_h

#include <X11/Intrinsic.h>
#include <xcb/xcb.h>

_XFUNCPROTOBEGIN

/*
 * Context management functions
 * 
 * These replace the Xlib context functions (XSaveContext, XFindContext,
 * XDeleteContext, XUniqueContext) with hash table-based implementations.
 */

/* Generate a unique context identifier */
extern XContext XawUniqueContext(
    void
);

/* Save data associated with a window/ID and context */
extern int XawSaveContext(
    xcb_connection_t*     /* display (unused, for API compatibility) */,
    XID       /* window or resource ID */,
    XContext     /* context identifier */,
    void*        /* data pointer */
);

/* Retrieve data associated with a window/ID and context */
extern int XawFindContext(
    xcb_connection_t*     /* display (unused, for API compatibility) */,
    XID       /* window or resource ID */,
    XContext     /* context identifier */,
    void**       /* data_return */
);

/* Delete context association */
extern int XawDeleteContext(
    xcb_connection_t*     /* display (unused, for API compatibility) */,
    XID       /* window or resource ID */,
    XContext     /* context identifier */
);

_XFUNCPROTOEND

#endif /* _XawContext_h */
