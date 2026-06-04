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

#include <xcb/xinput.h>
#include <xcb/xproto.h>
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
 * GRAPHICS_EXPOSE  - TRUE if the widget wants graphics expose events
 *                    dispatched.
 * NO_EXPOSE        - TRUE if the widget wants No expose events dispatched.
 */

#define COMP_EXPOSE   (widget->core.widget_class->core_class.compress_exposure)
#define COMP_EXPOSE_TYPE (COMP_EXPOSE & 0x0f)
#define GRAPHICS_EXPOSE  ((IswExposeGraphicsExpose & COMP_EXPOSE) || \
                          (IswExposeGraphicsExposeMerged & COMP_EXPOSE))
#define NO_EXPOSE        (IswExposeNoExpose & COMP_EXPOSE)

/* HiDPI: convert physical pixel event coordinates to logical pixels */
static void
_IswDescaleEventCoords(xcb_generic_event_t *event, double sf)
{
    if (sf <= 1.0)
        return;
    float inv = 1.0f / (float)sf;

    switch (event->response_type & ~0x80) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
        xcb_key_press_event_t *e = (xcb_key_press_event_t *)event;
        e->event_x = (int16_t)(e->event_x * inv);
        e->event_y = (int16_t)(e->event_y * inv);
        e->root_x = (int16_t)(e->root_x * inv);
        e->root_y = (int16_t)(e->root_y * inv);
        break;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *e = (xcb_button_press_event_t *)event;
        e->event_x = (int16_t)(e->event_x * inv);
        e->event_y = (int16_t)(e->event_y * inv);
        e->root_x = (int16_t)(e->root_x * inv);
        e->root_y = (int16_t)(e->root_y * inv);
        break;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)event;
        e->event_x = (int16_t)(e->event_x * inv);
        e->event_y = (int16_t)(e->event_y * inv);
        e->root_x = (int16_t)(e->root_x * inv);
        e->root_y = (int16_t)(e->root_y * inv);
        break;
    }
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY: {
        xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)event;
        e->event_x = (int16_t)(e->event_x * inv);
        e->event_y = (int16_t)(e->event_y * inv);
        e->root_x = (int16_t)(e->root_x * inv);
        e->root_y = (int16_t)(e->root_y * inv);
        break;
    }
    case XCB_EXPOSE: {
        xcb_expose_event_t *e = (xcb_expose_event_t *)event;
        e->x = (uint16_t)(e->x * inv);
        e->y = (uint16_t)(e->y * inv);
        e->width = (uint16_t)(e->width * inv + 0.5f);
        e->height = (uint16_t)(e->height * inv + 0.5f);
        break;
    }
    case XCB_GRAPHICS_EXPOSURE: {
        xcb_graphics_exposure_event_t *e = (xcb_graphics_exposure_event_t *)event;
        e->x = (uint16_t)(e->x * inv);
        e->y = (uint16_t)(e->y * inv);
        e->width = (uint16_t)(e->width * inv + 0.5f);
        e->height = (uint16_t)(e->height * inv + 0.5f);
        break;
    }
    case XCB_CONFIGURE_NOTIFY: {
        xcb_configure_notify_event_t *e = (xcb_configure_notify_event_t *)event;
        e->x = (int16_t)(e->x * inv);
        e->y = (int16_t)(e->y * inv);
        e->width = (uint16_t)(e->width * inv + 0.5f);
        e->height = (uint16_t)(e->height * inv + 0.5f);
        e->border_width = (uint16_t)(e->border_width * inv + 0.5f);
        break;
    }
    }
}

