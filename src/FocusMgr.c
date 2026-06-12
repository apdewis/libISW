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
#include <ISW/EventI.h>
#include <ISW/StringDefs.h>
#include <ISW/CompositeP.h>
#include <ISW/Shell.h>
#include <ISW/Simple.h>
#include <ISW/SimpleP.h>
#include <ISW/FocusMgrI.h>
#include <ISW/ISWRender.h>
#include <ISW/Text.h>
#include <ISW/TextP.h>
#include <ISW/MenuButton.h>
#include <ISW/MenuButtoP.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeBSBP.h>
#include <ISW/ISWPlatform.h>

#include <stdlib.h>
#include <string.h>


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
static Boolean    g_alt_held = False;

/* Currently-open SimpleMenu popup. Tracked via popup/popdown callbacks
 * installed lazily on each SimpleMenu we see. At most one at a time —
 * we enforce this in the Alt+letter handler by popping down any current
 * menu before opening the next. */
static Widget     g_open_menu = NULL;

/* True if the currently-open menu was triggered by a mnemonic
 * (Alt+letter or in-menu letter). When set, mnemonic underlines render
 * even after Alt is released, until the menu is dismissed. */
static Boolean    g_open_via_mnemonic = False;

Boolean
_IswFocusMgrShowMnemonicsForMenu(Widget menu)
{
    return g_alt_held || (g_open_via_mnemonic && menu == g_open_menu);
}

Boolean
_IswFocusMgrAltHeld(void)
{
    return g_alt_held;
}

int
_IswFocusMgrFindMnemonicIndex(const char *label, uint32_t mnemonic)
{
    if (!label || mnemonic == 0) return -1;
    /* Lowercase printable-letter mnemonic */
    int mk = (int) mnemonic;
    if (mk >= 'A' && mk <= 'Z') mk += ('a' - 'A');
    if (!(mk >= 'a' && mk <= 'z') && !(mk >= '0' && mk <= '9')) return -1;

    for (int i = 0; label[i]; i++) {
        int c = (unsigned char) label[i];
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
        if (c == mk) return i;
    }
    return -1;
}

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
    _IswRepaintWindowless(w);

}

/* The widget currently displaying the Tab-cycle focus ring, if any.
 * Tracked so a stray click or unrelated keypress can clear it without
 * walking the shell list. */
static Widget g_ring_widget = NULL;

/* True only while keyboard traversal is the active input mode.  Set by the
 * Tab / Shift+Tab path (advance_focus); cleared by any click or non-Tab key.
 * The focus halo is painted ONLY while this is True, so the halo appears
 * exclusively when key nav was actually used — never on click. */
static Boolean g_keynav_active = False;

void
_IswFocusMgrClearRing(void)
{
    Widget w = g_ring_widget;
    g_keynav_active = False;
    if (!w) return;
    g_ring_widget = NULL;
    if (IswIsSubclass(w, simpleWidgetClass)) {
        SimpleWidget sw = (SimpleWidget) w;
        if (sw->simple.has_focus) {
            sw->simple.has_focus = False;
            redraw_widget(w);
        }
    }
}

/* Click-to-focus: a pointer press on (or inside) a traversable widget makes
 * that widget the keyboard-traversal anchor, so subsequent Tab / Shift+Tab
 * continue from it.  The visible focus ring stays keyboard-only — it is not
 * shown on click, only on the next Tab — so the ring still signals active
 * keyboard navigation.  If the click is not on a traversable widget, the ring
 * is simply dismissed (old behaviour). */
