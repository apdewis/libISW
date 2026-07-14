/***********************************************************
Copyright (c) 1993, Oracle and/or its affiliates.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*

Copyright 1987, 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "IntrinsicI.h"
#include "InitialI.h"
#include "Shell.h"
#include "StringDefs.h"
#include "FocusMgrI.h"
#include "ShellI.h"
#include <ISW/SimpleP.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWPlatform.h>

typedef struct _IswEventRecExt {
    int type;
    IswPointer select_data[1];   /* actual dimension is [mask] */
} IswEventRecExt;

#define EXT_TYPE(p) (((IswEventRecExt*) ((p)+1))->type)
#define EXT_SELECT_DATA(p,n) (((IswEventRecExt*) ((p)+1))->select_data[n])

#define NonMaskableMask ((EventMask)0x80000000L)

/*
 * These are definitions to make the code that handles exposure compression
 * easier to read.
 *
 * COMP_EXPOSE      - The compression exposure field of "widget"
 * COMP_EXPOSE_TYPE - The type of compression (lower 4 bits of COMP_EXPOSE.
 */

#define COMP_EXPOSE   (widget->core.widget_class->core_class.compress_exposure)
#define COMP_EXPOSE_TYPE (COMP_EXPOSE & 0x0f)

/* HiDPI: convert physical pixel event coordinates to logical pixels.
   The backend translation deliberately copies coordinates straight through;
   this dispatch-core pass descales them before routing. */
static void
_IswDescaleEventCoords(IswEvent *event, double sf)
{
    if (sf <= 1.0)
        return;
    float inv = 1.0f / (float)sf;

    switch (event->kind) {
    case IswKeyDown:
    case IswKeyUp:
        event->key.x = (int32_t)(event->key.x * inv);
        event->key.y = (int32_t)(event->key.y * inv);
        event->key.root_x = (int16_t)(event->key.root_x * inv);
        event->key.root_y = (int16_t)(event->key.root_y * inv);
        event->key.shell_x = (int16_t)(event->key.shell_x * inv);
        event->key.shell_y = (int16_t)(event->key.shell_y * inv);
        break;
    case IswButtonDown:
    case IswButtonUp:
        event->button.x = (int32_t)(event->button.x * inv);
        event->button.y = (int32_t)(event->button.y * inv);
        event->button.root_x = (int16_t)(event->button.root_x * inv);
        event->button.root_y = (int16_t)(event->button.root_y * inv);
        event->button.shell_x = (int16_t)(event->button.shell_x * inv);
        event->button.shell_y = (int16_t)(event->button.shell_y * inv);
        break;
    case IswMotion:
        event->motion.x = (int32_t)(event->motion.x * inv);
        event->motion.y = (int32_t)(event->motion.y * inv);
        event->motion.root_x = (int16_t)(event->motion.root_x * inv);
        event->motion.root_y = (int16_t)(event->motion.root_y * inv);
        event->motion.shell_x = (int16_t)(event->motion.shell_x * inv);
        event->motion.shell_y = (int16_t)(event->motion.shell_y * inv);
        break;
    case IswScroll:
        event->scroll.x = (int32_t)(event->scroll.x * inv);
        event->scroll.y = (int32_t)(event->scroll.y * inv);
        event->scroll.root_x = (int16_t)(event->scroll.root_x * inv);
        event->scroll.root_y = (int16_t)(event->scroll.root_y * inv);
        event->scroll.shell_x = (int16_t)(event->scroll.shell_x * inv);
        event->scroll.shell_y = (int16_t)(event->scroll.shell_y * inv);
        break;
    case IswEnter:
    case IswLeave:
        event->crossing.x = (int32_t)(event->crossing.x * inv);
        event->crossing.y = (int32_t)(event->crossing.y * inv);
        event->crossing.root_x = (int16_t)(event->crossing.root_x * inv);
        event->crossing.root_y = (int16_t)(event->crossing.root_y * inv);
        event->crossing.shell_x = (int16_t)(event->crossing.shell_x * inv);
        event->crossing.shell_y = (int16_t)(event->crossing.shell_y * inv);
        break;
    case IswRedraw:
        event->redraw.x = (int16_t)(event->redraw.x * inv);
        event->redraw.y = (int16_t)(event->redraw.y * inv);
        event->redraw.width = (uint16_t)(event->redraw.width * inv + 0.5f);
        event->redraw.height = (uint16_t)(event->redraw.height * inv + 0.5f);
        break;
    case IswGeometry:
        event->geometry.x = (int16_t)(event->geometry.x * inv);
        event->geometry.y = (int16_t)(event->geometry.y * inv);
        event->geometry.width = (uint16_t)(event->geometry.width * inv + 0.5f);
        event->geometry.height = (uint16_t)(event->geometry.height * inv + 0.5f);
        event->geometry.border_width =
            (uint16_t)(event->geometry.border_width * inv + 0.5f);
        break;
    default:
        break;
    }
}

EventMask
IswBuildEventMask(Widget widget)
{
    EventMask mask = 0L;

    if (widget != NULL) {
        IswEventTable ev;

        WIDGET_TO_APPCON(widget);

        LOCK_APP(app);
        for (ev = widget->core.event_table; ev != NULL; ev = ev->next) {
            if (!ev->select)
                continue;

            if (!ev->has_type_specifier)
                mask |= ev->mask;
            else {
                if (EXT_TYPE(ev) < LASTEvent) {
                    Cardinal i;

                    for (i = 0; i < ev->mask; i++)
                        if (EXT_SELECT_DATA(ev, i))
                            mask |= *(EventMask *) EXT_SELECT_DATA(ev, i);
                }
            }
        }
        LOCK_PROCESS;
        if (widget->core.widget_class->core_class.expose != NULL)
            mask |= IswExposureMask;
        if (widget->core.widget_class->core_class.visible_interest)
            mask |= IswVisibilityChangeMask;
        UNLOCK_PROCESS;
        if (widget->core.tm.translations)
            mask |= widget->core.tm.translations->eventMask;

        mask = mask & ~NonMaskableMask;
        UNLOCK_APP(app);
    }
    return mask;
}

/* Union of IswBuildEventMask over a widget's windowless descendants.
   Windowless widgets have no window to select on, so the events they need
   must be selected on their nearest windowed ancestor's window. */
static EventMask
_IswWindowlessDescendantMask(Widget w)
{
    EventMask mask = 0L;
    CompositeWidget cw;
    Cardinal i;

    if (!IswIsComposite(w))
        return 0L;

    cw = (CompositeWidget) w;
    for (i = 0; i < cw->composite.num_children; i++) {
        Widget child = cw->composite.children[i];

        if (!IswIsWidget(child))
            continue;
        mask |= IswBuildEventMask(child);
        mask |= _IswWindowlessDescendantMask(child);
    }
    return mask;
}

/* Mask to actually select on a windowed widget's window: its own mask plus
   the masks of all windowless descendants that share the window. */
EventMask
_IswWindowSelectMask(Widget widget)
{
    EventMask mask = IswBuildEventMask(widget);

    if (widget != NULL && IswIsWidget(widget))
        mask |= (_IswWindowlessDescendantMask(widget) & ~NonMaskableMask);
    return mask;
}

/* Push the aggregate select mask onto the windowed ancestor's window after a
   windowless widget's handlers or translations changed. */
void
_IswUpdateWindowlessAncestorMask(Widget windowless)
{
    Widget anc;
    EventMask mask;

    if (!IswIsWidget(windowless))
        return;
    anc = _IswWidgetAncestor(windowless);
    if (anc == NULL || !IswIsRealized(anc) || anc->core.being_destroyed)
        return;
    mask = _IswWindowSelectMask(anc);
    {
        IswWindowAttributes attrs;
        memset(&attrs, 0, sizeof(attrs));
        attrs.event_mask = mask;
        _IswPlatformChangeAttributes(IswDisplayOf(anc),
                                     _IswPlatformWidgetWindow(IswDisplayOf(anc), anc),
                                     &attrs, ISW_ATTR_EVENT_MASK);
    }
}

static void
CallExtensionSelector(Widget widget, ExtSelectRec *rec, Boolean forceCall)
{
    IswEventRec *p;
    IswPointer *data;
    int *types;
    Cardinal i, count = 0;

    for (p = widget->core.event_table; p != NULL; p = p->next)
        if (p->has_type_specifier &&
            EXT_TYPE(p) >= rec->min && EXT_TYPE(p) <= rec->max)
            count = (Cardinal) (count + p->mask);

    if (count == 0 && !forceCall)
        return;

    data = (IswPointer *) ALLOCATE_LOCAL(count * sizeof(IswPointer));
    types = (int *) ALLOCATE_LOCAL(count * sizeof(int));
    count = 0;

    for (p = widget->core.event_table; p != NULL; p = p->next)
        if (p->has_type_specifier &&
            EXT_TYPE(p) >= rec->min && EXT_TYPE(p) <= rec->max)
            for (i = 0; i < p->mask; i++) {
                types[count] = EXT_TYPE(p);
                data[count++] = EXT_SELECT_DATA(p, i);
            }

    (*rec->proc) (widget, types, data, (int) count, rec->client_data);
    DEALLOCATE_LOCAL((char *) types);
    DEALLOCATE_LOCAL((char *) data);
}

