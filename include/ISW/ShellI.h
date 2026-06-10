#ifndef _IswShellInternal_h
#define _IswShellInternal_h

#include <ISW/IswFuncproto.h>

_XFUNCPROTOBEGIN

extern void _IswShellGetCoordinates(Widget widget, Position *x, Position *y);
extern void _IswShellUpdateUserTime(IswDisplay dpy, xcb_window_t event_window, IswTime time);

#endif /* _IswShellInternal_h */
