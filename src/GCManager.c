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

Copyright 1987, 1988, 1990 by Digital Equipment Corporation, Maynard, Massachusetts.

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

Copyright 1987, 1988, 1990, 1994, 1998  The Open Group

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
#include <xcb/xproto.h>

#include "IntrinsicI.h"
#include "utlist.h"

typedef struct _GCrec {
    xcb_screen_t *screen;       /* Screen for xcb_gcontext_t */
    unsigned char depth;        /* Depth for xcb_gcontext_t */
    char dashes;                /* Dashes value */
    xcb_pixmap_t clip_mask;     /* Clip_mask value */
    Cardinal ref_count;         /* # of shareholders */
    xcb_gcontext_t gc;          /* The xcb_gcontext_t itself. */
    xcb_create_gc_value_list_t *values; /*Copy of values used to create the xcb_gcontext_t as XCB doesn't have a get equivalent*/
    XtGCMask dynamic_mask;      /* Writable values */
    XtGCMask unused_mask;       /* Unused values */
    struct _GCrec *next;        /* Next xcb_gcontext_t for this widgetkind. */
} GCrec, *GCptr;

#define GCVAL(bit,mask,val,default) ((bit&mask) ? val : default)

#define CHECK(bit,comp,default) \
    if ((checkMask & bit) && \
        (GCVAL(bit,valueMask,v->comp,default) != gcv->comp)) return False

#define ALLGCVALS (XCB_GC_FUNCTION | XCB_GC_PLANE_MASK | XCB_GC_FOREGROUND | \
                   XCB_GC_BACKGROUND | XCB_GC_LINE_WIDTH | XCB_GC_LINE_STYLE | \
                   XCB_GC_CAP_STYLE | XCB_GC_JOIN_STYLE | XCB_GC_FILL_STYLE | \
                   XCB_GC_FILL_RULE | XCB_GC_TILE | XCB_GC_STIPPLE | \
                   XCB_GC_TILE_STIPPLE_ORIGIN_X | XCB_GC_TILE_STIPPLE_ORIGIN_Y | \
                   XCB_GC_FONT | XCB_GC_SUBWINDOW_MODE | XCB_GC_GRAPHICS_EXPOSURES | \
                   XCB_GC_CLIP_ORIGIN_X | XCB_GC_CLIP_ORIGIN_Y | XCB_GC_DASH_OFFSET | \
                   XCB_GC_ARC_MODE)

static Bool
Matches(xcb_connection_t *dpy,
        GCptr ptr,
        register XtGCMask valueMask,
        register xcb_create_gc_value_list_t *v,
        XtGCMask readOnlyMask,
        XtGCMask dynamicMask)
{
    xcb_create_gc_value_list_t *gcv = ptr->values;
    register XtGCMask checkMask;

    /* DIAGNOSTIC: Log ptr and ptr->values to identify invalid pointers */

    if (readOnlyMask & ptr->dynamic_mask)
        return False;
    if (((ptr->dynamic_mask | ptr->unused_mask) & dynamicMask) != dynamicMask)
        return False;
    if (!ptr->values) {
        return False;
    }
    checkMask = readOnlyMask & ~ptr->unused_mask;
    CHECK(XCB_GC_FOREGROUND, foreground, 0);
    CHECK(XCB_GC_BACKGROUND, background, 1);
    CHECK(XCB_GC_FONT, font, ~0UL);
    CHECK(XCB_GC_FILL_STYLE, fill_style, XCB_FILL_STYLE_SOLID);
    CHECK(XCB_GC_LINE_WIDTH, line_width, 0);
    CHECK(XCB_GC_FUNCTION, function, XCB_GX_COPY);
    CHECK(XCB_GC_GRAPHICS_EXPOSURES, graphics_exposures, True);
    CHECK(XCB_GC_TILE, tile, ~0UL);
    CHECK(XCB_GC_SUBWINDOW_MODE, subwindow_mode, XCB_SUBWINDOW_MODE_CLIP_BY_CHILDREN);
    CHECK(XCB_GC_PLANE_MASK, plane_mask, AllPlanes);
    CHECK(XCB_GC_LINE_STYLE, line_style, XCB_LINE_STYLE_SOLID);
    CHECK(XCB_GC_CAP_STYLE, cap_style, XCB_CAP_STYLE_BUTT);
    CHECK(XCB_GC_JOIN_STYLE, join_style, XCB_JOIN_STYLE_MITER);
    CHECK(XCB_GC_FILL_RULE, fill_rule, XCB_FILL_RULE_EVEN_ODD);
    CHECK(XCB_GC_ARC_MODE, arc_mode, XCB_ARC_MODE_PIE_SLICE);
    CHECK(XCB_GC_STIPPLE, stipple, ~0UL);
    //CHECK(XCB_GC_TILE_STIPPLE_ORIGIN_X, ts_x_origin, 0);
    //CHECK(XCB_GC_TILE_STIPPLE_ORIGIN_Y, ts_y_origin, 0);
    CHECK(XCB_GC_CLIP_ORIGIN_X, clip_x_origin, 0);
    CHECK(XCB_GC_CLIP_ORIGIN_Y, clip_y_origin, 0);
    CHECK(XCB_GC_DASH_OFFSET, dash_offset, 0);
    gcv->clip_mask = ptr->clip_mask;
    CHECK(XCB_GC_CLIP_MASK, clip_mask, None);
    gcv->dashes = ptr->dashes;
    CHECK(XCB_GC_DASH_LIST, dashes, 4);
    valueMask &= ptr->unused_mask | dynamicMask;
    if (valueMask) {
        /* xcb_change_gc_value_list_t and xcb_create_gc_value_list_t are
           identical structs — cast is safe */
        xcb_change_gc_aux(dpy, ptr->gc, valueMask, (const xcb_change_gc_value_list_t *) v);
        if (valueMask & XCB_GC_DASH_LIST)
            ptr->dashes = v->dashes;
        if (valueMask & XCB_GC_CLIP_MASK)
            ptr->clip_mask = v->clip_mask;
    }
    ptr->unused_mask &= ~(dynamicMask | readOnlyMask);
    ptr->dynamic_mask |= dynamicMask;
    return True;
}  

                   /* Called by CloseDisplay to free the per-display xcb_gcontext_t list */
