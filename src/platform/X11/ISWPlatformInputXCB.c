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

#ifdef HAVE_XCB_XINPUT
#include <xcb/xinput.h>
#endif

#include "IntrinsicI.h"
#include "InitialI.h"
#include "ISWPlatformPrivate.h"
#include "ISWPlatformDisplayXCB.h"
#include <ISW/IswEvent.h>

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

static void
xcb_in_keycode_range(IswDisplay dpy, int *min_out, int *max_out)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (conn) {
        const xcb_setup_t *setup = xcb_get_setup(conn);
        if (setup) {
            if (min_out) *min_out = setup->min_keycode;
            if (max_out) *max_out = setup->max_keycode;
            return;
        }
    }
    if (min_out) *min_out = 8;
    if (max_out) *max_out = 255;
}

/* ===========================================================================
 * XInput2 (XI2): smooth/trackpad scroll + semantic button remap.
 *
 * When xcb-xinput is available AND the server speaks XI2 ≥ 2.0, master
 * pointer motion carrying a scroll valuator is translated into an IswScroll
 * event with a sub-pixel delta (smooth=1).  XI button press/release detail
 * 4-7 (discrete wheel) becomes IswScroll discrete, and detail 1-3 is remapped
 * to the semantic Primary/Tertiary/Secondary roles — replacing the core
 * button events for XI devices.  When XI2 is absent, the core button 4-7
 * fallback in ISWPlatformEventXCB.c handles discrete scroll.
 * =========================================================================== */

#ifdef HAVE_XCB_XINPUT

/* Per-display cache of master-pointer scroll axes (one entry per scroll
   valuator of every master pointer).  `number` is the valuator index within
   the device's valuator report; `scroll_type` is VERTICAL/HORIZONTAL. */
typedef struct {
    xcb_input_device_id_t deviceid;
    uint16_t number;
    uint16_t scroll_type;       /* XCB_INPUT_SCROLL_TYPE_VERTICAL/HORIZONTAL */
    float    increment;         /* one click, in valuator units (fp3232) */
} IswXcbScrollAxis;

typedef struct {
    IswXcbScrollAxis *axes;
    int               count;
} IswXcbScrollValuators;

/* fp3232 (32.32 fixed point) → float. */
static float
xi_fp3232_to_float(xcb_input_fp3232_t v)
{
    return (float) ((double) v.integral + (double) v.frac / 4294967296.0);
}

/* fp1616 (16.16 fixed point) → float, for root/event coordinates. */
static float
xi_fp1616_to_float(xcb_input_fp1616_t v)
{
    return (float) ((double) v / 65536.0);
}

static IswXcbScrollValuators *
xi_scroll_cache(IswDisplay dpy)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    if (idx == NULL)
        return NULL;
    return (IswXcbScrollValuators *) idx->scroll_valuators;
}

/* Query master-pointer devices and record every scroll valuator. */
static void
xi_query_scroll_axes(IswDisplay dpy)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_input_xi_query_device_cookie_t cookie;
    xcb_input_xi_query_device_reply_t *reply;
    xcb_input_xi_device_info_iterator_t dit;
    IswXcbScrollValuators *cache;

    if (idx == NULL || conn == NULL || !idx->xi_present)
        return;

    cache = (IswXcbScrollValuators *) calloc(1, sizeof(*cache));
    if (cache == NULL)
        return;
    idx->scroll_valuators = cache;

    cookie = xcb_input_xi_query_device(conn, XCB_INPUT_DEVICE_ALL_MASTER);
    reply = xcb_input_xi_query_device_reply(conn, cookie, NULL);
    if (reply == NULL)
        return;

    for (dit = xcb_input_xi_query_device_infos_iterator(reply);
         dit.rem > 0; xcb_input_xi_device_info_next(&dit)) {
        xcb_input_xi_device_info_t *info = dit.data;
        xcb_input_device_class_iterator_t cit;

        for (cit = xcb_input_xi_device_info_classes_iterator(info);
             cit.rem > 0; xcb_input_device_class_next(&cit)) {
            xcb_input_device_class_t *cls = cit.data;
            if (cls->type == XCB_INPUT_DEVICE_CLASS_TYPE_SCROLL) {
                xcb_input_scroll_class_t *sc = (xcb_input_scroll_class_t *) cls;
                IswXcbScrollAxis *a;
                cache->axes = (IswXcbScrollAxis *)
                    realloc(cache->axes,
                            (size_t) (cache->count + 1) * sizeof(*a));
                if (cache->axes == NULL) {
                    cache->count = 0;
                    break;
                }
                a = &cache->axes[cache->count++];
                a->deviceid = sc->sourceid;
                a->number = sc->number;
                a->scroll_type = sc->scroll_type;
                a->increment = xi_fp3232_to_float(sc->increment);
                if (a->increment == 0.0f)
                    a->increment = 1.0f;
            }
        }
    }
    free(reply);
}

