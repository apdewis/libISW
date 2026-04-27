/*
 * focus_dispatch_test.c
 *
 * Diagnostic for keyboard responsiveness across interactive widgets.
 * Pre-work A established that dispatch works for a Command. This test
 * focuses each interactive widget in turn and injects the keyboard
 * shortcuts the user expects to work, logging what (if anything) fires.
 *
 * Widgets tested and shortcuts attempted:
 *   IconView : Left Right Up Down Home End space Return Ctrl+a
 *   List     : Up Down Home End Page_Up Page_Down Return
 *   ComboBox : Up Down Return Escape
 *   SpinBox  : Up Down
 *   Slider   : Left Down Right Up Home End Page_Up Page_Down
 *   Scrollbar: Up Down Page_Up Page_Down Home End
 *   Command  : space Return  (control: known to lack default keybindings,
 *              but Pre-work A proved dispatch path is fine)
 *
 * For each widget the program:
 *   1. Calls IswSetKeyboardFocus(shell, widget)
 *   2. Injects each key (synthetic XCB_KEY_PRESS to widget's window)
 *   3. Action-hook logs every action that fires
 *   4. Per-widget summary printed
 *
 * Self-driving via Xt timers; exits when finished.
 */

#include <ISW/Intrinsic.h>
#include <ISW/StringDefs.h>
#include <ISW/Shell.h>
#include <ISW/IswArgMacros.h>

#include <ISW/Box.h>
#include <ISW/Form.h>
#include <ISW/Command.h>
#include <ISW/IconView.h>
#include <ISW/List.h>
#include <ISW/ListP.h>
#include <ISW/ComboBox.h>
#include <ISW/SpinBox.h>
#include <ISW/Slider.h>
#include <ISW/Scrollbar.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- State ---- */
static IswAppContext g_app;
static Widget        g_shell;
static Widget        g_iconview, g_list, g_combo, g_spin, g_slider, g_scroll, g_command;
static xcb_key_symbols_t *g_keysyms;

static int g_seq = 0;
static int g_actions_fired_this_phase = 0;
static const char *g_current_phase = "init";
static const char *g_current_widget_name = "";

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

static const char *who_of(Widget w) {
    if (w == g_iconview) return "IconView";
    if (w == g_list)     return "List";
    if (w == g_combo)    return "ComboBox";
    if (w == g_spin)     return "SpinBox";
    if (w == g_slider)   return "Slider";
    if (w == g_scroll)   return "Scrollbar";
    if (w == g_command)  return "Command";
    if (w == g_shell)    return "shell";
    return w ? IswName(w) : "?";
}

static void
trace_hook(Widget w, IswPointer cl, String action_name,
           xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    int evtype = event ? (event->response_type & 0x7f) : -1;
    g_actions_fired_this_phase++;
    if (evtype == XCB_KEY_PRESS || evtype == XCB_KEY_RELEASE) {
        xcb_key_press_event_t *ke = (xcb_key_press_event_t *)event;
        fprintf(stderr, "    [%07.1fms #%03d] FIRED action=%-18s on=%s code=%u\n",
                now_ms(), ++g_seq, action_name, who_of(w), ke->detail);
    } else {
        const char *evn = "other";
        if (evtype == XCB_FOCUS_IN) evn = "FocusIn";
        else if (evtype == XCB_FOCUS_OUT) evn = "FocusOut";
        else if (evtype == XCB_ENTER_NOTIFY) evn = "Enter";
        else if (evtype == XCB_LEAVE_NOTIFY) evn = "Leave";
        fprintf(stderr, "    [%07.1fms #%03d] FIRED action=%-18s on=%s ev=%s\n",
                now_ms(), ++g_seq, action_name, who_of(w), evn);
    }
}

