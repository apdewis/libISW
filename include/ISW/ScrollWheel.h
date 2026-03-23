/*
 * ScrollWheel.h - Scroll wheel support for ISW widgets
 *
 * Intercepts scroll wheel events (X11 buttons 4-7) and routes them
 * to the appropriate scrollbar by walking up the widget tree from
 * the pointer position to find the nearest scrollable ancestor
 * (Viewport, Text, or standalone Scrollbar).
 *
 * The scroll increment is controlled by the scrollWheelIncrement
 * resource on individual Scrollbar widgets (default 50 pixels).
 *
 * Shift+scroll switches to horizontal scrolling.
 * Buttons 6/7 (horizontal wheel) always scroll horizontally.
 */

#ifndef _ISW_ScrollWheel_h
#define _ISW_ScrollWheel_h

#include <X11/Xfuncproto.h>
#include <xcb/xcb.h>

/* Resource names for scroll wheel configuration */
#define XtNscrollWheelIncrement "scrollWheelIncrement"
#define XtCScrollWheelIncrement "ScrollWheelIncrement"

/* Default scroll increment in pixels per wheel notch */
#define ISW_SCROLL_WHEEL_DEFAULT_INCREMENT 50

_XFUNCPROTOBEGIN

/*
 * Install the scroll wheel event dispatcher. Call once per connection.
 * Safe to call multiple times; subsequent calls are no-ops.
 */
extern void ISWScrollWheelInit(xcb_connection_t *conn);

_XFUNCPROTOEND

#endif /* _ISW_ScrollWheel_h */