static void
RemoveEventHandler(Widget widget,
                   IswPointer select_data,
                   int type,
                   Boolean has_type_specifier,
                   Boolean other,
                   const IswEventHandler proc,
                   const IswPointer closure,
                   Boolean raw)
{
    IswEventRec *p, **pp;
    EventMask oldMask = IswBuildEventMask(widget);

    if (raw)
        raw = 1;
    pp = &widget->core.event_table;
    while ((p = *pp) &&
           (p->proc != proc || p->closure != closure || p->select == raw ||
            has_type_specifier != p->has_type_specifier ||
            (has_type_specifier && EXT_TYPE(p) != type)))
        pp = &p->next;
    if (!p)
        return;

    /* un-register it */
    if (!has_type_specifier) {
        EventMask eventMask = *(EventMask *) select_data;

        eventMask &= ~NonMaskableMask;
        if (other)
            eventMask |= NonMaskableMask;
        p->mask &= ~eventMask;
    }
    else {
        Cardinal i;

        /* p->mask specifies count of EXT_SELECT_DATA(p,i)
         * search through the list of selection data, if not found
         * don't remove this handler
         */
        for (i = 0; i < p->mask && select_data != EXT_SELECT_DATA(p, i);)
            i++;
        if (i == p->mask)
            return;
        if (p->mask == 1)
            p->mask = 0;
        else {
            p->mask--;
            while (i < p->mask) {
                EXT_SELECT_DATA(p, i) = EXT_SELECT_DATA(p, i + 1);
                i++;
            }
        }
    }

    if (!p->mask) {             /* delete it entirely */
        *pp = p->next;
        IswFree((char *) p);
    }

    /* Reset select mask if realized and not raw. */
    if (!raw && IswIsRealized(widget) && !widget->core.being_destroyed) {
        EventMask mask = IswBuildEventMask(widget);

        if (!IswIsShell(widget)) {
            /* No own window — fold this widget's mask into the windowed
               ancestor's selection. */
            _IswUpdateWindowlessAncestorMask(widget);
        }
        else if (oldMask != mask) {
            EventMask sel = _IswWindowSelectMask(widget);
            IswWindowAttributes attrs;
            memset(&attrs, 0, sizeof(attrs));
            attrs.event_mask = sel;
            _IswPlatformChangeAttributes(IswDisplayOf(widget),
                                         _IswPlatformWidgetWindow(IswDisplayOf(widget), widget),
                                         &attrs, ISW_ATTR_EVENT_MASK);
        }

        if (has_type_specifier) {
            IswPerDisplay pd = _IswGetPerDisplay(IswDisplayOf(widget));
            int i;

            for (i = 0; i < pd->ext_select_count; i++) {
                if (type >= pd->ext_select_list[i].min &&
                    type <= pd->ext_select_list[i].max) {
                    CallExtensionSelector(widget, pd->ext_select_list + i,
                                          TRUE);
                    break;
                }
                if (type < pd->ext_select_list[i].min)
                    break;
            }
        }
    }
}

/*      Function Name: AddEventHandler
 *      Description: An Internal routine that does the actual work of
 *                   adding the event handlers.
 *      Arguments: widget - widget to register an event handler for.
 *                 eventMask - events to mask for.
 *                 other - pass non maskable events to this procedure.
 *                 proc - procedure to register.
 *                 closure - data to pass to the event handler.
 *                 position - where to add this event handler.
 *                 force_new_position - If the element is already in the
 *                                      list, this will force it to the
 *                                      beginning or end depending on position.
 *                 raw - If FALSE call XSelectInput for events in mask.
 *      Returns: none
 */

static void
AddEventHandler(Widget widget,
                IswPointer select_data,
                int type,
                Boolean has_type_specifier,
                Boolean other,
                IswEventHandler proc,
                IswPointer closure,
                IswListPosition position,
                Boolean force_new_position,
                Boolean raw)
{
    register IswEventRec *p, **pp;
    EventMask oldMask = 0, eventMask = 0;

    if (!has_type_specifier) {
        eventMask = *(EventMask *) select_data & ~NonMaskableMask;
        if (other)
            eventMask |= NonMaskableMask;
        if (!eventMask)
            return;
    }
    else if (!type)
        return;

    if (IswIsRealized(widget) && !raw)
        oldMask = IswBuildEventMask(widget);

    if (raw)
        raw = 1;
    pp = &widget->core.event_table;
    while ((p = *pp) &&
           (p->proc != proc || p->closure != closure || p->select == raw ||
            has_type_specifier != p->has_type_specifier ||
            (has_type_specifier && EXT_TYPE(p) != type)))
        pp = &p->next;

    if (!p) {                   /* New proc to add to list */
        if (has_type_specifier) {
            p = (IswEventRec *) __IswMalloc(sizeof(IswEventRec) +
                                          sizeof(IswEventRecExt));
            EXT_TYPE(p) = type;
            EXT_SELECT_DATA(p, 0) = select_data;
            p->mask = 1;
            p->has_type_specifier = True;
        }
        else {
            p = (IswEventRec *) __IswMalloc(sizeof(IswEventRec));
            p->mask = eventMask;
            p->has_type_specifier = False;
        }
        p->proc = proc;
        p->closure = closure;
        p->select = !raw;

        if (position == IswListHead) {
            p->next = widget->core.event_table;
            widget->core.event_table = p;
        }
        else {
            *pp = p;
            p->next = NULL;
        }
    }
    else {
        if (force_new_position) {
            *pp = p->next;

            if (position == IswListHead) {
                p->next = widget->core.event_table;
                widget->core.event_table = p;
            }
            else {
                /*
                 * Find the last element in the list.
                 */
                while (*pp)
                    pp = &(*pp)->next;
                *pp = p;
                p->next = NULL;
            }
        }

        if (!has_type_specifier)
            p->mask |= eventMask;
        else {
            Cardinal i;

            /* p->mask specifies count of EXT_SELECT_DATA(p,i) */
            for (i = 0; i < p->mask && select_data != EXT_SELECT_DATA(p, i);)
                i++;
            if (i == p->mask) {
                p = (IswEventRec *) IswRealloc((char *) p,
                                             (Cardinal) (sizeof(IswEventRec) +
                                                         sizeof(IswEventRecExt) +
                                                         p->mask *
                                                         sizeof(IswPointer)));
                EXT_SELECT_DATA(p, i) = select_data;
                p->mask++;
                *pp = p;
            }
        }
    }

    if (IswIsRealized(widget) && !raw) {
        EventMask mask = IswBuildEventMask(widget);

        if (!IswIsShell(widget)) {
            _IswUpdateWindowlessAncestorMask(widget);
        }
        else if (oldMask != mask) {
            EventMask sel = _IswWindowSelectMask(widget);
            IswWindowAttributes attrs;
            memset(&attrs, 0, sizeof(attrs));
            attrs.event_mask = sel;
            _IswPlatformChangeAttributes(IswDisplayOf(widget),
                                         _IswPlatformWidgetWindow(IswDisplayOf(widget), widget),
                                         &attrs, ISW_ATTR_EVENT_MASK);
        }

        if (has_type_specifier) {
            IswPerDisplay pd = _IswGetPerDisplay(IswDisplayOf(widget));
            int i;

            for (i = 0; i < pd->ext_select_count; i++) {
                if (type >= pd->ext_select_list[i].min &&
                    type <= pd->ext_select_list[i].max) {
                    CallExtensionSelector(widget, pd->ext_select_list + i,
                                          FALSE);
                    break;
                }
                if (type < pd->ext_select_list[i].min)
                    break;
            }
        }
    }
}

void
IswRemoveEventHandler(Widget widget,
                     EventMask eventMask,
                     _IswBoolean other,
                     IswEventHandler proc,
                     IswPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    RemoveEventHandler(widget, (IswPointer) &eventMask, 0, FALSE,
                       (Boolean) other, proc, closure, FALSE);
    UNLOCK_APP(app);
}

void
IswAddEventHandler(Widget widget,
                  EventMask eventMask,
                  _IswBoolean other,
                  IswEventHandler proc,
                  IswPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (IswPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, IswListTail, FALSE, FALSE);
    UNLOCK_APP(app);
}

