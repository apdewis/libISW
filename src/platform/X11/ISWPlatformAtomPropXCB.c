/*
 * ISWPlatformAtomPropXCB.c - XCB backend for atom, property, and WM-hint ops
 *
 * Copyright (c) 2026 ISW Project
 *
 * Implements IswPlatformAtomOps (intern / name), IswPlatformPropertyOps
 * (change / get / delete window properties, returning a neutral IswProperty),
 * and IswPlatformHintOps (semantic ICCCM/EWMH hints: title, icon title, class,
 * protocols, transient-for, window type, pid, normal/size hints, wm hints).
 * Atoms are neutral (Atom == uint32_t, numerically X11-compatible).  The
 * niche-EWMH long tail is NOT here — it rides the generic property ops with the
 * atom interned via the atom op (see docs/PHASE6_SCOPE.md).
 *
 * Phase 6 of the ISWPlatform vtable (docs/ISWPLATFORM_PLAN.md).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"

/* Interned once per backend (the standard EWMH/ICCCM atoms the hint ops set).
   X interns are stable per server; cached behind a small helper. */
static xcb_atom_t
intern_cached(xcb_connection_t *conn, const char *name)
{
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0,
                                                      (uint16_t) strlen(name),
                                                      name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);
    xcb_atom_t atom = XCB_ATOM_NONE;
    if (reply) {
        atom = reply->atom;
        free(reply);
    }
    return atom;
}

/* ---- atom ops ------------------------------------------------------------ */

static Atom
xcb_atom_intern(IswDisplay dpy, const char *name, Boolean only_if_exists)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    Atom atom = ISW_ATOM_NONE;

    if (!conn || !name)
        return ISW_ATOM_NONE;
    cookie = xcb_intern_atom(conn, only_if_exists ? 1 : 0,
                             (uint16_t) strlen(name), name);
    reply = xcb_intern_atom_reply(conn, cookie, NULL);
    if (reply) {
        atom = (Atom) reply->atom;
        free(reply);
    }
    return atom;
}

static Boolean
xcb_atom_get_name(IswDisplay dpy, Atom atom, char *buf, size_t buflen)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_get_atom_name_cookie_t cookie;
    xcb_get_atom_name_reply_t *reply;
    int len;
    const char *name;

    if (!conn || atom == ISW_ATOM_NONE || !buf || buflen == 0)
        return False;
    cookie = xcb_get_atom_name(conn, (xcb_atom_t) atom);
    reply = xcb_get_atom_name_reply(conn, cookie, NULL);
    if (!reply)
        return False;
    len = xcb_get_atom_name_name_length(reply);
    name = xcb_get_atom_name_name(reply);
    if ((size_t) len >= buflen)
        len = (int) buflen - 1;
    memcpy(buf, name, (size_t) len);
    buf[len] = '\0';
    free(reply);
    return True;
}

/* ---- property ops -------------------------------------------------------- */

static void
xcb_prop_change(IswDisplay dpy, IswWindow win, Atom property, Atom type,
                int format, IswPropMode mode,
                const void *data, uint32_t num_elements)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_change_property(conn, (uint8_t) mode, _IswXcbWindow(win),
                        (xcb_atom_t) property, (xcb_atom_t) type,
                        (uint8_t) format, num_elements, data);
}

static Boolean
xcb_prop_get(IswDisplay dpy, IswWindow win, Atom property, Atom type,
             uint32_t long_offset, uint32_t long_length, IswProperty *out)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    int vlen;
    void *vsrc;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!conn || !out)
        return False;
    cookie = xcb_get_property(conn, 0, _IswXcbWindow(win),
                              (xcb_atom_t) property, (xcb_atom_t) type,
                              long_offset, long_length);
    reply = xcb_get_property_reply(conn, cookie, NULL);
    if (!reply)
        return False;

    out->type        = (Atom) reply->type;
    out->format      = reply->format;
    out->bytes_after = reply->bytes_after;
    vlen = xcb_get_property_value_length(reply);
    vsrc = xcb_get_property_value(reply);
    /* num_items in format-bit units (X reports value_len in those units). */
    out->num_items = reply->value_len;
    if (vlen > 0 && vsrc) {
        out->value = malloc((size_t) vlen);
        if (out->value)
            memcpy(out->value, vsrc, (size_t) vlen);
    }
    free(reply);
    return True;
}

static void
xcb_prop_delete(IswDisplay dpy, IswWindow win, Atom property)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_delete_property(conn, _IswXcbWindow(win), (xcb_atom_t) property);
}

/* ---- hint ops ------------------------------------------------------------ */

