/*
 * ScrollWheel.c - Continuous scroll-axis dispatch for ISW widgets
 *
 * Installs a custom IswEventDispatchProc for the IswScroll event kind.
 * When a scroll event arrives (continuous trackpad motion or a discrete
 * wheel notch), walks up the widget tree from the pointer target to find the
 * nearest scrollable container (Viewport, Text) or standalone Scrollbar and
 * forwards it as a scrollProc callback carrying an IswScrollData payload
 * (sub-pixel dx/dy + signed discrete steps + smooth flag).
 *
 * Coordinate handling: the dispatch core (_IswDescaleEventCoords in Event.c,
 * invoked from IswDispatchEvent) has already converted the event's physical
 * coordinates to logical pixels BEFORE this dispatcher runs.  This dispatcher
 * must NOT re-descale — it reads event->scroll.x/y as logical pixels.
 *
 * Discrete-to-pixel scaling: a discrete wheel step carries discrete_x/y = +/-1
 * and delta_x/y = 0.  The dispatcher scales the discrete path by the target
 * Scrollbar's scrollWheelIncrement resource (default
 * ISW_SCROLL_WHEEL_DEFAULT_INCREMENT) into the pixel delta it passes to
 * scrollProc, so the per-scrollbar increment still governs wheel speed.
 * Smooth (continuous) scrolls pass delta_x/y through unchanged — the increment
 * resource is irrelevant for smooth input.
 *
 * Scroll stickiness: once a scrollable container is targeted, subsequent
 * scroll events within ISW_SCROLL_STICKY_MS continue scrolling the same
 * container even if the pointer has moved over a different scrollable.  This
 * prevents jarring target switches mid-scroll.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicI.h>
#include <ISW/StringDefs.h>
#include <ISW/ScrollWheel.h>
#include <ISW/IswScroll.h>
#include <ISW/ScrollbarP.h>
#include <ISW/ViewportP.h>
#include <ISW/TextP.h>
#include <ISW/EventI.h>
#include <ISW/PassivGraI.h>

#include <stdint.h>

static IswEventDispatchProc original_scroll_dispatcher = NULL;
static Boolean scroll_wheel_initialized = False;

/* Scroll stickiness state.  sticky_container is the scrollable container
 * (Viewport/Text/standalone Scrollbar) last scrolled; re-dispatching from it
 * routes both axes to the correct bars without re-hit-testing. */
#define ISW_SCROLL_STICKY_MS 250
static Widget   sticky_container = NULL;
static IswTime  sticky_timestamp = 0;

static void
StickyDestroyCallback(Widget w, IswPointer closure, IswPointer call_data)
{
    if (sticky_container == w)
        sticky_container = NULL;
}

static void
SetStickyTarget(Widget container, IswTime time)
{
    if (container != sticky_container) {
        if (sticky_container != NULL)
            IswRemoveCallback(sticky_container, IswNdestroyCallback,
                              StickyDestroyCallback, NULL);
        sticky_container = container;
        if (container != NULL)
            IswAddCallback(container, IswNdestroyCallback,
                           StickyDestroyCallback, NULL);
    }
    sticky_timestamp = time;
}

/*
 * Dispatch an IswScrollData to a Scrollbar widget's scrollProc, scaling the
 * discrete step count by the bar's scrollWheelIncrement into the pixel delta.
 * The discrete fields are preserved so line-oriented consumers (Text) can map
 * a notch to whole lines.  Updates stickiness to the bar's container.
 */
static void
DispatchToBar(Widget bar, const IswScrollData *in, IswTime time)
{
    ScrollbarWidget sbw = (ScrollbarWidget) bar;
    IswScrollData sd = *in;
    Dimension incr = sbw->scrollbar.scroll_wheel_increment;

    if (incr == 0)
        incr = ISW_SCROLL_WHEEL_DEFAULT_INCREMENT;

    /* Scale the discrete path into the continuous pixel delta.  Smooth input
       already carries pixel delta (discrete == 0) and passes through. */
    sd.dx += (float) sd.discrete_x * (float) incr;
    sd.dy += (float) sd.discrete_y * (float) incr;

    IswCallCallbacks(bar, IswNscrollProc, (IswPointer) &sd);
}

/*
 * Walk up the widget tree from 'start' looking for a scrollable container.
 * Dispatches to the axis-appropriate scrollbar(s) and returns the container
 * widget (for stickiness), or NULL if none found.
 */