void
IswInsertEventHandler(Widget widget,
                     EventMask eventMask,
                     _IswBoolean other,
                     IswEventHandler proc,
                     IswPointer closure,
                     IswListPosition position)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (IswPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, position, TRUE, FALSE);
    UNLOCK_APP(app);
}

void
IswRemoveRawEventHandler(Widget widget,
                        EventMask eventMask,
                        _IswBoolean other,
                        IswEventHandler proc,
                        IswPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    RemoveEventHandler(widget, (IswPointer) &eventMask, 0, FALSE,
                       (Boolean) other, proc, closure, TRUE);
    UNLOCK_APP(app);
}

void
IswInsertRawEventHandler(Widget widget,
                        EventMask eventMask,
                        _IswBoolean other,
                        IswEventHandler proc,
                        IswPointer closure,
                        IswListPosition position)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (IswPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, position, TRUE, TRUE);
    UNLOCK_APP(app);
}

void
IswAddRawEventHandler(Widget widget,
                     EventMask eventMask,
                     _IswBoolean other,
                     IswEventHandler proc,
                     IswPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (IswPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, IswListTail, FALSE, TRUE);
    UNLOCK_APP(app);
}

void
IswRemoveEventTypeHandler(Widget widget,
                         int type,
                         IswPointer select_data,
                         IswEventHandler proc,
                         IswPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    RemoveEventHandler(widget, select_data, type, TRUE,
                       FALSE, proc, closure, FALSE);
    UNLOCK_APP(app);
}

void
IswInsertEventTypeHandler(Widget widget,
                         int type,
                         IswPointer select_data,
                         IswEventHandler proc,
                         IswPointer closure,
                         IswListPosition position)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, select_data, type, TRUE, FALSE,
                    proc, closure, position, TRUE, FALSE);
    UNLOCK_APP(app);
}

/* Accumulate a widget's origin relative to its nearest windowed ancestor by
   walking up through windowless parents. */
static void
_IswWindowlessOffset(Widget w, int *dx, int *dy)
{
    int ox = 0, oy = 0;

    while (w != NULL && IswIsWidget(w) && !IswIsShell(w)) {
        ox += w->core.x + (int) w->core.border_width;
        oy += w->core.y + (int) w->core.border_width;
        w = w->core.parent;
    }
    *dx = ox;
    *dy = oy;
}

/* Expose delegation for windowless widgets.
 *
 * Windowless widgets have no window, so the server never sends them Expose
 * events.  When a windowed widget receives an Expose, walk its windowless
 * descendants in stacking order (array order: earlier children first, i.e.
 * bottom-to-top) and invoke each one's expose proc so it repaints.  Each
 * widget's render context resolves to the windowed ancestor's window and
 * offsets/clips itself (see ISWRenderBegin / the render backend).
 *
 * Recurses into windowless composites.  Stops at windowed children: they own
 * their own window and receive their own Expose from the server. */
static void _IswExposeWindowlessChildren(Widget w, IswEvent *event,
                                         int ox, int oy);

/* Paint one windowless child (and recurse into its windowless descendants).

   (ox, oy) is the parent's content origin in the damage rectangle's frame
   (the windowed root's).  When `event` carries a non-empty damage rectangle,
   a child whose footprint does not intersect it is skipped along with its
   whole subtree: an expose means the WINDOW's pixels were lost, not the
   widget's — its retained surface still holds current content and the
   composite that follows the walk folds every surface regardless, restoring
   the damaged region.  (A child sticking out of its parent is also safe to
   skip with it: the fold clips children to the parent's content rect.)
   A NULL event means no damage information: paint everything. */
static void
_IswPaintWindowlessChild(Widget child, IswEvent *event, int ox, int oy)
{
    if (!IswIsWidget(child) || IswIsShell(child))
        return;
    /* Paint realized windowless children that are mapped.  windowless_mapped is
       the live "the window is mapped" equivalent (driven by map/unmap/manage);
       an unmapped child (non-current Tabs page, last Paned grip) must not
       paint. */
    if (!IswIsRealized(child) && !child->core.windowless_realized)
        return;
    if (!child->core.windowless_mapped)
        return;

    if (event != NULL && event->redraw.width > 0 && event->redraw.height > 0) {
        int fx = ox + child->core.x;
        int fy = oy + child->core.y;
        int fw = (int) child->core.width + 2 * (int) child->core.border_width;
        int fh = (int) child->core.height + 2 * (int) child->core.border_width;
        /* 2px slop: the physical->logical descale of the damage rectangle
           (_IswDescaleEventCoords) floors its origin and rounds its size,
           which can pull each far edge up to ~1.5 logical px inside the true
           damage bound.  Widen rather than ever skip a damaged widget. */
        int dx1 = (int) event->redraw.x - 2;
        int dy1 = (int) event->redraw.y - 2;
        int dx2 = (int) event->redraw.x + (int) event->redraw.width + 2;
        int dy2 = (int) event->redraw.y + (int) event->redraw.height + 2;
        if (fx >= dx2 || fy >= dy2 || fx + fw <= dx1 || fy + fh <= dy1)
            return;
    }

    /* A clean widget with a retained surface needs no repaint on a damage
       expose: its surface content is current (every content change marks
       composite_dirty; folds clear it) and the composite folds it regardless.
       Only skip the expose proc — recursion continues below, because "clean"
       does not imply the descendants are (ISWRenderCreate marks only the new
       widget itself, not its chain).  A NULL event is an explicit repaint
       request and always paints. */
    Boolean skip_paint = (event != NULL &&
                          !child->core.composite_dirty &&
                          IswSurfaceOf(child) != NULL);

    if (!skip_paint &&
        child->core.widget_class->core_class.expose != NULL) {
        IswEvent nev;
        memset(&nev, 0, sizeof(nev));
        nev.kind = IswRedraw;
        nev.redraw.width = child->core.width;
        nev.redraw.height = child->core.height;
        (*child->core.widget_class->core_class.expose)(child, &nev, 0);
    }

    _IswExposeWindowlessChildren(child, event,
                                 ox + child->core.x
                                    + (int) child->core.border_width,
                                 oy + child->core.y
                                    + (int) child->core.border_width);
}

/* Paint a windowless widget and its windowless descendants into their own
   surfaces, then composite the subtree's windowed ancestor.  Used when a
   widget transitions hidden->shown (e.g. IswMapWidget on a Tabs page): the
   composite pass only folds existing surfaces, so a page that was never
   painted while hidden must be drawn now or it would appear blank. */
void
_IswRepaintWindowless(Widget w)
{
    Widget anc;
    if (!IswIsWidget(w) || IswIsShell(w))
        return;
    if (!IswIsRealized(w) && !w->core.windowless_realized)
        return;

    ISWRenderBeginCompositeBatch();
    if (w->core.widget_class->core_class.expose != NULL) {
        IswEvent nev;
        memset(&nev, 0, sizeof(nev));
        nev.kind = IswRedraw;
        nev.redraw.width = w->core.width;
        nev.redraw.height = w->core.height;
        (*w->core.widget_class->core_class.expose)(w, &nev, 0);
    }
    _IswExposeWindowlessChildren(w, NULL, 0, 0);
    ISWRenderEndCompositeBatch();

    /* The batch suppressed ISWRenderEnd's dirty-chain marking, but this
       widget's contribution DID just change — mark the chain so the composite
       below runs a real fold instead of the clean-tree re-present fast path. */
    _ISWRenderMarkDirtyChain(w);

    anc = _IswWidgetAncestor(w);
    if (anc != NULL && IswIsRealized(anc))
        ISWRenderRequestComposite(anc);
}

static void
_IswExposeWindowlessChildren(Widget w, IswEvent *event, int ox, int oy)
{
    /* Composite children held in composite.children. */
    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++)
            _IswPaintWindowlessChild(cw->composite.children[i], event, ox, oy);
    }

    /* Non-composite-tracked windowless sub-widgets (e.g. Text's scrollbars),
       enumerated via the Simple class hook. */
    if (IswIsSubclass(w, simpleWidgetClass)) {
        SimpleWidgetClass sc = (SimpleWidgetClass) w->core.widget_class;
        if (sc->simple_class.nth_windowless_child != NULL) {
            int i = 0;
            Widget child;
            while ((child = (*sc->simple_class.nth_windowless_child)(w, i++))
                   != NULL)
                _IswPaintWindowlessChild(child, event, ox, oy);
        }
    }
}

/* Dispatch a synthesized Enter/Leave crossing event to a windowless widget,
   derived from the triggering motion event (root coords preserved, window
   coords rebased to the target's windowed-ancestor frame). */
