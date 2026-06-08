/*
 * ISWPlatformDisplayXCB.c - XCB backend for the display + window platform ops
 *
 * Copyright (c) 2026 ISW Project
 *
 * Implements IswPlatformDisplayOps + IswPlatformWindowOps over XCB, and the
 * INTERNAL backend seam (_IswXcbConn / _IswXcbScreen / _IswXcbWindow / ...)
 * that not-yet-abstracted toolkit categories use to reach the native
 * connection while they await their own phase (atoms→6, color/font→4,
 * selection/cursor/grab→5, input→3).  The seam is declared only in the
 * src/-internal ISWPlatformPrivate.h — never in a public include/ISW/ header —
 * so the public API is fully decoupled from XCB even though the implementation
 * still is.  The seam shrinks phase by phase and is deleted after Phase 6.
 *
 * Handle representation (XCB backend only) — every handle is a native value
 * reinterpreted, never a separately-allocated wrapper, so the seam conversions
 * are plain casts and the handles stay one word:
 *   IswDisplay = xcb_connection_t* reinterpreted as the opaque IswDisplay.
 *                (core.display and the conn the dispatch/per-display layers
 *                carry are thus the same value — no lookup, no divergence.)
 *   IswScreen  = xcb_screen_t* reinterpreted as the opaque IswScreen.
 *   IswWindow  = xcb_window_t (a 32-bit id) reinterpreted as the opaque
 *                IswWindow pointer.  Never dereferenced.
 *
 * Phase 2 of the ISWPlatform vtable (docs/ISWPLATFORM_PLAN.md).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "IntrinsicI.h"
#include <ISW/InitialI.h>
#include "ISWPlatformPrivate.h"

/* ---- handle representation ------------------------------------------------
 *
 * Like IswWindow (the window id reinterpreted) and IswScreen (an xcb_screen_t*
 * reinterpreted), IswDisplay IS the xcb_connection_t* reinterpreted — there is
 * no separate wrapper struct.  This keeps the handle a single word, makes
 * core.display interchangeable with the connection the dispatch/per-display
 * layers carry, and turns the seam conversions into plain casts (no
 * dereference, so a stale/mis-sourced value can never crash).  A non-XCB
 * backend would map IswDisplay to its own connection object the same way.
 */

/* ---- handle <-> native conversions (the internal seam) ------------------- */

xcb_connection_t *
_IswXcbConn(IswDisplay dpy)
{
    return (xcb_connection_t *) dpy;
}

xcb_screen_t *
_IswXcbScreen(IswScreen screen)
{
    return (xcb_screen_t *) screen;
}

/* Default screen of a display, as a native xcb_screen_t.  The authoritative
   default-screen INDEX lives in the per-display table (set from xcb_connect's
   screen-number output); the toolkit's _IswGetDefaultScreen consults it.  This
   backend helper, used only to pick a root_visual at window-create, returns the
   first root, which is correct for the common single-screen case. */
xcb_screen_t *
_IswXcbDefaultScreen(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (!conn)
        return NULL;
    return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

xcb_window_t
_IswXcbWindow(IswWindow win)
{
    return (xcb_window_t) (uintptr_t) win;
}

IswWindow
_IswXcbWindowWrap(xcb_window_t id)
{
    return (IswWindow) (uintptr_t) id;
}

/* ---- display ops --------------------------------------------------------- */

static IswDisplay
xcb_disp_open(const char *display_name, int *default_screen)
{
    int scr = 0;
    xcb_connection_t *conn;

    conn = xcb_connect(display_name, &scr);
    if (conn == NULL || xcb_connection_has_error(conn)) {
        if (conn)
            xcb_disconnect(conn);
        return NULL;
    }
    if (default_screen)
        *default_screen = scr;
    return (IswDisplay) conn;
}

static void
xcb_disp_close(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (conn) {
        xcb_flush(conn);
        xcb_disconnect(conn);
    }
}

static Boolean
xcb_disp_has_error(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    return conn ? (xcb_connection_has_error(conn) != 0) : True;
}

static void
xcb_disp_flush(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (conn)
        xcb_flush(conn);
}

static int
xcb_disp_connection_fd(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    return conn ? xcb_get_file_descriptor(conn) : -1;
}

static int
xcb_disp_screen_count(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    return conn ? xcb_setup_roots_length(xcb_get_setup(conn)) : 0;
}

static IswScreen
xcb_disp_screen(IswDisplay dpy, int index)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    xcb_screen_iterator_t it;
    int i;
    if (!conn || index < 0)
        return NULL;
    it = xcb_setup_roots_iterator(xcb_get_setup(conn));
    for (i = 0; i < index && it.rem; i++)
        xcb_screen_next(&it);
    return it.rem ? (IswScreen) it.data : NULL;
}

