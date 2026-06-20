/*
 * IswDragDrop.h - generic drag-and-drop service for ISW
 *
 * A transport-neutral drag-and-drop API. Widgets initiate drags via
 * IswDndStartDrag and accept drops via IswNdropCallback. Enter/leave/motion
 * callbacks allow visual feedback during drag-over.
 *
 * Supports multiple MIME types, action negotiation (copy/move/link), and
 * interoperates with the host platform's native drag-and-drop. The service
 * names nothing platform-specific: the active platform backend (on X11, the
 * XDND v5 implementation in ISWPlatformDndXCB.c) supplies the wire protocol
 * behind the IswPlatformDndOps vtable.
 */

#ifndef _IswDragDrop_h
#define _IswDragDrop_h

#include <ISW/Intrinsic.h>
#include <ISW/IswEvent.h>

/* ------------------------------------------------------------------ */
/* Callback resource names                                            */
/* ------------------------------------------------------------------ */

#define IswNdropCallback         "dropCallback"
#define IswCDropCallback         "DropCallback"
#define IswNdragEnterCallback    "dragEnterCallback"
#define IswCDragEnterCallback    "DragEnterCallback"
#define IswNdragMotionCallback   "dragMotionCallback"
#define IswCDragMotionCallback   "DragMotionCallback"
#define IswNdragLeaveCallback    "dragLeaveCallback"
#define IswCDragLeaveCallback    "DragLeaveCallback"

/* ------------------------------------------------------------------ */
/* Action flags                                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    ISW_DND_ACTION_NONE    = 0,
    ISW_DND_ACTION_COPY    = (1 << 0),
    ISW_DND_ACTION_MOVE    = (1 << 1),
    ISW_DND_ACTION_LINK    = (1 << 2),
    ISW_DND_ACTION_ASK     = (1 << 3),
    ISW_DND_ACTION_PRIVATE = (1 << 4)
} IswDndAction;

/* ------------------------------------------------------------------ */
/* Drag source types                                                  */
/* ------------------------------------------------------------------ */

/*
 * IswDragConvertProc - Called when the drop target requests data.
 *
 * The source widget provides data in the requested MIME type.
 * Return True if the type is supported and data was provided.
 * The library takes ownership of *data_return (will IswFree it).
 */
typedef Boolean (*IswDragConvertProc)(
    Widget          widget,
    const char     *target_type,
    IswPointer      *data_return,
    unsigned long  *length_return,
    int            *format_return,
    IswPointer       client_data
);

/*
 * IswDragFinishedProc - Called when the drag operation completes.
 */
typedef void (*IswDragFinishedProc)(
    Widget          widget,
    IswDndAction    performed_action,
    Boolean         accepted,
    IswPointer       client_data
);

/*
 * IswDragSourceDesc - Configuration for initiating a drag.
 */
typedef struct {
    const char        **types;          /* offered MIME type strings */
    int                 num_types;
    IswDndAction        actions;        /* bitmask of offered actions */
    IswDragConvertProc  convert;        /* data provider */
    IswDragFinishedProc finished;       /* completion notification */
    IswPointer           client_data;
    /* Optional drag icon (0 for default cursor-only feedback) */
    IswPixmap           icon_pixmap;
    int                 icon_width;
    int                 icon_height;
    int                 icon_hotspot_x;
    int                 icon_hotspot_y;
} IswDragSourceDesc;

/* ------------------------------------------------------------------ */
/* Drop target callback data                                          */
/* ------------------------------------------------------------------ */

/*
 * IswDropCallbackData - Passed to IswNdropCallback.
 *
 * Legacy fields (uris, num_uris) are populated when the data type is
 * text/uri-list for backward compatibility. For other types, use the
 * data/data_length/data_type fields.
 */
typedef struct {
    char          **uris;               /* NULL-terminated URI array (text/uri-list only) */
    int             num_uris;           /* number of URIs (0 if not uri-list) */
    int             x, y;               /* drop position relative to widget */

    IswPointer       data;               /* raw data from source */
    unsigned long   data_length;        /* data length in bytes */
    const char     *data_type;          /* MIME type string */
    int             data_format;        /* 8, 16, or 32 */
    IswDndAction    action;             /* negotiated action */
} IswDropCallbackData;