static void
_IswSynthesizeCrossing(Widget w, IswEvent *source, IswEventKind kind)
{
    IswEvent ev;
    int dx, dy;

    if (!IswIsSensitive(w) || !IswIsRealized(w))
        return;
    if (!(IswBuildEventMask(w) &
          (kind == IswEnter ? IswEnterWindowMask : IswLeaveWindowMask)))
        return;

    _IswWindowlessOffset(w, &dx, &dy);

    memset(&ev, 0, sizeof(ev));
    ev.kind = kind;
    ev.crossing.mode = IswNotifyNormal;
    ev.crossing.modifiers = IswEventModifiers(source);
    ev.crossing.x = (int32_t) (IswEventX(source) - dx);
    ev.crossing.y = (int32_t) (IswEventY(source) - dy);
    ev.crossing.root_x = IswEventRootX(source);
    ev.crossing.root_y = IswEventRootY(source);
    ev.crossing.shell_x = IswEventShellX(source);
    ev.crossing.shell_y = IswEventShellY(source);
    ev.any.time = source->any.time;
    ev.any.target = (IswEventTarget) (void *) w;

    IswDispatchEventToWidget(w, &ev);
}

/*
 * Windowless hit-testing.
 *
 * Windowless widgets have no X window, so the server cannot route pointer
 * events to them; the event is reported against the nearest windowed
 * ancestor's window.  Given that ancestor (root) and a point in its
 * coordinate space, descend the widget tree to find the deepest windowless
 * widget under the point.  Descent stops at windowed children: the server
 * delivers their events directly, so they are never redirected here.
 *
 * On a match, dx and dy receive the origin of the returned widget relative
 * to root, so callers can rebase event coordinates to widget-local.
 */
/* Hit-test the normal (non-overlay) windowless subtree of `start` at (x,y),
   with `start`'s content already offset by (ox0, oy0) from the window origin.
   Returns the deepest windowless widget under the point, its origin in *dx/*dy.
   Overlays are skipped here — they are tested first, at a higher priority, by
   _IswFindWidgetAtPoint. */
static Widget
_IswFindWidgetAtPointFrom(Widget start, int ox0, int oy0,
                          int x, int y, int *dx, int *dy)
{
    Widget target = start;
    int ox = ox0, oy = oy0;

    for (;;) {
        Widget hit = NULL;

        if (IswIsComposite(target)) {
            CompositeWidget cw = (CompositeWidget) target;
            int i;

            /* Reverse stacking order: last child is topmost. */
            for (i = (int) cw->composite.num_children - 1; i >= 0; i--) {
                Widget child = cw->composite.children[i];
                int cx, cy, cw_, ch;

                if (!IswIsRectObj(child))
                    continue;
                /* Only windowless children are hit-tested here; windowed
                   children receive their own events from the server. */
                if (!IswIsWidget(child) || IswIsShell(child))
                    continue;
                /* Same "shown" rule as paint/clip: windowless_mapped is the
                   live mapped flag; an unmapped child is not hit-tested. */
                if (!child->core.windowless_mapped)
                    continue;
                /* Overlays are hit-tested first at higher priority (see
                   _IswFindWidgetAtPoint); skip them in the normal descent so a
                   point over an overlay is not claimed by content beneath it. */
                if (child->core.windowless_overlay)
                    continue;

                cx = child->core.x;
                cy = child->core.y;
                cw_ = (int) child->core.width + 2 * (int) child->core.border_width;
                ch = (int) child->core.height + 2 * (int) child->core.border_width;

                if (x >= ox + cx && x < ox + cx + cw_ &&
                    y >= oy + cy && y < oy + cy + ch) {
                    /* A composite-clipped child (e.g. a Viewport's scrolled
                       content) only occupies its clip region — the area outside
                       it (scrollbar bands) belongs to whatever is painted there.
                       Skip the child when the point is outside its clip so the
                       scrollbar (tested next) wins. */
                    int clx, cly, clw, clh;
                    if (ISWRenderGetCompositeClip(child, &clx, &cly, &clw, &clh) &&
                        !(x >= ox + clx && x < ox + clx + clw &&
                          y >= oy + cly && y < oy + cly + clh))
                        continue;
                    hit = child;
                    break;
                }
            }
        }

        /* Non-composite widgets that own windowless sub-widgets (e.g. Text's
           scrollbars) expose them through the Simple class hit_child hook. */
        if (hit == NULL && IswIsSubclass(target, simpleWidgetClass)) {
            SimpleWidgetClass sc = (SimpleWidgetClass) target->core.widget_class;
            if (sc->simple_class.hit_child != NULL) {
                int cdx = 0, cdy = 0;
                Widget child = (*sc->simple_class.hit_child)(target,
                                                             x - ox, y - oy,
                                                             &cdx, &cdy);
                if (child != NULL) {
                    target = child;
                    ox += cdx;
                    oy += cdy;
                    if (!IswIsComposite(target) &&
                        !IswIsSubclass(target, simpleWidgetClass))
                        break;
                    continue;
                }
            }
        }

        if (hit == NULL)
            break;

        target = hit;
        /* Descend into the hit child's content area (inside its border). */
        ox += hit->core.x + (int) hit->core.border_width;
        oy += hit->core.y + (int) hit->core.border_width;

        if (!IswIsComposite(target) &&
            !IswIsSubclass(target, simpleWidgetClass))
            break;
    }

    *dx = ox;
    *dy = oy;
    return target;
}

static Boolean
_IswIsAncestorOf(Widget ancestor, Widget descendant)
{
    Widget w;
    for (w = descendant; w != NULL && IswIsWidget(w) && !IswIsShell(w);
         w = w->core.parent) {
        if (w == ancestor)
            return True;
    }
    return False;
}

static Widget
_IswFindActiveOverlay(Widget parent)
{
    if (!IswIsComposite(parent))
        return NULL;

    CompositeWidget cw = (CompositeWidget) parent;
    int i;

    for (i = (int) cw->composite.num_children - 1; i >= 0; i--) {
        Widget child = cw->composite.children[i];

        if (!IswIsWidget(child) || IswIsShell(child))
            continue;

        if (child->core.windowless_overlay && child->core.windowless_mapped) {
            Widget deeper = _IswFindActiveOverlay(child);
            if (deeper != NULL)
                return deeper;
            return child;
        }

        if (IswIsComposite(child) && child->core.windowless_mapped) {
            Widget r = _IswFindActiveOverlay(child);
            if (r != NULL)
                return r;
        }
    }
    return NULL;
}

/* Find the topmost shown overlay (in-window popup menu) under (x,y), searching
   the whole subtree of `parent` (offset by ox,oy from the window origin).
   Overlays composite above all content, so they take hit-test priority and
   nested overlays (submenus) take priority over their parent overlay — the
   search descends into an overlay before accepting it.  Returns the deepest
   windowless widget within the winning overlay, its origin in *dx/*dy, or NULL
   if the point is over no overlay. */
static Widget
_IswFindOverlayAtPoint(Widget parent, int ox, int oy, int x, int y,
                       int *dx, int *dy)
{
    if (!IswIsComposite(parent))
        return NULL;

    CompositeWidget cw = (CompositeWidget) parent;
    int i;

    /* Reverse stacking order: last child is topmost. */
    for (i = (int) cw->composite.num_children - 1; i >= 0; i--) {
        Widget child = cw->composite.children[i];
        int cx, cy, cbo_x, cbo_y;

        if (!IswIsWidget(child) || IswIsShell(child))
            continue;

        cx = ox + child->core.x;
        cy = oy + child->core.y;
        cbo_x = cx + (int) child->core.border_width;
        cbo_y = cy + (int) child->core.border_width;

        if (child->core.windowless_overlay && child->core.windowless_mapped) {
            int cw_ = (int) child->core.width + 2 * (int) child->core.border_width;
            int ch  = (int) child->core.height + 2 * (int) child->core.border_width;

            /* A nested overlay (submenu) sits above this one — check it first. */
            Widget deeper = _IswFindOverlayAtPoint(child, cbo_x, cbo_y,
                                                   x, y, dx, dy);
            if (deeper != NULL)
                return deeper;

            /* Otherwise, if the point is inside this overlay, descend into its
               normal (non-overlay) content. */
            if (x >= cx && x < cx + cw_ && y >= cy && y < cy + ch)
                return _IswFindWidgetAtPointFrom(child, cbo_x, cbo_y,
                                                 x, y, dx, dy);
        }
        else if (IswIsComposite(child) && child->core.windowless_mapped) {
            /* Descend through non-overlay containers to find overlays nested
               anywhere in the tree. */
            Widget r = _IswFindOverlayAtPoint(child, cbo_x, cbo_y,
                                              x, y, dx, dy);
            if (r != NULL)
                return r;
        }
    }
    return NULL;
}

Widget
_IswFindWidgetAtPoint(Widget root, int x, int y, int *dx, int *dy)
{
    /* Overlays (in-window popup menus) composite above all content, so a point
       over an overlay must hit the overlay, not the content painted beneath it.
       Test overlays first; fall back to the normal subtree otherwise. */
    Widget over = _IswFindOverlayAtPoint(root, 0, 0, x, y, dx, dy);
    if (over != NULL)
        return over;

    return _IswFindWidgetAtPointFrom(root, 0, 0, x, y, dx, dy);
}