void
_XtGClistFree(xcb_connection_t *dpy, register XtPerDisplay pd)
{
    GCptr GClist, next;

    GClist = pd->GClist;
    while (GClist) {
        next = GClist->next;
        /* FIX: Free the values structure before freeing the GCrec */
        if (GClist->values) {
            XtFree((char *) GClist->values);
        }
        XtFree((char *) GClist);
        GClist = next;
    }
    if (pd->pixmap_tab) {
        XtScreenPixmapStructPtr current, tmp;
        HASH_ITER(hh, pd->pixmap_tab, current, tmp) {
            HASH_DEL(pd->pixmap_tab, current);
            free(current);
        }
    }
}

/*
 * Return a xcb_gcontext_t with the given values and characteristics.
 */

xcb_gcontext_t
XtAllocateGC(register Widget widget,
             Cardinal depth,
             XtGCMask valueMask,
             xcb_create_gc_value_list_t *values,
             XtGCMask dynamicMask,
             XtGCMask unusedMask)
{
    register GCptr *prev;
    register GCptr cur;
    xcb_screen_t *screen;
    register xcb_connection_t *dpy;
    register XtPerDisplay pd;
    XtScreenPixmapStructPtr screenPixmaps;
    XtPixmapStructPtr pixmap;
    xcb_drawable_t drawable;
    XtGCMask readOnlyMask;
    xcb_gcontext_t retval;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    if (!XtIsWidget(widget))
        widget = _XtWindowedAncestor(widget);
    if (!depth)
        depth = widget->core.depth;
    screen = widget->core.screen;
    dpy = widget->core.display;
    pd = _XtGetPerDisplay(dpy);
    unusedMask &= ~valueMask;
    readOnlyMask = ~(dynamicMask | unusedMask);

    /* Search for existing xcb_gcontext_t that matches exactly */
    //change this to just compare the request struct with one stored with the xcb_gcontext_t
    for (prev = &pd->GClist; (cur = *prev); prev = &cur->next) {
        if (cur->depth == depth &&
            cur->screen == screen &&
            Matches(dpy, cur, valueMask, values, readOnlyMask, dynamicMask)) {
            cur->ref_count++;
            /* Move this xcb_gcontext_t to front of list */
            *prev = cur->next;
            cur->next = pd->GClist;
            pd->GClist = cur;
            retval = cur->gc;
            UNLOCK_PROCESS;
            UNLOCK_APP(app);
            return retval;
        }
    }

    /* No matches, have to create a new one */
    cur = XtNew(GCrec);
    cur->screen = screen;
    cur->depth = (unsigned char) depth;
    cur->ref_count = 1;
    cur->dynamic_mask = dynamicMask;
    cur->unused_mask = (unusedMask & ~dynamicMask);
    cur->dashes = GCVAL(XCB_GC_DASH_LIST, valueMask, values->dashes, 4);
    cur->clip_mask = GCVAL(XCB_GC_CLIP_MASK, valueMask, values->clip_mask, None);
    
    /* FIX: Allocate and initialize cur->values to store the xcb_gcontext_t values */
    cur->values = XtNew(xcb_create_gc_value_list_t);
    memcpy(cur->values, values, sizeof(xcb_create_gc_value_list_t));
    drawable = 0;
    if (depth == widget->core.depth)
        drawable = XtWindow(widget);
    if (!drawable && depth == (Cardinal) screen->root_depth)
        drawable = screen->root;
    if (!drawable) {
        HASH_FIND_INT(pd->pixmap_tab, &cur->screen, screenPixmaps);
        if (!screenPixmaps) {
            screenPixmaps = (XtScreenPixmapStructPtr) XtMalloc(sizeof(XtScreenPixmapStruct));
            screenPixmaps->pixmaps = NULL;
            screenPixmaps->screen = cur->screen;
            HASH_ADD_INT(pd->pixmap_tab, screen, screenPixmaps);
        }

        HASH_FIND_INT(screenPixmaps->pixmaps, &cur->depth, pixmap);
        if (!pixmap) {
            xcb_pixmap_t pm = xcb_generate_id(dpy);
            xcb_void_cookie_t cookie = xcb_create_pixmap(dpy, cur->depth ? cur->depth : screen->root_depth,
                                                          pm, screen->root, 1, 1);
            xcb_generic_error_t *error = xcb_request_check(dpy, cookie);
            if (error) {
                fprintf(stderr, "[GCManager] Error creating pixmap (depth=%u): error_code=%d\n",
                        cur->depth, error->error_code);
                free(error);
                /* Fall back to screen root as drawable */
                pm = screen->root;
            }

            XtPixmapStructPtr p = (XtPixmapStructPtr) XtMalloc(sizeof(XtPixmapStruct));
            p->depth = cur->depth;
            p->pixmap = pm;
            HASH_ADD_INT(screenPixmaps->pixmaps, depth, p);
            drawable = pm;
        } else {
            drawable = pixmap->pixmap;
        }
    }
    /* Final fallback: always use screen root if drawable is still 0 */
    if (!drawable)
        drawable = screen->root;
    
    /* FIX: Initialize cur->gc with xcb_generate_id() before using it */
    cur->gc = xcb_generate_id(dpy);

    /* Use xcb_create_gc_aux (not xcb_create_gc) so the struct is correctly
       interpreted rather than being read as a raw packed-uint32 array. */
    xcb_create_gc_aux(dpy, cur->gc, drawable, valueMask, values);
    cur->next = pd->GClist;
    pd->GClist = cur;
    retval = cur->gc;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return retval;
}                               /* XtAllocateGC */