static IswWindow
xcb_disp_root_window(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? _IswXcbWindowWrap(s->root) : _IswXcbWindowWrap(0);
}

static uint32_t
xcb_disp_screen_width(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? s->width_in_pixels : 0;
}

static uint32_t
xcb_disp_screen_height(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? s->height_in_pixels : 0;
}

static void
xcb_disp_bell(IswDisplay dpy, int percent)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (conn) {
        xcb_bell(conn, (int8_t) percent);
        xcb_flush(conn);
    }
}

static const IswPlatformDisplayOps xcb_display_ops = {
    .open           = xcb_disp_open,
    .close          = xcb_disp_close,
    .has_error      = xcb_disp_has_error,
    .flush          = xcb_disp_flush,
    .connection_fd  = xcb_disp_connection_fd,
    .screen_count   = xcb_disp_screen_count,
    .screen         = xcb_disp_screen,
    .root_window    = xcb_disp_root_window,
    .screen_width   = xcb_disp_screen_width,
    .screen_height  = xcb_disp_screen_height,
    .bell           = xcb_disp_bell,
};

/* ---- window ops ---------------------------------------------------------- */

/* Translate the neutral attribute subset into an XCB value-mask/value-list,
   in the canonical XCB ordering.  Returns the number of values written. */
static uint32_t
attrs_to_values(const IswWindowAttributes *a, unsigned int mask,
                uint32_t *out_value_mask, uint32_t values[8])
{
    uint32_t vm = 0;
    int n = 0;
    /* XCB requires values in increasing CW_* bit order. */
    if (mask & ISW_ATTR_BACK_PIXEL) {
        vm |= XCB_CW_BACK_PIXEL;        values[n++] = a->background_pixel;
    }
    if (mask & ISW_ATTR_BORDER_PIXEL) {
        vm |= XCB_CW_BORDER_PIXEL;      values[n++] = a->border_pixel;
    }
    if (mask & ISW_ATTR_OVERRIDE) {
        vm |= XCB_CW_OVERRIDE_REDIRECT; values[n++] = a->override_redirect ? 1 : 0;
    }
    if (mask & ISW_ATTR_SAVE_UNDER) {
        vm |= XCB_CW_SAVE_UNDER;        values[n++] = a->save_under ? 1 : 0;
    }
    if (mask & ISW_ATTR_EVENT_MASK) {
        vm |= XCB_CW_EVENT_MASK;        values[n++] = a->event_mask;
    }
    *out_value_mask = vm;
    return (uint32_t) n;
}

static IswWindow
xcb_win_alloc_id(IswDisplay dpy)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return _IswXcbWindowWrap(0);
    return _IswXcbWindowWrap(xcb_generate_id(conn));
}

static IswWindow
xcb_win_create(IswDisplay dpy, IswWindow parent,
               const IswWindowGeometry *geom,
               const IswWindowAttributes *attrs)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    xcb_screen_t *s = _IswXcbDefaultScreen(dpy);
    xcb_window_t id;
    uint32_t value_mask = 0, values[8];
    uint32_t nv;

    if (!c || !s)
        return _IswXcbWindowWrap(0);

    id = xcb_generate_id(c);
    nv = attrs ? attrs_to_values(attrs, ISW_ATTR_BACK_PIXEL |
                                 ISW_ATTR_BORDER_PIXEL | ISW_ATTR_OVERRIDE |
                                 ISW_ATTR_SAVE_UNDER | ISW_ATTR_EVENT_MASK,
                                 &value_mask, values)
                : 0;
    (void) nv;
    xcb_create_window(c, XCB_COPY_FROM_PARENT, id, _IswXcbWindow(parent),
                      (int16_t) geom->x, (int16_t) geom->y,
                      (uint16_t) geom->width, (uint16_t) geom->height,
                      (uint16_t) geom->border_width,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      s->root_visual, value_mask, values);
    return _IswXcbWindowWrap(id);
}

static void
xcb_win_destroy(IswDisplay dpy, IswWindow win)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_destroy_window(c, _IswXcbWindow(win));
}

