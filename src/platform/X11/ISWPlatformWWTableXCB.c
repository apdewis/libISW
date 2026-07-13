/*
 * ISWPlatformWWTableXCB.c - window -> widget association table (X11 backend)
 *
 * Copyright (c) 2026 ISW Project
 *
 * The OS delivers input events tagged with a native window.  The toolkit core
 * is windowless and never sees windows; this table — owned by the X11 backend —
 * resolves a native window to the widget it belongs to, so the backend can
 * stamp each translated IswEvent with its target widget (see target_for_window
 * in ISWPlatformEventXCB.c).
 *
 * A widget's own top-level window hashes to the widget directly; "foreign" or
 * extra windows a widget wants events from (a selection requestor window, the
 * tray's screen-root and icon-docking windows) live in the `pairs` list.  This
 * is the window machinery that used to sit in the toolkit core (Event.c); it
 * has been relocated here intact.  IswRegisterDrawable / IswUnregisterDrawable /
 * IswWindowToWidget remain the public, window-facing API used by the backend
 * protocol code (selections, tray, shell).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <xcb/xcb.h>

#include "IntrinsicI.h"
#include "InitialI.h"
#include "ISWPlatformPrivate.h"
#include "ISWPlatformDisplayXCB.h"

typedef struct _WWPair {
    struct _WWPair *next;
    xcb_window_t window;
    Widget widget;
} *WWPair;

typedef struct _WWTable {
    unsigned int mask;          /* size of hash table - 1 */
    unsigned int rehash;        /* mask - 2 */
    unsigned int occupied;      /* number of occupied entries */
    unsigned int fakes;         /* number occupied by WWfake */
    Widget *entries;            /* the entries */
    WWPair pairs;               /* windows not equal to a widget's own window */
} *WWTable;

static const WidgetRec WWfake;  /* placeholder for deletions */

#define WWHASH(tab,win) ((win) & tab->mask)
#define WWREHASHVAL(tab,win) ((((win) % tab->rehash) + 2) | 1)
#define WWREHASH(tab,idx,rehash) ((unsigned)(idx + rehash) & (tab->mask))
#define WWTABLE(display) (((IswDisplayXCB *)(display))->wwtable)

static void ExpandWWTable(WWTable);

void
IswRegisterDrawable(IswDisplay display, IswWindow drawable, Widget widget)
{
    WWTable tab;
    int idx;
    Widget entry;
    xcb_window_t window = _IswXcbWindow(drawable);


    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);

    if (window != _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)))) {
        WWPair pair;
        pair = IswNew(struct _WWPair);

        pair->next = tab->pairs;
        pair->window = window;
        pair->widget = widget;
        tab->pairs = pair;
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return;
    }
    if ((tab->occupied + (tab->occupied >> 2)) > tab->mask)
        ExpandWWTable(tab);

    idx = (int) WWHASH(tab, window);
    if ((entry = tab->entries[idx]) && entry != &WWfake) {
        int rehash = (int) WWREHASHVAL(tab, window);

        do {
            idx = (int) WWREHASH(tab, idx, rehash);
        } while ((entry = tab->entries[idx]) && entry != &WWfake);
    }
    if (!entry)
        tab->occupied++;
    else if (entry == &WWfake)
        tab->fakes--;
    tab->entries[idx] = widget;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

void
IswUnregisterDrawable(IswDisplay display, IswWindow drawable)
{
    WWTable tab;
    int idx;
    Widget entry;
    xcb_window_t window = _IswXcbWindow(drawable);
    Widget widget = IswWindowToWidget(display, drawable);
    DPY_TO_APPCON(display);

    if (widget == NULL)
        return;

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);
    if (window != _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)))) {
        WWPair *prev, pair;

        prev = &tab->pairs;
        while ((pair = *prev) && pair->window != window)
            prev = &pair->next;
        if (pair) {
            *prev = pair->next;
            IswFree((char *) pair);
        }
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return;
    }
    idx = (int) WWHASH(tab, window);
    if ((entry = tab->entries[idx])) {
        if (entry != widget) {
            int rehash = (int) WWREHASHVAL(tab, window);

            do {
                idx = (int) WWREHASH(tab, idx, rehash);
                if (!(entry = tab->entries[idx])) {
                    UNLOCK_PROCESS;
                    UNLOCK_APP(app);
                    return;
                }
            } while (entry != widget);
        }
        tab->entries[idx] = (Widget) &WWfake;
        tab->fakes++;
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

static void
ExpandWWTable(register WWTable tab)
{
    unsigned int oldmask;
    register Widget *oldentries, *entries;
    register Cardinal oldidx, newidx, rehash;
    register Widget entry;

    LOCK_PROCESS;
    oldmask = tab->mask;
    oldentries = tab->entries;
    tab->occupied -= tab->fakes;
    tab->fakes = 0;
    if ((tab->occupied + (tab->occupied >> 2)) > tab->mask) {
        tab->mask = (tab->mask << 1) + 1;
        tab->rehash = tab->mask - 2;
    }
    entries = tab->entries =
        (Widget *) __IswCalloc(tab->mask + 1, sizeof(Widget));
    for (oldidx = 0; oldidx <= oldmask; oldidx++) {
        if ((entry = oldentries[oldidx]) && entry != &WWfake) {
            newidx = (Cardinal) WWHASH(tab, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(entry)), (Widget)(entry))));
            if (entries[newidx]) {
                rehash = (Cardinal) WWREHASHVAL(tab, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(entry)), (Widget)(entry))));
                do {
                    newidx = (Cardinal) WWREHASH(tab, newidx, rehash);
                } while (entries[newidx]);
            }
            entries[newidx] = entry;
        }
    }
    IswFree((char *) oldentries);
    UNLOCK_PROCESS;
}

