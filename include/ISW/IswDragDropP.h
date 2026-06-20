/*
 * IswDragDropP.h - Platform-neutral drag-and-drop core (private)
 *
 * Copyright (c) 2026 ISW Project
 *
 * The generic half of the DnD engine: drag/drop policy and state that carries
 * NO platform (xcb) type.  MIME types are plain C strings; the platform DnD
 * backend (ISWPlatformDndXCB.c) translates to/from wire atoms internally.
 * A platform DnD backend embeds an IswDndCore in its per-shell state and
 * drives the wire protocol; the policy functions below operate on the core
 * alone.
 */

#ifndef _IswDragDropP_h
#define _IswDragDropP_h

#include <ISW/Intrinsic.h>
#include <ISW/IswDragDrop.h>

/* Per-widget drop registration: accepted types as MIME strings, accepted
   actions, and direct enter/motion/leave/drop callbacks for widget classes that
   do not declare the DnD callback resources. */
typedef struct _DropConfig {
    Widget              widget;
    const char        **accepted_types;
    int                 num_accepted_types;
    IswDndAction        accepted_actions;
    IswCallbackProc     drop_proc;
    IswPointer          drop_closure;
    IswCallbackProc     motion_proc;
    IswPointer          motion_closure;
    IswCallbackProc     leave_proc;
    IswPointer          leave_closure;
    struct _DropConfig *next;
} DropConfig;

/* Neutral DnD state, embedded in the backend's per-shell record.  Holds all
   drag/drop policy state with no native handle.  MIME types are strings; the
   backend translates to/from wire atoms internally. */
typedef struct _IswDndCore {
    Widget          shell;
    DropConfig     *drop_configs;

    /* --- Drop target state (we are the drop target) --- */
    const char    **src_types;          /* MIME type strings offered by source  */
    int             src_num_types;
    IswDndAction    src_actions;        /* actions offered by source            */
    int             src_version;        /* source protocol version              */
    int             drop_x, drop_y;     /* last position (root coords)          */
    Widget          hover_widget;       /* widget currently under cursor        */
    const char     *negotiated_type;    /* MIME type accepted for current drop  */
    IswDndAction    negotiated_action;  /* action accepted for current drop     */

    /* --- Drag source state (we are the drag source) --- */
    Boolean             dragging;
    IswDragSourceDesc   drag_desc;
    Widget              drag_source;
    int                 drag_start_x;   /* root coords of initial press         */
    int                 drag_start_y;
    int                 drag_press_x;   /* widget-local press coords            */
    int                 drag_press_y;
    Boolean             drag_started;   /* past threshold?                      */

    /* Target tracking during drag */
    int                 drag_target_ver;     /* target's protocol version       */
    Boolean             drag_status_pending; /* waiting for status              */
    Boolean             drag_target_accepted;
    IswDndAction        drag_target_action;
    int                 drag_last_x;         /* last sent position              */
    int                 drag_last_y;
    Boolean             drag_position_deferred; /* moved while status pending   */

    IswIntervalId       finished_timer;
} IswDndCore;

/* ---- Neutral policy functions (ISWDragDrop.c) ---------------------------- */

/* Keyboard modifier state (neutral IswMod* bits) -> proposed action. */
IswDndAction _IswDndModifiersToAction(unsigned int modifiers);

/* Per-widget drop-config list management. */
DropConfig  *_IswDndFindConfig(IswDndCore *core, Widget w);
DropConfig  *_IswDndGetOrCreateConfig(IswDndCore *core, Widget w);

/* Negotiate the best matching type+action between the source's offered types
   (core->src_types/src_actions) and `target`'s registered config (or, with no
   config, accept the first offered type and copy/move/link).  Returns False if
   no type or action is common. */
Boolean      _IswDndNegotiateType(IswDndCore *core, Widget target,
                                  const char **type_out, IswDndAction *action_out);

/* Recursive widget-geometry hit-test: the deepest realized, managed child at
   widget-local (wx,wy) that is a registered drop target.  NULL if none. */
Widget       _IswDndFindDropChild(IswDndCore *core, Widget composite,
                                  int wx, int wy);

/* Parse an RFC-2483 text/uri-list payload into a NULL-terminated string array
   (file:// scheme stripped).  Caller frees each entry and the array. */
char       **_IswDndParseUriList(const char *data, int len, int *out_count);

#endif /* _IswDragDropP_h */