/* Rebase a pointer/key/crossing event's window-local coordinates by
   (-dx, -dy) so they are relative to a windowless target widget. */
static void
_IswRebaseEventCoords(IswEvent *event, int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;

    switch (event->kind) {
    case IswKeyDown:
    case IswKeyUp:
        event->key.x = (int32_t) (event->key.x - dx);
        event->key.y = (int32_t) (event->key.y - dy);
        break;
    case IswButtonDown:
    case IswButtonUp:
        event->button.x = (int32_t) (event->button.x - dx);
        event->button.y = (int32_t) (event->button.y - dy);
        break;
    case IswMotion:
        event->motion.x = (int32_t) (event->motion.x - dx);
        event->motion.y = (int32_t) (event->motion.y - dy);
        break;
    case IswEnter:
    case IswLeave:
        event->crossing.x = (int32_t) (event->crossing.x - dx);
        event->crossing.y = (int32_t) (event->crossing.y - dy);
        break;
    default:
        break;
    }
}

/* Extract window-local pointer coordinates from a pointer event.
   Returns False for events without pointer coordinates. */
static Boolean
_IswEventPointerXY(IswEvent *event, int *x, int *y)
{
    /* Keyboard events are intentionally excluded: they route to the focus
       widget, not by pointer position. */
    switch (event->kind) {
    case IswButtonDown:
    case IswButtonUp:
        *x = event->button.x;
        *y = event->button.y;
        return True;
    case IswMotion:
        *x = event->motion.x;
        *y = event->motion.y;
        return True;
    case IswScroll:
        *x = event->scroll.x;
        *y = event->scroll.y;
        return True;
    case IswEnter:
    case IswLeave:
        *x = event->crossing.x;
        *y = event->crossing.y;
        return True;
    default:
        return False;
    }
}

#define EHMAXSIZE 25            /* do not make whopping big */

static Boolean
CallEventHandlers(Widget widget, IswEvent *event,
                  IswEvent *nev, EventMask mask)
{
    register IswEventRec *p;
    IswEventHandler *proc;
    IswPointer *closure;
    Boolean cont_to_disp = True;
    int i, numprocs;

    (void) event;

    /* Have to copy the procs into an array, because one of them might
     * call IswRemoveEventHandler, which would break our linked list. */

    numprocs = 0;
    for (p = widget->core.event_table; p; p = p->next) {
        if ((!p->has_type_specifier && (mask & p->mask)) ||
            (p->has_type_specifier && False))
            numprocs++;
    }
    proc = IswMallocArray((Cardinal) numprocs, (Cardinal)
                         (sizeof(IswEventHandler) + sizeof(IswPointer)));
    closure = (IswPointer *) (proc + numprocs);

    numprocs = 0;
    for (p = widget->core.event_table; p; p = p->next) {
        if ((!p->has_type_specifier && (mask & p->mask)) ||
            (p->has_type_specifier && False)) {
            proc[numprocs] = p->proc;
            closure[numprocs] = p->closure;
            numprocs++;
        }
    }
    /* FUNCTIONS CALLED THROUGH POINTER proc:
       Selection.c:ReqCleanup,
       "Shell.c":EventHandler,
       PassivGrab.c:ActiveHandler,
       PassivGrab.c:RealizeHandler,
       Keyboard.c:QueryEventMask,
       _IswHandleFocus,
       Selection.c:HandleSelectionReplies,
       Selection.c:HandleGetIncrement,
       Selection.c:HandleIncremental,
       Selection.c:HandlePropertyGone,
       Selection.c:HandleSelectionEvents
     */
    for (i = 0; i < numprocs && cont_to_disp; i++)
        (*(proc[i])) (widget, closure[i], nev, &cont_to_disp);
    IswFree((char *) proc);
    return cont_to_disp;
}

/* keep this SMALL to avoid blowing stack cache! */
/* because some compilers allocate all local locals on procedure entry */
#define EHSIZE 4

Boolean
IswDispatchEventToWidget(Widget widget, IswEvent *event)
{
    register IswEventRec *p;
    Boolean was_dispatched = False;
    Boolean call_tm = False;
    Boolean cont_to_disp;
    EventMask mask;
    IswEvent *nev = event; /* neutral event handed to public procs/handlers */

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);

    mask = _IswConvertKindToMask(event->kind);

    LOCK_PROCESS;
    if ((mask == IswExposureMask)) {

        if (widget->core.widget_class->core_class.expose != NULL) {
            /* For redraw events, check if this is the last in the series */
            if (event->kind == IswRedraw) {
                /* Only dispatch when count == 0 (last event in series) or
                 * when compression is disabled */
                if (event->redraw.count == 0 ||
                    COMP_EXPOSE_TYPE == IswExposeNoCompress) {
                    (*widget->core.widget_class->core_class.expose)
                        (widget, nev, 0);
                    /* Repaint windowless descendants into their own surfaces,
                       then composite the subtree onto this window once.  The
                       damage rectangle culls untouched subtrees. */
                    ISWRenderBeginCompositeBatch();
                    _IswExposeWindowlessChildren(widget, event, 0, 0);
                    ISWRenderEndCompositeBatch();
                    ISWRenderRequestComposite(widget);
                    was_dispatched = True;
                }
            }
        }
        else if (event->kind == IswRedraw) {
            /* Windowed widget has no expose proc of its own (e.g. a bare
               shell or composite), but may host windowless children that
               still need to repaint. */
            if (event->redraw.count == 0 ||
                COMP_EXPOSE_TYPE == IswExposeNoCompress) {
                ISWRenderBeginCompositeBatch();
                _IswExposeWindowlessChildren(widget, event, 0, 0);
                ISWRenderEndCompositeBatch();
                ISWRenderRequestComposite(widget);
                was_dispatched = True;
            }
        }
    }

    if ((event->kind == IswVisibility) &&
        IswClass(widget)->core_class.visible_interest) {
        was_dispatched = True;
        /* our visibility just changed... */
        switch (event->structure.visibility) {
        case 0:                 /* unobscured */
            widget->core.visible = TRUE;
            break;

        case 1:                 /* partially obscured */
            /* well... some of us is visible */
            widget->core.visible = TRUE;
            break;

        case 2:                 /* fully obscured */
            widget->core.visible = FALSE;
            /* do we want to mark our children obscured? */
            break;
        }
    }
    UNLOCK_PROCESS;

    /* to maintain "copy" semantics we check TM now but call later */
    if (widget->core.tm.translations &&
        (mask & widget->core.tm.translations->eventMask))
        call_tm = True;


    cont_to_disp = True;
    p = widget->core.event_table;
    if (p) {
        if (p->next) {
            IswEventHandler proc[EHSIZE];
            IswPointer closure[EHSIZE];
            int numprocs = 0;

            /* Have to copy the procs into an array, because one of them might
             * call IswRemoveEventHandler, which would break our linked list. */

            for (; p; p = p->next) {
                if ((!p->has_type_specifier && (mask & p->mask)) ||
                    (p->has_type_specifier && False)) {
                    if (numprocs >= EHSIZE)
                        break;
                    proc[numprocs] = p->proc;
                    closure[numprocs] = p->closure;
                    numprocs++;
                }
            }
            if (numprocs) {
                if (p) {
                    cont_to_disp = CallEventHandlers(widget, event, nev, mask);
                }
                else {
                    int i;

                    for (i = 0; i < numprocs && cont_to_disp; i++)
                        (*(proc[i])) (widget, closure[i], nev, &cont_to_disp);
                    /* FUNCTIONS CALLED THROUGH POINTER proc:
                       Selection.c:ReqCleanup,
                       "Shell.c":EventHandler,
                       PassivGrab.c:ActiveHandler,
                       PassivGrab.c:RealizeHandler,
                       Keyboard.c:QueryEventMask,
                       _IswHandleFocus,
                       Selection.c:HandleSelectionReplies,
                       Selection.c:HandleGetIncrement,
                       Selection.c:HandleIncremental,
                       Selection.c:HandlePropertyGone,
                       Selection.c:HandleSelectionEvents
                     */
                }
                was_dispatched = True;
            }
        }
        else if ((!p->has_type_specifier && (mask & p->mask)) ||
                 (p->has_type_specifier && False)) {
            (*p->proc) (widget, p->closure, nev, &cont_to_disp);
            was_dispatched = True;
        }
    }
    if (call_tm && cont_to_disp)
        _IswTranslateEvent(widget, event);
    UNLOCK_APP(app);
    return (was_dispatched | call_tm);
}


void
_IswEventInitialize(void)
{
    /* No-op: null_region is now initialized per-display in Display.c */
}

