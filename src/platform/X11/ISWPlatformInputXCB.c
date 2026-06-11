/*
 * ISWPlatformInputXCB.c - XCB backend for the input platform ops
 *
 * Copyright (c) 2026 ISW Project
 *
 * Implements IswPlatformInputOps over XCB: keycode<->keysym translation, the
 * keysym-by-name resolver used by the translation parser, case folding, mapping
 * refresh, and pointer query.  The per-display keysym/modifier cache itself
 * lives in IswPerDisplay (managed by TMkey.c's _IswBuildKeysymTables); this
 * backend reaches the native xcb_key_symbols_t through that cache and the
 * internal seam.  Keysym VALUES remain numerically X11-compatible
 * (IswKeySym == the keysym number); only the toolkit-facing TYPE is neutral.
 *
 * Phase 3 of the ISWPlatform vtable (docs/ISWPLATFORM_PLAN.md).  The only TU
 * (besides the other backend TUs) that includes <xcb/xcb_keysyms.h> for the
 * input path.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "IntrinsicI.h"
#include "InitialI.h"
#include "ISWPlatformPrivate.h"

/* The keysym cache + the neutral keycode translator live in TMkey.c; reuse
   them so there is a single keysym-table implementation. */
extern xcb_key_symbols_t *_IswXcbKeysyms(IswDisplay dpy);   /* TMkey.c */
extern void _IswXcbRefreshKeysyms(IswDisplay dpy);          /* TMkey.c */

/* ---- input ops ----------------------------------------------------------- */

static IswKeySym
xcb_in_keycode_to_keysym(IswDisplay dpy, IswKeyCode kc, int col)
{
    xcb_key_symbols_t *ks = _IswXcbKeysyms(dpy);
    if (!ks)
        return IswNoSymbol;
    return (IswKeySym) xcb_key_symbols_get_keysym(ks, (xcb_keycode_t) kc, col);
}

static void
xcb_in_keysym_to_keycodes(IswDisplay dpy, IswKeySym sym,
                          IswKeyCode **out, int *count)
{
    xcb_key_symbols_t *ks = _IswXcbKeysyms(dpy);
    xcb_keycode_t *kcs;
    int n = 0;

    *out = NULL;
    *count = 0;
    if (!ks)
        return;
    kcs = xcb_key_symbols_get_keycode(ks, (xcb_keysym_t) sym);
    if (!kcs)
        return;
    while (kcs[n] != XCB_NO_SYMBOL && kcs[n] != 0)
        n++;
    if (n > 0) {
        IswKeyCode *res = (IswKeyCode *) malloc((size_t) n * sizeof(IswKeyCode));
        if (res) {
            int i;
            for (i = 0; i < n; i++)
                res[i] = (IswKeyCode) kcs[i];
            *out = res;
            *count = n;
        }
    }
    free(kcs);
}

static IswKeySym
xcb_in_keysym_from_name(const char *name)
{
    if (!name)
        return IswNoSymbol;
    return (IswKeySym) xkb_keysym_from_name(name, XKB_KEYSYM_NO_FLAGS);
}

static const char *
xcb_in_keysym_to_name(IswKeySym ks)
{
    static char buf[64];
    int n = xkb_keysym_get_name((xkb_keysym_t) ks, buf, sizeof(buf));
    return (n > 0) ? buf : NULL;
}

static void
xcb_in_convert_case(IswKeySym ks, IswKeySym *lower, IswKeySym *upper)
{
    IswKeySym lo = ks, up = ks;
    if (ks >= 32 && ks < 128) {
        lo = (IswKeySym) tolower((int) (char) ks);
        up = (IswKeySym) toupper((int) (char) ks);
    }
    if (lower) *lower = lo;
    if (upper) *upper = up;
}

static void
xcb_in_translate_keycode(IswDisplay dpy, IswKeyCode kc, IswModMask state,
                         IswModMask *mods_return, IswKeySym *keysym_return)
{
    /* Delegate to the toolkit's translate (which honours the per-display
       translator + cache).  Values are numerically compatible. */
    Modifiers mr = 0;
    xcb_keysym_t sym = 0;
    IswTranslateKeycode(dpy, (IswKeyCode) kc, (Modifiers) state, &mr, &sym);
    if (mods_return) *mods_return = (IswModMask) mr;
    if (keysym_return) *keysym_return = (IswKeySym) sym;
}

static void
xcb_in_refresh_mapping(IswDisplay dpy)
{
    _IswXcbRefreshKeysyms(dpy);
}

static Boolean
xcb_in_query_pointer(IswDisplay dpy, IswWindow win,
                     int *root_x, int *root_y, int *win_x, int *win_y,
                     IswModMask *mods, IswWindow *child)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_query_pointer_cookie_t cookie;
    xcb_query_pointer_reply_t *reply;

    if (!conn)
        return False;
    cookie = xcb_query_pointer(conn, _IswXcbWindow(win));
    reply = xcb_query_pointer_reply(conn, cookie, NULL);
    if (!reply)
        return False;
    if (root_x) *root_x = reply->root_x;
    if (root_y) *root_y = reply->root_y;
    if (win_x)  *win_x  = reply->win_x;
    if (win_y)  *win_y  = reply->win_y;
    if (mods)   *mods   = (IswModMask) reply->mask;
    if (child)  *child  = _IswXcbWindowWrap(reply->child);
    free(reply);
    return True;
}

static void
xcb_in_warp_pointer(IswDisplay dpy, IswWindow dst_win, int x, int y)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_warp_pointer(conn, XCB_NONE, _IswXcbWindow(dst_win), 0, 0, 0, 0,
                     (int16_t) x, (int16_t) y);
}

const IswPlatformInputOps isw_platform_xcb_input_ops = {
    .keycode_to_keysym  = xcb_in_keycode_to_keysym,
    .keysym_to_keycodes = xcb_in_keysym_to_keycodes,
    .keysym_from_name   = xcb_in_keysym_from_name,
    .keysym_to_name     = xcb_in_keysym_to_name,
    .convert_case       = xcb_in_convert_case,
    .translate_keycode  = xcb_in_translate_keycode,
    .refresh_mapping    = xcb_in_refresh_mapping,
    .query_pointer      = xcb_in_query_pointer,
    .warp_pointer       = xcb_in_warp_pointer,
};