/*
 * Return a read-only xcb_gcontext_t with the given values.
 */

xcb_gcontext_t
XtGetGC(register Widget widget, XtGCMask valueMask, xcb_create_gc_value_list_t *values)
{
    return XtAllocateGC(widget, 0, valueMask, values, 0, 0);
}                               /* XtGetGC */

void
XtReleaseGC(Widget widget, register xcb_gcontext_t gc)
{
    register GCptr cur, *prev;
    xcb_connection_t *dpy;
    XtPerDisplay pd;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    dpy = XtDisplayOfObject(widget);
    pd = _XtGetPerDisplay(dpy);

    for (prev = &pd->GClist; (cur = *prev); prev = &cur->next) {
        if (cur->gc == gc) {
            if (--(cur->ref_count) == 0) {
                *prev = cur->next;
                xcb_free_gc(dpy, gc);
                /* FIX: Free the values structure before freeing the GCrec */
                if (cur->values) {
                    XtFree((char *) cur->values);
                }
                XtFree((char *) cur);
            }
            break;
        }
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}                               /* XtReleaseGC */

/*  The following interface is broken and supplied only for backwards
 *  compatibility.  It will work properly in all cases only if there
 *  is exactly 1 Display created by the application.
 */

void
XtDestroyGC(register xcb_gcontext_t gc)
{
    GCptr cur, *prev;
    XtAppContext app;

    LOCK_PROCESS;
    app = _XtGetProcessContext()->appContextList;
    /* This is awful; we have to search through all the lists
       to find the xcb_gcontext_t. */
    for (; app; app = app->next) {
        int i;

        for (i = app->count; i;) {
            xcb_connection_t *dpy = app->list[--i];
            XtPerDisplay pd = _XtGetPerDisplay(dpy);

            for (prev = &pd->GClist; (cur = *prev); prev = &cur->next) {
                if (cur->gc == gc) {
                    if (--(cur->ref_count) == 0) {
                        *prev = cur->next;
                        xcb_free_gc(dpy, gc);
                        /* FIX: Free the values structure before freeing the GCrec */
                        if (cur->values) {
                            XtFree((char *) cur->values);
                        }
                        XtFree((char *) cur);
                    }
                    UNLOCK_PROCESS;
                    return;
                }
            }
        }
    }
    UNLOCK_PROCESS;
}                               /* XtDestroyGC */