static void
xcb_win_map(IswDisplay dpy, IswWindow win)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_map_window(c, _IswXcbWindow(win));
}

static void
xcb_win_unmap(IswDisplay dpy, IswWindow win)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_unmap_window(c, _IswXcbWindow(win));
}

static void
xcb_win_reparent(IswDisplay dpy, IswWindow win, IswWindow new_parent,
                 int32_t x, int32_t y)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_reparent_window(c, _IswXcbWindow(win), _IswXcbWindow(new_parent),
                            (int16_t) x, (int16_t) y);
}

static void
xcb_win_configure(IswDisplay dpy, IswWindow win,
                  const IswWindowGeometry *geom, unsigned int mask,
                  IswStackMode stack, IswWindow sibling)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    uint32_t cm = 0, values[7];
    int n = 0;

    if (!c)
        return;
    /* XCB requires values in increasing CONFIG_WINDOW_* bit order. */
    if (mask & ISW_CONFIG_X)      { cm |= XCB_CONFIG_WINDOW_X;
                                    values[n++] = (uint32_t)(int32_t) geom->x; }
    if (mask & ISW_CONFIG_Y)      { cm |= XCB_CONFIG_WINDOW_Y;
                                    values[n++] = (uint32_t)(int32_t) geom->y; }
    if (mask & ISW_CONFIG_WIDTH)  { cm |= XCB_CONFIG_WINDOW_WIDTH;
                                    values[n++] = geom->width; }
    if (mask & ISW_CONFIG_HEIGHT) { cm |= XCB_CONFIG_WINDOW_HEIGHT;
                                    values[n++] = geom->height; }
    if (mask & ISW_CONFIG_BORDER) { cm |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
                                    values[n++] = geom->border_width; }
    if ((mask & ISW_CONFIG_STACK) && sibling != NULL) {
        cm |= XCB_CONFIG_WINDOW_SIBLING; values[n++] = _IswXcbWindow(sibling);
    }
    if ((mask & ISW_CONFIG_STACK) && stack != ISW_STACK_NONE) {
        cm |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[n++] = (stack == ISW_STACK_ABOVE) ? XCB_STACK_MODE_ABOVE
                                                  : XCB_STACK_MODE_BELOW;
    }
    if (cm)
        xcb_configure_window(c, _IswXcbWindow(win), (uint16_t) cm, values);
}

static void
xcb_win_change_attributes(IswDisplay dpy, IswWindow win,
                          const IswWindowAttributes *attrs, unsigned int mask)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    uint32_t value_mask = 0, values[8];
    if (!c || !attrs)
        return;
    attrs_to_values(attrs, mask, &value_mask, values);
    if (value_mask)
        xcb_change_window_attributes(c, _IswXcbWindow(win), value_mask, values);
}

static void
xcb_win_clear_area(IswDisplay dpy, IswWindow win,
                   int16_t x, int16_t y, uint16_t w, uint16_t h,
                   Boolean generate_expose)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_clear_area(c, generate_expose ? 1 : 0, _IswXcbWindow(win),
                       x, y, w, h);
}

static IswWindowId
xcb_win_window_id(IswWindow win)
{
    return (IswWindowId) _IswXcbWindow(win);
}

static IswWindow
xcb_win_window_from_id(IswWindowId id)
{
    return _IswXcbWindowWrap((xcb_window_t) id);
}

static const IswPlatformWindowOps xcb_window_ops = {
    .alloc_id           = xcb_win_alloc_id,
    .create             = xcb_win_create,
    .destroy            = xcb_win_destroy,
    .map                = xcb_win_map,
    .unmap              = xcb_win_unmap,
    .reparent           = xcb_win_reparent,
    .configure          = xcb_win_configure,
    .change_attributes  = xcb_win_change_attributes,
    .clear_area         = xcb_win_clear_area,
    .window_id          = xcb_win_window_id,
    .window_from_id     = xcb_win_window_from_id,
};

/* ---- backend vtable + dispatcher ----------------------------------------- */

