/*
 * IswDragDropP.h - Platform-neutral drag-and-drop core (private)
 *
 * Copyright (c) 2026 ISW Project
 *
 * The generic half of the DnD engine: drag/drop policy and state that carries
 * NO platform (xcb) type.  Type identities cross as neutral `Atom` ids (the
 * backend's native atom values, compared opaquely here — never interpreted as X
 * atoms).  A platform DnD backend (ISWPlatformDndXCB.c) embeds an IswDndCore in
 * its per-shell state, fills the protocol-atom id fields from its interned
 * atoms, and drives the wire protocol; the policy functions below operate on the
 * core alone.
 */

#ifndef _IswDragDropP_h
#define _IswDragDropP_h

#include <ISW/Intrinsic.h>
#include <ISW/IswDragDrop.h>

/* Per-widget drop registration (neutral): accepted types as Atom ids, accepted
   actions, and direct enter/motion/leave/drop callbacks for widget classes that
   do not declare the DnD callback resources. */
typedef struct _DropConfig {
    Widget              widget;
    Atom               *accepted_types;
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
   drag/drop policy state with no native handle.  The action/MIME id fields are
   neutral Atom ids the backend fills from its interned protocol atoms. */
typedef struct _IswDndCore {
    Widget          shell;
    DropConfig     *drop_configs;

    /* Negotiation id vocabulary (filled by the backend from interned atoms). */
    Atom            action_copy;
    Atom            action_move;
    Atom            action_link;
    Atom            action_ask;
    Atom            action_private;
    Atom            text_uri_list;
    Atom            text_plain;
    Atom            targets_atom;

    /* --- Drop target state (we are the drop target) --- */
    Atom           *src_types;          /* types offered by source (Atom ids)   */
    int             src_num_types;
    IswDndAction    src_actions;        /* actions offered by source            */
    int             src_version;        /* source protocol version              */
    int             drop_x, drop_y;     /* last position (root coords)          */
    Widget          hover_widget;       /* widget currently under cursor        */
    Atom            negotiated_type;    /* type accepted for current drop       */
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

/* Map a negotiation action id <-> the IswDndAction enum, using the core's
   interned action ids. */
IswDndAction _IswDndAtomToAction(const IswDndCore *core, Atom atom);
Atom         _IswDndActionToAtom(const IswDndCore *core, IswDndAction action);

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
                                  Atom *type_out, IswDndAction *action_out);

/* Recursive widget-geometry hit-test: the deepest realized, managed child at
   widget-local (wx,wy) that is a registered drop target.  NULL if none. */
Widget       _IswDndFindDropChild(IswDndCore *core, Widget composite,
                                  int wx, int wy);

/* Parse an RFC-2483 text/uri-list payload into a NULL-terminated string array
   (file:// scheme stripped).  Caller frees each entry and the array. */
char       **_IswDndParseUriList(const char *data, int len, int *out_count);

#endif /* _IswDragDropP_h */