/* WM_NAME (Latin-1 slot) + _NET_WM_NAME (UTF8_STRING). */
static void
xcb_hint_set_window_title(IswDisplay dpy, IswWindow win, const char *utf8)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_window_t w = _IswXcbWindow(win);
    size_t len;

    if (!conn || !utf8)
        return;
    len = strlen(utf8);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        (uint32_t) len, utf8);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w,
                        intern_cached(conn, "_NET_WM_NAME"),
                        intern_cached(conn, "UTF8_STRING"), 8,
                        (uint32_t) len, utf8);
}

static void
xcb_hint_set_icon_title(IswDisplay dpy, IswWindow win, const char *utf8)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_window_t w = _IswXcbWindow(win);
    size_t len;

    if (!conn || !utf8)
        return;
    len = strlen(utf8);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w,
                        XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8,
                        (uint32_t) len, utf8);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w,
                        intern_cached(conn, "_NET_WM_ICON_NAME"),
                        intern_cached(conn, "UTF8_STRING"), 8,
                        (uint32_t) len, utf8);
}

/* WM_CLASS = "name\0class\0". */
static void
xcb_hint_set_wm_class(IswDisplay dpy, IswWindow win,
                      const char *name, const char *class_name)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    size_t nlen, clen;
    char *buf;

    if (!conn)
        return;
    nlen = name ? strlen(name) : 0;
    clen = class_name ? strlen(class_name) : 0;
    buf = malloc(nlen + clen + 2);
    if (!buf)
        return;
    if (nlen) memcpy(buf, name, nlen);
    buf[nlen] = '\0';
    if (clen) memcpy(buf + nlen + 1, class_name, clen);
    buf[nlen + 1 + clen] = '\0';
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(win),
                        XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
                        (uint32_t) (nlen + clen + 2), buf);
    free(buf);
}

static void
xcb_hint_set_wm_protocols(IswDisplay dpy, IswWindow win,
                          const Atom *protocols, int num_protocols)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_atom_t wm_protocols;

    if (!conn || !protocols || num_protocols <= 0)
        return;
    wm_protocols = intern_cached(conn, "WM_PROTOCOLS");
    /* Atom == uint32_t == xcb_atom_t, so the array is layout-compatible. */
    xcb_icccm_set_wm_protocols(conn, _IswXcbWindow(win), wm_protocols,
                               (uint32_t) num_protocols,
                               (xcb_atom_t *) protocols);
}

static void
xcb_hint_set_transient_for(IswDisplay dpy, IswWindow win, IswWindow leader)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_window_t leader_id = _IswXcbWindow(leader);
    if (!conn)
        return;
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(win),
                        XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32,
                        1, &leader_id);
}

static void
xcb_hint_set_window_type(IswDisplay dpy, IswWindow win, IswWindowType type)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    const char *name;
    xcb_atom_t type_atom, prop_atom;

    if (!conn)
        return;
    switch (type) {
        case ISW_WINDOW_TYPE_DIALOG:
            name = "_NET_WM_WINDOW_TYPE_DIALOG"; break;
        case ISW_WINDOW_TYPE_TOOLTIP:
            name = "_NET_WM_WINDOW_TYPE_TOOLTIP"; break;
        case ISW_WINDOW_TYPE_MENU:
            name = "_NET_WM_WINDOW_TYPE_MENU"; break;
        case ISW_WINDOW_TYPE_POPUP_MENU:
            name = "_NET_WM_WINDOW_TYPE_POPUP_MENU"; break;
        case ISW_WINDOW_TYPE_UTILITY:
            name = "_NET_WM_WINDOW_TYPE_UTILITY"; break;
        case ISW_WINDOW_TYPE_DOCK:
            name = "_NET_WM_WINDOW_TYPE_DOCK"; break;
        case ISW_WINDOW_TYPE_DESKTOP:
            name = "_NET_WM_WINDOW_TYPE_DESKTOP"; break;
        case ISW_WINDOW_TYPE_TOOLBAR:
            name = "_NET_WM_WINDOW_TYPE_TOOLBAR"; break;
        case ISW_WINDOW_TYPE_SPLASH:
            name = "_NET_WM_WINDOW_TYPE_SPLASH"; break;
        case ISW_WINDOW_TYPE_NOTIFICATION:
            name = "_NET_WM_WINDOW_TYPE_NOTIFICATION"; break;
        case ISW_WINDOW_TYPE_DROPDOWN_MENU:
            name = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"; break;
        case ISW_WINDOW_TYPE_COMBO:
            name = "_NET_WM_WINDOW_TYPE_COMBO"; break;
        case ISW_WINDOW_TYPE_NORMAL:
        default:
            name = "_NET_WM_WINDOW_TYPE_NORMAL"; break;
    }
    prop_atom = intern_cached(conn, "_NET_WM_WINDOW_TYPE");
    type_atom = intern_cached(conn, name);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(win),
                        prop_atom, XCB_ATOM_ATOM, 32, 1, &type_atom);
}

