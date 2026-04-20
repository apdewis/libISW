/*
 * FocusMgr.c - Tab/Shift-Tab focus traversal manager
 *
 * Tracks an ordered list of focusable widgets per shell and cycles
 * keyboard focus through them on Tab / Shift+Tab.
 *
 * A widget participates by setting Simple's IswNtraversalOn resource
 * (default False). Focus order is widget-tree order, sorted by the
 * optional IswNtabIndex resource (0 = follow tree position).
 *
 * The focused widget's SimplePart.has_focus flag is set; widgets are
 * expected to draw a focus ring in their expose path when that flag
 * is True.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/Intrinsic.h>
#include <ISW/StringDefs.h>
#include <ISW/CompositeP.h>
#include <ISW/Shell.h>
#include <ISW/Simple.h>
#include <ISW/SimpleP.h>
#include <ISW/FocusMgrI.h>
#include <ISW/ISWRender.h>
#include <ISW/Text.h>
#include <ISW/TextP.h>
#include <cairo/cairo.h>

#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

#define MAX_FOCUS_LIST 256

/* --- Per-shell state, kept in a small side table --- */

typedef struct {
    Widget shell;
    Boolean translations_installed;
} ShellSlot;

static ShellSlot *g_slots = NULL;
static int        g_slot_count = 0;
static int        g_slot_cap = 0;
static Boolean    g_actions_registered = False;

static ShellSlot *
find_slot(Widget shell)
{
    int i;
    for (i = 0; i < g_slot_count; i++) {
        if (g_slots[i].shell == shell) return &g_slots[i];
    }
    return NULL;
}

static ShellSlot *
ensure_slot(Widget shell)
{
    ShellSlot *s = find_slot(shell);
    if (s) return s;
    if (g_slot_count == g_slot_cap) {
        int new_cap = g_slot_cap ? g_slot_cap * 2 : 8;
        ShellSlot *n = (ShellSlot *) realloc(g_slots, new_cap * sizeof(ShellSlot));
        if (!n) return NULL;
        g_slots = n;
        g_slot_cap = new_cap;
    }
    s = &g_slots[g_slot_count++];
    memset(s, 0, sizeof(*s));
    s->shell = shell;
    return s;
}

void
_IswFocusMgrDestroyShell(Widget shell)
{
    int i;
    for (i = 0; i < g_slot_count; i++) {
        if (g_slots[i].shell == shell) {
            g_slots[i] = g_slots[--g_slot_count];
            return;
        }
    }
}

/* --- Tree walk: collect traversable widgets --- */

static Widget
nearest_shell(Widget w)
{
    while (w && !IswIsShell(w))
        w = IswParent(w);
    return w;
}

static Boolean
widget_is_traversable(Widget w)
{
    if (!w) return False;
    if (!IswIsManaged(w)) return False;
    if (!IswIsRealized(w)) return False;
    if (!IswIsSensitive(w)) return False;
    if (!IswIsSubclass(w, simpleWidgetClass)) return False;
    {
        SimpleWidget sw = (SimpleWidget) w;
        if (!sw->simple.traversal_on) return False;
    }
    return True;
}

static void
collect_recursive(Widget w, Widget *out, int *count, int max)
{
    if (*count >= max) return;
    if (widget_is_traversable(w)) {
        out[(*count)++] = w;
    }
    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++) {
            collect_recursive(cw->composite.children[i], out, count, max);
        }
    }
}

/* Stable sort (bubble) by tab_index ascending; ties keep tree order. */
static void
sort_by_tab_index(Widget *list, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        Widget cur = list[i];
        int cur_idx = ((SimpleWidget) cur)->simple.tab_index;
        j = i - 1;
        while (j >= 0 &&
               ((SimpleWidget) list[j])->simple.tab_index > cur_idx) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = cur;
    }
}

static int
build_focus_list(Widget shell, Widget *out, int max)
{
    int count = 0;
    if (!IswIsComposite(shell)) return 0;
    {
        CompositeWidget cw = (CompositeWidget) shell;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++) {
            collect_recursive(cw->composite.children[i], out, &count, max);
        }
    }
    sort_by_tab_index(out, count);
    return count;
}

/* --- Focus state changes --- */

static void
redraw_widget(Widget w)
{
    if (!w || !IswIsRealized(w)) return;
    /* Ask the X server to generate a real Expose event for the whole
     * widget. Calling core_class.expose directly with a NULL event is
     * unsafe: some widgets (e.g. Text) dereference the event. */
    xcb_clear_area(IswDisplay(w), 1 /* exposures */, IswWindow(w),
                   0, 0, w->core.width, w->core.height);
    xcb_flush(IswDisplay(w));
}

static void
set_focus(Widget shell, Widget new_focus)
{
    Widget old = IswGetKeyboardFocusWidget(shell);

    if (old == new_focus) return;

    if (old && IswIsSubclass(old, simpleWidgetClass)) {
        SimpleWidget sw = (SimpleWidget) old;
        if (sw->simple.has_focus) {
            sw->simple.has_focus = False;
            redraw_widget(old);
        }
    }

    if (new_focus) {
        IswSetKeyboardFocus(shell, new_focus);
        if (IswIsSubclass(new_focus, simpleWidgetClass)) {
            SimpleWidget sw = (SimpleWidget) new_focus;
            sw->simple.has_focus = True;
            redraw_widget(new_focus);
        }
    }
}

/* --- Focus advance: +1 (Tab) or -1 (Shift+Tab) --- */