/*
 * IswDragOverCallbackData - Passed to dragEnter, dragMotion, dragLeave.
 *
 * For dragEnter and dragMotion, set accepted_type and accepted_action
 * to indicate willingness to accept the drop. Leave them as NONE/0 to
 * reject (or let the library match against registered accepted types).
 */
typedef struct {
    int             x, y;               /* position relative to widget */
    const char    **offered_types;      /* MIME type strings the source offers */
    int             num_offered_types;
    IswDndAction    offered_actions;    /* actions the source supports */
    IswDndAction    proposed_action;    /* action proposed for this position */

    /* Set by callback to accept/reject */
    const char     *accepted_type;      /* set non-NULL to accept */
    IswDndAction    accepted_action;    /* set nonzero to accept */
} IswDragOverCallbackData;

/* ------------------------------------------------------------------ */
/* Public functions                                                   */
/* ------------------------------------------------------------------ */

/*
 * IswDndEnable - Enable drag-and-drop on a toplevel shell window.
 * Called automatically during shell realization. Advertises the window
 * as a drop target and installs the platform's protocol event handlers.
 */
void IswDndEnable(Widget shell);

/*
 * IswDndWidgetAcceptDrops - Register a widget as a drop target.
 * The widget should have an IswNdropCallback. Optionally also register
 * dragEnter/dragMotion/dragLeave callbacks for visual feedback.
 */
void IswDndWidgetAcceptDrops(Widget w);

/*
 * IswDndStartDrag - Initiate a drag from a widget.
 *
 * Call from a button press action proc. The library grabs the pointer,
 * tracks motion, and drives the full platform drag-and-drop exchange.
 * The drag runs asynchronously within the Xt event loop — this function
 * returns immediately.
 *
 * The trigger_event is the button press that initiated the drag; its
 * timestamp is used for selection ownership and the protocol exchange.
 */
void IswDndStartDrag(
    Widget                      source_widget,
    IswEvent                   *trigger_event,
    IswDragSourceDesc          *desc
);

/*
 * IswDndSetAcceptedTypes - Filter which MIME types a drop target accepts.
 * Pass NULL/0 to accept any type offered. The types array is copied.
 */
void IswDndSetAcceptedTypes(
    Widget          w,
    const char    **types,
    int             num_types
);

/*
 * IswDndSetAcceptedActions - Filter which actions a drop target accepts.
 * Pass 0 to accept any action offered.
 */
void IswDndSetAcceptedActions(
    Widget          w,
    IswDndAction    actions
);

/*
 * IswDndSetDropCallback - Set a direct drop callback on a widget.
 *
 * Use this instead of IswAddCallback(w, IswNdropCallback, ...) when
 * the widget's class doesn't declare IswNdropCallback as a resource
 * (i.e. any widget not inheriting from Simple).  The callback
 * receives IswDropCallbackData* as call_data, same as IswNdropCallback.
 */
void IswDndSetDropCallback(
    Widget          w,
    IswCallbackProc  proc,
    IswPointer       closure
);

/*
 * IswDndSetDragMotionCallback / IswDndSetDragLeaveCallback
 *
 * Direct motion/leave callbacks for widgets whose class doesn't declare
 * IswNdragMotionCallback / IswNdragLeaveCallback as resources.
 * Same pattern as IswDndSetDropCallback.
 */
void IswDndSetDragMotionCallback(
    Widget          w,
    IswCallbackProc  proc,
    IswPointer       closure
);
void IswDndSetDragLeaveCallback(
    Widget          w,
    IswCallbackProc  proc,
    IswPointer       closure
);

/*
 * IswDndIsDragging - Return True if a drag operation is active.
 * The widget can be any widget in the shell's tree.
 */
Boolean IswDndIsDragging(Widget w);

#endif /* _IswDragDrop_h */
