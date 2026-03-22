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
#include "Shell.h"
#include "StringDefs.h"

typedef struct _XtEventRecExt {
    int type;
    XtPointer select_data[1];   /* actual dimension is [mask] */
} XtEventRecExt;

#define EXT_TYPE(p) (((XtEventRecExt*) ((p)+1))->type)
#define EXT_SELECT_DATA(p,n) (((XtEventRecExt*) ((p)+1))->select_data[n])

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
#define GRAPHICS_EXPOSE  ((XtExposeGraphicsExpose & COMP_EXPOSE) || \
                          (XtExposeGraphicsExposeMerged & COMP_EXPOSE))
#define NO_EXPOSE        (XtExposeNoExpose & COMP_EXPOSE)

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
XtBuildEventMask(Widget widget)
{
    EventMask mask = 0L;

    if (widget != NULL) {
        XtEventTable ev;

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
            mask |= ExposureMask;
        if (widget->core.widget_class->core_class.visible_interest)
            mask |= VisibilityChangeMask;
        UNLOCK_PROCESS;
        if (widget->core.tm.translations)
            mask |= widget->core.tm.translations->eventMask;

        mask = mask & ~NonMaskableMask;
        UNLOCK_APP(app);
    }
    return mask;
}

static void
CallExtensionSelector(Widget widget, ExtSelectRec *rec, Boolean forceCall)
{
    XtEventRec *p;
    XtPointer *data;
    int *types;
    Cardinal i, count = 0;

    for (p = widget->core.event_table; p != NULL; p = p->next)
        if (p->has_type_specifier &&
            EXT_TYPE(p) >= rec->min && EXT_TYPE(p) <= rec->max)
            count = (Cardinal) (count + p->mask);

    if (count == 0 && !forceCall)
        return;

    data = (XtPointer *) ALLOCATE_LOCAL(count * sizeof(XtPointer));
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
                   XtPointer select_data,
                   int type,
                   Boolean has_type_specifier,
                   Boolean other,
                   const XtEventHandler proc,
                   const XtPointer closure,
                   Boolean raw)
{
    XtEventRec *p, **pp;
    EventMask oldMask = XtBuildEventMask(widget);

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
        XtFree((char *) p);
    }

    /* Reset select mask if realized and not raw. */
    if (!raw && XtIsRealized(widget) && !widget->core.being_destroyed) {
        EventMask mask = XtBuildEventMask(widget);
        xcb_connection_t *dpy = XtDisplay(widget);

        if (oldMask != mask)
            xcb_change_window_attributes(dpy, XtWindow(widget), XCB_CW_EVENT_MASK, &mask);

        if (has_type_specifier) {
            XtPerDisplay pd = _XtGetPerDisplay(dpy);
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
                XtPointer select_data,
                int type,
                Boolean has_type_specifier,
                Boolean other,
                XtEventHandler proc,
                XtPointer closure,
                XtListPosition position,
                Boolean force_new_position,
                Boolean raw)
{
    register XtEventRec *p, **pp;
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

    if (XtIsRealized(widget) && !raw)
        oldMask = XtBuildEventMask(widget);

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
            p = (XtEventRec *) __XtMalloc(sizeof(XtEventRec) +
                                          sizeof(XtEventRecExt));
            EXT_TYPE(p) = type;
            EXT_SELECT_DATA(p, 0) = select_data;
            p->mask = 1;
            p->has_type_specifier = True;
        }
        else {
            p = (XtEventRec *) __XtMalloc(sizeof(XtEventRec));
            p->mask = eventMask;
            p->has_type_specifier = False;
        }
        p->proc = proc;
        p->closure = closure;
        p->select = !raw;

        if (position == XtListHead) {
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

            if (position == XtListHead) {
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
                p = (XtEventRec *) XtRealloc((char *) p,
                                             (Cardinal) (sizeof(XtEventRec) +
                                                         sizeof(XtEventRecExt) +
                                                         p->mask *
                                                         sizeof(XtPointer)));
                EXT_SELECT_DATA(p, i) = select_data;
                p->mask++;
                *pp = p;
            }
        }
    }

    if (XtIsRealized(widget) && !raw) {
        EventMask mask = XtBuildEventMask(widget);
        xcb_connection_t *dpy = XtDisplay(widget);

        if (oldMask != mask)
            xcb_change_window_attributes(dpy, XtWindow(widget), XCB_CW_EVENT_MASK, &mask);

        if (has_type_specifier) {
            XtPerDisplay pd = _XtGetPerDisplay(dpy);
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
XtRemoveEventHandler(Widget widget,
                     EventMask eventMask,
                     _XtBoolean other,
                     XtEventHandler proc,
                     XtPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    RemoveEventHandler(widget, (XtPointer) &eventMask, 0, FALSE,
                       (Boolean) other, proc, closure, FALSE);
    UNLOCK_APP(app);
}

void
XtAddEventHandler(Widget widget,
                  EventMask eventMask,
                  _XtBoolean other,
                  XtEventHandler proc,
                  XtPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (XtPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, XtListTail, FALSE, FALSE);
    UNLOCK_APP(app);
}

void
XtInsertEventHandler(Widget widget,
                     EventMask eventMask,
                     _XtBoolean other,
                     XtEventHandler proc,
                     XtPointer closure,
                     XtListPosition position)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (XtPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, position, TRUE, FALSE);
    UNLOCK_APP(app);
}

void
XtRemoveRawEventHandler(Widget widget,
                        EventMask eventMask,
                        _XtBoolean other,
                        XtEventHandler proc,
                        XtPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    RemoveEventHandler(widget, (XtPointer) &eventMask, 0, FALSE,
                       (Boolean) other, proc, closure, TRUE);
    UNLOCK_APP(app);
}

void
XtInsertRawEventHandler(Widget widget,
                        EventMask eventMask,
                        _XtBoolean other,
                        XtEventHandler proc,
                        XtPointer closure,
                        XtListPosition position)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (XtPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, position, TRUE, TRUE);
    UNLOCK_APP(app);
}