/* Map a neutral event kind to the select mask a widget's window uses to receive
   it.  Mask values are the neutral Isw*Mask constants (Intrinsic.h).  Kinds the
   toolkit never selects on (protocol/close) map to NonMaskableMask. */
EventMask
_IswConvertKindToMask(IswEventKind kind)
{
    switch (kind) {
    case IswKeyDown:        return IswKeyPressMask;
    case IswKeyUp:          return IswKeyReleaseMask;
    case IswButtonDown:     return IswButtonPressMask;
    case IswButtonUp:       return IswButtonReleaseMask;
    case IswMotion:         return IswPointerMotionMask |
                                   IswPointerMotionHintMask |
                                   IswButton1MotionMask | IswButton2MotionMask |
                                   IswButton3MotionMask | IswButton4MotionMask |
                                   IswButton5MotionMask | IswButtonMotionMask;
    case IswEnter:          return IswEnterWindowMask;
    case IswLeave:          return IswLeaveWindowMask;
    case IswFocusIn:
    case IswFocusOut:       return IswFocusChangeMask;
    case IswRedraw:         return IswExposureMask;
    case IswVisibility:     return IswVisibilityChangeMask;
    case IswGeometry:
    case IswReparent:
    case IswMap:
    case IswUnmap:
    case IswDestroy:        return IswStructureNotifyMask;
    case IswMappingChanged:
    case IswProtocol:
    default:                return NonMaskableMask;
    }
}

Boolean
_IswOnGrabList(register Widget widget, IswGrabRec *grabList)
{
    register IswGrabRec *gl;

    for (; widget != NULL; widget = (Widget) widget->core.parent) {
        for (gl = grabList; gl != NULL; gl = gl->next) {
            if (gl->widget == widget)
                return TRUE;
            if (gl->exclusive)
                break;
        }
    }
    return FALSE;
}

static Boolean
DispatchEvent(IswEvent *event, Widget widget)
{
    return IswDispatchEventToWidget(widget, event);
}

typedef enum _GrabType { pass, ignore, remap } GrabType;

static void
WindowlessGrabDestroyCallback(Widget widget,
                              IswPointer closure _X_UNUSED,
                              IswPointer call_data _X_UNUSED)
{
    IswPerDisplayInput pdi =
        _IswGetPerDisplayInput(IswDisplayOf(widget));
    if (pdi->windowlessButtonGrab == widget) {
        pdi->windowlessButtonGrab = NULL;
        pdi->buttonsDown = 0;
    }
    if (pdi->pointerWidget == widget)
        pdi->pointerWidget = NULL;
}