xcb_window_t get_event_window(xcb_generic_event_t *event) {
    uint32_t window_id = 0;

    switch (event->response_type & ~0x80) {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE: {
            xcb_key_press_event_t *key_event = (xcb_key_press_event_t *)event;
            window_id = key_event->event;
            break;
        }
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE: {
            xcb_button_press_event_t *button_event = (xcb_button_press_event_t*)event;
            window_id = button_event->event;
            break;
        }
        case XCB_ENTER_NOTIFY:
        case XCB_LEAVE_NOTIFY: {
            xcb_enter_notify_event_t *enter_event = (xcb_enter_notify_event_t*)event;
            window_id = enter_event->event;
            break;
        }
        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t *motion_event = (xcb_motion_notify_event_t*)event;
            window_id = motion_event->event;
            break;
        }
        case XCB_KEYMAP_NOTIFY:
            /* No window field in this event */
            break;
        case XCB_EXPOSE: {
            xcb_expose_event_t *expose_event = (xcb_expose_event_t*)event;
            window_id = expose_event->window;
            break;
        }
        case XCB_GRAPHICS_EXPOSURE: {
            xcb_graphics_exposure_event_t *g_expose_event = (xcb_graphics_exposure_event_t*)event;
            window_id = g_expose_event->drawable;
            break;
        }
        case XCB_NO_EXPOSURE: {
            xcb_no_exposure_event_t *no_expose_event = (xcb_no_exposure_event_t*)event;
            window_id = no_expose_event->drawable;
            break;
        }
        case XCB_VISIBILITY_NOTIFY: {
            xcb_visibility_notify_event_t *vis_event = (xcb_visibility_notify_event_t*)event;
            window_id = vis_event->window;
            break;
        }
        case XCB_CREATE_NOTIFY: {
            xcb_create_notify_event_t *create_event = (xcb_create_notify_event_t*)event;
            window_id = create_event->window;
            break;
        }
        case XCB_DESTROY_NOTIFY: {
            xcb_destroy_notify_event_t *dn_event = (xcb_destroy_notify_event_t*)event;
            window_id = dn_event->window;
            break;
        }
        case XCB_UNMAP_NOTIFY: {
            xcb_unmap_notify_event_t *un_event = (xcb_unmap_notify_event_t*)event;
            window_id = un_event->window;
            break;
        }
        case XCB_MAP_NOTIFY: {
            xcb_map_notify_event_t *mn_event = (xcb_map_notify_event_t*)event;
            window_id = mn_event->window;
            break;
        }
        case XCB_MAP_REQUEST: {
            xcb_map_request_event_t *mr_event = (xcb_map_request_event_t*)event;
            window_id = mr_event->window;
            break;
        }
        case XCB_REPARENT_NOTIFY: {
            xcb_reparent_notify_event_t *rn_event = (xcb_reparent_notify_event_t*)event;
            window_id = rn_event->window;
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t *cn_event = (xcb_configure_notify_event_t*)event;
            window_id = cn_event->window;
            break;
        }
        case XCB_CONFIGURE_REQUEST: {
            xcb_configure_request_event_t *cr_event = (xcb_configure_request_event_t*)event;
            window_id = cr_event->window;
            break;
        }
        case XCB_GRAVITY_NOTIFY: {
            xcb_gravity_notify_event_t *gn_event = (xcb_gravity_notify_event_t*)event;
            window_id = gn_event->window;
            break;
        }
        case XCB_RESIZE_REQUEST: {
            xcb_resize_request_event_t *rr_event = (xcb_resize_request_event_t*)event;
            window_id = rr_event->window;
            break;
        }
        case XCB_CIRCULATE_NOTIFY: {
            xcb_circulate_notify_event_t *circ_event = (xcb_circulate_notify_event_t*)event;
            window_id = circ_event->window;
            break;
        }
        case XCB_CIRCULATE_REQUEST: {
            xcb_circulate_request_event_t *circr_event = (xcb_circulate_request_event_t*)event;
            window_id = circr_event->window;
            break;
        }
        case XCB_PROPERTY_NOTIFY: {
            xcb_property_notify_event_t *prop_event = (xcb_property_notify_event_t*)event;
            window_id = prop_event->window;
            break;
        }
        case XCB_SELECTION_CLEAR: {
            xcb_selection_clear_event_t *sc_event = (xcb_selection_clear_event_t*)event;
            window_id = sc_event->owner;
            break;
        }
        case XCB_SELECTION_REQUEST: {
            xcb_selection_request_event_t *sr_event = (xcb_selection_request_event_t*)event;
            window_id = sr_event->owner;
            break;
        }
        case XCB_SELECTION_NOTIFY: {
            xcb_selection_notify_event_t *sn_event = (xcb_selection_notify_event_t*)event;
            window_id = sn_event->requestor;
            break;
        }
        case XCB_COLORMAP_NOTIFY: {
            xcb_colormap_notify_event_t *cm_event = (xcb_colormap_notify_event_t*)event;
            window_id = cm_event->window;
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t *client_event = (xcb_client_message_event_t*)event;
            window_id = client_event->window;
            break;
        }
        case XCB_MAPPING_NOTIFY:
            /* MappingNotify doesn't have a window field */
            break;
        case XCB_FOCUS_IN:
        case XCB_FOCUS_OUT: {
            xcb_focus_in_event_t *focus_event = (xcb_focus_in_event_t*)event;
            window_id = focus_event->event;
            break;
        }
        default:
            /* Unknown event type - return 0 */
            break;
    }

    return window_id;
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
            mask |= XCB_EVENT_MASK_EXPOSURE;
        if (widget->core.widget_class->core_class.visible_interest)
            mask |= XCB_EVENT_MASK_VISIBILITY_CHANGE;
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

        if (!IswIsWidget(child) || !child->core.windowless)
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
static void
_IswUpdateWindowlessAncestorMask(Widget windowless)
{
    Widget anc;
    EventMask mask;

    if (!IswIsWidget(windowless))
        return;
    anc = _IswWindowedAncestor(windowless);
    if (anc == NULL || !IswIsRealized(anc) || anc->core.being_destroyed)
        return;
    mask = _IswWindowSelectMask(anc);
    xcb_change_window_attributes(IswDisplay(anc), anc->core.window,
                                 XCB_CW_EVENT_MASK, &mask);
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
        xcb_connection_t *dpy = IswDisplay(widget);

        if (widget->core.windowless) {
            /* No own window — fold this widget's mask into the windowed
               ancestor's selection. */
            _IswUpdateWindowlessAncestorMask(widget);
        }
        else if (oldMask != mask) {
            EventMask sel = _IswWindowSelectMask(widget);
            xcb_change_window_attributes(dpy, IswWindow(widget),
                                         XCB_CW_EVENT_MASK, &sel);
        }

        if (has_type_specifier) {
            IswPerDisplay pd = _IswGetPerDisplay(dpy);
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
            p = (IswEventRec *) __XtMalloc(sizeof(IswEventRec) +
                                          sizeof(IswEventRecExt));
            EXT_TYPE(p) = type;
            EXT_SELECT_DATA(p, 0) = select_data;
            p->mask = 1;
            p->has_type_specifier = True;
        }
        else {
            p = (IswEventRec *) __XtMalloc(sizeof(IswEventRec));
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
        xcb_connection_t *dpy = IswDisplay(widget);

        if (widget->core.windowless) {
            _IswUpdateWindowlessAncestorMask(widget);
        }
        else if (oldMask != mask) {
            EventMask sel = _IswWindowSelectMask(widget);
            xcb_change_window_attributes(dpy, IswWindow(widget),
                                         XCB_CW_EVENT_MASK, &sel);
        }

        if (has_type_specifier) {
            IswPerDisplay pd = _IswGetPerDisplay(dpy);
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
    WWPair pairs;               /* bogus entries */
} *WWTable;

static const WidgetRec WWfake;  /* placeholder for deletions */

#define WWHASH(tab,win) ((win) & tab->mask)
#define WWREHASHVAL(tab,win) ((((win) % tab->rehash) + 2) | 1)
#define WWREHASH(tab,idx,rehash) ((unsigned)(idx + rehash) & (tab->mask))
#define WWTABLE(display) (_IswGetPerDisplay(display)->WWtable)

static void ExpandWWTable(WWTable);

void
IswRegisterDrawable(xcb_connection_t *display, xcb_drawable_t drawable, Widget widget)
{
    WWTable tab;
    int idx;
    Widget entry;
    xcb_window_t window = (xcb_window_t) drawable;


    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);

    if (window != IswWindow(widget)) {
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
IswUnregisterDrawable(xcb_connection_t *display, xcb_drawable_t drawable)
{
    WWTable tab;
    int idx;
    Widget entry;
    xcb_window_t window = (xcb_window_t) drawable;
    Widget widget = IswWindowToWidget(display, window);
    DPY_TO_APPCON(display);

    if (widget == NULL)
        return;

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);
    if (window != IswWindow(widget)) {
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
        (Widget *) __XtCalloc(tab->mask + 1, sizeof(Widget));
    for (oldidx = 0; oldidx <= oldmask; oldidx++) {
        if ((entry = oldentries[oldidx]) && entry != &WWfake) {
            newidx = (Cardinal) WWHASH(tab, IswWindow(entry));
            if (entries[newidx]) {
                rehash = (Cardinal) WWREHASHVAL(tab, IswWindow(entry));
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
IswWindowToWidget(register xcb_connection_t *display, register xcb_window_t window)
{
    WWTable tab;
    int idx;
    Widget entry;
    WWPair pair;
    DPY_TO_APPCON(display);

    if (!window)
        return NULL;

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);
    idx = (int) WWHASH(tab, window);
    /* Compare the raw window field, not IswWindow(): table entries are
       always windowed widgets, and the WWfake deletion sentinel is a zeroed
       WidgetRec that must not be dereferenced as a widget (IswWindow() would
       read its NULL widget_class). */
    if ((entry = tab->entries[idx]) && entry->core.window != window) {
        int rehash = (int) WWREHASHVAL(tab, window);

        do {
            idx = (int) WWREHASH(tab, idx, rehash);
        } while ((entry = tab->entries[idx]) && entry->core.window != window);
    }
    if (entry) {
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return entry;
    }
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

/* Accumulate a widget's origin relative to its nearest windowed ancestor by
   walking up through windowless parents. */
static void
_IswWindowlessOffset(Widget w, int *dx, int *dy)
{
    int ox = 0, oy = 0;

    while (w != NULL && IswIsWidget(w) && w->core.windowless) {
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
 * offsets/clips itself (see ISWRenderBegin / the Cairo-XCB backend).
 *
 * Recurses into windowless composites.  Stops at windowed children: they own
 * their own window and receive their own Expose from the server. */
static void _IswExposeWindowlessChildren(Widget w, xcb_generic_event_t *event);

/* Paint one windowless child (and recurse into its windowless descendants). */
static void
_IswPaintWindowlessChild(Widget child, xcb_generic_event_t *event)
{
    if (!IswIsWidget(child) || !child->core.windowless)
        return;
    /* Paint realized windowless children that are mapped.  windowless_mapped is
       the live "the window is mapped" equivalent (driven by map/unmap/manage);
       an unmapped child (non-current Tabs page, last Paned grip) must not
       paint. */
    if (!IswIsRealized(child) && !child->core.windowless_realized)
        return;
    if (!child->core.windowless_mapped)
        return;

    if (child->core.widget_class->core_class.expose != NULL)
        (*child->core.widget_class->core_class.expose)(child, event, 0);

    _IswExposeWindowlessChildren(child, event);
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
    if (!IswIsWidget(w) || !w->core.windowless)
        return;
    if (!IswIsRealized(w) && !w->core.windowless_realized)
        return;

    ISWRenderBeginCompositeBatch();
    if (w->core.widget_class->core_class.expose != NULL)
        (*w->core.widget_class->core_class.expose)(w, NULL, 0);
    _IswExposeWindowlessChildren(w, NULL);
    ISWRenderEndCompositeBatch();

    anc = _IswWindowedAncestor(w);
    if (anc != NULL && IswIsRealized(anc))
        ISWRenderRequestComposite(anc);
}

static void
_IswExposeWindowlessChildren(Widget w, xcb_generic_event_t *event)
{
    /* Composite children held in composite.children. */
    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++)
            _IswPaintWindowlessChild(cw->composite.children[i], event);
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
                _IswPaintWindowlessChild(child, event);
        }
    }
}

/* Dispatch a synthesized Enter/Leave crossing event to a windowless widget,
   derived from the triggering motion event (root coords preserved, window
   coords rebased to the target's windowed-ancestor frame). */
static void
_IswSynthesizeCrossing(Widget w, xcb_generic_event_t *motion, uint8_t type)
{
    xcb_motion_notify_event_t *m = (xcb_motion_notify_event_t *) motion;
    xcb_enter_notify_event_t ev = {0};
    int dx, dy;

    if (!IswIsSensitive(w) || !IswIsRealized(w))
        return;
    if (!(IswBuildEventMask(w) &
          (type == XCB_ENTER_NOTIFY ? XCB_EVENT_MASK_ENTER_WINDOW
                                    : XCB_EVENT_MASK_LEAVE_WINDOW)))
        return;

    _IswWindowlessOffset(w, &dx, &dy);

    ev.response_type = type;
    ev.time = m->time;
    ev.root = m->root;
    ev.event = IswWindow(w);
    ev.child = m->child;
    ev.root_x = m->root_x;
    ev.root_y = m->root_y;
    ev.event_x = (int16_t) (m->event_x - dx);
    ev.event_y = (int16_t) (m->event_y - dy);
    ev.state = m->state;
    ev.mode = XCB_NOTIFY_MODE_NORMAL;
    ev.detail = XCB_NOTIFY_DETAIL_NONLINEAR;
    ev.same_screen_focus = 1;

    IswDispatchEventToWidget(w, (xcb_generic_event_t *) &ev);
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
Widget
_IswFindWidgetAtPoint(Widget root, int x, int y, int *dx, int *dy)
{
    Widget target = root;
    int ox = 0, oy = 0;

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
                if (!IswIsWidget(child) || !child->core.windowless)
                    continue;
                /* Same "shown" rule as paint/clip: windowless_mapped is the
                   live mapped flag; an unmapped child is not hit-tested. */
                if (!child->core.windowless_mapped)
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

/* Rebase a pointer/key/crossing event's window-local coordinates by
   (-dx, -dy) so they are relative to a windowless target widget. */
static void
_IswRebaseEventCoords(xcb_generic_event_t *event, int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;

    switch (event->response_type & ~0x80) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *e = (xcb_button_press_event_t *) event;
        e->event_x = (int16_t) (e->event_x - dx);
        e->event_y = (int16_t) (e->event_y - dy);
        break;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *) event;
        e->event_x = (int16_t) (e->event_x - dx);
        e->event_y = (int16_t) (e->event_y - dy);
        break;
    }
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY: {
        xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) event;
        e->event_x = (int16_t) (e->event_x - dx);
        e->event_y = (int16_t) (e->event_y - dy);
        break;
    }
    }
}

/* Extract window-local pointer coordinates from a pointer event.
   Returns False for events without pointer coordinates. */
static Boolean
_IswEventPointerXY(xcb_generic_event_t *event, int *x, int *y)
{
    /* Keyboard events are intentionally excluded: they route to the focus
       widget, not by pointer position. */
    switch (event->response_type & ~0x80) {
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *e = (xcb_button_press_event_t *) event;
        *x = e->event_x;
        *y = e->event_y;
        return True;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *) event;
        *x = e->event_x;
        *y = e->event_y;
        return True;
    }
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY: {
        xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) event;
        *x = e->event_x;
        *y = e->event_y;
        return True;
    }
    }
    return False;
}

void
_IswAllocWWTable(IswPerDisplay pd)
{
    register WWTable tab;

    tab = (WWTable) __XtMalloc(sizeof(struct _WWTable));
    tab->mask = 0x7f;
    tab->rehash = tab->mask - 2;
    tab->entries = (Widget *) __XtCalloc(tab->mask + 1, sizeof(Widget));
    tab->occupied = 0;
    tab->fakes = 0;
    tab->pairs = NULL;
    pd->WWtable = tab;
}

void
_IswFreeWWTable(register IswPerDisplay pd)
{
    register WWPair pair, next;

    for (pair = pd->WWtable->pairs; pair; pair = next) {
        next = pair->next;
        IswFree((char *) pair);
    }
    IswFree((char *) pd->WWtable->entries);
    IswFree((char *) pd->WWtable);
}

#define EHMAXSIZE 25            /* do not make whopping big */

static Boolean
CallEventHandlers(Widget widget,xcb_generic_event_t *event, EventMask mask)
{
    register IswEventRec *p;
    IswEventHandler *proc;
    IswPointer *closure;
    Boolean cont_to_disp = True;
    int i, numprocs;

    /* Have to copy the procs into an array, because one of them might
     * call IswRemoveEventHandler, which would break our linked list. */

    numprocs = 0;
    for (p = widget->core.event_table; p; p = p->next) {
        if ((!p->has_type_specifier && (mask & p->mask)) ||
            (p->has_type_specifier && event->response_type == EXT_TYPE(p)))
            numprocs++;
    }
    proc = IswMallocArray((Cardinal) numprocs, (Cardinal)
                         (sizeof(IswEventHandler) + sizeof(IswPointer)));
    closure = (IswPointer *) (proc + numprocs);

    numprocs = 0;
    for (p = widget->core.event_table; p; p = p->next) {
        if ((!p->has_type_specifier && (mask & p->mask)) ||
            (p->has_type_specifier && event->response_type == EXT_TYPE(p))) {
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
        (*(proc[i])) (widget, closure[i], event, &cont_to_disp);
    IswFree((char *) proc);
    return cont_to_disp;
}

#define KnownButtons (XCB_EVENT_MASK_BUTTON_1_MOTION|XCB_EVENT_MASK_BUTTON_2_MOTION|XCB_EVENT_MASK_BUTTON_3_MOTION|\
                      XCB_EVENT_MASK_BUTTON_4_MOTION|XCB_EVENT_MASK_BUTTON_5_MOTION)

/* keep this SMALL to avoid blowing stack cache! */
/* because some compilers allocate all local locals on procedure entry */
#define EHSIZE 4

Boolean
IswDispatchEventToWidget(Widget widget, xcb_generic_event_t *event)
{
    register IswEventRec *p;
    Boolean was_dispatched = False;
    Boolean call_tm = False;
    Boolean cont_to_disp;
    xcb_event_mask_t mask;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);

    mask = _IswConvertTypeToMask(event->response_type);
    if (event->response_type == XCB_INPUT_DEVICE_MOTION_NOTIFY)
        mask |= (((xcb_input_device_motion_notify_event_t *)event)->state & KnownButtons);

    LOCK_PROCESS;
    if ((mask == XCB_EVENT_MASK_EXPOSURE)) {

        if (widget->core.widget_class->core_class.expose != NULL) {
            uint8_t event_type = event->response_type & ~0x80;

            /* For Expose events, check if this is the last in the series */
            if (event_type == XCB_EXPOSE) {
                xcb_expose_event_t *ev = (xcb_expose_event_t *)event;
                /* Only dispatch when count == 0 (last event in series) or
                 * when compression is disabled */
                if (ev->count == 0 || COMP_EXPOSE_TYPE == IswExposeNoCompress) {
                    (*widget->core.widget_class->core_class.expose)
                        (widget, event, 0);
                    /* Repaint windowless descendants into their own surfaces,
                       then composite the subtree onto this window once. */
                    ISWRenderBeginCompositeBatch();
                    _IswExposeWindowlessChildren(widget, event);
                    ISWRenderEndCompositeBatch();
                    ISWRenderRequestComposite(widget);
                    was_dispatched = True;
                }
            }
            /* GraphicsExpose / NoExpose: forwarded to the expose proc when
             * the class opts in via compress_exposure flags. The Text
             * widget relies on these to drain its copy_area_offsets
             * queue after xcb_copy_area scrolls — without the dispatch,
             * the queue grows unbounded and TranslateExposeRegion
             * mis-maps every subsequent real Expose rectangle. */
            else if (event_type == XCB_GRAPHICS_EXPOSURE && GRAPHICS_EXPOSE) {
                (*widget->core.widget_class->core_class.expose)
                    (widget, event, 0);
                was_dispatched = True;
            }
            else if (event_type == XCB_NO_EXPOSURE && NO_EXPOSE) {
                (*widget->core.widget_class->core_class.expose)
                    (widget, event, 0);
                was_dispatched = True;
            }
        }
        else if ((event->response_type & ~0x80) == XCB_EXPOSE) {
            /* Windowed widget has no expose proc of its own (e.g. a bare
               shell or composite), but may host windowless children that
               still need to repaint. */
            xcb_expose_event_t *ev = (xcb_expose_event_t *)event;
            if (ev->count == 0 || COMP_EXPOSE_TYPE == IswExposeNoCompress) {
                ISWRenderBeginCompositeBatch();
                _IswExposeWindowlessChildren(widget, event);
                ISWRenderEndCompositeBatch();
                ISWRenderRequestComposite(widget);
                was_dispatched = True;
            }
        }
    }

    if ((mask == XCB_EVENT_MASK_VISIBILITY_CHANGE) &&
        IswClass(widget)->core_class.visible_interest) {
        was_dispatched = True;
        /* our visibility just changed... */
        switch (((xcb_visibility_notify_event_t *) event)->state) {
        case XCB_VISIBILITY_UNOBSCURED:
            widget->core.visible = TRUE;
            break;

        case XCB_VISIBILITY_PARTIALLY_OBSCURED:
            /* what do we want to say here? */
            /* well... some of us is visible */
            widget->core.visible = TRUE;
            break;

        case XCB_VISIBILITY_FULLY_OBSCURED:
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
                    (p->has_type_specifier && event->response_type == EXT_TYPE(p))) {
                    if (numprocs >= EHSIZE)
                        break;
                    proc[numprocs] = p->proc;
                    closure[numprocs] = p->closure;
                    numprocs++;
                }
            }
            if (numprocs) {
                if (p) {
                    cont_to_disp = CallEventHandlers(widget, event, mask);
                }
                else {
                    int i;

                    for (i = 0; i < numprocs && cont_to_disp; i++)
                        (*(proc[i])) (widget, closure[i], event, &cont_to_disp);
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
                 (p->has_type_specifier && event->response_type == EXT_TYPE(p))) {
            (*p->proc) (widget, p->closure, event, &cont_to_disp);
            was_dispatched = True;
        }
    }
    if (call_tm && cont_to_disp)
        _IswTranslateEvent(widget, event);
    UNLOCK_APP(app);
    return (was_dispatched | call_tm);
}

/*
 * This structure is passed into the check exposure proc.
 */

typedef struct _CheckExposeInfo {
    int type1, type2;           /* Types of events to check for. */
    Boolean maximal;            /* Ignore non-exposure events? */
    Boolean non_matching;       /* Was there an event that did not
                                   match either type? */
    xcb_window_t window;              /* xcb_window_t to match. */
} CheckExposeInfo;

#define GetCount(ev) (((XExposeEvent *)(ev))->count)

/*      Function Name: CompressExposures
 *      Description: Handles all exposure compression
 *      Arguments: event - the xevent that is to be dispatched
 *                 widget - the widget that this event occurred in.
 *      Returns: none.
 *
 *      NOTE: Event must be of type Expose or GraphicsExpose.
 *      PORTING NOTE: this seems to depend on blocking behaviour of Xlib somewhat, and
 *                    the equivalents seem to be better placed in the man XCB loop as events come in
 *                    #FIXME, analyze and implement event compression in event handling loop
 */

//static void
//CompressExposures(xcb_connection_t *dpy, xcb_generic_event_t *event, Widget widget)
//{
//    CheckExposeInfo info;
//    int count;
//    xcb_connection_t *dpy = IswDisplay(widget);
//    IswPerDisplay pd = _IswGetPerDisplay(dpy);
//    IswEnum comp_expose;
//    IswEnum comp_expose_type;
//    Boolean no_region;
//
//    LOCK_PROCESS;
//    comp_expose = COMP_EXPOSE;
//    UNLOCK_PROCESS;
//    comp_expose_type = comp_expose & 0x0f;
//    no_region = ((comp_expose & IswExposeNoRegion) ? True : False);
//
//    if (no_region)
//        AddExposureToRectangularRegion(event, pd->region);
//    else
//        IswAddExposureToRegion(event, pd->region);
//
//    if (GetCount(event) != 0)
//        return;
//
//    if ((comp_expose_type == IswExposeCompressSeries) ||
//        (XEventsQueued(dpy, QueuedAfterReading) == 0)) {
//        SendExposureEvent(event, widget, pd);
//        return;
//    }
//
//    if (comp_expose & IswExposeGraphicsExposeMerged) {
//        info.type1 = Expose;
//        info.type2 = GraphicsExpose;
//    }
//    else {
//        info.type1 = event->response_type;
//        info.type2 = 0;
//    }
//    info.maximal = (comp_expose_type == IswExposeCompressMaximal);
//    info.non_matching = FALSE;
//    info.window = IswWindow(widget);
//
//    /*
//     * We have to be very careful here not to hose down the processor
//     * when blocking until count gets to zero.
//     *
//     * First, check to see if there are any events in the queue for this
//     * widget, and of the correct type.
//     *
//     * Once we cannot find any more events, check to see that count is zero.
//     * If it is not then block until we get another exposure event.
//     *
//     * If we find no more events, and count on the last one we saw was zero we
//     * we can be sure that all events have been processed.
//     *
//     * Unfortunately, we wind up having to look at the entire queue
//     * event if we're not doing Maximal compression, due to the
//     * semantics of XCheckIfEvent (we can't abort without re-ordering
//     * the event queue as a side-effect).
//     */
//
//    count = 0;
//    while (TRUE) {
//        xcb_generic_event_t event_return;
//
//        if (XCheckIfEvent(dpy, &event_return,
//                          CheckExposureEvent, (char *) &info)) {
//
//            count = GetCount(&event_return);
//            if (no_region)
//                AddExposureToRectangularRegion(&event_return, pd->region);
//            else
//                IswAddExposureToRegion(&event_return, pd->region);
//        }
//        else if (count != 0) {
//            XIfEvent(dpy, &event_return, CheckExposureEvent, (char *) &info);
//            count = GetCount(&event_return);
//            if (no_region)
//                AddExposureToRectangularRegion(&event_return, pd->region);
//            else
//                IswAddExposureToRegion(&event_return, pd->region);
//        }
//        else                    /* count == 0 && XCheckIfEvent Failed. */
//            break;
//    }
//
//    SendExposureEvent(dpy, event, widget, pd);
//}

void
IswAddExposureToRegion(xcb_connection_t *dpy, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    xcb_rectangle_t rect;
    xcb_expose_event_t *ev = (xcb_expose_event_t *)event;
    xcb_generic_error_t *error = NULL;

    /* These Expose and GraphicsExpose fields are at identical offsets */

    if (event->response_type == XCB_EXPOSE || event->response_type == XCB_GRAPHICS_EXPOSURE) {
        rect.x = (Position) ev->x;
        rect.y = (Position) ev->y;
        rect.width = (Dimension) ev->width;
        rect.height = (Dimension) ev->height;
        
        xcb_xfixes_region_t new_region = xcb_generate_id(dpy);
        (void)xcb_xfixes_create_region(dpy, new_region, 1, &rect);
        if (error) {
            fprintf(stderr, "Error creating new region: %d\n", error->error_code);
            free(error);
            return;
        }

        (void)xcb_xfixes_union_region(dpy, region, new_region, region);
        if (error) {
            fprintf(stderr, "Error in union_region: %d\n", error->error_code);
            free(error);
            return;
        }
    }
}

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif


void get_region_bounding_box(xcb_connection_t *dpy, xcb_xfixes_region_t region, 
                           xcb_rectangle_t * rect) {
    // Fetch the region data
    xcb_xfixes_fetch_region_cookie_t fetch_cookie = xcb_xfixes_fetch_region(dpy, region);
    xcb_generic_error_t *error;// = xcb_request_check(connection, fetch_cookie);
    xcb_xfixes_fetch_region_reply_t *reply = xcb_xfixes_fetch_region_reply (dpy, fetch_cookie, &error);
    
    if (error) {
        fprintf(stderr, "Error fetching region: %d\n", error->error_code);
        free(error);
        return;
    }
    
    // Get the number of rectangles
    uint32_t rectangle_count = xcb_xfixes_fetch_region_rectangles_length(reply);
    
    if (rectangle_count == 0) {
        rect->x = rect->y = 0;
        rect->width = rect->height = 0;
        return;
    }
    
    // Get the rectangles
    xcb_rectangle_t *rectangles = xcb_xfixes_fetch_region_rectangles(reply);
    
    // Find bounding box
    int min_x = rectangles[0].x;
    int min_y = rectangles[0].y;
    int max_x = rectangles[0].x + rectangles[0].width;
    int max_y = rectangles[0].y + rectangles[0].height;
    
    for (uint32_t i = 1; i < rectangle_count; i++) {
        if (rectangles[i].x < min_x) min_x = rectangles[i].x;
        if (rectangles[i].y < min_y) min_y = rectangles[i].y;
        if (rectangles[i].x + rectangles[i].width > max_x) 
            max_x = rectangles[i].x + rectangles[i].width;
        if (rectangles[i].y + rectangles[i].height > max_y) 
            max_y = rectangles[i].y + rectangles[i].height;
    }
    
    rect->x = min_x;
    rect->y = min_y;
    rect->width = max_x - min_x;
    rect->height = max_y - min_y;
}

/* No longer need a global nullRegion - each display has its own null_region in IswPerDisplayStruct */

void
_IswEventInitialize(void)
{
    /* No-op: null_region is now initialized per-display in Display.c */
}

static uint32_t const masks[] = {
    0,                          /* 0 - Error, should never see  */
    0,                          /* 1 - Reply, should never see  */
    XCB_EVENT_MASK_KEY_PRESS,               /* 2 - KeyPress                 */
    XCB_EVENT_MASK_KEY_RELEASE,             /* 3 - KeyRelease               */
    XCB_EVENT_MASK_BUTTON_PRESS,            /* 4 - ButtonPress              */
    XCB_EVENT_MASK_BUTTON_RELEASE,          /* 5 - ButtonRelease            */
    XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_POINTER_MOTION_HINT |
        XCB_EVENT_MASK_BUTTON_1_MOTION |
        XCB_EVENT_MASK_BUTTON_2_MOTION |
        XCB_EVENT_MASK_BUTTON_3_MOTION |
        XCB_EVENT_MASK_BUTTON_4_MOTION |
        XCB_EVENT_MASK_BUTTON_5_MOTION |
        XCB_EVENT_MASK_BUTTON_MOTION,       /* 6 - MotionNotify             */
    XCB_EVENT_MASK_ENTER_WINDOW,            /* 7 - EnterNotify              */
    XCB_EVENT_MASK_LEAVE_WINDOW,            /* 8 - LeaveNotify              */
    XCB_EVENT_MASK_FOCUS_CHANGE,            /* 9 - FocusIn                  */
    XCB_EVENT_MASK_FOCUS_CHANGE,            /* 10 - FocusOut                 */
    XCB_EVENT_MASK_KEYMAP_STATE,            /* 11 - KeymapNotify             */
    XCB_EVENT_MASK_EXPOSURE,                  /* 12 - Expose                   */
    XCB_EVENT_MASK_EXPOSURE,                  /* 13 - GraphicsExpose, in xcb_gcontext_t    */
    XCB_EVENT_MASK_EXPOSURE,                  /* 14 - NoExpose, in xcb_gcontext_t          */
    XCB_EVENT_MASK_VISIBILITY_CHANGE,       /* 15 - VisibilityNotify         */
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,     /* 16 - CreateNotify             */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY,        /* 17 - DestroyNotify            */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY,        /* 18 - UnmapNotify              */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY,        /* 19 - MapNotify                */
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,   /* 20 - MapRequest               */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY,        /* 21 - ReparentNotify           */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY,        /* 22 - ConfigureNotify          */
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,   /* 23 - ConfigureRequest         */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY,        /* 24 - GravityNotify            */
    XCB_EVENT_MASK_RESIZE_REDIRECT,         /* 25 - ResizeRequest            */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY,        /* 26 - CirculateNotify          */
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,   /* 27 - CirculateRequest         */
    XCB_EVENT_MASK_PROPERTY_CHANGE,         /* 28 - PropertyNotify           */
    NonMaskableMask,                        /* 29 - SelectionClear           */
    NonMaskableMask,                        /* 30 - SelectionRequest         */
    NonMaskableMask,                        /* 31 - SelectionNotify          */
    XCB_EVENT_MASK_COLOR_MAP_CHANGE,        /* 32 - ColormapNotify           */
    NonMaskableMask,                        /* 33 - ClientMessage            */
    NonMaskableMask                         /* 34 - MappingNotify            */
};

EventMask
_IswConvertTypeToMask(int eventType)
{
    eventType &= ~0x80; /* strip SendEvent (synthetic) bit */
    if ((Cardinal) eventType < IswNumber(masks))
        return masks[eventType];
    else
        return XCB_EVENT_MASK_NO_EVENT;
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
DispatchEvent(xcb_generic_event_t *event, Widget widget)
{
    //#FIXME, the code previously here was doing event compression/coalescing in 
    //a way that is not replicatable using XCB, however can be done at the connection level.

    return IswDispatchEventToWidget(widget, event);
}

typedef enum _GrabType { pass, ignore, remap } GrabType;

static Boolean
_IswDefaultDispatcher(xcb_generic_event_t *event, xcb_connection_t *dpy)
{
    register Widget widget;
    GrabType grabType;
    IswPerDisplayInput pdi;
    IswGrabList grabList;
    Boolean was_dispatched = False;
    DPY_TO_APPCON(dpy);

    /* HiDPI: convert physical event coordinates to logical pixels */
    _IswDescaleEventCoords(event, _IswGetScaleFactor(dpy));

    int raw_type = event->response_type;
    int type = raw_type & ~0x80;
    /* the default dispatcher discards all extension events */
    if (type >= LASTEvent) {
        return False;
    }

    LOCK_APP(app);

    switch (event->response_type) {
    case XCB_INPUT_KEY_PRESS:
    case XCB_INPUT_KEY_RELEASE:
    case XCB_INPUT_BUTTON_PRESS:
    case XCB_INPUT_BUTTON_RELEASE:
        grabType = remap;
        break;
    case XCB_INPUT_MOTION:
    case XCB_INPUT_ENTER:
        grabType = ignore;
        break;
    default:
        grabType = pass;
        break;
    }

    widget = IswWindowToWidget(dpy, get_event_window(event));
    pdi = _IswGetPerDisplayInput(dpy);

    /* Windowless hit-testing: pointer events are reported against the
       windowed ancestor's window.  Redirect to the deepest windowless
       widget under the pointer and rebase the event coordinates to it.
       Keyboard events are routed by focus, not position, so are excluded. */
    if (widget != NULL) {
        int px, py;
        if (_IswEventPointerXY(event, &px, &py)) {
            int dx, dy;
            uint8_t etype = event->response_type & ~0x80;
            Widget target = _IswFindWidgetAtPoint(widget, px, py, &dx, &dy);

            /* Implicit windowless pointer grab: the first button press starts a
               grab on the windowless widget hit; while any button stays down,
               motion is routed to that widget instead of being re-hit-tested
               by position.  Without this a drag (Paned sash, Slider thumb)
               loses its target the moment the pointer slips off the widget. */
            if (etype == XCB_BUTTON_PRESS) {
                if (pdi->buttonsDown == 0)
                    pdi->windowlessButtonGrab =
                        (target != widget && IswIsWidget(target) &&
                         target->core.windowless) ? target : NULL;
                pdi->buttonsDown |=
                    (1u << ((xcb_button_press_event_t *) event)->detail);
            }

            /* While the grab is held, redirect motion AND the button release
               to the grabbed widget, rebasing coordinates to it (its offset
               from the windowed root, same convention as
               _IswFindWidgetAtPoint's dx,dy).  Routing the release here lets
               the grab's <BtnUp> action fire even if the pointer has slipped
               off the widget. */
            if ((etype == XCB_MOTION_NOTIFY || etype == XCB_BUTTON_RELEASE) &&
                pdi->buttonsDown != 0 &&
                pdi->windowlessButtonGrab != NULL &&
                IswIsWidget(pdi->windowlessButtonGrab) &&
                !pdi->windowlessButtonGrab->core.being_destroyed) {
                Widget g = pdi->windowlessButtonGrab;
                int gx = 0, gy = 0;
                Widget a;
                for (a = g; a != NULL && IswIsWidget(a) && a->core.windowless;
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
            if (etype == XCB_BUTTON_RELEASE) {
                pdi->buttonsDown &=
                    ~(1u << ((xcb_button_release_event_t *) event)->detail);
                if (pdi->buttonsDown == 0)
                    pdi->windowlessButtonGrab = NULL;
            }

            /* On motion, synthesize Enter/Leave when the windowless widget
               under the pointer changes. */
            if (etype == XCB_MOTION_NOTIFY &&
                target != pdi->pointerWidget) {
                Widget old_pw = pdi->pointerWidget;
                pdi->pointerWidget = (target != widget) ? target : NULL;
                if (old_pw != NULL && IswIsWidget(old_pw)
                    && old_pw->core.windowless && !old_pw->core.being_destroyed)
                    _IswSynthesizeCrossing(old_pw, event, XCB_LEAVE_NOTIFY);
                if (pdi->pointerWidget != NULL)
                    _IswSynthesizeCrossing(pdi->pointerWidget, event,
                                           XCB_ENTER_NOTIFY);

                /* Update the windowed ancestor's cursor to match the widget
                   now under the pointer (or restore it when leaving one). */
                if (pdi->pointerWidget != NULL)
                    _IswSimpleApplyCursor(pdi->pointerWidget);
                else if (old_pw != NULL && IswIsWidget(old_pw)
                         && !old_pw->core.being_destroyed)
                    _IswSimpleApplyCursor(old_pw);
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
        uint8_t ftype = event->response_type & 0x7f;
        if (ftype == XCB_KEY_PRESS || ftype == XCB_KEY_RELEASE) {
            if (_IswFocusMgrMaybeHandleKey(widget, event)) {
                UNLOCK_APP(app);
                return True;
            }
        }
        else if (ftype == XCB_BUTTON_PRESS) {
            _IswFocusMgrFocusOnClick(widget);
        }
    }

    if (widget == NULL) {
        /* event occurred in a non-widget window -- drop it */
    }
    else if (grabType == pass) {
        if (event->response_type == XCB_LEAVE_NOTIFY ||
            event->response_type == XCB_FOCUS_IN || event->response_type == XCB_FOCUS_OUT) {
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
        EventMask mask = _IswConvertTypeToMask(event->response_type);
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
IswDispatchEvent(xcb_generic_event_t *event, xcb_connection_t *dpy)
{
    Boolean was_dispatched, safe;
    int dispatch_level;
    int starting_count;
    IswPerDisplay pd;
    xcb_timestamp_t time = 0;
    Boolean is_user_input = False;
    IswEventDispatchProc dispatch = _IswDefaultDispatcher;
    IswAppContext app = IswDisplayToApplicationContext(dpy);

    LOCK_APP(app);
    dispatch_level = ++app->dispatch_level;
    starting_count = app->destroy_count;

    /* Coalesce all widget repaints triggered by this dispatch (and any nested
       dispatches) into a single composite+blit per affected window. */
    ISWRenderBeginDeferComposite();

    switch (event->response_type & ~0x80) {
    case XCB_INPUT_KEY_PRESS:
        time = ((xcb_input_key_press_event_t *)event)->time;
        is_user_input = True;
        break;
    case XCB_INPUT_KEY_RELEASE:
        time = ((xcb_input_key_release_event_t *)event)->time;
        break;
    case XCB_INPUT_BUTTON_PRESS:
        time = ((xcb_input_button_press_event_t *)event)->time;
        is_user_input = True;
        break;
    case XCB_INPUT_BUTTON_RELEASE:
        time = ((xcb_input_button_release_event_t *)event)->time;
        break;
    case XCB_INPUT_MOTION:
        time = ((xcb_input_motion_event_t *)event)->time;
        break;
    case XCB_INPUT_ENTER:
        time = ((xcb_input_enter_event_t *)event)->time;
        break;
    case XCB_INPUT_LEAVE:
        time = ((xcb_input_leave_event_t *)event)->time;
        break;
    case XCB_INPUT_PROPERTY:
        time = ((xcb_input_property_event_t *)event)->time;
        break;
    case XCB_SELECTION_CLEAR:
        time = ((xcb_selection_clear_event_t *)event)->time;
        break;

    case XCB_INPUT_DEVICE_MAPPING_NOTIFY:
        _IswRefreshMapping(dpy, event, True);
        break;
    }
    pd = _IswGetPerDisplay(dpy);

    if (time)
        pd->last_timestamp = time;
    if (is_user_input)
        _IswShellUpdateUserTime(dpy, get_event_window(event), time);
    pd->last_event = *event;

    if (pd->dispatcher_list) {
        /* Mask off the "sent" bit (0x80) to stay within the 128-entry
           dispatcher_list.  Bit 7 indicates SendEvent origin and must
           not be used as an array index. */
        int dispatch_type = event->response_type & 0x7f;
        dispatch = pd->dispatcher_list[dispatch_type];
        if (dispatch == NULL)
            dispatch = _IswDefaultDispatcher;
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
    grabListPtr = _IswGetGrabList(_IswGetPerDisplayInput(IswDisplay(widget)));

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

    grabListPtr = _IswGetGrabList(_IswGetPerDisplayInput(IswDisplay(widget)));

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

xcb_timestamp_t
IswLastTimestampProcessed(xcb_connection_t *dpy)
{
    xcb_timestamp_t time;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    LOCK_PROCESS;
    time = _IswGetPerDisplay(dpy)->last_timestamp;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return (time);
}

xcb_generic_event_t *
IswLastEventProcessed(xcb_connection_t *dpy)
{
    xcb_generic_event_t *le = NULL;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    le = &_IswGetPerDisplay(dpy)->last_event;
    if (!le->full_sequence)
        le = NULL;
    UNLOCK_APP(app);
    return le;
}

void
_IswSendFocusEvent(Widget child, int type)
{
    child = IswIsWidget(child) ? child : _IswWindowedAncestor(child);
    if (IswIsSensitive(child) && !child->core.being_destroyed
        && IswIsRealized(child)
        && (IswBuildEventMask(child) & XCB_EVENT_MASK_FOCUS_CHANGE)) {
        
        if(type == XCB_FOCUS_IN) {
            xcb_focus_in_event_t event = {0};
            event.response_type = XCB_FOCUS_IN;
            event.event = IswWindow(child);
            event.mode = XCB_NOTIFY_MODE_NORMAL;
            event.detail = XCB_NOTIFY_DETAIL_ANCESTOR;
            IswDispatchEventToWidget(child, (xcb_generic_event_t *) &event);
        } else if (type == XCB_FOCUS_OUT) {
            xcb_focus_out_event_t event = {0};
            event.response_type = XCB_FOCUS_OUT;
            event.event = IswWindow(child);
            event.mode = XCB_NOTIFY_MODE_NORMAL;
            event.detail = XCB_NOTIFY_DETAIL_ANCESTOR;
            IswDispatchEventToWidget(child, (xcb_generic_event_t *) &event);
        } else {
            return;
        }
    }
}

static IswEventDispatchProc *
NewDispatcherList(void)
{
    IswEventDispatchProc *l = (IswEventDispatchProc *)
        __XtCalloc((Cardinal) 128,
                   (Cardinal)
                   sizeof(IswEventDispatchProc));

    return l;
}

IswEventDispatchProc
IswSetEventDispatcher(xcb_connection_t *dpy,
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
IswRegisterExtensionSelector(xcb_connection_t *dpy,
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

    pd = _IswGetPerDisplay(IswDisplay(widget));

    for (i = 0; i < pd->ext_select_count; i++) {
        CallExtensionSelector(widget, pd->ext_select_list + i, FALSE);
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}