void
_IswXcbInputInit(IswDisplay dpy)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_query_extension_cookie_t eq;
    xcb_query_extension_reply_t *ereply;
    xcb_input_get_extension_version_cookie_t vck;
    xcb_input_get_extension_version_reply_t *vreply;

    if (idx == NULL || conn == NULL)
        return;

    eq = xcb_query_extension(conn, 16, "XInputExtension");
    ereply = xcb_query_extension_reply(conn, eq, NULL);
    if (ereply == NULL || !ereply->present) {
        free(ereply);
        return;
    }
    idx->xi_opcode = ereply->major_opcode;
    free(ereply);

    vck = xcb_input_get_extension_version(conn, 16, "XInputExtension");
    vreply = xcb_input_get_extension_version_reply(conn, vck, NULL);
    if (vreply == NULL)
        return;
    if (vreply->present && vreply->server_major >= 2)
        idx->xi_present = 1;
    free(vreply);

    if (idx->xi_present)
        xi_query_scroll_axes(dpy);
}

void
_IswXcbInputFree(IswDisplay dpy)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    if (idx == NULL || idx->scroll_valuators == NULL)
        return;
    {
        IswXcbScrollValuators *cache =
            (IswXcbScrollValuators *) idx->scroll_valuators;
        free(cache->axes);
        free(cache);
        idx->scroll_valuators = NULL;
    }
}

void
_IswXcbInputSelectForWindow(IswDisplay dpy, xcb_window_t window)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    xcb_connection_t *conn = _IswXcbConn(dpy);
    /* Select ONLY XI motion: smooth scroll arrives as XI MotionNotify carrying
       a scroll valuator.  We deliberately do NOT select XI button press/release
       — X delivers core button events AND XI button events independently, so
       selecting both would duplicate every button press.  Discrete wheel
       (button 4-7) and semantic button remap stay on the core path (which has
       no such duplication).  Core MotionNotify is also still delivered (XI
       selection does not suppress core events), so plain pointer motion is
       handled by the core path; this translator consumes XI motion only when a
       scroll valuator is present and otherwise returns False. */
    struct {
        xcb_input_event_mask_t h;
        uint32_t v;
    } packed;

    if (idx == NULL || conn == NULL || !idx->xi_present)
        return;

    packed.h.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
    packed.h.mask_len = 1;
    packed.v = XCB_INPUT_XI_EVENT_MASK_MOTION;
    xcb_input_xi_select_events(conn, window, 1, &packed.h);
}

/* Fill the common scroll pointer-position fields from an XI button/motion
   event (fp1616 root/event coords). */
static void
xi_fill_scroll_pos(IswDisplay dpy, xcb_input_button_press_event_t *xi,
                   IswEvent *out)
{
    float fx = xi_fp1616_to_float(xi->event_x);
    float fy = xi_fp1616_to_float(xi->event_y);
    float frx = xi_fp1616_to_float(xi->root_x);
    float fry = xi_fp1616_to_float(xi->root_y);
    int16_t ex = (int16_t) fx, ey = (int16_t) fy;
    int16_t rx = (int16_t) frx, ry = (int16_t) fry;
    out->scroll.target = _IswXcbTargetForWindow(dpy, xi->event);
    out->scroll.time = xi->time;
    out->scroll.x = ex;
    out->scroll.y = ey;
    out->scroll.root_x = rx;
    out->scroll.root_y = ry;
    _IswXcbShellCoordsForEvent(dpy, xi->event, ex, ey, rx, ry,
                               &out->scroll.shell_x, &out->scroll.shell_y);
}

/* Extract scroll-valuator deltas from an XI motion event.  Returns the number
   of scroll axes found in the valuator report. */
