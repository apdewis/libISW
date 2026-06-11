/*
 * IswEvent.h - Platform-neutral event representation for ISW
 *
 * Copyright (c) 2026 ISW Project
 *
 * IswEvent is the toolkit's own event type, carried through the public API
 * (action procs, event handlers, dispatch) in place of xcb_generic_event_t.
 *
 * It contains ONLY events the toolkit core and widgets consume semantically:
 * keyboard input, pointer input, crossing, focus, redraw, geometry, the
 * handful of structure transitions widgets observe, and a window-close
 * request.  Everything that is X11 *protocol* — selections/clipboard,
 * property transfers, client messages, drag-and-drop, tray docking, keyboard
 * mapping, atoms, raw keysyms, raw keycodes — lives ENTIRELY inside the
 * platform backend and is never surfaced to the toolkit as an event.  Where
 * the toolkit needs such a capability it calls a platform SERVICE (e.g. the
 * clipboard / drag-drop APIs), it does not decode protocol events.
 *
 * A platform backend translates its native events into this union and routes
 * its own protocol events internally without ever constructing an IswEvent.
 *
 * Neutral vocabulary, driven by what the toolkit actually branches on:
 *  - Keyboard: a resolved key identity (IswKey) + the UTF-8 text produced.
 *    The toolkit never sees platform keycodes or X keysyms.
 *  - Modifiers: IswModMask, neutral names ("Ctrl<Key>a" matches against these).
 *  - Crossing/focus: IswNotifyMode (Normal/Grab/Ungrab) + IswFocusSource —
 *    the only distinctions widgets make.
 *  - Coordinates: widget-local logical pixels (already HiDPI-descaled).
 *  - Target: an opaque IswEventTarget, not a raw window id.
 *
 * CRITICAL: NO xcb_* types, NO atoms, NO keysyms appear in this header.
 */

#ifndef _IswEvent_h
#define _IswEvent_h

#include <stdint.h>

/* Opaque dispatch target — the root widget the event belongs to.  The platform
 * backend owns the OS windows; during translation it resolves the event to the
 * root widget of the window that received it and stores that widget here.  The
 * toolkit core casts this back to a Widget to begin hit-testing; it never sees
 * a window.  Pointer-width so it can carry the widget handle directly. */
typedef uintptr_t IswEventTarget;

/* Event timestamp in milliseconds, monotonic per backend. */
typedef uint32_t IswTime;

/* Neutral identity of a protocol-message type (the interned name's id).  The
 * translation manager matches a parsed message name against this; widgets and
 * shells compare it to a known interned id.  Numerically equal to the backend
 * atom on X11, but carried as a neutral id so this header stays type-pure. */
typedef uint32_t IswProtocolId;

/*
 * -----------------------------------------------------------------------
 * Event kinds — the categories the toolkit dispatches on
 * -----------------------------------------------------------------------
 */
typedef enum {
    IswNoEvent = 0,
    IswKeyDown,
    IswKeyUp,
    IswButtonDown,
    IswButtonUp,
    IswMotion,
    IswEnter,
    IswLeave,
    IswFocusIn,
    IswFocusOut,
    IswRedraw,          /* damage / expose                          */
    IswGeometry,        /* position/size changed (configure)        */
    IswReparent,        /* window re-parented (position validity)   */
    IswMap,
    IswUnmap,
    IswVisibility,
    IswDestroy,
    IswMappingChanged,  /* keyboard mapping changed (refresh maps)  */
    IswProtocol         /* generic protocol message (client message);
                           a WM close request is a WM_PROTOCOLS message
                           whose data[0] is WM_DELETE_WINDOW              */
} IswEventKind;

/*
 * -----------------------------------------------------------------------
 * Modifier mask — neutral names the translation manager matches against
 * -----------------------------------------------------------------------
 */
/*
 * Bit positions match the X11 wire modifier/button layout (Shift=0, Lock=1,
 * Control=2, Mod1..Mod5 = 3..7, Button1..Button5 = 8..12) so the backend copies
 * the native modifier word straight through with no remap and the translation
 * manager preserves every distinction a table can express (<Mod3>, <Button4>).
 * The semantic names (Alt/Super/Meta/Hyper) are aliases onto the raw Mod bits
 * the backend's keymap assigns them to; widget code branches on the semantic
 * names, translation tables may use either.
 */