extern const IswPlatformInputOps isw_platform_xcb_input_ops; /* ISWPlatformInputXCB.c */
extern const IswPlatformColorOps isw_platform_xcb_color_ops; /* ISWPlatformColorFontXCB.c */
extern const IswPlatformFontOps  isw_platform_xcb_font_ops;  /* ISWPlatformColorFontXCB.c */
extern const IswPlatformCursorOps    isw_platform_xcb_cursor_ops;    /* ISWPlatformGrabCursorXCB.c */
extern const IswPlatformGrabOps      isw_platform_xcb_grab_ops;      /* ISWPlatformGrabCursorXCB.c */
extern const IswPlatformSelectionOps isw_platform_xcb_selection_ops; /* ISWPlatformGrabCursorXCB.c */
extern const IswPlatformAtomOps      isw_platform_xcb_atom_ops;      /* ISWPlatformAtomPropXCB.c */
extern const IswPlatformPropertyOps  isw_platform_xcb_property_ops;  /* ISWPlatformAtomPropXCB.c */
extern const IswPlatformHintOps      isw_platform_xcb_hint_ops;      /* ISWPlatformAtomPropXCB.c */
extern const IswPlatformDndOps       isw_platform_xcb_dnd_ops;       /* ISWPlatformDndXCB.c */

const IswPlatformOps isw_platform_xcb_ops = {
    .display   = &xcb_display_ops,
    .window    = &xcb_window_ops,
    .event     = NULL,   /* Phase 1 translator is standalone for now */
    .input     = &isw_platform_xcb_input_ops,
    .selection = &isw_platform_xcb_selection_ops,
    .color     = &isw_platform_xcb_color_ops,
    .font      = &isw_platform_xcb_font_ops,
    .cursor    = &isw_platform_xcb_cursor_ops,
    .grab      = &isw_platform_xcb_grab_ops,
    .atom      = &isw_platform_xcb_atom_ops,
    .property  = &isw_platform_xcb_property_ops,
    .hint      = &isw_platform_xcb_hint_ops,
    .dnd       = &isw_platform_xcb_dnd_ops,
};

/* Neutral event-loop fd accessor (replaces the ConnectionNumber XCB macro),
   dispatched through the active backend's display vtable. */
int
_IswPlatformConnectionFd(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->connection_fd)
        return ops->display->connection_fd(dpy);
    return -1;
}

/* ---- thin dispatch wrappers (color / font / cursor / grab / selection) ----
   Toolkit/widget code calls these instead of walking the ops vtable.  Each
   null-guards the sub-vtable + op (a backend that hasn't filled it degrades to
   no-op / failure).  Same convention as _IswPlatformConnectionFd. */

/* Color (Phase 4) */
Boolean
_IswPlatformQueryColor(IswDisplay dpy, IswColormap cmap,
                       unsigned long pixel, IswColor *out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->query_color)
        return ops->color->query_color(dpy, cmap, pixel, out);
    return False;
}

Boolean
_IswPlatformAllocColor(IswDisplay dpy, IswColormap cmap,
                       unsigned short red, unsigned short green,
                       unsigned short blue, unsigned long *pixel_out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->alloc_color)
        return ops->color->alloc_color(dpy, cmap, red, green, blue, pixel_out);
    return False;
}

Boolean
_IswPlatformAllocNamedColor(IswDisplay dpy, IswColormap cmap,
                            const char *name, unsigned long *pixel_out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->alloc_named_color)
        return ops->color->alloc_named_color(dpy, cmap, name, pixel_out);
    return False;
}

Boolean
_IswPlatformLookupColor(IswDisplay dpy, IswColormap cmap, const char *name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->lookup_color)
        return ops->color->lookup_color(dpy, cmap, name);
    return False;
}

void
_IswPlatformFreeColors(IswDisplay dpy, IswColormap cmap, unsigned long pixel)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->free_colors)
        ops->color->free_colors(dpy, cmap, pixel);
}

Boolean
_IswPlatformMatchVisualInfo(IswDisplay dpy, IswScreen screen,
                            int depth, int visual_class, IswVisualInfo *out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->match_visual_info)
        return ops->color->match_visual_info(dpy, screen, depth,
                                             visual_class, out);
    return False;
}

/* Font (Phase 4) */
IswFontId
_IswPlatformLoadFont(IswDisplay dpy, const char *name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->font && ops->font->load_font)
        return ops->font->load_font(dpy, name);
    return 0;
}

void
_IswPlatformFreeFont(IswDisplay dpy, IswFontId fid)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->font && ops->font->free_font)
        ops->font->free_font(dpy, fid);
}

