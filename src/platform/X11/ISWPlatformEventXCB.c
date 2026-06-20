/*
 * ISWPlatformEventXCB.c - XCB → IswEvent translation (platform backend)
 *
 * Copyright (c) 2026 ISW Project
 *
 * Translates native xcb_generic_event_t into the toolkit's platform-neutral
 * IswEvent (include/ISW/IswEvent.h).  This is the XCB platform backend's event
 * path: the toolkit-semantic event kinds are produced here, including the few
 * protocol events the toolkit branches on once given a neutral form — keyboard
 * mapping change (IswMappingChanged), window re-parent (IswReparent) and client
 * messages (IswProtocol, carrying the message-type atom + data; a WM close
 * request is a WM_PROTOCOLS message whose data[0] is WM_DELETE_WINDOW, decoded
 * by the handler, not special-cased here).  Pure protocol events (selection,
 * property, drag-and-drop, tray docking) are NOT translated — they are handled
 * inside their respective backend modules (Selection.c, ISWPlatformDndXCB.c,
 * IswTrayIcon.c) and never become an IswEvent.
 *
 * Folds in, at the translation boundary, the work the dispatch core used to do
 * inline: keysym → neutral key identity + UTF-8.  (HiDPI descale stays in the
 * dispatch core; coordinates arrive already-logical — see _IswEventFromXcb.)
 *
 * Phase 1 of the ISWPlatform vtable work (docs/ISWPLATFORM_PLAN.md).  Lives in
 * src/ for now; moves behind the ISWPlatformEvent sub-vtable in Phase 2.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon.h>

#include "IntrinsicI.h"
#include "InitialI.h"
#include <ISW/IswEvent.h>
#include "ISWPlatformPrivate.h"

/* ---- modifier mapping: X mod bits → neutral IswModMask -------------------- */

static uint16_t
xcb_state_to_modmask(uint16_t state)
{
    /* IswModMask bit positions match the X11 wire layout (Shift=0 .. Button5=12),
       so the modifier/button portion of the native state word copies straight
       through.  Mask to the 13 defined bits; higher X state bits are not
       toolkit modifiers. */
    return (uint16_t) (state & 0x1FFF);
}

/* ---- keysym → neutral key identity ---------------------------------------- */

/* Map an X/xkb keysym to an IswKey value for non-printable keys, or to its
 * Unicode code point for printable keys.  Returns the IswKey/codepoint and
 * fills `unicode` (0 for non-printable) and UTF-8 `text` (empty if none). */