static int
xi_extract_scroll(IswDisplay dpy, xcb_input_button_press_event_t *xi,
                  float *dx_out, float *dy_out)
{
    IswXcbScrollValuators *cache = xi_scroll_cache(dpy);
    const uint32_t *vmask;
    const xcb_input_fp3232_t *vals;
    int nvals, i, found = 0;

    if (cache == NULL)
        return 0;
    vmask = xcb_input_button_press_valuator_mask(xi);
    nvals = xcb_input_button_press_axisvalues_length(xi);
    vals = xcb_input_button_press_axisvalues(xi);
    (void) nvals;

    for (i = 0; i < cache->count; i++) {
        IswXcbScrollAxis *a = &cache->axes[i];
        uint32_t word = a->number >> 5;
        uint32_t bit = 1u << (a->number & 31);
        int idx, b;
        if (vmask == NULL || (vmask[word] & bit) == 0)
            continue;
        /* index into axisvalues = number of set bits below a->number */
        idx = 0;
        for (b = 0; b < a->number; b++)
            if (vmask[b >> 5] & (1u << (b & 31)))
                idx++;
        if (idx < nvals) {
            float v = xi_fp3232_to_float(vals[idx]);
            if (a->scroll_type == XCB_INPUT_SCROLL_TYPE_VERTICAL) {
                *dy_out += v;
                found++;
            } else if (a->scroll_type == XCB_INPUT_SCROLL_TYPE_HORIZONTAL) {
                *dx_out += v;
                found++;
            }
        }
    }
    return found;
}

Boolean
_IswXcbInputTranslateEvent(IswDisplay dpy, xcb_generic_event_t *xev,
                           IswEvent *out)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    xcb_input_button_press_event_t *xi;
    uint8_t rt;
    uint16_t mods;

    if (idx == NULL || !idx->xi_present || xev == NULL)
        return False;

    rt = xev->response_type & ~0x80;
    if (rt != XCB_GE_GENERIC)
        return False;

    xi = (xcb_input_button_press_event_t *) xev;
    if (xi->extension != idx->xi_opcode)
        return False;

    mods = (uint16_t) (xi->mods.effective & 0x1FFF);

    /* Only XI MotionNotify is selected (see _IswXcbInputSelectForWindow), so
       this is the only XI event type that reaches here.  A motion event
       carrying a scroll valuator becomes a smooth IswScroll; a plain pointer
       motion (no scroll valuator) returns False so the core MotionNotify path
       handles pointer motion — avoiding any duplication, since XI selection
       does not suppress core events. */
    switch (xi->event_type) {
    case XCB_INPUT_MOTION: {
        float dx = 0.0f, dy = 0.0f;
        if (xi_extract_scroll(dpy, xi, &dx, &dy) == 0)
            return False;   /* plain motion: let core MotionNotify handle it */
        memset(out, 0, sizeof(*out));
        out->kind = IswScroll;
        out->any.synthetic = (xev->response_type & 0x80) ? 1 : 0;
        xi_fill_scroll_pos(dpy, xi, out);
        out->scroll.modifiers = mods;
        out->scroll.smooth = 1;
        out->scroll.delta_x = dx;
        out->scroll.delta_y = dy;
        out->scroll.discrete_x = 0;
        out->scroll.discrete_y = 0;
        /* Heap-copy the full XI event into the native escape hatch.  XI events
           carry variable-length valuator/button-mask data beyond the fixed
           32-byte xcb_generic_event_t header, so the copy must be the full
           wire size (32 + 4*length), NOT the fixed 32 the protocol-event path
           uses — a 32-byte copy would truncate the valuator report any native
           consumer (IswEventNative) needs.  The dispatch core frees this. */
        
        uint32_t full = 32u + 4u * (uint32_t) xev->full_sequence;
        void *copy = __IswMalloc(full);
        if (copy != NULL)
            memcpy(copy, xev, full);
        out->any.native = copy;
        
        return True;
    }
    default:
        return False;
    }
}

#else  /* !HAVE_XCB_XINPUT */

/* Stubs so the display/event path compiles without xcb-xinput. */
void      _IswXcbInputInit(IswDisplay dpy) { (void) dpy; }
void      _IswXcbInputFree(IswDisplay dpy) { (void) dpy; }
void      _IswXcbInputSelectForWindow(IswDisplay dpy, xcb_window_t window)
{ (void) dpy; (void) window; }
Boolean   _IswXcbInputTranslateEvent(IswDisplay dpy, xcb_generic_event_t *xev,
                                     IswEvent *out)
{ (void) dpy; (void) xev; (void) out; return False; }

#endif /* HAVE_XCB_XINPUT */

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
    .keycode_range      = xcb_in_keycode_range,
};