void
_IswFocusMgrFocusOnClick(Widget w)
{
    Widget shell;

    /* Find the nearest traversable widget at or above the click target. */
    while (w && !widget_is_traversable(w))
        w = IswParent(w);

    if (!w) {
        _IswFocusMgrClearRing();
        return;
    }

    shell = nearest_shell(w);
    if (!shell) {
        _IswFocusMgrClearRing();
        return;
    }

    /* Dismiss any visible ring, then move the (invisible) keyboard-focus
     * anchor to the clicked widget without lighting its ring. */
    _IswFocusMgrClearRing();
    IswSetKeyboardFocus(shell, w);
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
            if (g_ring_widget == old) g_ring_widget = NULL;
        }
    }

    if (new_focus) {
        IswSetKeyboardFocus(shell, new_focus);
        /* Only light the halo when keyboard traversal is the active input
         * mode.  A non-keynav focus change moves the anchor silently. */
        if (g_keynav_active && IswIsSubclass(new_focus, simpleWidgetClass)) {
            SimpleWidget sw = (SimpleWidget) new_focus;
            sw->simple.has_focus = True;
            redraw_widget(new_focus);
            g_ring_widget = new_focus;
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

    /* Tab / Shift+Tab is keyboard traversal: enable the halo. */
    g_keynav_active = True;

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
FocusNext(Widget w, IswEvent *e, String *p, Cardinal *np)
{
    Widget shell = nearest_shell(w);
    (void)e; (void)p; (void)np;
    if (shell) advance_focus(shell, +1);
}

static void
FocusPrev(Widget w, IswEvent *e, String *p, Cardinal *np)
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
    if (!ctx) return;

    double rx = pad;
    double ry = pad;
    double rw = (double) w->core.width  - 2 * pad;
    double rh = (double) w->core.height - 2 * pad;
    if (rw <= 0 || rh <= 0) return;

    double dashes[2] = { 2.0, 2.0 };
    ISWRenderSave(ctx);
    ISWRenderPathBegin(ctx);
    ISWRenderPathRectangle(ctx, rx, ry, rw, rh);
    ISWRenderSetColor(ctx, (Pixel) color);
    ISWRenderSetDash(ctx, dashes, 2, 0);
    ISWRenderSetLineWidth(ctx, 1.0);
    ISWRenderStroke(ctx);
    ISWRenderSetDash(ctx, NULL, 0, 0);
    ISWRenderRestore(ctx);
}

/* --- Early-dispatch Tab intercept --- */

/* Walk a subtree, repainting MenuButton + SimpleMenu (via clear-with-expose)
 * so their mnemonic underlines redraw when Alt state changes. */
static void
repaint_menu_widgets(Widget w)
{
    if (!w) return;
    if (IswIsSubclass(w, menuButtonWidgetClass) && IswIsRealized(w)) {
        /* MenuButton is windowless (inherits Label): repaint its surface and
         * composite the ancestor rather than clearing the shared window. */
        if (!IswIsShell(w))
            _IswRepaintWindowless(w);
        else
            _IswPlatformClearArea(IswDisplayOf(w),
                                  _IswPlatformWidgetWindow(IswDisplayOf(w), w),
                                  0, 0, w->core.width, w->core.height, True);
    }
    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        for (Cardinal i = 0; i < cw->composite.num_children; i++)
            repaint_menu_widgets(cw->composite.children[i]);
    }
}

/* Also walk all registered shells and currently-popped-up SimpleMenus. */
static void
repaint_for_alt_change(IswDisplay dpy)
{
    for (int i = 0; i < g_slot_count; i++) {
        Widget shell = g_slots[i].shell;
        if (!shell || !IswIsRealized(shell)) continue;
        repaint_menu_widgets(shell);
        /* SimpleMenu shells need their window cleared so the SmeBSB
         * entries redraw their underlines. */
        if (IswIsSubclass(shell, simpleMenuWidgetClass)) {
            _IswPlatformClearArea(dpy,
                                  _IswPlatformWidgetWindow(dpy, shell),
                                  0, 0, shell->core.width, shell->core.height,
                                  True);
        }
    }
    _IswPlatformFlush(dpy);
}

static void shell_destroy_cb(Widget, IswPointer, IswPointer);

/* --- Open-menu tracking via popup/popdown callbacks -------------------
 * Every registered shell gets these callbacks. When a SimpleMenu pops up
 * it becomes the one 'g_open_menu'; on popdown we clear it. The Alt+letter
 * handler uses this both to close any current menu before opening another
 * and to find where mnemonic letters should be dispatched. */

