/*
 * ISWXdnd.h - XDND (drag-and-drop) protocol support for ISW
 *
 * Implements XDND version 5 drop target support. Widgets that want
 * to accept drops register an XtNdropCallback. The callback receives
 * an IswDropCallbackData with the list of dropped file URIs.
 *
 * Pure XCB — no Xlib dependencies.
 */

#ifndef _ISWXdnd_h
#define _ISWXdnd_h

#include <X11/Intrinsic.h>

/* Callback resource name for widgets accepting drops */
#define XtNdropCallback "dropCallback"
#define XtCDropCallback "DropCallback"

/* Callback data passed to XtNdropCallback */
typedef struct {
    char  **uris;        /* NULL-terminated array of URI strings */
    int     num_uris;    /* number of URIs */
    int     x, y;        /* drop position relative to widget */
} IswDropCallbackData;

/*
 * ISWXdndEnable - Enable XDND drop support on a toplevel shell window.
 * Call after the shell is realized. Sets the XdndAware property and
 * installs the event handler for XDND client messages.
 */
void ISWXdndEnable(Widget shell);

/*
 * ISWXdndWidgetAcceptDrops - Mark a widget as willing to accept drops.
 * The widget must have an XtNdropCallback resource. When a drop lands
 * on this widget, the callback is fired with IswDropCallbackData.
 */
void ISWXdndWidgetAcceptDrops(Widget w);

#endif /* _ISWXdnd_h */