typedef enum {
    IswModShift   = 1u << 0,
    IswModLock    = 1u << 1,   /* caps lock */
    IswModControl = 1u << 2,
    IswModMod1    = 1u << 3,
    IswModMod2    = 1u << 4,
    IswModMod3    = 1u << 5,
    IswModMod4    = 1u << 6,
    IswModMod5    = 1u << 7,
    IswModButton1 = 1u << 8,
    IswModButton2 = 1u << 9,
    IswModButton3 = 1u << 10,
    IswModButton4 = 1u << 11,
    IswModButton5 = 1u << 12,

    /* Semantic aliases (mapped onto raw Mod bits by the backend keymap). */
    IswModAlt     = IswModMod1, /* typically Mod1 */
    IswModMeta    = IswModMod2,
    IswModSuper   = IswModMod4, /* "windows" / command, typically Mod4 */
    IswModHyper   = IswModMod3
} IswModMask;

/* How a crossing/focus transition was caused — X's
 * Normal/Grab/Ungrab/WhileGrabbed soup collapses to what widgets branch on. */
typedef enum {
    IswNotifyNormal = 0,
    IswNotifyGrab,
    IswNotifyUngrab
} IswNotifyMode;

/* Whether focus moved because of the pointer or the keyboard. */
typedef enum {
    IswFocusByKeyboard = 0,
    IswFocusByPointer
} IswFocusSource;

/* Relationship of the event window to the focus/crossing target — the neutral
 * form of X's NotifyAncestor/Virtual/Inferior/Nonlinear[Virtual]/Pointer detail.
 * The focus manager uses this to track whether focus is on the widget itself, a
 * descendant, or an ancestor. */
typedef enum {
    IswNotifyDetailNone = 0,
    IswNotifyAncestor,
    IswNotifyVirtual,
    IswNotifyInferior,
    IswNotifyNonlinear,
    IswNotifyNonlinearVirtual,
    IswNotifyPointer,
    IswNotifyPointerRoot
} IswNotifyDetail;

/* Logical button identity. */
typedef enum {
    IswButtonNone   = 0,
    IswButtonLeft   = 1,
    IswButtonMiddle = 2,
    IswButtonRight  = 3,
    IswButtonWheelUp   = 4,
    IswButtonWheelDown = 5
} IswButton;

/*
 * -----------------------------------------------------------------------
 * Neutral key identity
 * -----------------------------------------------------------------------
 * A backend-independent name for "which key".  Printable keys carry their
 * Unicode code point in `unicode`; non-printable keys use the IswKey* enum.
 * The translation manager matches "<Key>a", "<Key>Return" etc. against this
 * — it never sees a platform keysym or keycode.  The backend maps its native
 * key identity onto this.
 */
typedef enum {
    IswKeyNone = 0,

    /* Non-printable keys get values above the Unicode range so they never
     * collide with a code point carried in IswKeyEvent.unicode. */
    IswKeyBackspace = 0x110000,
    IswKeyTab,
    IswKeyReturn,
    IswKeyEscape,
    IswKeyDelete,
    IswKeyHome, IswKeyEnd,
    IswKeyArrowLeft, IswKeyArrowRight, IswKeyArrowUp, IswKeyArrowDown,
    IswKeyPageUp, IswKeyPageDown,
    IswKeyInsert,
    IswKeyF1, IswKeyF2, IswKeyF3, IswKeyF4, IswKeyF5, IswKeyF6,
    IswKeyF7, IswKeyF8, IswKeyF9, IswKeyF10, IswKeyF11, IswKeyF12,
    IswKeyShift, IswKeyControl, IswKeyAlt, IswKeySuper, IswKeyMeta,
    IswKeyCapsLock, IswKeyNumLock,
    IswKeyMenu, IswKeyPause, IswKeyPrint
} IswKey;

/*
 * -----------------------------------------------------------------------
 * Per-category event bodies
 * -----------------------------------------------------------------------
 * Every body begins with the IswAnyEvent prefix so common code can read
 * event->any.* regardless of the active variant.
 */
/* Shared prefix of every event body.  `native` is the backend migration
 * bridge (see the union below); it sits in the common header so it is at the
 * same offset in every variant and never aliases category fields. */
#define ISW_EVENT_HEADER \
    IswEventKind   kind;      \
    uint8_t        synthetic; /* synthesized, not hardware-generated */ \
    IswEventTarget target;    \
    IswTime        time;      \
    void          *native     /* backend-owned; do not dereference */

typedef struct { ISW_EVENT_HEADER; } IswAnyEvent;

