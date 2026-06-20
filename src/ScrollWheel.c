/*
 * ScrollWheel.c - Scroll wheel support for ISW widgets
 *
 * Installs a custom IswEventDispatchProc for ButtonPress and ButtonRelease
 * events. When a scroll wheel button (4/5 vertical, 6/7 horizontal) is
 * detected, walks up the widget tree from the pointer target to find the
 * nearest scrollable container (Viewport, Text) or standalone Scrollbar,
 * and forwards the scroll event as a scrollProc callback.
 *
 * The scroll increment is read from the target Scrollbar widget's
 * scrollWheelIncrement resource (default ISW_SCROLL_WHEEL_DEFAULT_INCREMENT
 * pixels per notch).
 *
 * Shift+button4/5 switches to horizontal scrolling.
 * Buttons 6/7 always scroll horizontally.
 *
 * Scroll stickiness: once a scrollbar is targeted, subsequent scroll
 * events within ISW_SCROLL_STICKY_MS continue scrolling the same widget
 * even if the pointer has moved over a different scrollable. This
 * prevents jarring target switches mid-scroll.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ScrollWheel.h>
#include <ISW/ScrollbarP.h>
#include <ISW/ViewportP.h>
#include <ISW/TextP.h>
#include <ISW/EventI.h>

/* Defined in Initialize.c — avoids pulling in InitialI.h's heavy deps. */
extern double _IswGetScaleFactor(IswDisplay dpy);

#include <stdint.h>

/* Saved original dispatchers so we can chain to them */
static IswEventDispatchProc original_press_dispatcher = NULL;
static IswEventDispatchProc original_release_dispatcher = NULL;
static Boolean scroll_wheel_initialized = False;

/* Scroll stickiness state */
#define ISW_SCROLL_STICKY_MS 250
static Widget         sticky_scrollbar = NULL;
static IswTime sticky_timestamp = 0;

/*
 * Called when the sticky scrollbar widget is destroyed, so we don't
 * hold a dangling pointer.
 */
static void
StickyDestroyCallback(Widget w, IswPointer closure, IswPointer call_data)
{
    if (sticky_scrollbar == w)
        sticky_scrollbar = NULL;
}

/*
 * Set the sticky scrollbar target.  Registers a destroy callback so
 * the pointer is cleared if the widget goes away.
 */
static void
SetStickyTarget(Widget bar, IswTime time)
{
    if (bar != sticky_scrollbar) {
        if (sticky_scrollbar != NULL)
            IswRemoveCallback(sticky_scrollbar, IswNdestroyCallback,
                             StickyDestroyCallback, NULL);
        sticky_scrollbar = bar;
        if (bar != NULL)
            IswAddCallback(bar, IswNdestroyCallback,
                          StickyDestroyCallback, NULL);
    }
    sticky_timestamp = time;
}

/*
 * Dispatch a scroll to the given scrollbar widget and update stickiness.
 */
static void
ScrollTo(Widget bar, int direction, IswTime time)
{
    ScrollbarWidget sbw = (ScrollbarWidget)bar;
    intptr_t increment = direction *
        (intptr_t)sbw->scrollbar.scroll_wheel_increment;
    IswCallCallbacks(bar, IswNscrollProc, (IswPointer)increment);
    SetStickyTarget(bar, time);
}

/*
 * Determine scroll direction and axis from button detail and modifier state.
 * Returns True if this is a scroll wheel event, False otherwise.
 */
static Boolean
DecodeScrollWheel(IswEvent *event,
                  int *direction_out, Boolean *horizontal_out)
{
    Boolean shift = (event->button.modifiers & IswModShift) != 0;
    switch (event->button.button) {
    case IswButtonWheelUp:
        *direction_out = -1;
        *horizontal_out = shift;
        return True;
    case IswButtonWheelDown:
        *direction_out = 1;
        *horizontal_out = shift;
        return True;
    case IswButtonWheelLeft:
        *direction_out = -1;
        *horizontal_out = True;
        return True;
    case IswButtonWheelRight:
        *direction_out = 1;
        *horizontal_out = True;
        return True;
    default:
        return False;
    }
}

/*
 * Walk up the widget tree from 'start' looking for a scrollable container.
 * Returns the scrollbar widget that was dispatched to, or NULL.
 */