/* ---- Inject a synthetic KeyPress to a specific window ---- */
static void
inject_key_to(xcb_window_t target, xcb_keysym_t sym, const char *name, unsigned int state)
{
    xcb_connection_t *c = IswDisplay(g_shell);
    xcb_keycode_t *kcs = xcb_key_symbols_get_keycode(g_keysyms, sym);
    if (!kcs || kcs[0] == XCB_NO_SYMBOL) {
        fprintf(stderr, "    !! no keycode for %s\n", name);
        if (kcs) free(kcs);
        return;
    }
    xcb_keycode_t kc = kcs[0];
    free(kcs);

    fprintf(stderr, "  >> %-12s key=%-10s -> win=0x%x\n",
            g_current_widget_name, name, target);

    xcb_key_press_event_t ev;
    memset(&ev, 0, sizeof(ev));
    {
        const xcb_setup_t *setup = xcb_get_setup(c);
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
        ev.root = it.data ? it.data->root : XCB_NONE;
    }
    ev.detail      = kc;
    ev.time        = XCB_CURRENT_TIME;
    ev.event       = target;
    ev.child       = XCB_NONE;
    ev.state       = state;
    ev.same_screen = 1;

    ev.response_type = XCB_KEY_PRESS | 0x80;
    xcb_send_event(c, 1, target, XCB_EVENT_MASK_KEY_PRESS, (const char *)&ev);

    ev.response_type = XCB_KEY_RELEASE | 0x80;
    xcb_send_event(c, 1, target, XCB_EVENT_MASK_KEY_RELEASE, (const char *)&ev);

    xcb_flush(c);
}

/* ---- Inject one synthetic KeyPress to the focused widget's window ---- */
static void
inject_key(xcb_keysym_t sym, const char *name, unsigned int state)
{
    xcb_connection_t *c = IswDisplay(g_shell);
    xcb_keycode_t *kcs = xcb_key_symbols_get_keycode(g_keysyms, sym);
    if (!kcs || kcs[0] == XCB_NO_SYMBOL) {
        fprintf(stderr, "    !! no keycode for %s\n", name);
        if (kcs) free(kcs);
        return;
    }
    xcb_keycode_t kc = kcs[0];
    free(kcs);

    Widget focus = IswGetKeyboardFocusWidget(g_shell);
    xcb_window_t target = (focus && focus != g_shell && IswIsRealized(focus))
                            ? IswWindow(focus)
                            : IswWindow(g_shell);

    fprintf(stderr, "  >> %-12s key=%-10s (sym=0x%04x kc=%u state=0x%x) -> win=0x%x\n",
            g_current_widget_name, name, sym, kc, state, target);

    xcb_key_press_event_t ev;
    memset(&ev, 0, sizeof(ev));
    {
        const xcb_setup_t *setup = xcb_get_setup(c);
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
        ev.root = it.data ? it.data->root : XCB_NONE;
    }
    ev.detail      = kc;
    ev.time        = XCB_CURRENT_TIME;
    ev.event       = target;
    ev.child       = XCB_NONE;
    ev.state       = state;
    ev.same_screen = 1;

    ev.response_type = XCB_KEY_PRESS | 0x80;
    xcb_send_event(c, 1, target, XCB_EVENT_MASK_KEY_PRESS, (const char *)&ev);

    ev.response_type = XCB_KEY_RELEASE | 0x80;
    xcb_send_event(c, 1, target, XCB_EVENT_MASK_KEY_RELEASE, (const char *)&ev);

    xcb_flush(c);
}

/* ---- Phase machinery ---- */
typedef struct {
    const char  *name;
    Widget      *widget;
    struct {
        xcb_keysym_t sym;
        const char  *name;
        unsigned int state;
    } keys[12];
    int num_keys;
} Phase;

#define KEY(sym) { sym, #sym, 0 }
#define KEYM(sym, mod) { sym, #sym "+" #mod, mod }

#define XK_Mod_Ctrl  XCB_MOD_MASK_CONTROL

static Phase phases[] = {
    { "Command",  &g_command,
      { KEY(XK_space), KEY(XK_Return) }, 2 },

    { "IconView", &g_iconview,
      { KEY(XK_Left), KEY(XK_Right), KEY(XK_Up), KEY(XK_Down),
        KEY(XK_Home), KEY(XK_End),
        KEY(XK_space), KEY(XK_Return),
        KEYM(XK_a, XCB_MOD_MASK_CONTROL) }, 9 },

    { "List",     &g_list,
      { KEY(XK_Up), KEY(XK_Down), KEY(XK_Home), KEY(XK_End),
        KEY(XK_Page_Up), KEY(XK_Page_Down), KEY(XK_Return) }, 7 },

    { "ComboBox", &g_combo,
      { KEY(XK_Up), KEY(XK_Down), KEY(XK_Return), KEY(XK_Escape) }, 4 },

    { "SpinBox",  &g_spin,
      { KEY(XK_Up), KEY(XK_Down) }, 2 },

    { "Slider",   &g_slider,
      { KEY(XK_Left), KEY(XK_Down), KEY(XK_Right), KEY(XK_Up),
        KEY(XK_Home), KEY(XK_End),
        KEY(XK_Page_Up), KEY(XK_Page_Down) }, 8 },

    { "Scrollbar",&g_scroll,
      { KEY(XK_Up), KEY(XK_Down),
        KEY(XK_Page_Up), KEY(XK_Page_Down),
        KEY(XK_Home), KEY(XK_End) }, 6 },
};
#define NUM_PHASES (sizeof(phases) / sizeof(phases[0]))