/* Key down / up.  key is the neutral identity (IswKey or a Unicode code
 * point); unicode is the code point for printable keys (0 otherwise); text
 * is the UTF-8 the key produced ("" for non-text keys). */
typedef struct {
    ISW_EVENT_HEADER;
    uint32_t    key;         /* IswKey value or Unicode code point */
    uint32_t    unicode;     /* code point, or 0 */
    char        text[8];     /* UTF-8 + NUL */
    uint16_t    modifiers;   /* IswModMask */
    int16_t     x, y;        /* pointer position, widget-local logical px */
    int16_t     root_x, root_y; /* pointer position, screen-relative */
} IswKeyEvent;

/* Button down / up. */
typedef struct {
    ISW_EVENT_HEADER;
    uint8_t     button;      /* IswButton */
    uint16_t    modifiers;   /* IswModMask, incl. held buttons */
    int16_t     x, y;        /* widget-local logical px */
    int16_t     root_x, root_y;
} IswButtonEvent;

/* Pointer motion. */
typedef struct {
    ISW_EVENT_HEADER;
    uint16_t    modifiers;
    int16_t     x, y;
    int16_t     root_x, root_y;
} IswMotionEvent;

/* Enter / leave. */
typedef struct {
    ISW_EVENT_HEADER;
    IswNotifyMode   mode;
    IswNotifyDetail detail;
    uint16_t        modifiers;
    int16_t         x, y;
    int16_t         root_x, root_y;
    uint8_t         same_screen; /* pointer on same screen + window focused */
} IswCrossingEvent;

/* Focus in / out. */
typedef struct {
    ISW_EVENT_HEADER;
    IswNotifyMode   mode;
    IswNotifyDetail detail;
    IswFocusSource  source;
} IswFocusEvent;

/* Redraw (expose).  Damaged rect in widget-local logical px; count is how
 * many redraw events still follow (0 == last, paint now). */
typedef struct {
    ISW_EVENT_HEADER;
    int16_t     x, y;
    uint16_t    width, height;
    uint16_t    count;
} IswRedrawEvent;

/* Geometry (configure): new position/size in logical px. */
typedef struct {
    ISW_EVENT_HEADER;
    int16_t     x, y;
    uint16_t    width, height;
    uint16_t    border_width;
} IswGeometryEvent;

/* Reparent: the window's parent changed.  x,y are the new origin in logical
 * px; to_root is true when re-parented directly to the screen root (the only
 * distinction shells make for position-validity tracking). */
typedef struct {
    ISW_EVENT_HEADER;
    int16_t     x, y;
    uint8_t     to_root;
} IswReparentEvent;

/* Structure: map / unmap / destroy / visibility. */
typedef struct {
    ISW_EVENT_HEADER;
    uint8_t     visibility;  /* 0 unobscured, 1 partial, 2 fully obscured */
} IswStructureEvent;

/* Generic protocol message (X11 client message and equivalents).  message_type
 * is the neutral id of the message-type name the translation manager matches
 * against; data carries the message's payload words (XdndPosition coords, tray
 * opcodes, WM_PROTOCOLS protocol atom, ...).  Pure protocol the toolkit core
 * does not interpret — widgets/shells/backend services decode data by
 * message_type. */
typedef struct {
    ISW_EVENT_HEADER;
    IswProtocolId message_type;
    uint8_t       format;       /* 8, 16 or 32: width of each data word */
    uint32_t      data[5];      /* up to 5 32-bit words (X client-message data) */
} IswProtocolEvent;

/* ClientMessage / protocol close request carries no neutral payload beyond
 * the header — the backend has already decided it is a close request. */

/*
 * -----------------------------------------------------------------------
 * The neutral event union
 * -----------------------------------------------------------------------
 * Read `.kind` (== any.kind) to discriminate, then the matching member.
 *
 * `native` is an opaque, backend-owned pointer to the platform event this was
 * translated from.  It exists ONLY as a migration bridge and for the few
 * backend-internal paths (grabs, keysym round-trips) that still reach the
 * native event during the platform-abstraction work; toolkit and widget code
 * MUST NOT dereference it.  Retrieve it via IswEventNative().  It will be
 * removed once every consumer reads neutral fields.
 */