/* Cursor (Phase 5) */
IswCursor
_IswPlatformLoadNamedCursor(IswDisplay dpy, IswScreen screen,
                            const char *name, unsigned int fallback_shape)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->cursor && ops->cursor->load_named)
        return ops->cursor->load_named(dpy, screen, name, fallback_shape);
    return 0;
}

void
_IswPlatformSetWindowCursor(IswDisplay dpy, IswWindow win, IswCursor cursor)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->cursor && ops->cursor->set_window_cursor)
        ops->cursor->set_window_cursor(dpy, win, cursor);
}

void
_IswPlatformFreeCursor(IswDisplay dpy, IswCursor cursor)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->cursor && ops->cursor->free_cursor)
        ops->cursor->free_cursor(dpy, cursor);
}

/* Grabs (Phase 5) */
int
_IswPlatformGrabPointer(IswDisplay dpy, IswWindow grab_window,
                        Boolean owner_events, unsigned int event_mask,
                        int pointer_mode, int keyboard_mode,
                        IswWindow confine_to, IswCursor cursor, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_pointer)
        return ops->grab->grab_pointer(dpy, grab_window, owner_events,
                                       event_mask, pointer_mode, keyboard_mode,
                                       confine_to, cursor, time);
    return -1;
}

void
_IswPlatformUngrabPointer(IswDisplay dpy, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->ungrab_pointer)
        ops->grab->ungrab_pointer(dpy, time);
}

int
_IswPlatformGrabKeyboard(IswDisplay dpy, IswWindow grab_window,
                         Boolean owner_events, int pointer_mode,
                         int keyboard_mode, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_keyboard)
        return ops->grab->grab_keyboard(dpy, grab_window, owner_events,
                                        pointer_mode, keyboard_mode, time);
    return -1;
}

void
_IswPlatformUngrabKeyboard(IswDisplay dpy, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->ungrab_keyboard)
        ops->grab->ungrab_keyboard(dpy, time);
}

void
_IswPlatformGrabButton(IswDisplay dpy, IswWindow grab_window, int button,
                       unsigned int modifiers, Boolean owner_events,
                       unsigned int event_mask, int pointer_mode,
                       int keyboard_mode, IswWindow confine_to, IswCursor cursor)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_button)
        ops->grab->grab_button(dpy, grab_window, button, modifiers,
                               owner_events, event_mask, pointer_mode,
                               keyboard_mode, confine_to, cursor);
}

void
_IswPlatformGrabKey(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                    unsigned int modifiers, Boolean owner_events,
                    int pointer_mode, int keyboard_mode)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_key)
        ops->grab->grab_key(dpy, grab_window, keycode, modifiers,
                            owner_events, pointer_mode, keyboard_mode);
}

/* Selection (Phase 5) */
void
_IswPlatformSetSelectionOwner(IswDisplay dpy, IswWindow owner,
                              Atom selection, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->set_owner)
        ops->selection->set_owner(dpy, owner, selection, time);
}

IswWindow
_IswPlatformGetSelectionOwner(IswDisplay dpy, Atom selection)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->get_owner)
        return ops->selection->get_owner(dpy, selection);
    return _IswXcbWindowWrap(XCB_NONE);
}

void
_IswPlatformConvertSelection(IswDisplay dpy, IswWindow requestor,
                             Atom selection, Atom target,
                             Atom property, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->convert)
        ops->selection->convert(dpy, requestor, selection, target,
                                property, time);
}

/* Atom (Phase 6) */
Atom
_IswPlatformInternAtomOp(IswDisplay dpy, const char *name, Boolean only_if_exists)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->atom && ops->atom->intern)
        return ops->atom->intern(dpy, name, only_if_exists);
    return ISW_ATOM_NONE;
}

Boolean
_IswPlatformGetAtomName(IswDisplay dpy, Atom atom, char *buf, size_t buflen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->atom && ops->atom->get_name)
        return ops->atom->get_name(dpy, atom, buf, buflen);
    return False;
}

/* Property (Phase 6) */
void
_IswPlatformChangeProperty(IswDisplay dpy, IswWindow win, Atom property,
                           Atom type, int format, IswPropMode mode,
                           const void *data, uint32_t num_elements)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->property && ops->property->change)
        ops->property->change(dpy, win, property, type, format, mode,
                              data, num_elements);
}