static int g_phase_idx = 0;
static int g_phase_results[NUM_PHASES];
static int g_phase_keys[NUM_PHASES];

static void next_phase(IswPointer, IswIntervalId*);
static void test_combo_dropdown(IswPointer, IswIntervalId*);

static void
finish_phase(IswPointer cl, IswIntervalId *id)
{
    int idx = g_phase_idx;
    g_phase_results[idx] = g_actions_fired_this_phase;
    g_phase_keys[idx]    = phases[idx].num_keys;
    fprintf(stderr, "  -> %s: %d action(s) fired across %d injected keys\n\n",
            phases[idx].name, g_phase_results[idx], g_phase_keys[idx]);
    g_phase_idx++;
    next_phase(NULL, NULL);
}

static void
next_phase(IswPointer cl, IswIntervalId *id)
{
    if (g_phase_idx >= (int)NUM_PHASES) {
        fprintf(stderr, "\n========== SUMMARY ==========\n");
        fprintf(stderr, "%-12s  %12s  %s\n", "Widget", "actions/keys", "verdict");
        for (size_t i = 0; i < NUM_PHASES; i++) {
            const char *verdict;
            if (g_phase_results[i] == 0) verdict = "DEAD - no actions fired";
            else if (g_phase_results[i] >= g_phase_keys[i]) verdict = "responsive";
            else verdict = "PARTIAL";
            fprintf(stderr, "%-12s  %6d / %-4d  %s\n",
                    phases[i].name, g_phase_results[i], g_phase_keys[i], verdict);
        }
        /* Continue to dropdown test rather than exit */
        IswAppAddTimeOut(g_app, 100, test_combo_dropdown, NULL);
        return;
    }

    Phase *p = &phases[g_phase_idx];
    g_current_phase = p->name;
    g_current_widget_name = p->name;
    g_actions_fired_this_phase = 0;

    fprintf(stderr, "=== Phase %d: focus -> %s ===\n", g_phase_idx, p->name);
    IswSetKeyboardFocus(g_shell, *p->widget);
    xcb_flush(IswDisplay(g_shell));

    /* Process pending events (including the FocusIn we just triggered) before
     * we start injecting, so per-key correlation is clean. */
    while (IswAppPending(g_app)) {
        IswAppProcessEvent(g_app, IswIMAll);
    }

    for (int k = 0; k < p->num_keys; k++) {
        int before = g_actions_fired_this_phase;
        inject_key(p->keys[k].sym, p->keys[k].name, p->keys[k].state);
        /* Drain events from this single key so we attribute fires correctly */
        xcb_flush(IswDisplay(g_shell));
        struct timespec wait_until;
        clock_gettime(CLOCK_MONOTONIC, &wait_until);
        double deadline = (wait_until.tv_sec * 1000.0 + wait_until.tv_nsec / 1.0e6) + 60;
        while (IswAppPending(g_app)) {
            IswAppProcessEvent(g_app, IswIMAll);
        }
        /* Brief settle time for any synthetic delivery */
        while (1) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            double now = ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
            if (now >= deadline) break;
            if (IswAppPending(g_app)) {
                IswAppProcessEvent(g_app, IswIMAll);
            } else {
                struct timespec slp = { 0, 5 * 1000 * 1000 };
                nanosleep(&slp, NULL);
            }
        }
        int delta = g_actions_fired_this_phase - before;
        fprintf(stderr, "     ^ %-18s -> %d action(s)\n", p->keys[k].name, delta);
    }

    /* Move to next phase */
    IswAppAddTimeOut(g_app, 50, finish_phase, NULL);
}