static uint32_t
keysym_to_key(xcb_keysym_t ks, uint32_t *unicode, char text[8])
{
    uint32_t cp;

    *unicode = 0;
    text[0] = '\0';

    switch (ks) {
    case 0xff08: return IswKeyBackspace;
    case 0xff09: return IswKeyTab;
    case 0xff0d: return IswKeyReturn;
    case 0xff1b: return IswKeyEscape;
    case 0xffff: return IswKeyDelete;
    case 0xff50: return IswKeyHome;
    case 0xff57: return IswKeyEnd;
    case 0xff51: return IswKeyArrowLeft;
    case 0xff53: return IswKeyArrowRight;
    case 0xff52: return IswKeyArrowUp;
    case 0xff54: return IswKeyArrowDown;
    case 0xff55: return IswKeyPageUp;
    case 0xff56: return IswKeyPageDown;
    case 0xff63: return IswKeyInsert;
    case 0xffbe: return IswKeyF1;
    case 0xffbf: return IswKeyF2;
    case 0xffc0: return IswKeyF3;
    case 0xffc1: return IswKeyF4;
    case 0xffc2: return IswKeyF5;
    case 0xffc3: return IswKeyF6;
    case 0xffc4: return IswKeyF7;
    case 0xffc5: return IswKeyF8;
    case 0xffc6: return IswKeyF9;
    case 0xffc7: return IswKeyF10;
    case 0xffc8: return IswKeyF11;
    case 0xffc9: return IswKeyF12;
    case 0xffe1: case 0xffe2: return IswKeyShift;
    case 0xffe3: case 0xffe4: return IswKeyControl;
    case 0xffe9: case 0xffea: return IswKeyAlt;
    case 0xffeb: case 0xffec: return IswKeySuper;
    case 0xffe7: case 0xffe8: return IswKeyMeta;
    case 0xffe5: return IswKeyCapsLock;
    case 0xff7f: return IswKeyNumLock;
    case 0xff67: return IswKeyMenu;
    case 0xff13: return IswKeyPause;
    case 0xff61: return IswKeyPrint;

    /* Keypad navigation/editing keys → logical equivalents. */
    case 0xff80: return ' ';            /* KP_Space */
    case 0xff89: return IswKeyTab;      /* KP_Tab */
    case 0xff8d: return IswKeyReturn;   /* KP_Enter */
    case 0xff95: return IswKeyHome;     /* KP_Home */
    case 0xff96: return IswKeyArrowLeft;  /* KP_Left */
    case 0xff97: return IswKeyArrowUp;    /* KP_Up */
    case 0xff98: return IswKeyArrowRight; /* KP_Right */
    case 0xff99: return IswKeyArrowDown;  /* KP_Down */
    case 0xff9a: return IswKeyPageUp;   /* KP_Prior/KP_Page_Up */
    case 0xff9b: return IswKeyPageDown; /* KP_Next/KP_Page_Down */
    case 0xff9c: return IswKeyEnd;      /* KP_End */
    case 0xff9e: return IswKeyInsert;   /* KP_Insert */
    case 0xff9f: return IswKeyDelete;   /* KP_Delete */
    default:
        break;
    }

    /* Printable: resolve to a Unicode code point via xkb. */
    cp = xkb_keysym_to_utf32((xkb_keysym_t) ks);
    if (cp != 0) {
        int n = xkb_keysym_to_utf8((xkb_keysym_t) ks, text, 8);
        if (n <= 0)
            text[0] = '\0';
        *unicode = cp;
        return cp;
    }
    return IswKeyNone;
}

/* Resolve a key name to a neutral key identity (IswKey / Unicode code point),
   matching the vocabulary keysym_to_key produces for dispatched events.  The
   translation-table parser calls this so "<Key>Return" matches IswKeyReturn. */
uint32_t
_IswPlatformKeyFromName(const char *name)
{
    xkb_keysym_t ks;
    uint32_t unicode;
    char text[8];

    if (name == NULL || name[0] == '\0')
        return IswKeyNone;

    /* Single printable ASCII char: its code point is its identity. */
    if (name[1] == '\0' && (unsigned char) name[0] >= ' ' &&
        (unsigned char) name[0] <= '~')
        return (uint32_t) (unsigned char) name[0];

    ks = xkb_keysym_from_name(name, XKB_KEYSYM_NO_FLAGS);
    if (ks == XKB_KEY_NoSymbol)
        ks = xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE);
    if (ks == XKB_KEY_NoSymbol)
        return IswKeyNone;

    return keysym_to_key((xcb_keysym_t) ks, &unicode, text);
}

/* X crossing/focus detail -> neutral IswNotifyDetail. */
static IswNotifyDetail
xcb_notify_detail(uint8_t detail)
{
    switch (detail) {
    case XCB_NOTIFY_DETAIL_ANCESTOR:           return IswNotifyAncestor;
    case XCB_NOTIFY_DETAIL_VIRTUAL:            return IswNotifyVirtual;
    case XCB_NOTIFY_DETAIL_INFERIOR:          return IswNotifyInferior;
    case XCB_NOTIFY_DETAIL_NONLINEAR:         return IswNotifyNonlinear;
    case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL: return IswNotifyNonlinearVirtual;
    case XCB_NOTIFY_DETAIL_POINTER:           return IswNotifyPointer;
    case XCB_NOTIFY_DETAIL_POINTER_ROOT:      return IswNotifyPointerRoot;
    default:                                  return IswNotifyDetailNone;
    }
}

