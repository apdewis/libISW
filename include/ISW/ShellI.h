#ifndef _IswShellInternal_h
#define _IswShellInternal_h

#include <ISW/IswFuncproto.h>

_XFUNCPROTOBEGIN

extern void _IswShellGetCoordinates(Widget widget, Position *x, Position *y);
extern void _IswShellUpdateUserTime(xcb_connection_t *dpy, xcb_window_t event_window, xcb_timestamp_t time);

#endif /* _IswShellInternal_h */