static void
advance_focus(Widget shell, int direction)
{
    Widget list[MAX_FOCUS_LIST];
    int n = build_focus_list(shell, list, MAX_FOCUS_LIST);
    if (n == 0) return;

    Widget cur = IswGetKeyboardFocusWidget(shell);
    int cur_idx = -1;
    int i;
    for (i = 0; i < n; i++) {
        if (list[i] == cur) { cur_idx = i; break; }
    }

    int next;
    if (cur_idx < 0) {
        next = (direction > 0) ? 0 : n - 1;
    } else {
        next = (cur_idx + direction + n) % n;
    }

    set_focus(shell, list[next]);
}

/* --- Action procs --- */

static void
FocusNext(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    Widget shell = nearest_shell(w);
    (void)e; (void)p; (void)np;
    if (shell) advance_focus(shell, +1);
}

static void
FocusPrev(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    Widget shell = nearest_shell(w);
    (void)e; (void)p; (void)np;
    if (shell) advance_focus(shell, -1);
}

static IswActionsRec focus_actions[] = {
    { "focus-next", FocusNext },
    { "focus-prev", FocusPrev },
};

static char focus_translations[] =
    "<Key>Tab:        focus-next()\n"
    "Shift<Key>Tab:   focus-prev()\n"
    "<Key>ISO_Left_Tab: focus-prev()";

/* --- Public install --- */

void
_IswFocusMgrDrawRing(Widget w, void *ctx_void, unsigned long color, double pad)
{
    if (!w || !ctx_void) return;
    if (!IswIsSubclass(w, simpleWidgetClass)) return;
    if (!((SimpleWidget) w)->simple.has_focus) return;

    ISWRenderContext *ctx = (ISWRenderContext *) ctx_void;
    cairo_t *cr = (cairo_t *) ISWRenderGetCairoContext(ctx);
    if (!cr) return;

    double rx = pad;
    double ry = pad;
    double rw = (double) w->core.width  - 2 * pad;
    double rh = (double) w->core.height - 2 * pad;
    if (rw <= 0 || rh <= 0) return;

    double dashes[2] = { 2.0, 2.0 };
    cairo_save(cr);
    cairo_new_path(cr);
    cairo_rectangle(cr, rx, ry, rw, rh);
    ISWRenderSetColor(ctx, (Pixel) color);
    cairo_set_dash(cr, dashes, 2, 0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
    cairo_restore(cr);
}

/* --- Early-dispatch Tab intercept --- */

static xcb_key_symbols_t *
get_keysyms(xcb_connection_t *c)
{
    static xcb_key_symbols_t *cached = NULL;
    static xcb_connection_t  *cached_for = NULL;
    if (cached && cached_for == c) return cached;
    if (cached) xcb_key_symbols_free(cached);
    cached_for = c;
    cached = xcb_key_symbols_alloc(c);
    return cached;
}

Boolean
_IswFocusMgrMaybeHandleKey(Widget widget, xcb_generic_event_t *event)
{
    if (!widget || !event) return False;
    uint8_t type = event->response_type & 0x7f;
    if (type != XCB_KEY_PRESS) return False;

    xcb_key_press_event_t *ke = (xcb_key_press_event_t *)event;
    xcb_connection_t *c = IswDisplay(widget);
    xcb_key_symbols_t *syms = get_keysyms(c);
    if (!syms) return False;

    Boolean shift = (ke->state & XCB_MOD_MASK_SHIFT) != 0;
    xcb_keysym_t sym = xcb_key_symbols_get_keysym(syms, ke->detail, shift ? 1 : 0);

    int direction;
    if (sym == XK_Tab && !shift)               direction = +1;
    else if (sym == XK_Tab && shift)           direction = -1;
    else if (sym == XK_ISO_Left_Tab)           direction = -1;
    else                                        return False;

    Widget shell = nearest_shell(widget);
    if (!shell) return False;

    /* Let Text widgets that opt to consume Tab handle it as input.
     * Only plain (unmodified) Tab is consumed — Shift+Tab still traverses
     * so users can always exit a Text widget. */
    if (direction == +1) {
        Widget focused = IswGetKeyboardFocusWidget(shell);
        if (focused && IswIsSubclass(focused, textWidgetClass)) {
            TextWidget tw = (TextWidget) focused;
            if (tw->text.consume_tab) return False;
        }
    }

    /* Only handle if there are traversable widgets in this shell. */
    Widget list[MAX_FOCUS_LIST];
    int n = build_focus_list(shell, list, MAX_FOCUS_LIST);
    if (n == 0) return False;

    advance_focus(shell, direction);
    return True;
}

static void
shell_destroy_cb(Widget shell, IswPointer closure, IswPointer call_data)
{
    (void)closure; (void)call_data;
    _IswFocusMgrDestroyShell(shell);
}

void
_IswFocusMgrEnsureInstalled(Widget shell)
{
    ShellSlot *slot;
    IswAppContext app;

    if (!shell || !IswIsShell(shell)) return;

    slot = ensure_slot(shell);
    if (!slot || slot->translations_installed) return;

    app = IswWidgetToApplicationContext(shell);
    if (!g_actions_registered) {
        IswAppAddActions(app, focus_actions,
                         sizeof(focus_actions) / sizeof(focus_actions[0]));
        g_actions_registered = True;
    }

    IswAugmentTranslations(shell,
        IswParseTranslationTable(focus_translations));
    IswAddCallback(shell, IswNdestroyCallback, shell_destroy_cb, NULL);
    slot->translations_installed = True;
}