Boolean
_IswPlatformGetProperty(IswDisplay dpy, IswWindow win, Atom property, Atom type,
                        uint32_t long_offset, uint32_t long_length,
                        IswProperty *out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->property && ops->property->get)
        return ops->property->get(dpy, win, property, type,
                                  long_offset, long_length, out);
    if (out) memset(out, 0, sizeof(*out));
    return False;
}

void
_IswPlatformDeleteProperty(IswDisplay dpy, IswWindow win, Atom property)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->property && ops->property->delete_)
        ops->property->delete_(dpy, win, property);
}

/* WM hints (Phase 6) */
void
_IswPlatformSetWindowTitle(IswDisplay dpy, IswWindow win, const char *utf8)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_window_title)
        ops->hint->set_window_title(dpy, win, utf8);
}

void
_IswPlatformSetIconTitle(IswDisplay dpy, IswWindow win, const char *utf8)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_icon_title)
        ops->hint->set_icon_title(dpy, win, utf8);
}

void
_IswPlatformSetWmClass(IswDisplay dpy, IswWindow win,
                       const char *name, const char *class_name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_wm_class)
        ops->hint->set_wm_class(dpy, win, name, class_name);
}

void
_IswPlatformSetWmProtocols(IswDisplay dpy, IswWindow win,
                           const Atom *protocols, int num_protocols)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_wm_protocols)
        ops->hint->set_wm_protocols(dpy, win, protocols, num_protocols);
}

void
_IswPlatformSetTransientFor(IswDisplay dpy, IswWindow win, IswWindow leader)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_transient_for)
        ops->hint->set_transient_for(dpy, win, leader);
}

void
_IswPlatformSetWindowType(IswDisplay dpy, IswWindow win, IswWindowType type)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_window_type)
        ops->hint->set_window_type(dpy, win, type);
}

void
_IswPlatformSetPid(IswDisplay dpy, IswWindow win, uint32_t pid)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_pid)
        ops->hint->set_pid(dpy, win, pid);
}

void
_IswPlatformSetNormalHints(IswDisplay dpy, IswWindow win, uint32_t flags,
                           int x, int y, int width, int height,
                           int min_width, int min_height,
                           int max_width, int max_height,
                           int width_inc, int height_inc,
                           int min_aspect_num, int min_aspect_den,
                           int max_aspect_num, int max_aspect_den,
                           int base_width, int base_height, int win_gravity)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_normal_hints)
        ops->hint->set_normal_hints(dpy, win, flags, x, y, width, height,
                                    min_width, min_height, max_width, max_height,
                                    width_inc, height_inc,
                                    min_aspect_num, min_aspect_den,
                                    max_aspect_num, max_aspect_den,
                                    base_width, base_height, win_gravity);
}

/* Drag-and-drop (Phase 7) */
void
_IswPlatformDndEnable(Widget shell)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(shell))->ops;
    if (ops && ops->dnd && ops->dnd->enable)
        ops->dnd->enable(shell);
}

void
_IswPlatformDndWidgetAcceptDrops(Widget w)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->widget_accept_drops)
        ops->dnd->widget_accept_drops(w);
}

void
_IswPlatformDndStartDrag(Widget source, IswEvent *trigger,
                         IswDragSourceDesc *desc)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(source))->ops;
    if (ops && ops->dnd && ops->dnd->start_drag)
        ops->dnd->start_drag(source, trigger, desc);
}

void
_IswPlatformDndSetAcceptedTypes(Widget w, Atom *types, int num_types)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_accepted_types)
        ops->dnd->set_accepted_types(w, types, num_types);
}

void
_IswPlatformDndSetAcceptedActions(Widget w, IswDndAction actions)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_accepted_actions)
        ops->dnd->set_accepted_actions(w, actions);
}

void
_IswPlatformDndSetDropCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_drop_callback)
        ops->dnd->set_drop_callback(w, proc, closure);
}

void
_IswPlatformDndSetDragMotionCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_drag_motion_callback)
        ops->dnd->set_drag_motion_callback(w, proc, closure);
}

void
_IswPlatformDndSetDragLeaveCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_drag_leave_callback)
        ops->dnd->set_drag_leave_callback(w, proc, closure);
}

Atom
_IswPlatformDndInternType(Widget w, const char *mime_type)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->intern_type)
        return ops->dnd->intern_type(w, mime_type);
    return ISW_ATOM_NONE;
}

Boolean
_IswPlatformDndIsDragging(Widget w)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->is_dragging)
        return ops->dnd->is_dragging(w);
    return False;
}