/* Event dispatch target: the root widget of the window the event hit, as an
   opaque IswEventTarget.  The core casts it back to a Widget; it never sees the
   window.  NULL window/widget yields target 0. */
static IswEventTarget
target_for_window(IswDisplay dpy, xcb_window_t window)
{
    return (IswEventTarget) (void *) _IswXcbWidgetForWindow(dpy, window);
}

/*
 * _IswEventFromXcb - translate a native XCB event into a neutral IswEvent.
 *
 * Returns True and fills `out` for toolkit-semantic events; returns False for
 * X11 protocol events the toolkit must not see as events (caller routes those
 * to the backend's protocol handlers).  `out->native` is set to `xev` so the
 * native-event escape hatch (IswEventNative) and backend-internal paths can
 * still reach the native event.
 *
 * Coordinates are copied straight through: the dispatch core
 * (_IswDescaleEventCoords in Event.c) has already converted the native event's
 * coordinates from physical to logical pixels (and, for windowless widgets,
 * rebased them to widget-local) BEFORE this runs.  Descaling here too would
 * double-apply the scale factor.  Translation is format-only; coordinate
 * scaling stays the dispatch core's responsibility.
 */
Boolean
_IswEventFromXcb(IswDisplay dpy, xcb_generic_event_t *xev, IswEvent *out)
{
    uint8_t type;
    uint8_t synthetic;

    /* A synthetic action invocation (e.g. IswCallActionProc from MenuBar's
       OpenMenu) has no triggering native event.  Produce a zeroed neutral
       event with no kind and no native backing rather than dereferencing
       NULL — the toolkit-neutral equivalent of "no event". */
    if (xev == NULL) {
        memset(out, 0, sizeof(*out));
        return False;
    }

    type = xev->response_type & ~0x80;
    synthetic = (xev->response_type & 0x80) ? 1 : 0;

    memset(out, 0, sizeof(*out));
    out->any.synthetic = synthetic;
    //out->any.native = xev;

    switch (type) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
        xcb_key_press_event_t *e = (xcb_key_press_event_t *) xev;
        Modifiers mods_ret = 0;
        xcb_keysym_t ks = 0;
        out->kind = (type == XCB_KEY_PRESS) ? IswKeyDown : IswKeyUp;
        out->key.target = target_for_window(dpy, e->event);
        out->key.time = e->time;
        out->key.modifiers = xcb_state_to_modmask(e->state);
        out->key.x = e->event_x;
        out->key.y = e->event_y;
        out->key.root_x = e->root_x;
        out->key.root_y = e->root_y;
        IswTranslateKeycode(dpy, (IswKeyCode) e->detail, e->state,
                            &mods_ret, &ks);
        out->key.key = keysym_to_key(ks, &out->key.unicode, out->key.text);
        return True;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *e = (xcb_button_press_event_t *) xev;
        out->kind = (type == XCB_BUTTON_PRESS) ? IswButtonDown : IswButtonUp;
        out->button.target = target_for_window(dpy, e->event);
        out->button.time = e->time;
        out->button.button = (uint8_t) e->detail;
        out->button.modifiers = xcb_state_to_modmask(e->state);
        out->button.x = e->event_x;
        out->button.y = e->event_y;
        out->button.root_x = e->root_x;
        out->button.root_y = e->root_y;
        return True;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *) xev;
        out->kind = IswMotion;
        out->motion.target = target_for_window(dpy, e->event);
        out->motion.time = e->time;
        out->motion.modifiers = xcb_state_to_modmask(e->state);
        out->motion.x = e->event_x;
        out->motion.y = e->event_y;
        out->motion.root_x = e->root_x;
        out->motion.root_y = e->root_y;
        return True;
    }
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY: {
        xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) xev;
        out->kind = (type == XCB_ENTER_NOTIFY) ? IswEnter : IswLeave;
        out->crossing.target = target_for_window(dpy, e->event);
        out->crossing.time = e->time;
        out->crossing.modifiers = xcb_state_to_modmask(e->state);
        switch (e->mode) {
        case XCB_NOTIFY_MODE_GRAB:   out->crossing.mode = IswNotifyGrab;   break;
        case XCB_NOTIFY_MODE_UNGRAB: out->crossing.mode = IswNotifyUngrab; break;
        default:                     out->crossing.mode = IswNotifyNormal; break;
        }
        out->crossing.detail = xcb_notify_detail(e->detail);
        out->crossing.x = e->event_x;
        out->crossing.y = e->event_y;
        out->crossing.root_x = e->root_x;
        out->crossing.root_y = e->root_y;
        out->crossing.same_screen = (e->same_screen_focus & 0x01) ? 1 : 0;
        return True;
    }
    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT: {
        xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) xev;
        out->kind = (type == XCB_FOCUS_IN) ? IswFocusIn : IswFocusOut;
        out->focus.target = target_for_window(dpy, e->event);
        switch (e->mode) {
        case XCB_NOTIFY_MODE_GRAB:   out->focus.mode = IswNotifyGrab;   break;
        case XCB_NOTIFY_MODE_UNGRAB: out->focus.mode = IswNotifyUngrab; break;
        default:                     out->focus.mode = IswNotifyNormal; break;
        }
        out->focus.detail = xcb_notify_detail(e->detail);
        out->focus.source = (e->detail == XCB_NOTIFY_DETAIL_POINTER)
                            ? IswFocusByPointer : IswFocusByKeyboard;
        return True;
    }
    case XCB_EXPOSE: {
        xcb_expose_event_t *e = (xcb_expose_event_t *) xev;
        out->kind = IswRedraw;
        out->redraw.target = target_for_window(dpy, e->window);
        out->redraw.x = (int16_t) e->x;
        out->redraw.y = (int16_t) e->y;
        out->redraw.width = (uint16_t) (int16_t) e->width;
        out->redraw.height = (uint16_t) (int16_t) e->height;
        out->redraw.count = e->count;
        return True;
    }
    case XCB_CONFIGURE_NOTIFY: {
        xcb_configure_notify_event_t *e = (xcb_configure_notify_event_t *) xev;
        out->kind = IswGeometry;
        out->geometry.target = target_for_window(dpy, e->window);
        out->geometry.x = e->x;
        out->geometry.y = e->y;
        out->geometry.width = (uint16_t) (int16_t) e->width;
        out->geometry.height = (uint16_t) (int16_t) e->height;
        out->geometry.border_width = (uint16_t) e->border_width;
        return True;
    }
    case XCB_REPARENT_NOTIFY: {
        xcb_reparent_notify_event_t *e = (xcb_reparent_notify_event_t *) xev;
        xcb_screen_t *screen = _IswXcbScreen(_IswDefaultScreenOf(dpy));
        out->kind = IswReparent;
        out->reparent.target = target_for_window(dpy, e->window);
        out->reparent.x = e->x;
        out->reparent.y = e->y;
        out->reparent.to_root =
            (screen != NULL && e->parent == screen->root) ? 1 : 0;
        return True;
    }
    case XCB_MAPPING_NOTIFY: {
        out->kind = IswMappingChanged;
        return True;
    }
    case XCB_MAP_NOTIFY: {
        xcb_map_notify_event_t *e = (xcb_map_notify_event_t *) xev;
        out->kind = IswMap;
        out->structure.target = target_for_window(dpy, e->window);
        return True;
    }
    case XCB_UNMAP_NOTIFY: {
        xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *) xev;
        out->kind = IswUnmap;
        out->structure.target = target_for_window(dpy, e->window);
        return True;
    }
    case XCB_DESTROY_NOTIFY: {
        xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *) xev;
        out->kind = IswDestroy;
        out->structure.target = target_for_window(dpy, e->window);
        return True;
    }
    case XCB_VISIBILITY_NOTIFY: {
        xcb_visibility_notify_event_t *e = (xcb_visibility_notify_event_t *) xev;
        out->kind = IswVisibility;
        out->structure.target = target_for_window(dpy, e->window);
        out->structure.visibility = e->state;
        return True;
    }
    case XCB_CLIENT_MESSAGE: {
        xcb_client_message_event_t *e = (xcb_client_message_event_t *) xev;
        int i;

        /* Check for WM_DELETE_WINDOW: a WM_PROTOCOLS client message whose
           data[0] is the WM_DELETE_WINDOW atom.  Emit IswWindowClose so
           the core never needs to intern atoms for protocol matching. */
        {
            static xcb_atom_t wm_protocols_atom = 0;
            static xcb_atom_t wm_delete_atom = 0;
            if (!wm_protocols_atom) {
                xcb_connection_t *conn = _IswXcbConn(dpy);
                xcb_intern_atom_reply_t *r;
                r = xcb_intern_atom_reply(conn,
                    xcb_intern_atom(conn, 1, 12, "WM_PROTOCOLS"), NULL);
                if (r) { wm_protocols_atom = r->atom; free(r); }
                r = xcb_intern_atom_reply(conn,
                    xcb_intern_atom(conn, 1, 16, "WM_DELETE_WINDOW"), NULL);
                if (r) { wm_delete_atom = r->atom; free(r); }
            }
            if (wm_protocols_atom && e->type == wm_protocols_atom &&
                wm_delete_atom && e->data.data32[0] == wm_delete_atom) {
                out->kind = IswWindowClose;
                out->any.target = target_for_window(dpy, e->window);
                return True;
            }
        }

        out->kind = IswProtocol;
        out->protocol.target = target_for_window(dpy, e->window);
        out->protocol.message_type = (IswProtocolId) e->type;
        out->protocol.format = e->format;
        for (i = 0; i < 5; i++)
            out->protocol.data[i] = e->data.data32[i];
        return True;
    }
    default:
        /* X11 protocol event — not a toolkit-semantic IswEvent. */
        return False;
    }
}