Widget
IswWindowToWidget(IswDisplay display, IswWindow window_opaque)
{
    WWTable tab;
    int idx;
    Widget entry;
    WWPair pair;
    register xcb_window_t window = _IswXcbWindow(window_opaque);
    DPY_TO_APPCON(display);

    if (!window)
        return NULL;

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);
    idx = (int) WWHASH(tab, window);
    /* Probe the open-addressed table.  The WWfake deletion sentinel is a zeroed
       WidgetRec that is NOT a real widget: it must never be returned, and it
       must never terminate the probe (it marks a slot vacated by a deletion,
       past which a live entry may still hash).  Treat it as "occupied but not a
       match" — keep probing — and only a genuine window match wins. */
#define WWMATCH(e) ((e) != NULL && (e) != &WWfake && \
                    _IswXcbWindow(_IswPlatformWidgetWindow(display, (e))) == window)
    entry = tab->entries[idx];
    if (entry != NULL && !WWMATCH(entry)) {
        int rehash = (int) WWREHASHVAL(tab, window);

        do {
            idx = (int) WWREHASH(tab, idx, rehash);
            entry = tab->entries[idx];
        } while (entry != NULL && !WWMATCH(entry));
    }
    if (entry != NULL && entry != &WWfake) {
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return entry;
    }
#undef WWMATCH
    for (pair = tab->pairs; pair; pair = pair->next) {
        if (pair->window == window) {
            entry = pair->widget;
            UNLOCK_PROCESS;
            UNLOCK_APP(app);
            return entry;
        }
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return NULL;
}

/* True if `widget` is still registered in this display's window->widget table
   (as a window owner or an extra/foreign window).  A widget is unregistered
   when its window is destroyed (Phase 2 destroy), so this is a liveness test
   for a Widget pointer that survives in a queued event: a dangling target whose
   widget was freed since the event was enqueued is no longer registered and the
   event must be discarded rather than dispatched into freed memory. */
Boolean
_IswXcbWidgetRegistered(IswDisplay display, Widget widget)
{
    WWTable tab;
    WWPair pair;
    unsigned int i;
    Boolean found = FALSE;
    DPY_TO_APPCON(display);

    if (widget == NULL)
        return FALSE;

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);
    if (tab != NULL) {
        for (i = 0; i <= tab->mask; i++) {
            Widget entry = tab->entries[i];
            if (entry == widget) {     /* &WWfake never equals a real widget */
                found = TRUE;
                break;
            }
        }
        if (!found) {
            for (pair = tab->pairs; pair; pair = pair->next) {
                if (pair->widget == widget) {
                    found = TRUE;
                    break;
                }
            }
        }
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return found;
}

/* Allocate / free the table; called from display open/close. */
void
_IswXcbAllocWWTable(IswDisplay display)
{
    register WWTable tab;

    tab = (WWTable) __IswMalloc(sizeof(struct _WWTable));
    tab->mask = 0x7f;
    tab->rehash = tab->mask - 2;
    tab->entries = (Widget *) __IswCalloc(tab->mask + 1, sizeof(Widget));
    tab->occupied = 0;
    tab->fakes = 0;
    tab->pairs = NULL;
    ((IswDisplayXCB *) display)->wwtable = tab;
}

void
_IswXcbFreeWWTable(IswDisplay display)
{
    register WWPair pair, next;
    WWTable tab = ((IswDisplayXCB *) display)->wwtable;

    if (tab == NULL)
        return;
    for (pair = tab->pairs; pair; pair = next) {
        next = pair->next;
        IswFree((char *) pair);
    }
    IswFree((char *) tab->entries);
    IswFree((char *) tab);
    ((IswDisplayXCB *) display)->wwtable = NULL;
}