static void
menu_popup_cb(Widget shell, IswPointer closure, IswPointer call_data)
{
    (void)closure; (void)call_data;
    if (IswIsSubclass(shell, simpleMenuWidgetClass))
        g_open_menu = shell;
}

static void
menu_popdown_cb(Widget shell, IswPointer closure, IswPointer call_data)
{
    (void)closure; (void)call_data;
    if (g_open_menu == shell) {
        g_open_menu = NULL;
        g_open_via_mnemonic = False;
    }
}

void
_IswFocusMgrRegisterMenu(Widget menu)
{
    if (!menu || !IswIsSubclass(menu, simpleMenuWidgetClass)) return;
    ShellSlot *slot = ensure_slot(menu);
    if (!slot || slot->translations_installed) return;
    slot->translations_installed = True;
    IswAddCallback(menu, IswNpopupCallback,   menu_popup_cb,   NULL);
    IswAddCallback(menu, IswNpopdownCallback, menu_popdown_cb, NULL);
    IswAddCallback(menu, IswNdestroyCallback, shell_destroy_cb, NULL);
}

/* Find a MenuButton anywhere in the tree whose mnemonic_key matches. */
static Widget
find_menubutton_mnemonic(Widget w, uint32_t target)
{
    if (!w) return NULL;
    if (IswIsSubclass(w, menuButtonWidgetClass)) {
        MenuButtonWidget mbw = (MenuButtonWidget) w;
        if (mbw->menu_button.mnemonic_key != 0) {
            uint32_t a = mbw->menu_button.mnemonic_key;
            uint32_t b = target;
            if (a >= 'A' && a <= 'Z') a += ('a' - 'A');
            if (b >= 'A' && b <= 'Z') b += ('a' - 'A');
            if (a == b) return w;
        }
    }
    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        for (Cardinal i = 0; i < cw->composite.num_children; i++) {
            Widget r = find_menubutton_mnemonic(cw->composite.children[i], target);
            if (r) return r;
        }
    }
    return NULL;
}

/* Within a SimpleMenu, find an SmeBSB entry whose mnemonic_key matches. */
static Widget
find_menu_entry_mnemonic(Widget menu, uint32_t target)
{
    if (!IswIsSubclass(menu, simpleMenuWidgetClass)) return NULL;
    CompositeWidget cw = (CompositeWidget) menu;
    for (Cardinal i = 0; i < cw->composite.num_children; i++) {
        Widget child = cw->composite.children[i];
        if (!IswIsSubclass(child, smeBSBObjectClass)) continue;
        SmeBSBObject sme = (SmeBSBObject) child;
        uint32_t a = sme->sme_bsb.mnemonic_key;
        uint32_t b = target;
        if (a == 0) continue;
        if (a >= 'A' && a <= 'Z') a += ('a' - 'A');
        if (b >= 'A' && b <= 'Z') b += ('a' - 'A');
        if (a == b) return child;
    }
    return NULL;
}

static void
trigger_menu_button(Widget mb)
{
    _IswMenuButtonPopup(mb);
}

/* Case-fold a letter code point so mnemonic matching is case-insensitive. */
static uint32_t
fold_key(uint32_t k)
{
    if (k >= 'A' && k <= 'Z')
        return k - 'A' + 'a';
    return k;
}