static void
xcb_hint_set_pid(IswDisplay dpy, IswWindow win, uint32_t pid)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(win),
                        intern_cached(conn, "_NET_WM_PID"),
                        XCB_ATOM_CARDINAL, 32, 1, &pid);
}

static void
xcb_hint_set_normal_hints(IswDisplay dpy, IswWindow win, uint32_t flags,
                          int x, int y, int width, int height,
                          int min_width, int min_height,
                          int max_width, int max_height,
                          int width_inc, int height_inc,
                          int min_aspect_num, int min_aspect_den,
                          int max_aspect_num, int max_aspect_den,
                          int base_width, int base_height, int win_gravity)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_size_hints_t hints;

    if (!conn)
        return;
    memset(&hints, 0, sizeof(hints));
    hints.flags = flags;
    hints.x = x; hints.y = y;
    hints.width = width; hints.height = height;
    hints.min_width = min_width; hints.min_height = min_height;
    hints.max_width = max_width; hints.max_height = max_height;
    hints.width_inc = width_inc; hints.height_inc = height_inc;
    hints.min_aspect_num = min_aspect_num; hints.min_aspect_den = min_aspect_den;
    hints.max_aspect_num = max_aspect_num; hints.max_aspect_den = max_aspect_den;
    hints.base_width = base_width; hints.base_height = base_height;
    hints.win_gravity = (uint32_t) win_gravity;
    xcb_icccm_set_wm_normal_hints(conn, _IswXcbWindow(win), &hints);
}

/* Full WM_HINTS.  The neutral IswWmHints mirrors xcb_icccm_wm_hints_t field for
   field with X-compatible flag bits, so it copies straight across. */
static void
xcb_hint_set_wm_hints(IswDisplay dpy, IswWindow win, const IswWmHints *h)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_icccm_wm_hints_t x;

    if (!conn || !h)
        return;
    memset(&x, 0, sizeof(x));
    x.flags         = h->flags;
    x.input         = h->input;
    x.initial_state = h->initial_state;
    x.icon_pixmap   = (xcb_pixmap_t) h->icon_pixmap;
    x.icon_window   = _IswXcbWindow(h->icon_window);
    x.icon_x        = h->icon_x;
    x.icon_y        = h->icon_y;
    x.icon_mask     = (xcb_pixmap_t) h->icon_mask;
    x.window_group  = _IswXcbWindow(h->window_group);
    xcb_icccm_set_wm_hints(conn, _IswXcbWindow(win), &x);
}

static void
xcb_hint_set_strut_partial(IswDisplay dpy, IswWindow win,
                           const IswStrutPartial *strut)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn || !strut)
        return;

    uint32_t data[12] = {
        strut->left,         strut->right,
        strut->top,          strut->bottom,
        strut->left_start_y, strut->left_end_y,
        strut->right_start_y, strut->right_end_y,
        strut->top_start_x,  strut->top_end_x,
        strut->bottom_start_x, strut->bottom_end_x
    };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(win),
                        intern_cached(conn, "_NET_WM_STRUT_PARTIAL"),
                        XCB_ATOM_CARDINAL, 32, 12, data);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(win),
                        intern_cached(conn, "_NET_WM_STRUT"),
                        XCB_ATOM_CARDINAL, 32, 4, data);
}

/* ---- IswProperty release (backend-neutral; payload is plain malloc) ------- */

void
_IswPlatformFreeProperty(IswProperty *prop)
{
    if (prop && prop->value) {
        free(prop->value);
        prop->value = NULL;
    }
}

/* ---- vtables ------------------------------------------------------------- */

const IswPlatformAtomOps isw_platform_xcb_atom_ops = {
    .intern   = xcb_atom_intern,
    .get_name = xcb_atom_get_name,
};

const IswPlatformPropertyOps isw_platform_xcb_property_ops = {
    .change  = xcb_prop_change,
    .get     = xcb_prop_get,
    .delete_ = xcb_prop_delete,
};

const IswPlatformHintOps isw_platform_xcb_hint_ops = {
    .set_window_title  = xcb_hint_set_window_title,
    .set_icon_title    = xcb_hint_set_icon_title,
    .set_wm_class      = xcb_hint_set_wm_class,
    .set_wm_protocols  = xcb_hint_set_wm_protocols,
    .set_transient_for = xcb_hint_set_transient_for,
    .set_window_type   = xcb_hint_set_window_type,
    .set_pid           = xcb_hint_set_pid,
    .set_normal_hints  = xcb_hint_set_normal_hints,
    .set_wm_hints      = xcb_hint_set_wm_hints,
    .set_strut_partial = xcb_hint_set_strut_partial,
};