typedef union _IswEvent {
    IswEventKind      kind;   /* fast discriminant (== any.kind) */
    IswAnyEvent       any;
    IswKeyEvent       key;
    IswButtonEvent    button;
    IswMotionEvent    motion;
    IswCrossingEvent  crossing;
    IswFocusEvent     focus;
    IswRedrawEvent    redraw;
    IswGeometryEvent  geometry;
    IswReparentEvent  reparent;
    IswStructureEvent structure;
    IswProtocolEvent  protocol;
} IswEvent;

/*
 * -----------------------------------------------------------------------
 * Neutral field accessors
 * -----------------------------------------------------------------------
 * Read the common fields without knowing which variant is active.  The
 * pointer-position accessors return the widget-local logical coordinate
 * carried by whichever input variant the event is (key / button / motion /
 * crossing); they return 0 for events with no pointer position.
 */

static inline int16_t IswEventX(const IswEvent *e)
{
    switch (e->kind) {
    case IswKeyDown: case IswKeyUp:           return e->key.x;
    case IswButtonDown: case IswButtonUp:     return e->button.x;
    case IswMotion:                           return e->motion.x;
    case IswEnter: case IswLeave:             return e->crossing.x;
    default:                                  return 0;
    }
}

static inline int16_t IswEventY(const IswEvent *e)
{
    switch (e->kind) {
    case IswKeyDown: case IswKeyUp:           return e->key.y;
    case IswButtonDown: case IswButtonUp:     return e->button.y;
    case IswMotion:                           return e->motion.y;
    case IswEnter: case IswLeave:             return e->crossing.y;
    default:                                  return 0;
    }
}

/* Modifier mask (IswModMask) for input events; 0 otherwise. */
static inline uint16_t IswEventModifiers(const IswEvent *e)
{
    switch (e->kind) {
    case IswKeyDown: case IswKeyUp:       return e->key.modifiers;
    case IswButtonDown: case IswButtonUp: return e->button.modifiers;
    case IswMotion:                       return e->motion.modifiers;
    case IswEnter: case IswLeave:         return e->crossing.modifiers;
    default:                              return 0;
    }
}

/* Button identity (IswButton) for button events; IswButtonNone otherwise. */
static inline uint8_t IswEventButton(const IswEvent *e)
{
    return (e->kind == IswButtonDown || e->kind == IswButtonUp)
           ? e->button.button : (uint8_t) IswButtonNone;
}

/*
 * Native-event escape hatch: the backend event this IswEvent was translated
 * from (the XCB backend's xcb_generic_event_t *, as an opaque void *).
 *
 * Toolkit-semantic reads should use the neutral fields/accessors above — most
 * widget code has been migrated and no longer needs this.  It remains, by
 * design, only for code that genuinely operates below the neutral layer:
 *
 *   1. Backend-internal X11 protocol handlers (Selection.c, Shell.c WM,
 *      ISWPlatformDndXCB.c, IswTrayIcon.c) — they decode atoms / client
 *      messages / selection transfers the neutral event deliberately omits.
 *   2. Re-dispatch through the native-event API: IswCallActionProc() still
 *      takes a native event, so a proc that re-invokes an action passes
 *      IswEventNative(iswev) (Scrollbar HandleThumb, SimpleMenu, MenuBar).
 *      Retires when the action API goes neutral.
 *   3. X-only fields not yet abstracted: root-window coordinates, the native
 *      event-window for IswWindowToWidget(), the EnterNotify INFERIOR detail,
 *      same_screen flags.  These get neutral forms in the Display/Window and
 *      Input phases (see docs/ISWPLATFORM_PLAN.md).
 *   4. Public callback contracts that still expose the native event
 *      (ISWDrawingCallbackData / Grip-Paned) — retired when those are revised.
 *
 * Returns NULL if there is no backing native event (e.g. a synthesized
 * IswEvent whose .native was not set).
 */
void *IswEventNative(const IswEvent *event);

/*
 * Convenience for the cases above: declares `xcb_generic_event_t *event`
 * bound to the native event, as a proc's first statement:
 *
 *     void Foo(Widget w, IswEvent *iswev, String *p, Cardinal *n) {
 *         ISW_NATIVE_EVENT(iswev);   // declares `xcb_generic_event_t *event`
 *         ... native-level body using `event` ...
 *     }
 *
 * Cast through void* so this header needs no xcb type; the expansion site must
 * have xcb_generic_event_t in scope.
 */
#define ISW_NATIVE_EVENT(iswev) \
    xcb_generic_event_t *event = (xcb_generic_event_t *) IswEventNative(iswev)

#endif /* _IswEvent_h */
