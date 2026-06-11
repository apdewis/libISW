/*
 * ISWContextI.h - X11-backend-private resource-id → data context table
 *
 * Copyright (c) 2026 ISW Project
 *
 * XCB replacement for Xlib's XSaveContext/XFindContext/XDeleteContext: a
 * generic {resource-id, context} → data side table.  This is NOT toolkit
 * infrastructure — its only users are the X11 selection-transfer bookkeeping
 * (Selection.c, state attached to foreign requestor windows/atoms the toolkit
 * does not own) and the X11 XDND backend (ISWPlatformDndXCB.c).  Both are
 * X11-protocol concerns, so this header lives in the X11 backend dir and is
 * never exposed under include/ISW/.  Implemented in ISWPlatformGrabCursorXCB.c
 * beside the selection ops.
 *
 * The IswDisplay parameter is carried only for Xlib API shape; the table is
 * keyed purely on {id, context}.
 */

#ifndef _ISWContextI_h
#define _ISWContextI_h

#include <ISW/Intrinsic.h>      /* IswDisplay, XID, XContext, IswPointer */

_XFUNCPROTOBEGIN

/* Generate a unique context identifier (never 0). */
extern XContext IswUniqueContext(void);

/* Associate data with {id, context}.  Returns 0 on success. */
extern int IswSaveContext(IswDisplay dpy, XID id, XContext context, IswPointer data);

/* Retrieve data for {id, context}.  Returns 0 if found. */
extern int IswFindContext(IswDisplay dpy, XID id, XContext context,
                          IswPointer *data_return);

/* Remove the association for {id, context}.  Returns 0 if deleted. */
extern int IswDeleteContext(IswDisplay dpy, XID id, XContext context);

_XFUNCPROTOEND

#endif /* _ISWContextI_h */
