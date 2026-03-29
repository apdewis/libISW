/*
 * ISWXdnd.h - XDND (drag-and-drop) protocol support for ISW
 *
 * Implements XDND version 5 with full drag source and drop target support.
 * Widgets initiate drags via ISWXdndStartDrag and accept drops via
 * XtNdropCallback. Enter/leave/motion callbacks allow visual feedback
 * during drag-over.
 *
 * Supports multiple MIME types, action negotiation (copy/move/link),
 * and interoperates with any XDND v5 application (GTK, Qt, etc.).
 *
 * Pure XCB — no Xlib dependencies.
 */

#ifndef _ISWXdnd_h
#define _ISWXdnd_h

#include <X11/Intrinsic.h>

/* ------------------------------------------------------------------ */
/* Callback resource names                                            */
/* ------------------------------------------------------------------ */

#define XtNdropCallback         "dropCallback"
#define XtCDropCallback         "DropCallback"
#define XtNdragEnterCallback    "dragEnterCallback"
#define XtCDragEnterCallback    "DragEnterCallback"
#define XtNdragMotionCallback   "dragMotionCallback"
#define XtCDragMotionCallback   "DragMotionCallback"
#define XtNdragLeaveCallback    "dragLeaveCallback"
#define XtCDragLeaveCallback    "DragLeaveCallback"

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
 * The library takes ownership of *data_return (will XtFree it).
 */
typedef Boolean (*IswDragConvertProc)(
    Widget          widget,
    xcb_atom_t      target_type,
    XtPointer      *data_return,
    unsigned long  *length_return,
    int            *format_return,
    XtPointer       client_data
);

/*
 * IswDragFinishedProc - Called when the drag operation completes.
 */
typedef void (*IswDragFinishedProc)(
    Widget          widget,
    IswDndAction    performed_action,
    Boolean         accepted,
    XtPointer       client_data
);

/*
 * IswDragSourceDesc - Configuration for initiating a drag.
 */
typedef struct {
    xcb_atom_t         *types;          /* offered MIME type atoms */
    int                 num_types;
    IswDndAction        actions;        /* bitmask of offered actions */
    IswDragConvertProc  convert;        /* data provider */
    IswDragFinishedProc finished;       /* completion notification */
    XtPointer           client_data;
    /* Optional drag icon (0 for default cursor-only feedback) */
    xcb_pixmap_t        icon_pixmap;
    int                 icon_width;
    int                 icon_height;
    int                 icon_hotspot_x;
    int                 icon_hotspot_y;
} IswDragSourceDesc;

/* ------------------------------------------------------------------ */
/* Drop target callback data                                          */
/* ------------------------------------------------------------------ */

/*
 * IswDropCallbackData - Passed to XtNdropCallback.
 *
 * Legacy fields (uris, num_uris) are populated when the data type is
 * text/uri-list for backward compatibility. For other types, use the
 * data/data_length/data_type fields.
 */
typedef struct {
    /* Legacy fields — kept at original offsets for ABI compat */
    char          **uris;               /* NULL-terminated URI array (text/uri-list only) */
    int             num_uris;           /* number of URIs (0 if not uri-list) */
    int             x, y;               /* drop position relative to widget */

    /* Extended fields */
    XtPointer       data;               /* raw data from source */
    unsigned long   data_length;        /* data length in bytes */
    xcb_atom_t      data_type;          /* MIME type atom of the data */
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
    xcb_atom_t     *offered_types;      /* types the source offers */
    int             num_offered_types;
    IswDndAction    offered_actions;    /* actions the source supports */
    IswDndAction    proposed_action;    /* action proposed for this position */

    /* Set by callback to accept/reject */
    xcb_atom_t      accepted_type;      /* set nonzero to accept */
    IswDndAction    accepted_action;    /* set nonzero to accept */
} IswDragOverCallbackData;

/* ------------------------------------------------------------------ */
/* Public functions                                                   */
/* ------------------------------------------------------------------ */

/*
 * ISWXdndEnable - Enable XDND on a toplevel shell window.
 * Called automatically during shell realization. Sets the XdndAware
 * property and installs protocol event handlers.
 */
void ISWXdndEnable(Widget shell);

/*
 * ISWXdndWidgetAcceptDrops - Register a widget as a drop target.
 * The widget should have an XtNdropCallback. Optionally also register
 * dragEnter/dragMotion/dragLeave callbacks for visual feedback.
 */
void ISWXdndWidgetAcceptDrops(Widget w);

/*
 * ISWXdndStartDrag - Initiate a drag from a widget.
 *
 * Call from a button press action proc. The library grabs the pointer,
 * tracks motion, and handles the full XDND protocol exchange. The
 * drag runs asynchronously within the Xt event loop — this function
 * returns immediately.
 *
 * The trigger_event is the button press that initiated the drag; its
 * timestamp is used for selection ownership and the XDND protocol.
 */
void ISWXdndStartDrag(
    Widget                      source_widget,
    xcb_button_press_event_t   *trigger_event,
    IswDragSourceDesc          *desc
);

/*
 * ISWXdndSetAcceptedTypes - Filter which MIME types a drop target accepts.
 * Pass NULL/0 to accept any type offered. The types array is copied.
 */
void ISWXdndSetAcceptedTypes(
    Widget          w,
    xcb_atom_t     *types,
    int             num_types
);

/*
 * ISWXdndSetAcceptedActions - Filter which actions a drop target accepts.
 * Pass 0 to accept any action offered.
 */
void ISWXdndSetAcceptedActions(
    Widget          w,
    IswDndAction    actions
);

/*
 * ISWXdndInternType - Convenience: intern a MIME type string as an atom.
 */
xcb_atom_t ISWXdndInternType(Widget w, const char *mime_type);

#endif /* _ISWXdnd_h */