static Widget
FindAndDispatchScroll(Widget start, int direction, Boolean horizontal,
                      IswTime time)
{
    Widget w;
    Widget scrollbar_found = NULL;

    for (w = start; w != NULL; w = IswParent(w)) {
        /* Check for Viewport - preferred target */
        if (IswIsSubclass(w, viewportWidgetClass)) {
            ViewportWidget vw = (ViewportWidget)w;
            Widget bar = horizontal ? vw->viewport.horiz_bar
                                    : vw->viewport.vert_bar;
            if (bar != NULL)
                ScrollTo(bar, direction, time);
            return bar;
        }

        /* Check for Text widget */
        if (IswIsSubclass(w, textWidgetClass)) {
            TextWidget tw = (TextWidget)w;
            Widget bar = horizontal ? tw->text.hbar : tw->text.vbar;
            if (bar != NULL)
                ScrollTo(bar, direction, time);
            return bar;
        }

        /* Remember first scrollbar seen (fallback for standalone scrollbars) */
        if (scrollbar_found == NULL && IswIsSubclass(w, scrollbarWidgetClass)) {
            scrollbar_found = w;
        }
    }

    /* No Viewport or Text found; use standalone scrollbar if we passed one */
    if (scrollbar_found != NULL) {
        ScrollTo(scrollbar_found, direction, time);
        return scrollbar_found;
    }

    return NULL;
}

/*
 * Custom ButtonPress event dispatcher.
 * Intercepts scroll wheel buttons (4-7) and routes them via ancestor walk.
 * All other button events are passed to the original dispatcher.
 */
static Boolean
ScrollWheelPressDispatcher(IswEvent *event, IswDisplay conn)
{
    if (event->kind == IswButtonDown) {
        int direction;
        Boolean horizontal;

        if (DecodeScrollWheel(event, &direction, &horizontal)) {
            IswTime time = event->any.time;
            /* If we have a recent sticky target, keep using it */
            if (sticky_scrollbar != NULL &&
                (time - sticky_timestamp) < ISW_SCROLL_STICKY_MS) {
                ScrollTo(sticky_scrollbar, direction, time);
            } else {
                /* The event's target is the root widget; the scrollable
                   container under the pointer is a windowless descendant.
                   Hit-test the pointer to find the deepest windowless widget,
                   then walk up to its Viewport/Text. */
                Widget root = (Widget) (void *) event->any.target;
                Widget target = NULL;
                if (root != NULL) {
                    int dx = 0, dy = 0;
                    /* This custom dispatcher runs before the default one
                       descales event coords, so x/y are physical; the hit-test
                       works in logical pixels. */
                    double sf = _IswGetScaleFactor(conn);
                    int lx = (sf > 1.0) ? (int)(event->button.x / sf)
                                        : event->button.x;
                    int ly = (sf > 1.0) ? (int)(event->button.y / sf)
                                        : event->button.y;
                    target = _IswFindWidgetAtPoint(root, lx, ly, &dx, &dy);
                }
                if (target != NULL)
                    FindAndDispatchScroll(target, direction, horizontal, time);
                else
                    SetStickyTarget(NULL, 0);
            }
            /* Always consume scroll wheel press events */
            return True;
        }
    }

    /* Not a scroll wheel event — chain to original dispatcher */
    return (*original_press_dispatcher)(event, conn);
}

/*
 * Custom ButtonRelease event dispatcher.
 * Consumes scroll wheel release events (4-7) to prevent them from
 * triggering EndScroll or other unintended actions on scrollbar widgets.
 */
static Boolean
ScrollWheelReleaseDispatcher(IswEvent *event, IswDisplay conn)
{
    if (event->kind == IswButtonUp) {
        if (event->button.button >= 4 && event->button.button <= 7)
            return True; /* consume silently */
    }

    return (*original_release_dispatcher)(event, conn);
}

/*
 * Install scroll wheel event dispatchers for the given connection.
 * Safe to call multiple times; only the first call has any effect.
 */
void
ISWScrollWheelInit(IswDisplay conn)
{
    if (scroll_wheel_initialized)
        return;
    scroll_wheel_initialized = True;

    original_press_dispatcher = IswSetEventDispatcher(
        conn, IswButtonDown, ScrollWheelPressDispatcher);

    original_release_dispatcher = IswSetEventDispatcher(
        conn, IswButtonUp, ScrollWheelReleaseDispatcher);
}