/* --- Dedicated ComboBox-open-dropdown test --- */
static int g_dropdown_actions = 0;

static void
trace_hook_dropdown(Widget w, IswPointer cl, String name,
                    xcb_generic_event_t *e, String *p, Cardinal *np)
{
    int evt = e ? (e->response_type & 0x7f) : -1;
    g_dropdown_actions++;
    if (evt == XCB_KEY_PRESS) {
        xcb_key_press_event_t *ke = (xcb_key_press_event_t *)e;
        fprintf(stderr, "    [dropdown] FIRED action=%-14s on=%s code=%u\n",
                name, w ? IswName(w) : "?", ke->detail);
    } else {
        fprintf(stderr, "    [dropdown] FIRED action=%-14s on=%s\n",
                name, w ? IswName(w) : "?");
    }
}

static void
test_combo_dropdown_finish(IswPointer cl, IswIntervalId *id)
{
    fprintf(stderr, "\n========== DROPDOWN SUMMARY ==========\n");
    fprintf(stderr, "Dropdown nav actions fired: %d\n", g_dropdown_actions);
    fprintf(stderr, "Expected: NextEntry, NextEntry, NextEntry, PrevEntry, "
                    "FirstEntry, LastEntry, Notify (Return)\n");
    exit(0);
}

static void
test_combo_dropdown(IswPointer cl, IswIntervalId *id)
{
    fprintf(stderr, "\n=== Phase: ComboBox dropdown opened ===\n");

    /* Replace trace hook so we count clearly */
    g_dropdown_actions = 0;
    IswAppAddActionHook(g_app, trace_hook_dropdown, NULL);

    /* Programmatically open the dropdown by calling its Set action with a
     * synthetic ButtonPress at coordinates inside the widget */
    xcb_button_press_event_t be;
    memset(&be, 0, sizeof(be));
    be.response_type = XCB_BUTTON_PRESS | 0x80;
    be.detail = 1;  /* Btn1 */
    be.event = IswWindow(g_combo);
    be.event_x = 5;
    be.event_y = 5;
    be.same_screen = 1;
    String params[1] = {NULL};
    Cardinal nparams = 0;
    IswCallActionProc(g_combo, "Set", (xcb_generic_event_t*)&be, params, nparams);

    /* Drain so dropdown shows up */
    xcb_flush(IswDisplay(g_shell));
    while (IswAppPending(g_app)) IswAppProcessEvent(g_app, IswIMAll);

    /* Find the popup shell created by the Set action */
    fprintf(stderr, "  pre-popup-access\n"); fflush(stderr);
    Widget popup = ((ListWidget)g_combo)->list.popup_shell;
    fprintf(stderr, "  popup_shell=%p\n", (void*)popup); fflush(stderr);
    if (!popup) {
        fprintf(stderr, "  !!! dropdown not opened\n");
        IswAppAddTimeOut(g_app, 100, test_combo_dropdown_finish, NULL);
        return;
    }
    if (!IswIsRealized(popup)) {
        fprintf(stderr, "  !!! popup not realized\n");
        IswAppAddTimeOut(g_app, 100, test_combo_dropdown_finish, NULL);
        return;
    }
    xcb_window_t popup_win = IswWindow(popup);
    fprintf(stderr, "  popup win=0x%x\n", popup_win); fflush(stderr);

    g_current_widget_name = "popup";
    fprintf(stderr, "  injecting keys to dropdown ...\n");

    xcb_keysym_t seq[] = { XK_Down, XK_Down, XK_Down, XK_Up, XK_Home, XK_End, XK_Return };
    const char  *nms[] = { "Down",  "Down",  "Down",  "Up",  "Home",  "End",  "Return" };
    for (size_t i = 0; i < sizeof(seq)/sizeof(seq[0]); i++) {
        inject_key_to(popup_win, seq[i], nms[i], 0);
        xcb_flush(IswDisplay(g_shell));
        while (IswAppPending(g_app)) IswAppProcessEvent(g_app, IswIMAll);
        struct timespec slp = { 0, 30 * 1000 * 1000 };
        nanosleep(&slp, NULL);
        while (IswAppPending(g_app)) IswAppProcessEvent(g_app, IswIMAll);
    }

    IswAppAddTimeOut(g_app, 200, test_combo_dropdown_finish, NULL);
}

