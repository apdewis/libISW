/*
 * IswContext.h - Context management for XCB-based Isw3d
 * 
 * Provides XContext functionality for XCB, replacing Xlib's XSaveContext/
 * XFindContext/XDeleteContext which are not available in XCB.
 *
 * Copyright (c) 2026 Isw3d Project
 */

#ifndef _ISW_IswContext_h
#define _ISW_IswContext_h

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
extern XContext IswUniqueContext(
    void
);

/* Save data associated with a window/ID and context */
extern int IswSaveContext(
    xcb_connection_t*     /* display (unused, for API compatibility) */,
    XID       /* window or resource ID */,
    XContext     /* context identifier */,
    void*        /* data pointer */
);

/* Retrieve data associated with a window/ID and context */
extern int IswFindContext(
    xcb_connection_t*     /* display (unused, for API compatibility) */,
    XID       /* window or resource ID */,
    XContext     /* context identifier */,
    void**       /* data_return */
);

/* Delete context association */
extern int IswDeleteContext(
    xcb_connection_t*     /* display (unused, for API compatibility) */,
    XID       /* window or resource ID */,
    XContext     /* context identifier */
);

_XFUNCPROTOEND

#endif /* _ISW_IswContext_h */
