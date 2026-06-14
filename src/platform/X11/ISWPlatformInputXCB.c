/*
 * ISWPlatformInputXCB.c - XCB backend for the input platform ops
 *
 * Copyright (c) 2026 ISW Project
 *
 * Implements IswPlatformInputOps over XCB: keycode<->keysym translation, the
 * keysym-by-name resolver used by the translation parser, case folding, mapping
 * refresh, and pointer query.  This backend OWNS the native keysym table: it
 * allocates xcb_key_symbols_t on demand and stores it as an opaque handle in
 * IswPerDisplay->keysyms (only this TU dereferences it).  The toolkit reaches
 * keysyms exclusively through these ops.  Keysym VALUES remain numerically
 * X11-compatible (IswKeySym == the keysym number); only the TYPE is neutral.
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

/* ---- keysym table ownership (the native xcb_key_symbols_t lives here) -----
   The per-display record carries an opaque keysym-table handle in pd->keysyms;
   only this backend TU dereferences it as xcb_key_symbols_t.  The toolkit
   (TMkey.c) reaches keysyms exclusively through the input ops below. */

static xcb_key_symbols_t *
backend_keysyms(IswDisplay dpy)
{
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    if (pd == NULL)
        return NULL;
    if (pd->keysyms == NULL) {
        xcb_connection_t *conn = _IswXcbConn(dpy);
        if (conn)
            pd->keysyms = (void *) xcb_key_symbols_alloc(conn);
    }
    return (xcb_key_symbols_t *) pd->keysyms;
}

/* ---- input ops ----------------------------------------------------------- */

static IswKeySym
xcb_in_keycode_to_keysym(IswDisplay dpy, IswKeyCode kc, int col)
{
    xcb_key_symbols_t *ks = backend_keysyms(dpy);
    if (!ks)
        return IswNoSymbol;
    return (IswKeySym) xcb_key_symbols_get_keysym(ks, (xcb_keycode_t) kc, col);
}

static void
xcb_in_keysym_to_keycodes(IswDisplay dpy, IswKeySym sym,
                          IswKeyCode **out, int *count)
{
    xcb_key_symbols_t *ks = backend_keysyms(dpy);
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
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    if (pd && pd->keysyms) {
        xcb_key_symbols_free((xcb_key_symbols_t *) pd->keysyms);
        pd->keysyms = NULL;
    }
}

/* Read the server's modifier mapping and build the late-binding tables: for
   each of the 8 modifiers, the keysyms its keycodes produce.  Fills the
   caller's 8-entry `mods_return` and a freshly-malloc'd keysym pool. */
static void
xcb_in_build_mod_map(IswDisplay dpy, IswModKeysymEntry *mods_return,
                     IswKeySym **keysyms_return, int *count_return)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_key_symbols_t *ks = backend_keysyms(dpy);
    xcb_get_modifier_mapping_cookie_t mod_cookie;
    xcb_get_modifier_mapping_reply_t *mod_mapping;
    xcb_keycode_t *modmap;
    int i, j, k;
    int max_keys_per_mod;
    int keysyms_count = 0;
    IswKeySym *pool = NULL;

    if (keysyms_return) *keysyms_return = NULL;
    if (count_return)   *count_return = 0;
    if (!conn || !ks || !mods_return)
        return;

    mod_cookie = xcb_get_modifier_mapping(conn);
    mod_mapping = xcb_get_modifier_mapping_reply(conn, mod_cookie, NULL);
    if (mod_mapping == NULL)
        return;

    max_keys_per_mod = mod_mapping->keycodes_per_modifier;
    modmap = xcb_get_modifier_mapping_keycodes(mod_mapping);

    for (i = 0; i < 8; i++)
        for (j = 0; j < max_keys_per_mod; j++)
            if (modmap[i * max_keys_per_mod + j] != 0)
                keysyms_count++;

    if (keysyms_count > 0)
        pool = (IswKeySym *) malloc((size_t) keysyms_count * sizeof(IswKeySym));

    k = 0;
    for (i = 0; i < 8; i++) {
        mods_return[i].mask = (Modifiers) (1 << i);
        mods_return[i].idx = k;
        mods_return[i].count = 0;
        for (j = 0; pool && j < max_keys_per_mod; j++) {
            xcb_keycode_t keycode = modmap[i * max_keys_per_mod + j];
            if (keycode != 0) {
                xcb_keysym_t keysym =
                    xcb_key_symbols_get_keysym(ks, keycode, 0);
                if (keysym != XCB_NO_SYMBOL) {
                    pool[k++] = (IswKeySym) keysym;
                    mods_return[i].count++;
                }
            }
        }
    }

    free(mod_mapping);
    if (keysyms_return) *keysyms_return = pool;
    if (count_return)   *count_return = keysyms_count;
}

static void
xcb_in_free_keysyms(IswDisplay dpy)
{
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    if (pd && pd->keysyms) {
        xcb_key_symbols_free((xcb_key_symbols_t *) pd->keysyms);
        pd->keysyms = NULL;
    }
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
    .build_mod_map      = xcb_in_build_mod_map,
    .free_keysyms       = xcb_in_free_keysyms,
    .query_pointer      = xcb_in_query_pointer,
    .warp_pointer       = xcb_in_warp_pointer,
};