static Boolean
_IswDefaultDispatcher(IswEvent *event, IswDisplay dpy)
{
    register Widget widget;
    GrabType grabType;
    IswPerDisplayInput pdi;
    IswGrabList grabList;
    Boolean was_dispatched = False;
    DPY_TO_APPCON(dpy);

    /* HiDPI descale now happens in IswDispatchEvent before the dispatcher is
       selected, so a custom dispatcher (e.g. the IswScroll dispatcher) sees
       logical coordinates and must NOT re-descale. */

    LOCK_APP(app);

    if (event->kind == IswNoEvent) {
        UNLOCK_APP(app);
        return False;
    }

    switch (event->kind) {
    case IswKeyDown:
    case IswKeyUp:
    case IswButtonDown:
    case IswButtonUp:
        grabType = remap;
        break;
    case IswMotion:
    case IswEnter:
        grabType = ignore;
        break;
    default:
        grabType = pass;
        break;
    }

    widget = (Widget) (void *) event->any.target;
    pdi = _IswGetPerDisplayInput(dpy);

    /* Windowless hit-testing: pointer events are reported against the
       windowed ancestor's window.  Redirect to the deepest windowless
       widget under the pointer and rebase the event coordinates to it.
       Keyboard events are routed by focus, not position, so are excluded.

       Suppressed entirely during an XDnd drag: a real pointer grab on the
       shell owns the pointer, so every pointer event must pass straight
       through to the shell's HandleDragEvent (which tracks the target and
       moves the drag icon).  Redirecting/hit-testing them to a windowless
       child under the pointer would freeze the drag while the cursor is still
       over this app's own window. */
    if (widget != NULL && !pdi->xdndDragActive) {
        int px, py;
        if (_IswEventPointerXY(event, &px, &py)) {
            int dx, dy;
            IswEventKind etype = event->kind;
            Widget target = _IswFindWidgetAtPoint(widget, px, py, &dx, &dy);

            /* Cross-window grab flush: the implicit windowless grab owns a
               widget under one shell, but a server-level pointer grab (e.g.
               a popup menu's IswGrabPointer) can redirect events to a
               different shell's window.  When that happens the grabbed widget
               will never see its Leave or ButtonUp via the normal hit-test
               path.  Detect the mismatch, synthesize Leave, and drop the
               grab so the widget clears its pressed state. */
            if (pdi->windowlessButtonGrab != NULL &&
                (pdi->windowlessButtonGrab->core.being_destroyed ||
                 _IswWidgetAncestor(pdi->windowlessButtonGrab) != widget)) {
                Widget old_pw = pdi->pointerWidget;
                Widget gw = pdi->windowlessButtonGrab;
                pdi->pointerWidget = NULL;
                pdi->buttonsDown = 0;
                IswRemoveCallback(gw, IswNdestroyCallback,
                    WindowlessGrabDestroyCallback, NULL);
                pdi->windowlessButtonGrab = NULL;
                if (old_pw != NULL && IswIsWidget(old_pw)
                    && !IswIsShell(old_pw) && !old_pw->core.being_destroyed)
                    _IswSynthesizeCrossing(old_pw, event, IswLeave);
                else if (IswIsWidget(gw) && !gw->core.being_destroyed)
                    _IswSynthesizeCrossing(gw, event, IswLeave);
            }

            /* Windowless event propagation: the X server used to deliver a
               pointer event to the first ancestor *window* that had the event
               selected, walking up the window tree.  With windowless widgets
               sharing one window that propagation is lost — hit-testing returns
               only the deepest widget under the pointer.  Restore the server's
               behaviour for maskable button press+release by ascending from the
               deepest windowless target to the first windowless ancestor whose
               selected event mask includes this event, and treating that widget
               as the recipient (so the implicit grab below pins it too, keeping
               a press/drag/release on one widget).  Motion/crossing keep
               targeting the deepest widget, matching how the server reported
               those to the innermost window. */
            if (target != widget &&
                (etype == IswButtonDown || etype == IswButtonUp)) {
                EventMask emask = _IswConvertKindToMask(event->kind);
                Widget a;
                for (a = target;
                     a != NULL && a != widget && IswIsWidget(a) &&
                     !IswIsShell(a);
                     a = a->core.parent) {
                    if (IswBuildEventMask(a) & emask) {
                        int ax = 0, ay = 0;
                        Widget b;
                        for (b = a;
                             b != NULL && IswIsWidget(b) && !IswIsShell(b);
                             b = b->core.parent) {
                            ax += b->core.x + (int) b->core.border_width;
                            ay += b->core.y + (int) b->core.border_width;
                        }
                        target = a;
                        dx = ax;
                        dy = ay;
                        break;
                    }
                }
            }

            if (etype == IswButtonDown) {
                Widget active_overlay = _IswFindActiveOverlay(widget);
                if (active_overlay != NULL &&
                    target != active_overlay &&
                    !_IswIsAncestorOf(active_overlay, target)) {
                    int ox = 0, oy = 0;
                    Widget a;
                    for (a = active_overlay;
                         a != NULL && IswIsWidget(a) && !IswIsShell(a);
                         a = a->core.parent) {
                        ox += a->core.x + (int) a->core.border_width;
                        oy += a->core.y + (int) a->core.border_width;
                    }
                    target = active_overlay;
                    dx = ox;
                    dy = oy;
                }
            }

            /* Implicit windowless pointer grab: the first button press starts a
               grab on the windowless widget hit; while any button stays down,
               motion is routed to that widget instead of being re-hit-tested
               by position.  Without this a drag (Paned sash, Slider thumb)
               loses its target the moment the pointer slips off the widget. */
            if (etype == IswButtonDown) {
                if (pdi->buttonsDown == 0) {
                    Widget newGrab =
                        (target != widget && IswIsWidget(target) &&
                         !IswIsShell(target)) ? target : NULL;
                    if (pdi->windowlessButtonGrab != newGrab) {
                        if (pdi->windowlessButtonGrab != NULL)
                            IswRemoveCallback(pdi->windowlessButtonGrab,
                                IswNdestroyCallback,
                                WindowlessGrabDestroyCallback, NULL);
                        pdi->windowlessButtonGrab = newGrab;
                        if (newGrab != NULL)
                            IswAddCallback(newGrab, IswNdestroyCallback,
                                WindowlessGrabDestroyCallback, NULL);
                    }
                }
                pdi->buttonsDown |= (1u << event->button.button);
            }

            /* While the grab is held, redirect motion AND the button release
               to the grabbed widget, rebasing coordinates to it (its offset
               from the windowed root, same convention as
               _IswFindWidgetAtPoint's dx,dy).  Routing the release here lets
               the grab's <BtnUp> action fire even if the pointer has slipped
               off the widget.  The real hit-test result (hitTarget) is kept
               for crossing synthesis so the grabbed widget still sees
               Enter/Leave as the pointer moves in and out of its bounds. */
            Widget hitTarget = target;
            if ((etype == IswMotion || etype == IswButtonUp ||
                 etype == IswScroll) &&
                pdi->buttonsDown != 0 &&
                pdi->windowlessButtonGrab != NULL &&
                IswIsWidget(pdi->windowlessButtonGrab) &&
                !pdi->windowlessButtonGrab->core.being_destroyed) {
                Widget g = pdi->windowlessButtonGrab;
                int gx = 0, gy = 0;
                Widget a;
                for (a = g; a != NULL && IswIsWidget(a) && !IswIsShell(a);
                     a = a->core.parent) {
                    gx += a->core.x + (int) a->core.border_width;
                    gy += a->core.y + (int) a->core.border_width;
                }
                target = g;
                dx = gx;
                dy = gy;
            }

            /* Release: clear the per-button bit; drop the grab when the last
               button comes up. */
            if (etype == IswButtonUp) {
                pdi->buttonsDown &= ~(1u << event->button.button);
                if (pdi->buttonsDown == 0 &&
                    pdi->windowlessButtonGrab != NULL) {
                    IswRemoveCallback(pdi->windowlessButtonGrab,
                        IswNdestroyCallback,
                        WindowlessGrabDestroyCallback, NULL);
                    pdi->windowlessButtonGrab = NULL;
                }
            }

            /* When the pointer leaves the windowed ancestor entirely
               (real LeaveNotify from the server), flush the windowless
               pointer state: synthesize Leave for the tracked widget
               and drop any implicit button grab — the pointer is now
               over a different window (e.g. a popup menu). */
            if (etype == IswLeave && pdi->pointerWidget != NULL) {
                Widget old_pw = pdi->pointerWidget;
                pdi->pointerWidget = NULL;
                if (IswIsWidget(old_pw) && !IswIsShell(old_pw)
                    && !old_pw->core.being_destroyed)
                    _IswSynthesizeCrossing(old_pw, event, IswLeave);
                pdi->buttonsDown = 0;
                if (pdi->windowlessButtonGrab != NULL) {
                    IswRemoveCallback(pdi->windowlessButtonGrab,
                        IswNdestroyCallback,
                        WindowlessGrabDestroyCallback, NULL);
                    pdi->windowlessButtonGrab = NULL;
                }
            }

            /* Synthesize Enter/Leave when the windowless widget under the
               pointer changes.  During an implicit grab, use the real
               hit-test result (hitTarget) so the grabbed widget receives
               crossing events as the pointer moves in and out of its
               bounds — the grab redirects the dispatch target but must
               not suppress the widget's knowledge of pointer presence. */
            if (etype == IswMotion) {
                Widget crossTarget =
                    (hitTarget != widget && IswIsWidget(hitTarget)
                     && !IswIsShell(hitTarget)) ? hitTarget : NULL;
                Widget pointerFor = pdi->windowlessButtonGrab != NULL
                    ? pdi->windowlessButtonGrab : crossTarget;
                Boolean inside = (crossTarget == pointerFor);

                if (pdi->windowlessButtonGrab != NULL) {
                    if (pdi->pointerWidget != NULL && !inside) {
                        pdi->pointerWidget = NULL;
                        _IswSynthesizeCrossing(
                            pdi->windowlessButtonGrab, event, IswLeave);
                    } else if (pdi->pointerWidget == NULL && inside) {
                        pdi->pointerWidget = pdi->windowlessButtonGrab;
                        _IswSynthesizeCrossing(
                            pdi->windowlessButtonGrab, event, IswEnter);
                    }
                } else if (crossTarget != pdi->pointerWidget) {
                    Widget old_pw = pdi->pointerWidget;
                    pdi->pointerWidget = crossTarget;
                    if (old_pw != NULL && IswIsWidget(old_pw)
                        && !IswIsShell(old_pw)
                        && !old_pw->core.being_destroyed)
                        _IswSynthesizeCrossing(old_pw, event, IswLeave);
                    if (pdi->pointerWidget != NULL)
                        _IswSynthesizeCrossing(
                            pdi->pointerWidget, event, IswEnter);
                }

                if (pdi->pointerWidget != NULL)
                    _IswSimpleApplyCursor(pdi->pointerWidget);
                else if (pdi->windowlessButtonGrab == NULL) {
                    Widget old_pw_cursor = crossTarget;
                    if (old_pw_cursor == NULL)
                        old_pw_cursor = (Widget)(void *)event->any.target;
                    if (old_pw_cursor != NULL && IswIsWidget(old_pw_cursor)
                        && !old_pw_cursor->core.being_destroyed)
                        _IswSimpleApplyCursor(old_pw_cursor);
                }
            }

            if (target != widget) {
                _IswRebaseEventCoords(event, dx, dy);
                widget = target;
            }
        }
    }

    grabList = *_IswGetGrabList(pdi);

    /* Focus manager: intercept Tab / Shift+Tab before normal key dispatch
     * so traversal works regardless of which widget currently holds the
     * Xt focus descendant (it may not bind Tab). */
    if (widget != NULL) {
        if (event->kind == IswKeyDown || event->kind == IswKeyUp) {
            if (_IswFocusMgrMaybeHandleKey(widget, event)) {
                UNLOCK_APP(app);
                return True;
            }
        }
        else if (event->kind == IswButtonDown) {
            _IswFocusMgrFocusOnClick(widget);
        }
    }

    if (widget == NULL) {
        /* event occurred in a non-widget window -- drop it */
    }
    else if (grabType == pass) {
        if (event->kind == IswLeave ||
            event->kind == IswFocusIn || event->kind == IswFocusOut) {
            if (IswIsSensitive(widget))
                was_dispatched = IswDispatchEventToWidget(widget, event);
        }
        else
            was_dispatched = IswDispatchEventToWidget(widget, event);
    }
    else if (grabType == ignore) {
        if ((grabList == NULL || _IswOnGrabList(widget, grabList))
            && IswIsSensitive(widget)) {
            was_dispatched = DispatchEvent(event, widget);
        }
    }
    else if (grabType == remap) {
        EventMask mask = _IswConvertKindToMask(event->kind);
        Widget dspWidget;

        dspWidget = _IswFindRemapWidget(event, widget, mask, pdi);

        if ((grabList == NULL || _IswOnGrabList(dspWidget, grabList))
            && IswIsSensitive(dspWidget)) {
            was_dispatched = IswDispatchEventToWidget(dspWidget, event);
        }
        else
            _IswUngrabBadGrabs(event, widget, mask, pdi);
    }
    UNLOCK_APP(app);
    return was_dispatched;
}

Boolean
IswDispatchEvent(IswEvent *event, IswDisplay dpy)
{
    Boolean was_dispatched, safe;
    int dispatch_level;
    int starting_count;
    IswPerDisplay pd;
    IswTime time = 0;
    Boolean is_user_input = False;
    IswEventDispatchProc dispatch = _IswDefaultDispatcher;
    IswAppContext app = IswDisplayToApplicationContext(dpy);

    LOCK_APP(app);
    dispatch_level = ++app->dispatch_level;
    starting_count = app->destroy_count;

    /* Coalesce all widget repaints triggered by this dispatch (and any nested
       dispatches) into a single composite+blit per affected window. */
    ISWRenderBeginDeferComposite();

    time = event->any.time;
    is_user_input = (event->kind == IswKeyDown || event->kind == IswButtonDown);
    if (event->kind == IswMappingChanged)
        _IswRefreshMapping(dpy, event, True);
    pd = _IswGetPerDisplay(dpy);

    if (time)
        pd->last_timestamp = time;
    if (is_user_input)
        _IswShellUpdateUserTime(dpy, (Widget) (void *) event->any.target, time);
    pd->last_event = *event;

    /* HiDPI: descale physical event coordinates to logical pixels BEFORE the
       dispatcher is selected, so both the default dispatcher and any custom
       dispatcher (e.g. IswScroll) see logical coordinates.  This runs once per
       real event from the event loop; synthesized events bypass IswDispatchEvent
       and are built from already-logical coordinates. */
    _IswDescaleEventCoords(event, _IswGetScaleFactor(dpy));

    if (pd->dispatcher_list) {
        /* Index by neutral kind; clamp to the 128-entry dispatcher_list. */
        int dispatch_type = (int) event->kind;
        if (dispatch_type >= 0 && dispatch_type < 128) {
            dispatch = pd->dispatcher_list[dispatch_type];
            if (dispatch == NULL)
                dispatch = _IswDefaultDispatcher;
        }
    }
    was_dispatched = (*dispatch) (event, dpy);

    /*
     * To make recursive IswDispatchEvent work, we need to do phase 2 destroys
     * only on those widgets destroyed by this particular dispatch.
     *
     */

    if (app->destroy_count > starting_count)
        _IswDoPhase2Destroy(app, dispatch_level);

    /* Fold the dirty roots once now that this dispatch is done.  Nested
       dispatches decrement the defer depth here but only the outermost flush
       actually composites. */
    ISWRenderFlushComposites();

    app->dispatch_level = dispatch_level - 1;

    if ((safe = _IswSafeToDestroy(app))) {
        if (app->dpy_destroy_count != 0)
            _IswCloseDisplays(app);
        if (app->free_bindings)
            _IswDoFreeBindings(app);
    }
    UNLOCK_APP(app);
    LOCK_PROCESS;
    if (_IswAppDestroyCount != 0 && safe)
        _IswDestroyAppContexts();
    UNLOCK_PROCESS;
    return was_dispatched;
}

