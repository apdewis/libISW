/*
 * ISWXdnd.h - backward-compatibility shim.
 *
 * The drag-and-drop API is now the transport-neutral IswDragDrop service
 * (include/ISW/IswDragDrop.h); on X11 it is implemented by the XDND v5
 * backend in src/ISWPlatformDndXCB.c.  This header keeps the historical
 * ISWXdnd* names compiling by aliasing them to the IswDnd* service.  New
 * code should include <ISW/IswDragDrop.h> and use the IswDnd* names.
 */

#ifndef _ISWXdnd_h
#define _ISWXdnd_h

#include <ISW/IswDragDrop.h>
#include <xcb/xcb.h>

#define ISWXdndEnable               IswDndEnable
#define ISWXdndWidgetAcceptDrops    IswDndWidgetAcceptDrops
#define ISWXdndSetAcceptedTypes     IswDndSetAcceptedTypes
#define ISWXdndSetAcceptedActions   IswDndSetAcceptedActions
#define ISWXdndSetDropCallback      IswDndSetDropCallback
#define ISWXdndSetDragMotionCallback IswDndSetDragMotionCallback
#define ISWXdndSetDragLeaveCallback  IswDndSetDragLeaveCallback
#define ISWXdndInternType           IswDndInternType
#define ISWXdndIsDragging           IswDndIsDragging

/*
 * ISWXdndStartDrag kept its original native-event signature for source
 * compatibility (the neutral service takes IswEvent *).  Thin wrapper in
 * the X11 backend bridges the native button event to the neutral path.
 */
void ISWXdndStartDrag(
    Widget                      source_widget,
    xcb_button_press_event_t   *trigger_event,
    IswDragSourceDesc          *desc
);

#endif /* _ISWXdnd_h */
