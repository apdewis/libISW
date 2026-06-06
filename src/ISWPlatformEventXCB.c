/*
 * ISWPlatformEventXCB.c - XCB → IswEvent translation (platform backend)
 *
 * Copyright (c) 2026 ISW Project
 *
 * Translates native xcb_generic_event_t into the toolkit's platform-neutral
 * IswEvent (include/ISW/IswEvent.h).  This is the XCB platform backend's event
 * path: only the toolkit-semantic event kinds are produced here.  X11 protocol
 * events (selection, property, client-message, mapping, ...) are NOT translated
 * — they are handled inside their respective backend modules (Selection.c,
 * ISWXdnd.c, IswTrayIcon.c, keyboard mapping) and never become an IswEvent.
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

/* ---- modifier mapping: X mod bits → neutral IswModMask -------------------- */

static uint16_t
xcb_state_to_modmask(uint16_t state)
{
    uint16_t m = 0;
    if (state & XCB_MOD_MASK_SHIFT)   m |= IswModShift;
    if (state & XCB_MOD_MASK_LOCK)    m |= IswModLock;
    if (state & XCB_MOD_MASK_CONTROL) m |= IswModControl;
    if (state & XCB_MOD_MASK_1)       m |= IswModAlt;
    if (state & XCB_MOD_MASK_4)       m |= IswModSuper;
    if (state & XCB_MOD_MASK_2)       m |= IswModMeta;  /* commonly numlock; see note */
    if (state & XCB_BUTTON_MASK_1)    m |= IswModButton1;
    if (state & XCB_BUTTON_MASK_2)    m |= IswModButton2;
    if (state & XCB_BUTTON_MASK_3)    m |= IswModButton3;
    return m;
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
_IswEventFromXcb(xcb_connection_t *dpy, xcb_generic_event_t *xev, IswEvent *out)
{
    uint8_t type = xev->response_type & ~0x80;
    uint8_t synthetic = (xev->response_type & 0x80) ? 1 : 0;

    memset(out, 0, sizeof(*out));
    out->any.synthetic = synthetic;
    out->any.native = xev;

    switch (type) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
        xcb_key_press_event_t *e = (xcb_key_press_event_t *) xev;
        Modifiers mods_ret = 0;
        xcb_keysym_t ks = 0;
        out->kind = (type == XCB_KEY_PRESS) ? IswKeyDown : IswKeyUp;
        out->key.target = e->event;
        out->key.time = e->time;
        out->key.modifiers = xcb_state_to_modmask(e->state);
        out->key.x = e->event_x;
        out->key.y = e->event_y;
        IswTranslateKeycode(dpy, (_IswKeyCode) e->detail, e->state,
                            &mods_ret, &ks);
        out->key.key = keysym_to_key(ks, &out->key.unicode, out->key.text);
        return True;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *e = (xcb_button_press_event_t *) xev;
        out->kind = (type == XCB_BUTTON_PRESS) ? IswButtonDown : IswButtonUp;
        out->button.target = e->event;
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
        out->motion.target = e->event;
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
        out->crossing.target = e->event;
        out->crossing.time = e->time;
        out->crossing.modifiers = xcb_state_to_modmask(e->state);
        switch (e->mode) {
        case XCB_NOTIFY_MODE_GRAB:   out->crossing.mode = IswNotifyGrab;   break;
        case XCB_NOTIFY_MODE_UNGRAB: out->crossing.mode = IswNotifyUngrab; break;
        default:                     out->crossing.mode = IswNotifyNormal; break;
        }
        out->crossing.x = e->event_x;
        out->crossing.y = e->event_y;
        return True;
    }
    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT: {
        xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) xev;
        out->kind = (type == XCB_FOCUS_IN) ? IswFocusIn : IswFocusOut;
        out->focus.target = e->event;
        switch (e->mode) {
        case XCB_NOTIFY_MODE_GRAB:   out->focus.mode = IswNotifyGrab;   break;
        case XCB_NOTIFY_MODE_UNGRAB: out->focus.mode = IswNotifyUngrab; break;
        default:                     out->focus.mode = IswNotifyNormal; break;
        }
        out->focus.source = (e->detail == XCB_NOTIFY_DETAIL_POINTER)
                            ? IswFocusByPointer : IswFocusByKeyboard;
        return True;
    }
    case XCB_EXPOSE: {
        xcb_expose_event_t *e = (xcb_expose_event_t *) xev;
        out->kind = IswRedraw;
        out->redraw.target = e->window;
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
        out->geometry.target = e->window;
        out->geometry.x = e->x;
        out->geometry.y = e->y;
        out->geometry.width = (uint16_t) (int16_t) e->width;
        out->geometry.height = (uint16_t) (int16_t) e->height;
        return True;
    }
    case XCB_MAP_NOTIFY: {
        xcb_map_notify_event_t *e = (xcb_map_notify_event_t *) xev;
        out->kind = IswMap;
        out->structure.target = e->window;
        return True;
    }
    case XCB_UNMAP_NOTIFY: {
        xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *) xev;
        out->kind = IswUnmap;
        out->structure.target = e->window;
        return True;
    }
    case XCB_DESTROY_NOTIFY: {
        xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *) xev;
        out->kind = IswDestroy;
        out->structure.target = e->window;
        return True;
    }
    case XCB_VISIBILITY_NOTIFY: {
        xcb_visibility_notify_event_t *e = (xcb_visibility_notify_event_t *) xev;
        out->kind = IswVisibility;
        out->structure.target = e->window;
        out->structure.visibility = e->state;
        return True;
    }
    default:
        /* X11 protocol event — not a toolkit-semantic IswEvent. */
        return False;
    }
}

/* Migration bridge (declared in IswEvent.h). */
void *
IswEventNative(const IswEvent *event)
{
    return event ? event->any.native : NULL;
}