static void
start_tests(IswPointer cl, IswIntervalId *id)
{
    g_phase_idx = 0;
    next_phase(NULL, NULL);
}

/* ---- Construction ---- */
int
main(int argc, char *argv[])
{
    Widget toplevel, box;
    IswArgBuilder ab = IswArgBuilderInit();

    setvbuf(stderr, NULL, _IOLBF, 0);
    fprintf(stderr, "[%07.1fms] starting widget keyboard test\n", now_ms());

    toplevel = IswAppInitialize(&g_app, "FocusDispatchTest",
                                NULL, 0, &argc, argv, NULL, NULL, 0);
    g_shell = toplevel;

    IswArgWidth(&ab, 640);
    IswArgHeight(&ab, 480);
    IswArgTitle(&ab, "Focus Dispatch Test");
    IswSetValues(toplevel, ab.args, ab.count);

    IswAppAddActionHook(g_app, trace_hook, NULL);

    box = IswCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Cmd");
    g_command = IswCreateManagedWidget("cmd", commandWidgetClass, box, ab.args, ab.count);

    /* IconView with items */
    {
        static String iv_labels[] = {"A","B","C","D","E","F","G","H"};
        static String iv_icons[]  = {
            "<svg viewBox='0 0 16 16'><rect x='2' y='2' width='12' height='12' fill='none' stroke='black'/></svg>",
            "<svg viewBox='0 0 16 16'><circle cx='8' cy='8' r='6' fill='none' stroke='black'/></svg>",
            "<svg viewBox='0 0 16 16'><rect x='2' y='2' width='12' height='12' fill='none' stroke='black'/></svg>",
            "<svg viewBox='0 0 16 16'><circle cx='8' cy='8' r='6' fill='none' stroke='black'/></svg>",
            "<svg viewBox='0 0 16 16'><rect x='2' y='2' width='12' height='12' fill='none' stroke='black'/></svg>",
            "<svg viewBox='0 0 16 16'><circle cx='8' cy='8' r='6' fill='none' stroke='black'/></svg>",
            "<svg viewBox='0 0 16 16'><rect x='2' y='2' width='12' height='12' fill='none' stroke='black'/></svg>",
            "<svg viewBox='0 0 16 16'><circle cx='8' cy='8' r='6' fill='none' stroke='black'/></svg>",
        };
        IswArgBuilderReset(&ab);
        IswArgIconLabels(&ab, iv_labels);
        IswArgIconData(&ab, iv_icons);
        IswArgNumIcons(&ab, 8);
        IswArgWidth(&ab, 200);
        IswArgHeight(&ab, 120);
        IswArgMultiSelect(&ab, True);
        g_iconview = IswCreateManagedWidget("iv", iconViewWidgetClass, box, ab.args, ab.count);
    }

    /* List with items */
    {
        static String lst[] = {"alpha","beta","gamma","delta","epsilon",NULL};
        IswArgBuilderReset(&ab);
        IswArgList(&ab, lst);
        IswArgWidth(&ab, 200);
        g_list = IswCreateManagedWidget("lst", listWidgetClass, box, ab.args, ab.count);
    }

    /* ComboBox */
    {
        static String cb_items[] = {"red","green","blue",NULL};
        IswArgBuilderReset(&ab);
        IswArgList(&ab, cb_items);
        IswArgWidth(&ab, 200);
        g_combo = IswCreateManagedWidget("cb", comboBoxWidgetClass, box, ab.args, ab.count);
    }

    /* SpinBox */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 200);
    g_spin = IswCreateManagedWidget("sb", spinBoxWidgetClass, box, ab.args, ab.count);

    /* Slider */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 200);
    g_slider = IswCreateManagedWidget("sl", sliderWidgetClass, box, ab.args, ab.count);

    /* Scrollbar */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 200);
    IswArgHeight(&ab, 20);
    IswArgOrientation(&ab, IswOrientHorizontal);
    g_scroll = IswCreateManagedWidget("sc", scrollbarWidgetClass, box, ab.args, ab.count);

    IswRealizeWidget(toplevel);

    g_keysyms = xcb_key_symbols_alloc(IswDisplay(toplevel));

    fprintf(stderr, "[%07.1fms] realized; starting tests in 500ms\n", now_ms());
    IswAppAddTimeOut(g_app, 500, start_tests, NULL);

    IswAppMainLoop(g_app);
    return 0;
}