/* X11 <Message> translation match: intern the quark-stored name to an atom
   and compare against the protocol event's message_type (which is an atom
   on X11).  This is the only atom-interning code the translation manager
   needs, and it lives here in the backend rather than in core TMstate.c. */
Boolean
_IswMatchProtocolName(TMTypeMatch typeMatch,
             TMModifierMatch modMatch _X_UNUSED,
             TMEventPtr eventSeq)
{
    const char *atom_name = XrmQuarkToString((XrmQuark) (typeMatch->eventCode));
    Atom atom = _IswPlatformInternAtomOp((IswDisplay) eventSeq->dpy, atom_name, False);
    if (atom == ISW_ATOM_NONE)
        return False;
    return (atom == eventSeq->event.eventCode);
}

/* Migration bridge (declared in IswEvent.h). */
void *
IswEventNative(const IswEvent *event)
{
    return event ? event->any.native : NULL;
}

static IswEvent *
xcb_event_poll(IswDisplay dpy)
{
    xcb_generic_event_t *xev = xcb_poll_for_event(_IswXcbConn(dpy));
    IswEvent *event = IswNew(IswEvent);
    Boolean ok = _IswEventFromXcb(dpy, xev, event);
    free(xev);                      /* native buffer is never retained */
    if (!ok) {
        IswFree((char *) event);
        return NULL;
    }
    return event;
}

static IswEvent *
xcb_event_poll_queued(IswDisplay dpy)
{
    xcb_generic_event_t *xev = xcb_poll_for_queued_event(_IswXcbConn(dpy));
    IswEvent *event = IswNew(IswEvent);
    Boolean ok = _IswEventFromXcb(dpy, xev, event);
    free(xev);                      /* native buffer is never retained */
    if (!ok) {
        IswFree((char *) event);
        return NULL;
    }
    return event;
}

const IswPlatformEventOps isw_platform_xcb_event_ops = {
    .poll        = xcb_event_poll,
    .poll_queued = xcb_event_poll_queued,
};