static void
GrabDestroyCallback(Widget widget,
                    IswPointer closure _X_UNUSED,
                    IswPointer call_data _X_UNUSED)
{
    /* Remove widget from grab list if it destroyed */
    IswRemoveGrab(widget);
}

static IswGrabRec *
NewGrabRec(Widget widget, Boolean exclusive)
{
    register IswGrabList gl;

    gl = IswNew(IswGrabRec);
    gl->next = NULL;
    gl->widget = widget;
    IswSetBit(gl->exclusive, exclusive);

    return gl;
}

void
IswAddGrab(Widget widget, _IswBoolean exclusive)
{
    register IswGrabList gl;
    IswGrabList *grabListPtr;
    IswAppContext app = IswWidgetToApplicationContext(widget);
    (void)app;

    LOCK_APP(app);
    LOCK_PROCESS;
    grabListPtr = _IswGetGrabList(_IswGetPerDisplayInput(IswDisplayOf(widget)));

    gl = NewGrabRec(widget, (Boolean) exclusive);
    gl->next = *grabListPtr;
    *grabListPtr = gl;

    IswAddCallback(widget, IswNdestroyCallback,
                  GrabDestroyCallback, (IswPointer) NULL);
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

void
IswRemoveGrab(Widget widget)
{
    register IswGrabList gl;
    register Boolean done;
    IswGrabList *grabListPtr;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    LOCK_PROCESS;

    grabListPtr = _IswGetGrabList(_IswGetPerDisplayInput(IswDisplayOf(widget)));

    for (gl = *grabListPtr; gl != NULL; gl = gl->next) {
        if (gl->widget == widget)
            break;
    }

    if (gl == NULL) {
        IswAppWarningMsg(app,
                        "grabError", "xtRemoveGrab", IswCIswToolkitError,
                        "IswRemoveGrab asked to remove a widget not on the list",
                        NULL, NULL);
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return;
    }

    do {
        gl = *grabListPtr;
        done = (gl->widget == widget);
        *grabListPtr = gl->next;
        IswRemoveCallback(gl->widget, IswNdestroyCallback,
                         GrabDestroyCallback, (IswPointer) NULL);
        IswFree((char *) gl);
    } while (!done);
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return;
}

void
IswAppMainLoop(IswAppContext app)
{
    IswInputMask m = IswIMAll;
    IswInputMask t;

    LOCK_APP(app);
    for (;;) {
        if (app->exit_flag)
            break;
        if (m == 0) {
            m = IswIMAll;
            IswAppProcessEvent(app, m);
        }
        else if (((t = IswAppPending(app)) & m)) {
            IswAppProcessEvent(app, t & m);
        }
        m >>= 1;
    }
    UNLOCK_APP(app);
}

void
_IswFreeEventTable(IswEventTable *event_table)
{
    register IswEventTable event;

    event = *event_table;
    while (event != NULL) {
        register IswEventTable next = event->next;

        IswFree((char *) event);
        event = next;
    }
}

IswTime
IswLastTimestampProcessed(IswDisplay dpy)
{
    IswTime time;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    LOCK_PROCESS;
    time = _IswGetPerDisplay(dpy)->last_timestamp;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return (time);
}

IswEvent *
IswLastEventProcessed(IswDisplay dpy)
{
    IswEvent *le = NULL;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    le = &_IswGetPerDisplay(dpy)->last_event;
    if (le->kind == IswNoEvent)
        le = NULL;
    UNLOCK_APP(app);
    return le;
}

void
_IswSendFocusEvent(Widget child, IswEventKind kind)
{
    child = IswIsWidget(child) ? child : _IswWidgetAncestor(child);
    if (IswIsSensitive(child) && !child->core.being_destroyed
        && IswIsRealized(child)
        && (IswBuildEventMask(child) & IswFocusChangeMask)) {

        if (kind == IswFocusIn || kind == IswFocusOut) {
            IswEvent event;
            memset(&event, 0, sizeof(event));
            event.kind = kind;
            event.focus.mode = IswNotifyNormal;
            event.any.target = (IswEventTarget) (void *) child;
            IswDispatchEventToWidget(child, &event);
        } else {
            return;
        }
    }
}

static IswEventDispatchProc *
NewDispatcherList(void)
{
    IswEventDispatchProc *l = (IswEventDispatchProc *)
        __IswCalloc((Cardinal) 128,
                   (Cardinal)
                   sizeof(IswEventDispatchProc));

    return l;
}

IswEventDispatchProc
IswSetEventDispatcher(IswDisplay dpy,
                     int event_type,
                     IswEventDispatchProc proc)
{
    IswEventDispatchProc *list;
    IswEventDispatchProc old_proc;
    register IswPerDisplay pd;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    LOCK_PROCESS;
    pd = _IswGetPerDisplay(dpy);

    list = pd->dispatcher_list;
    if (!list) {
        if (proc)
            list = pd->dispatcher_list = NewDispatcherList();
        else
            return _IswDefaultDispatcher;
    }
    old_proc = list[event_type];
    list[event_type] = proc;
    if (old_proc == NULL)
        old_proc = _IswDefaultDispatcher;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return old_proc;
}

void
IswRegisterExtensionSelector(IswDisplay dpy,
                            int min_event_type,
                            int max_event_type,
                            IswExtensionSelectProc proc,
                            IswPointer client_data)
{
    IswPerDisplay pd;
    int i;

    DPY_TO_APPCON(dpy);

    if (dpy == NULL)
        IswErrorMsg("nullDisplay",
                   "xtRegisterExtensionSelector", IswCIswToolkitError,
                   "IswRegisterExtensionSelector requires a non-NULL display",
                   NULL, NULL);

    LOCK_APP(app);
    LOCK_PROCESS;
    pd = _IswGetPerDisplay(dpy);

    for (i = 0; i < pd->ext_select_count; i++) {
        ExtSelectRec *e = &pd->ext_select_list[i];

        if (e->min == min_event_type && e->max == max_event_type) {
            e->proc = proc;
            e->client_data = client_data;
            return;
        }
        if ((min_event_type >= e->min && min_event_type <= e->max) ||
            (max_event_type >= e->min && max_event_type <= e->max)) {
            IswErrorMsg("rangeError", "xtRegisterExtensionSelector",
                       IswCIswToolkitError,
                       "Attempt to register multiple selectors for one extension event type",
                       NULL, NULL);
            UNLOCK_PROCESS;
            UNLOCK_APP(app);
            return;
        }
    }
    pd->ext_select_count++;
    pd->ext_select_list = IswReallocArray(pd->ext_select_list,
                                         (Cardinal) pd->ext_select_count,
                                         (Cardinal) sizeof(ExtSelectRec));
    for (i = pd->ext_select_count - 1; i > 0; i--) {
        if (pd->ext_select_list[i - 1].min > min_event_type) {
            pd->ext_select_list[i] = pd->ext_select_list[i - 1];
        }
        else
            break;
    }
    pd->ext_select_list[i].min = min_event_type;
    pd->ext_select_list[i].max = max_event_type;
    pd->ext_select_list[i].proc = proc;
    pd->ext_select_list[i].client_data = client_data;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

void
_IswExtensionSelect(Widget widget)
{
    int i;
    IswPerDisplay pd;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;

    pd = _IswGetPerDisplay(IswDisplayOf(widget));

    for (i = 0; i < pd->ext_select_count; i++) {
        CallExtensionSelector(widget, pd->ext_select_list + i, FALSE);
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}