static Widget
FindAndDispatchScroll(Widget start, const IswScrollData *sd, IswTime time)
{
    Widget w;
    Widget scrollbar_found = NULL;
    Boolean has_vert  = (sd->dy != 0.0f || sd->discrete_y != 0);
    Boolean has_horiz = (sd->dx != 0.0f || sd->discrete_x != 0);

    for (w = start; w != NULL; w = IswParent(w)) {
        /* Viewport: vert bar takes the vertical axis, horiz bar the horizontal. */
        if (IswIsSubclass(w, viewportWidgetClass)) {
            ViewportWidget vw = (ViewportWidget) w;
            if (has_vert && vw->viewport.vert_bar != NULL)
                DispatchToBar(vw->viewport.vert_bar, sd, time);
            if (has_horiz && vw->viewport.horiz_bar != NULL)
                DispatchToBar(vw->viewport.horiz_bar, sd, time);
            return w;
        }

        /* Text: same axis split between vbar/hbar. */
        if (IswIsSubclass(w, textWidgetClass)) {
            TextWidget tw = (TextWidget) w;
            if (has_vert && tw->text.vbar != NULL)
                DispatchToBar(tw->text.vbar, sd, time);
            if (has_horiz && tw->text.hbar != NULL)
                DispatchToBar(tw->text.hbar, sd, time);
            return w;
        }

        /* Remember first standalone scrollbar as a fallback. */
        if (scrollbar_found == NULL && IswIsSubclass(w, scrollbarWidgetClass))
            scrollbar_found = w;
    }

    /* No Viewport/Text found; use a standalone scrollbar if we passed one.
       Route only the axis matching its orientation. */
    if (scrollbar_found != NULL) {
        ScrollbarWidget sbw = (ScrollbarWidget) scrollbar_found;
        IswScrollData axis = *sd;
        if (sbw->scrollbar.orientation == IswOrientVertical) {
            axis.dx = 0.0f;
            axis.discrete_x = 0;
            if (has_vert)
                DispatchToBar(scrollbar_found, &axis, time);
        } else {
            axis.dy = 0.0f;
            axis.discrete_y = 0;
            if (has_horiz)
                DispatchToBar(scrollbar_found, &axis, time);
        }
        return scrollbar_found;
    }

    return NULL;
}

/*
 * Custom IswScroll event dispatcher.  Hit-tests the pointer to find the
 * nearest scrollable, honoring the sticky target and an in-flight windowless
 * pointer grab (so a scroll during a drag stays targeted).  Coordinates are
 * already logical (descaled by the dispatch core before this runs).
 */
static Boolean
ScrollDispatcher(IswEvent *event, IswDisplay conn)
{
    IswScrollData sd;
    IswTime time;
    Widget root;
    Widget target = NULL;

    if (event->kind != IswScroll)
        return False;

    sd.dx = event->scroll.delta_x;
    sd.dy = event->scroll.delta_y;
    sd.discrete_x = event->scroll.discrete_x;
    sd.discrete_y = event->scroll.discrete_y;
    sd.smooth = event->scroll.smooth;
    time = event->any.time;

    /* Sticky target: keep scrolling the same container within the sticky
       window so a quick succession of wheel notches doesn't retarget. */
    if (sticky_container != NULL &&
        (time - sticky_timestamp) < ISW_SCROLL_STICKY_MS &&
        IswIsWidget(sticky_container) &&
        !sticky_container->core.being_destroyed) {
        Widget c = FindAndDispatchScroll(sticky_container, &sd, time);
        if (c != NULL) {
            SetStickyTarget(c, time);
            return True;
        }
    }

    root = (Widget) (void *) event->any.target;
    if (root != NULL) {
        int dx = 0, dy = 0;
        target = _IswFindWidgetAtPoint(root, event->scroll.x, event->scroll.y,
                                       &dx, &dy);
    }

    /* Honor an in-flight windowless pointer grab: a scroll while a button
       drag is active stays targeted at the grabbed widget's scrollable. */
    if (target == NULL) {
        IswPerDisplayInput pdi = _IswGetPerDisplayInput(conn);
        if (pdi != NULL && pdi->windowlessButtonGrab != NULL &&
            IswIsWidget(pdi->windowlessButtonGrab) &&
            !pdi->windowlessButtonGrab->core.being_destroyed)
            target = pdi->windowlessButtonGrab;
    }

    if (target != NULL) {
        Widget c = FindAndDispatchScroll(target, &sd, time);
        if (c != NULL)
            SetStickyTarget(c, time);
        else
            SetStickyTarget(NULL, 0);
    } else {
        SetStickyTarget(NULL, 0);
    }

    /* Always consume scroll events — they are fully handled here. */
    return True;
}

/*
 * Install the scroll event dispatcher for the given connection.
 * Safe to call multiple times; only the first call has any effect.
 */
void
ISWScrollWheelInit(IswDisplay conn)
{
    if (scroll_wheel_initialized)
        return;
    scroll_wheel_initialized = True;

    original_scroll_dispatcher = IswSetEventDispatcher(
        conn, IswScroll, ScrollDispatcher);
}