void
XtAddRawEventHandler(Widget widget,
                     EventMask eventMask,
                     _XtBoolean other,
                     XtEventHandler proc,
                     XtPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    AddEventHandler(widget, (XtPointer) &eventMask, 0, FALSE, (Boolean) other,
                    proc, closure, XtListTail, FALSE, TRUE);
    UNLOCK_APP(app);
}

void
XtRemoveEventTypeHandler(Widget widget,
                         int type,
                         XtPointer select_data,
                         XtEventHandler proc,
                         XtPointer closure)
{
    WIDGET_TO_APPCON(widget);
    LOCK_APP(app);
    RemoveEventHandler(widget, select_data, type, TRUE,
                       FALSE, proc, closure, FALSE);
    UNLOCK_APP(app);
}

void
XtInsertEventTypeHandler(Widget widget,
                         int type,
                         XtPointer select_data,
                         XtEventHandler proc,
                         XtPointer closure,
                         XtListPosition position)
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
#define WWTABLE(display) (_XtGetPerDisplay(display)->WWtable)

static void ExpandWWTable(WWTable);

void
XtRegisterDrawable(xcb_connection_t *display, Drawable drawable, Widget widget)
{
    WWTable tab;
    int idx;
    Widget entry;
    xcb_window_t window = (xcb_window_t) drawable;


    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);

    if (window != XtWindow(widget)) {
        WWPair pair;
        pair = XtNew(struct _WWPair);

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
XtUnregisterDrawable(xcb_connection_t *display, Drawable drawable)
{
    WWTable tab;
    int idx;
    Widget entry;
    xcb_window_t window = (xcb_window_t) drawable;
    Widget widget = XtWindowToWidget(display, window);
    DPY_TO_APPCON(display);

    if (widget == NULL)
        return;

    LOCK_APP(app);
    LOCK_PROCESS;
    tab = WWTABLE(display);
    if (window != XtWindow(widget)) {
        WWPair *prev, pair;

        prev = &tab->pairs;
        while ((pair = *prev) && pair->window != window)
            prev = &pair->next;
        if (pair) {
            *prev = pair->next;
            XtFree((char *) pair);
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
            newidx = (Cardinal) WWHASH(tab, XtWindow(entry));
            if (entries[newidx]) {
                rehash = (Cardinal) WWREHASHVAL(tab, XtWindow(entry));
                do {
                    newidx = (Cardinal) WWREHASH(tab, newidx, rehash);
                } while (entries[newidx]);
            }
            entries[newidx] = entry;
        }
    }
    XtFree((char *) oldentries);
    UNLOCK_PROCESS;
}

Widget
XtWindowToWidget(register xcb_connection_t *display, register xcb_window_t window)
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
    if ((entry = tab->entries[idx]) && XtWindow(entry) != window) {
        int rehash = (int) WWREHASHVAL(tab, window);

        do {
            idx = (int) WWREHASH(tab, idx, rehash);
        } while ((entry = tab->entries[idx]) && XtWindow(entry) != window);
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

void
_XtAllocWWTable(XtPerDisplay pd)
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
_XtFreeWWTable(register XtPerDisplay pd)
{
    register WWPair pair, next;

    for (pair = pd->WWtable->pairs; pair; pair = next) {
        next = pair->next;
        XtFree((char *) pair);
    }
    XtFree((char *) pd->WWtable->entries);
    XtFree((char *) pd->WWtable);
}

#define EHMAXSIZE 25            /* do not make whopping big */

static Boolean
CallEventHandlers(Widget widget,xcb_generic_event_t *event, EventMask mask)
{
    register XtEventRec *p;
    XtEventHandler *proc;
    XtPointer *closure;
    Boolean cont_to_disp = True;
    int i, numprocs;

    /* Have to copy the procs into an array, because one of them might
     * call XtRemoveEventHandler, which would break our linked list. */

    numprocs = 0;
    for (p = widget->core.event_table; p; p = p->next) {
        if ((!p->has_type_specifier && (mask & p->mask)) ||
            (p->has_type_specifier && event->response_type == EXT_TYPE(p)))
            numprocs++;
    }
    proc = XtMallocArray((Cardinal) numprocs, (Cardinal)
                         (sizeof(XtEventHandler) + sizeof(XtPointer)));
    closure = (XtPointer *) (proc + numprocs);

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
       _XtHandleFocus,
       Selection.c:HandleSelectionReplies,
       Selection.c:HandleGetIncrement,
       Selection.c:HandleIncremental,
       Selection.c:HandlePropertyGone,
       Selection.c:HandleSelectionEvents
     */
    for (i = 0; i < numprocs && cont_to_disp; i++)
        (*(proc[i])) (widget, closure[i], event, &cont_to_disp);
    XtFree((char *) proc);
    return cont_to_disp;
}

static void CompressExposures(xcb_generic_event_t *, Widget);

#define KnownButtons (Button1MotionMask|Button2MotionMask|Button3MotionMask|\
                      Button4MotionMask|Button5MotionMask)

/* keep this SMALL to avoid blowing stack cache! */
/* because some compilers allocate all local locals on procedure entry */
#define EHSIZE 4

Boolean
XtDispatchEventToWidget(Widget widget, xcb_generic_event_t *event)
{
    register XtEventRec *p;
    Boolean was_dispatched = False;
    Boolean call_tm = False;
    Boolean cont_to_disp;
    xcb_event_mask_t mask;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);

    mask = _XtConvertTypeToMask(event->response_type);
    if (event->response_type == XCB_INPUT_DEVICE_MOTION_NOTIFY)
        mask |= (((xcb_input_device_motion_notify_event_t *)event)->state & KnownButtons);

    LOCK_PROCESS;
    if ((mask == XCB_EVENT_MASK_EXPOSURE)) {

        if (widget->core.widget_class->core_class.expose != NULL) {
            uint8_t event_type = event->response_type & ~0x80;

            /* For Expose events, check if this is the last in the series */
            if (event_type == Expose) {
                xcb_expose_event_t *ev = (xcb_expose_event_t *)event;
                /* Only dispatch when count == 0 (last event in series) or
                 * when compression is disabled */
                if (ev->count == 0 || COMP_EXPOSE_TYPE == XtExposeNoCompress) {
                    (*widget->core.widget_class->core_class.expose)
                        (widget, event, 0);
                    was_dispatched = True;
                }
            }
            /* Note: GraphicsExpose and NoExpose events are not currently
             * handled here. Event compression is commented out pending
             * reimplementation at the connection level. */
        }
    }

    if ((mask == XCB_EVENT_MASK_VISIBILITY_CHANGE) &&
        XtClass(widget)->core_class.visible_interest) {
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
            XtEventHandler proc[EHSIZE];
            XtPointer closure[EHSIZE];
            int numprocs = 0;

            /* Have to copy the procs into an array, because one of them might
             * call XtRemoveEventHandler, which would break our linked list. */

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
                       _XtHandleFocus,
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
        _XtTranslateEvent(widget, event);
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

static void SendExposureEvent(xcb_connection_t *, xcb_generic_event_t *, Widget, XtPerDisplay);
static Bool CheckExposureEvent(xcb_connection_t *, xcb_generic_event_t *, char *);
static void AddExposureToRectangularRegion(xcb_connection_t *, xcb_generic_event_t *, xcb_xfixes_region_t);

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
//    xcb_connection_t *dpy = XtDisplay(widget);
//    XtPerDisplay pd = _XtGetPerDisplay(dpy);
//    XtEnum comp_expose;
//    XtEnum comp_expose_type;
//    Boolean no_region;
//
//    LOCK_PROCESS;
//    comp_expose = COMP_EXPOSE;
//    UNLOCK_PROCESS;
//    comp_expose_type = comp_expose & 0x0f;
//    no_region = ((comp_expose & XtExposeNoRegion) ? True : False);
//
//    if (no_region)
//        AddExposureToRectangularRegion(event, pd->region);
//    else
//        XtAddExposureToRegion(event, pd->region);
//
//    if (GetCount(event) != 0)
//        return;
//
//    if ((comp_expose_type == XtExposeCompressSeries) ||
//        (XEventsQueued(dpy, QueuedAfterReading) == 0)) {
//        SendExposureEvent(event, widget, pd);
//        return;
//    }
//
//    if (comp_expose & XtExposeGraphicsExposeMerged) {
//        info.type1 = Expose;
//        info.type2 = GraphicsExpose;
//    }
//    else {
//        info.type1 = event->response_type;
//        info.type2 = 0;
//    }
//    info.maximal = (comp_expose_type == XtExposeCompressMaximal);
//    info.non_matching = FALSE;
//    info.window = XtWindow(widget);
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
//        XEvent event_return;
//
//        if (XCheckIfEvent(dpy, &event_return,
//                          CheckExposureEvent, (char *) &info)) {
//
//            count = GetCount(&event_return);
//            if (no_region)
//                AddExposureToRectangularRegion(&event_return, pd->region);
//            else
//                XtAddExposureToRegion(&event_return, pd->region);
//        }
//        else if (count != 0) {
//            XIfEvent(dpy, &event_return, CheckExposureEvent, (char *) &info);
//            count = GetCount(&event_return);
//            if (no_region)
//                AddExposureToRectangularRegion(&event_return, pd->region);
//            else
//                XtAddExposureToRegion(&event_return, pd->region);
//        }
//        else                    /* count == 0 && XCheckIfEvent Failed. */
//            break;
//    }
//
//    SendExposureEvent(dpy, event, widget, pd);
//}

void
XtAddExposureToRegion(xcb_connection_t *dpy, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    xcb_rectangle_t rect;
    xcb_expose_event_t *ev = (xcb_expose_event_t *)event;
    xcb_generic_error_t *error;

    /* These Expose and GraphicsExpose fields are at identical offsets */

    if (event->response_type == Expose || event->response_type == GraphicsExpose) {
        rect.x = (Position) ev->x;
        rect.y = (Position) ev->y;
        rect.width = (Dimension) ev->width;
        rect.height = (Dimension) ev->height;
        
        xcb_xfixes_region_t new_region = xcb_generate_id(dpy);
        xcb_void_cookie_t cookie = xcb_xfixes_create_region(dpy, new_region, 1, &rect);
        if (error) {
            fprintf(stderr, "Error creating new region: %d\n", error->error_code);
            free(error);
            return;
        }

        cookie = xcb_xfixes_union_region(dpy, region, new_region, region);
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

static void
AddExposureToRectangularRegion(xcb_connection_t *dpy, xcb_generic_event_t *event,   /* when called internally, type is always appropriate */
                               xcb_xfixes_region_t region)
{
    xcb_rectangle_t rect;
    xcb_expose_event_t *ev = (xcb_expose_event_t *)event;
    xcb_generic_error_t *error;

    /* These Expose and GraphicsExpose fields are at identical offsets */

    rect.x = (Position) ev->x;
    rect.y = (Position) ev->y;
    rect.width = (Dimension) ev->width;
    rect.height = (Dimension) ev->height;

    xcb_xfixes_region_t new_region = xcb_generate_id(dpy);
    xcb_void_cookie_t cookie;

    //check if region is empty
    xcb_xfixes_fetch_region_cookie_t fetch_cookie = xcb_xfixes_fetch_region(dpy, region);
    xcb_xfixes_fetch_region_reply_t *reply = xcb_xfixes_fetch_region_reply (dpy, fetch_cookie, &error);
    if (error) {
        fprintf(stderr, "Error fetching region: %d\n", error->error_code);
        free(error);
        return;
    }
    
    if (xcb_xfixes_fetch_region_rectangles_length(reply) == 0) {
        //XUnionRectWithRegion(&rect, region, region);
        cookie = xcb_xfixes_create_region(dpy, new_region, 1, &rect);
    }
    else {
        xcb_rectangle_t merged, bbox;

        //XClipBox(region, &bbox);
        get_region_bounding_box(dpy, region, &bbox);
        merged.x = MIN(rect.x, bbox.x);
        merged.y = MIN(rect.y, bbox.y);
        merged.width = (unsigned short) (MAX(rect.x + rect.width,
                                             bbox.x + bbox.width) - merged.x);
        merged.height = (unsigned short) (MAX(rect.y + rect.height,
                                              bbox.y + bbox.height) - merged.y);
       // XUnionRectWithRegion(&merged, region, region);
       cookie = xcb_xfixes_create_region(dpy, new_region, 1, &merged);
    }

    error = xcb_request_check(dpy, cookie);
    if (error) {
        fprintf(stderr, "Error creating new region: %d\n", error->error_code);
        free(error);
        return; // or handle appropriately
    }

    cookie = xcb_xfixes_union_region(dpy, region, new_region, region);
    if (error) {
        fprintf(stderr, "Error in union_region: %d\n", error->error_code);
        free(error);
        return; // or handle appropriately
    }
}

/* No longer need a global nullRegion - each display has its own null_region in XtPerDisplayStruct */

void
_XtEventInitialize()
{
    /* No-op: null_region is now initialized per-display in Display.c */
}

/*      Function Name: SendExposureEvent
 *      Description: Sets the x, y, width, and height of the event
 *                   to be the clip box of Expose Region.
 *      Arguments: event  - the X Event to mangle; Expose or GraphicsExpose.
 *                 widget - the widget that this event occurred in.
 *                 pd     - the per display information for this widget.
 *      Returns: none.
 */

static void
SendExposureEvent(xcb_connection_t *dpy, xcb_generic_event_t *event, Widget widget, XtPerDisplay pd)
{
    XtExposeProc expose;
    xcb_rectangle_t rect;
    XtEnum comp_expose;
    xcb_expose_event_t *ev = (xcb_expose_event_t *) event;

    //XClipBox(pd->region, &rect);
    get_region_bounding_box(dpy, pd->region, &rect);
    ev->x = rect.x;
    ev->y = rect.y;
    ev->width = rect.width;
    ev->height = rect.height;

    LOCK_PROCESS;
    comp_expose = COMP_EXPOSE;
    expose = widget->core.widget_class->core_class.expose;
    UNLOCK_PROCESS;
    if (comp_expose & XtExposeNoRegion)
        (*expose) (widget, event, 0);
    else
        (*expose) (widget, event, pd->region);
    /* Clear the region by intersecting with empty null_region */
    xcb_void_cookie_t cookie = xcb_xfixes_intersect_region(dpy, pd->null_region, pd->region, pd->region);
    
    // Check for errors
    xcb_generic_error_t *error = xcb_request_check(dpy, cookie);
    if (error) {
        fprintf(stderr, "Error intersecting regions: %d\n", error->error_code);
        free(error);
    }
}

/*      Function Name: CheckExposureEvent
 *      Description: Checks the event queue for an expose event
 *      Arguments: display - the display connection.
 *                 event - the event to check.
 *                 arg - a pointer to the exposure info structure.
 *      Returns: TRUE if we found an event of the correct type
 *               with the right window.
 *
 * NOTE: The only valid types (info.type1 and info.type2) are Expose
 *       and GraphicsExpose.
 */

static Bool
CheckExposureEvent(xcb_connection_t *disp _X_UNUSED, xcb_generic_event_t *event, char *arg)
{
    CheckExposeInfo *info = ((CheckExposeInfo *) arg);

    if ((info->type1 == event->response_type) || (info->type2 == event->response_type)) {
        if (!info->maximal && info->non_matching)
            return FALSE;
        if (event->response_type == GraphicsExpose)
            return (((xcb_graphics_exposure_event_t  *)event)->drawable == info->window);
        return (((xcb_expose_event_t *)event)->window == info->window);
    }
    info->non_matching = TRUE;
    return (FALSE);
}

static uint32_t const masks[] = {
    0,                          /* 0 - Error, should never see  */
    0,                          /* 1 - Reply, should never see  */
    XCB_EVENT_MASK_KEY_PRESS,               /* 2 - KeyPress                 */
    XCB_EVENT_MASK_KEY_RELEASE,             /* 3 - KeyRelease               */
    XCB_EVENT_MASK_BUTTON_PRESS,            /* 4 - ButtonPress              */
    XCB_EVENT_MASK_BUTTON_RELEASE,          /* 5 - ButtonRelease            */
    XCB_EVENT_MASK_POINTER_MOTION,          /* 6 - MotionNotify             */
    XCB_EVENT_MASK_ENTER_WINDOW,            /* 7 - EnterNotify              */
    XCB_EVENT_MASK_LEAVE_WINDOW,            /* 8 - LeaveNotify              */
    XCB_EVENT_MASK_FOCUS_CHANGE,            /* 9 - FocusIn                  */
    XCB_EVENT_MASK_FOCUS_CHANGE,            /* 10 - FocusOut                 */
    XCB_EVENT_MASK_KEYMAP_STATE,            /* 11 - KeymapNotify             */
    XCB_EVENT_MASK_EXPOSURE,                  /* 12 - Expose                   */
    XCB_EVENT_MASK_EXPOSURE,                  /* 13 - GraphicsExpose, in GC    */
    XCB_EVENT_MASK_EXPOSURE,                  /* 14 - NoExpose, in GC          */
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
    XCB_EVENT_MASK_PROPERTY_CHANGE,         /* 29 - SelectionClear           */
    XCB_EVENT_MASK_PROPERTY_CHANGE,         /* 30 - SelectionRequest         */
    XCB_EVENT_MASK_PROPERTY_CHANGE,         /* 31 - SelectionNotify          */
    XCB_EVENT_MASK_COLOR_MAP_CHANGE         /* 32 - ColormapNotify           */
    //XCB_EVENT_MASK_CLIENT_MESSAGE,          /* 33 - ClientMessage            */
    //XCB_EVENT_MASK_MAPPING_NOTIFY           /* 34 - MappingNotify            */
};

EventMask
_XtConvertTypeToMask(int eventType)
{
    if ((Cardinal) eventType < XtNumber(masks))
        return masks[eventType];
    else
        return NoEventMask;
}

Boolean
_XtOnGrabList(register Widget widget, XtGrabRec *grabList)
{
    register XtGrabRec *gl;

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

static Widget
LookupSpringLoaded(XtGrabList grabList)
{
    XtGrabList gl;

    for (gl = grabList; gl != NULL; gl = gl->next) {
        if (gl->spring_loaded) {
            if (XtIsSensitive(gl->widget))
                return gl->widget;
            else
                return NULL;
        }
        if (gl->exclusive)
            break;
    }
    return NULL;
}

static Boolean
DispatchEvent(xcb_generic_event_t *event, Widget widget)
{
    //#FIXME, the code previously here was doing event compression/coalescing in 
    //a way that is not replicatable using XCB, however can be done at the connection level.

    return XtDispatchEventToWidget(widget, event);
}

typedef enum _GrabType { pass, ignore, remap } GrabType;

static Boolean
_XtDefaultDispatcher(xcb_generic_event_t *event, xcb_connection_t *dpy)
{
    register Widget widget;
    GrabType grabType;
    XtPerDisplayInput pdi;
    XtGrabList grabList;
    Boolean was_dispatched = False;
    DPY_TO_APPCON(dpy);

    int raw_type = event->response_type;
    int type = raw_type & ~0x80;
    xcb_window_t win = get_event_window(event);

    /* the default dispatcher discards all extension events */
    if (event->response_type >= LASTEvent) {
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

    widget = XtWindowToWidget(dpy, get_event_window(event));
    pdi = _XtGetPerDisplayInput(dpy);

    grabList = *_XtGetGrabList(pdi);

    if (widget == NULL) {
        if (grabType == remap
            && (widget = LookupSpringLoaded(grabList)) != NULL) {
            /* event occurred in a non-widget window, but we've promised also
               to dispatch it to the nearest accessible spring_loaded widget */
            //was_dispatched = (XFilterEvent(event, XtWindow(widget))
            //                  || XtDispatchEventToWidget(widget, event));
            
            was_dispatched = XtDispatchEventToWidget(widget, event);
        }
        //else
        //    was_dispatched = (Boolean) XFilterEvent(event, None);
    }
    else if (grabType == pass) {
        if (event->response_type == LeaveNotify ||
            event->response_type == FocusIn || event->response_type == FocusOut) {
            if (XtIsSensitive(widget))
                was_dispatched = XtDispatchEventToWidget(widget, event);
        }
        else
            was_dispatched = XtDispatchEventToWidget(widget, event);
    }
    else if (grabType == ignore) {
        if ((grabList == NULL || _XtOnGrabList(widget, grabList))
            && XtIsSensitive(widget)) {
            was_dispatched = DispatchEvent(event, widget);
        }
    }
    else if (grabType == remap) {
        EventMask mask = _XtConvertTypeToMask(event->response_type);
        Widget dspWidget;
        Boolean was_filtered = False;

        dspWidget = _XtFindRemapWidget(event, widget, mask, pdi);

        if ((grabList == NULL || _XtOnGrabList(dspWidget, grabList))
            && XtIsSensitive(dspWidget)) {
            //if ((was_filtered =
            //     (Boolean) XFilterEvent(event, XtWindow(dspWidget)))) {
            //    /* If this event activated a device grab, release it. */
            //    _XtUngrabBadGrabs(event, widget, mask, pdi);
            //    was_dispatched = True;
            //}
            //else
                was_dispatched = XtDispatchEventToWidget(dspWidget, event);
        }
        else
            _XtUngrabBadGrabs(event, widget, mask, pdi);

        //if (!was_filtered) {
            /* Also dispatch to nearest accessible spring_loaded. */
            /* Fetch this afterward to reflect modal list changes */
            grabList = *_XtGetGrabList(pdi);
            widget = LookupSpringLoaded(grabList);
            if (widget != NULL && widget != dspWidget) {
                was_dispatched = XtDispatchEventToWidget(widget, event)
                                  || was_dispatched;
            }
       // }
    }
    UNLOCK_APP(app);
    return was_dispatched;
}

Boolean
XtDispatchEvent(xcb_generic_event_t *event, xcb_connection_t *dpy)
{
    Boolean was_dispatched, safe;
    int dispatch_level;
    int starting_count;
    XtPerDisplay pd;
    xcb_timestamp_t time = 0;
    XtEventDispatchProc dispatch = _XtDefaultDispatcher;
    XtAppContext app = XtDisplayToApplicationContext(dpy);

    LOCK_APP(app);
    dispatch_level = ++app->dispatch_level;
    starting_count = app->destroy_count;

    switch (event->response_type & ~0x80) {
    case XCB_INPUT_KEY_PRESS:
        time = ((xcb_input_key_press_event_t *)event)->time;
        break;
    case XCB_INPUT_KEY_RELEASE:
        time = ((xcb_input_key_release_event_t *)event)->time;
        break;
    case XCB_INPUT_BUTTON_PRESS:
        time = ((xcb_input_button_press_event_t *)event)->time;
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
    case SelectionClear:
        time = ((xcb_selection_clear_event_t *)event)->time;
        break;

    case XCB_INPUT_DEVICE_MAPPING_NOTIFY:
        _XtRefreshMapping(dpy, event, True);
        break;
    }
    pd = _XtGetPerDisplay(dpy);

    if (time)
        pd->last_timestamp = time;
    pd->last_event = *event;

    if (pd->dispatcher_list) {
        dispatch = pd->dispatcher_list[event->response_type];
        if (dispatch == NULL)
            dispatch = _XtDefaultDispatcher;
    }
    was_dispatched = (*dispatch) (event, dpy);

    /*
     * To make recursive XtDispatchEvent work, we need to do phase 2 destroys
     * only on those widgets destroyed by this particular dispatch.
     *
     */

    if (app->destroy_count > starting_count)
        _XtDoPhase2Destroy(app, dispatch_level);

    app->dispatch_level = dispatch_level - 1;

    if ((safe = _XtSafeToDestroy(app))) {
        if (app->dpy_destroy_count != 0)
            _XtCloseDisplays(app);
        if (app->free_bindings)
            _XtDoFreeBindings(app);
    }
    UNLOCK_APP(app);
    LOCK_PROCESS;
    if (_XtAppDestroyCount != 0 && safe)
        _XtDestroyAppContexts();
    UNLOCK_PROCESS;
    return was_dispatched;
}

static void
GrabDestroyCallback(Widget widget,
                    XtPointer closure _X_UNUSED,
                    XtPointer call_data _X_UNUSED)
{
    /* Remove widget from grab list if it destroyed */
    XtRemoveGrab(widget);
}

static XtGrabRec *
NewGrabRec(Widget widget, Boolean exclusive, Boolean spring_loaded)
{
    register XtGrabList gl;

    gl = XtNew(XtGrabRec);
    gl->next = NULL;
    gl->widget = widget;
    XtSetBit(gl->exclusive, exclusive);
    XtSetBit(gl->spring_loaded, spring_loaded);

    return gl;
}

void
XtAddGrab(Widget widget, _XtBoolean exclusive, _XtBoolean spring_loaded)
{
    register XtGrabList gl;
    XtGrabList *grabListPtr;
    XtAppContext app = XtWidgetToApplicationContext(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    grabListPtr = _XtGetGrabList(_XtGetPerDisplayInput(XtDisplay(widget)));

    if (spring_loaded && !exclusive) {
        XtAppWarningMsg(app,
                        "grabError", "xtAddGrab", XtCXtToolkitError,
                        "XtAddGrab requires exclusive grab if spring_loaded is TRUE",
                        NULL, NULL);
        exclusive = TRUE;
    }

    gl = NewGrabRec(widget, (Boolean) exclusive, (Boolean) spring_loaded);
    gl->next = *grabListPtr;
    *grabListPtr = gl;

    XtAddCallback(widget, XtNdestroyCallback,
                  GrabDestroyCallback, (XtPointer) NULL);
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

void
XtRemoveGrab(Widget widget)
{
    register XtGrabList gl;
    register Boolean done;
    XtGrabList *grabListPtr;
    XtAppContext app = XtWidgetToApplicationContext(widget);

    LOCK_APP(app);
    LOCK_PROCESS;

    grabListPtr = _XtGetGrabList(_XtGetPerDisplayInput(XtDisplay(widget)));

    for (gl = *grabListPtr; gl != NULL; gl = gl->next) {
        if (gl->widget == widget)
            break;
    }

    if (gl == NULL) {
        XtAppWarningMsg(app,
                        "grabError", "xtRemoveGrab", XtCXtToolkitError,
                        "XtRemoveGrab asked to remove a widget not on the list",
                        NULL, NULL);
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return;
    }

    do {
        gl = *grabListPtr;
        done = (gl->widget == widget);
        *grabListPtr = gl->next;
        XtRemoveCallback(gl->widget, XtNdestroyCallback,
                         GrabDestroyCallback, (XtPointer) NULL);
        XtFree((char *) gl);
    } while (!done);
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return;
}

void
XtMainLoop(void)
{
    XtAppMainLoop(_XtDefaultAppContext());
}

void
XtAppMainLoop(XtAppContext app)
{
    XtInputMask m = XtIMAll;
    XtInputMask t;

    LOCK_APP(app);
    do {
        if (m == 0) {
            m = XtIMAll;
            /* wait for any event, blocking */
            XtAppProcessEvent(app, m);
        }
        else if (((t = XtAppPending(app)) & m)) {
            /* wait for certain events, stepping through choices */
            XtAppProcessEvent(app, t & m);
        }
        m >>= 1;
    } while (app->exit_flag == FALSE);
    UNLOCK_APP(app);
}

void
_XtFreeEventTable(XtEventTable *event_table)
{
    register XtEventTable event;

    event = *event_table;
    while (event != NULL) {
        register XtEventTable next = event->next;

        XtFree((char *) event);
        event = next;
    }
}

xcb_timestamp_t
XtLastTimestampProcessed(xcb_connection_t *dpy)
{
    xcb_timestamp_t time;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    LOCK_PROCESS;
    time = _XtGetPerDisplay(dpy)->last_timestamp;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return (time);
}

xcb_generic_event_t *
XtLastEventProcessed(xcb_connection_t *dpy)
{
    xcb_generic_event_t *le = NULL;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    le = &_XtGetPerDisplay(dpy)->last_event;
    if (!le->full_sequence)
        le = NULL;
    UNLOCK_APP(app);
    return le;
}

void
_XtSendFocusEvent(Widget child, int type)
{
    child = XtIsWidget(child) ? child : _XtWindowedAncestor(child);
    if (XtIsSensitive(child) && !child->core.being_destroyed
        && XtIsRealized(child)
        && (XtBuildEventMask(child) & FocusChangeMask)) {
        
        xcb_connection_t *dpy = XtDisplay(child);
        if(type == FocusIn) {
            xcb_focus_out_event_t event;
            event.event = XtWindow(child);
            event.mode = NotifyNormal;
            event.detail = NotifyAncestor;
            /* NOTE: XFilterEvent (Xlib input method filtering) has no direct
             * XCB equivalent. Input method support would require integration
             * with libxkbcommon-x11 or similar libraries. For now, events are
             * dispatched directly without IM filtering. */
            XtDispatchEventToWidget(child, (xcb_generic_event_t *) &event);

        } else if (type == FocusOut) {
            xcb_focus_in_event_t event;
            event.event = XtWindow(child);
            event.mode = NotifyNormal;
            event.detail = NotifyAncestor;
            /* NOTE: XFilterEvent (Xlib input method filtering) has no direct
             * XCB equivalent. Input method support would require integration
             * with libxkbcommon-x11 or similar libraries. For now, events are
             * dispatched directly without IM filtering. */
            XtDispatchEventToWidget(child, (xcb_generic_event_t *) &event);
        } else {
            return;
        }
    }
}

static XtEventDispatchProc *
NewDispatcherList(void)
{
    XtEventDispatchProc *l = (XtEventDispatchProc *)
        __XtCalloc((Cardinal) 128,
                   (Cardinal)
                   sizeof(XtEventDispatchProc));

    return l;
}

XtEventDispatchProc
XtSetEventDispatcher(xcb_connection_t *dpy,
                     int event_type,
                     XtEventDispatchProc proc)
{
    XtEventDispatchProc *list;
    XtEventDispatchProc old_proc;
    register XtPerDisplay pd;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    LOCK_PROCESS;
    pd = _XtGetPerDisplay(dpy);

    list = pd->dispatcher_list;
    if (!list) {
        if (proc)
            list = pd->dispatcher_list = NewDispatcherList();
        else
            return _XtDefaultDispatcher;
    }
    old_proc = list[event_type];
    list[event_type] = proc;
    if (old_proc == NULL)
        old_proc = _XtDefaultDispatcher;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return old_proc;
}

void
XtRegisterExtensionSelector(xcb_connection_t *dpy,
                            int min_event_type,
                            int max_event_type,
                            XtExtensionSelectProc proc,
                            XtPointer client_data)
{
    XtPerDisplay pd;
    int i;

    DPY_TO_APPCON(dpy);

    if (dpy == NULL)
        XtErrorMsg("nullDisplay",
                   "xtRegisterExtensionSelector", XtCXtToolkitError,
                   "XtRegisterExtensionSelector requires a non-NULL display",
                   NULL, NULL);

    LOCK_APP(app);
    LOCK_PROCESS;
    pd = _XtGetPerDisplay(dpy);

    for (i = 0; i < pd->ext_select_count; i++) {
        ExtSelectRec *e = &pd->ext_select_list[i];

        if (e->min == min_event_type && e->max == max_event_type) {
            e->proc = proc;
            e->client_data = client_data;
            return;
        }
        if ((min_event_type >= e->min && min_event_type <= e->max) ||
            (max_event_type >= e->min && max_event_type <= e->max)) {
            XtErrorMsg("rangeError", "xtRegisterExtensionSelector",
                       XtCXtToolkitError,
                       "Attempt to register multiple selectors for one extension event type",
                       NULL, NULL);
            UNLOCK_PROCESS;
            UNLOCK_APP(app);
            return;
        }
    }
    pd->ext_select_count++;
    pd->ext_select_list = XtReallocArray(pd->ext_select_list,
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
_XtExtensionSelect(Widget widget)
{
    int i;
    XtPerDisplay pd;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;

    pd = _XtGetPerDisplay(XtDisplay(widget));

    for (i = 0; i < pd->ext_select_count; i++) {
        CallExtensionSelector(widget, pd->ext_select_list + i, FALSE);
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}