Boolean
_IswFocusMgrMaybeHandleKey(Widget widget, IswEvent *event)
{
    if (!widget || !event) return False;
    if (event->kind != IswKeyDown && event->kind != IswKeyUp) return False;

    IswDisplay dpy = IswDisplayOf(widget);
    Boolean shift = (event->key.modifiers & IswModShift) != 0;
    uint32_t sym = event->key.key;     /* neutral key identity / code point */

    /* --- Track Alt press/release so mnemonic underlines show/hide. ----- */
    if (sym == IswKeyAlt) {
        Boolean new_state = (event->kind == IswKeyDown);
        if (new_state != g_alt_held) {
            g_alt_held = new_state;
            repaint_for_alt_change(dpy);
        }
        return False;  /* don't swallow; let it propagate normally */
    }

    if (event->kind != IswKeyDown) return False;

    Boolean alt_held = (event->key.modifiers & IswModAlt) != 0;
    Boolean ctrl_held = (event->key.modifiers & IswModControl) != 0;

    /* --- Menubar mnemonic: Alt + letter, anywhere in the shell. ---
     *     This takes priority over in-menu letter matching so Alt+E
     *     always switches to the Edit menu rather than activating an
     *     entry called "Export" in whatever menu happens to be open. */
    if (alt_held && !ctrl_held) {
        uint32_t base = fold_key(sym);
        Widget shell = nearest_shell(widget);
        if (shell) {
            Widget hit = find_menubutton_mnemonic(shell, base);
            if (hit) {
                if (g_open_menu) {
                    Widget old = g_open_menu;
                    g_open_menu = NULL;
                    IswPopdown(old);
                }
                trigger_menu_button(hit);
                /* The popup callback set g_open_menu; mark it as opened
                 * via mnemonic so underlines stay visible after Alt is
                 * released, until the menu is dismissed. */
                g_open_via_mnemonic = True;
                return True;
            }
        }
    }

    /* --- When a menu is open: bare letter activates a matching entry;
     *     Escape closes. (Alt+letter was already handled above.) --- */
    if (g_open_menu != NULL &&
        IswIsSubclass(g_open_menu, simpleMenuWidgetClass) &&
        !ctrl_held && !alt_held) {
        if (sym == IswKeyEscape) {
            Widget menu = g_open_menu;
            g_open_menu = NULL;
            IswPopdown(menu);
            return True;
        }
        uint32_t base = fold_key(sym);
        Widget entry = find_menu_entry_mnemonic(g_open_menu, base);
        if (entry) {
            SmeObjectClass sc = (SmeObjectClass) entry->core.widget_class;
            Widget menu = g_open_menu;
            g_open_menu = NULL;
            IswPopdown(menu);
            if (sc->sme_class.notify)
                (sc->sme_class.notify)(entry);
            return True;
        }
    }

    /* --- Tab / Shift+Tab focus traversal --- */
    int direction;
    if (sym == IswKeyTab && !shift)            direction = +1;
    else if (sym == IswKeyTab && shift)        direction = -1;
    else {
        /* Any other key press (that isn't a bare modifier) dismisses the
         * Tab-cycle focus ring. Bare modifiers are left alone so e.g.
         * pressing Shift before Shift+Tab doesn't kill the ring. */
        if (sym != IswKeyShift && sym != IswKeyControl &&
            sym != IswKeyAlt && sym != IswKeyMeta &&
            sym != IswKeySuper && sym != IswKeyCapsLock &&
            sym != IswKeyNumLock) {
            _IswFocusMgrClearRing();
        }
        return False;
    }

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

/* Event handler: ensures KEY_RELEASE events are selected on the shell
 * window so we can update the Alt-held state when the user lifts Alt. */
static void
shell_key_release_handler(Widget w, IswPointer closure,
                          IswEvent *iswev, Boolean *cont)
{
    (void)closure; (void)cont;
    if (iswev->kind != IswKeyUp) return;
    /* Forward into the main intercept so Alt-release is processed. */
    _IswFocusMgrMaybeHandleKey(w, iswev);
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

    /* Select KEY_RELEASE on the shell window so we see Alt-release. Widgets
     * normally only request KEY_PRESS, and the default dispatcher wouldn't
     * otherwise have a chance to update our g_alt_held flag. */
    IswAddEventHandler(shell, IswKeyReleaseMask, False,
                       shell_key_release_handler, NULL);

    /* Track when a SimpleMenu opens/closes so mnemonic dispatch can find
     * the right popup without inspecting the grab list. */
    if (IswIsSubclass(shell, simpleMenuWidgetClass)) {
        IswAddCallback(shell, IswNpopupCallback,   menu_popup_cb,   NULL);
        IswAddCallback(shell, IswNpopdownCallback, menu_popdown_cb, NULL);
    }

    slot->translations_installed = True;
}
