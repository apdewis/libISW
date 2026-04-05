/***********************************************************

Copyright (c) 1987, 1988, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <stdio.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/SimpleP.h>
#include "ISWXcbDraw.h"

#define offset(field) XtOffsetOf(SimpleRec, simple.field)

static XtResource resources[] = {
  {XtNcursor, XtCCursor, XtRCursor, sizeof(xcb_cursor_t),
     offset(cursor), XtRImmediate, (XtPointer) None},
  {XtNinsensitiveBorder, XtCInsensitive, XtRPixmap, sizeof(xcb_pixmap_t),
     offset(insensitive_border), XtRImmediate, (XtPointer) NULL},
  /* Color cursor resources removed - not available in XCB
  {XtNpointerColor, XtCForeground, XtRPixel, sizeof(Pixel),
     offset(pointer_fg), XtRString, XtDefaultForeground},
  {XtNpointerColorBackground, XtCBackground, XtRPixel, sizeof(Pixel),
     offset(pointer_bg), XtRString, XtDefaultBackground},
  */
  {XtNcursorName, XtCCursor, XtRString, sizeof(String),
     offset(cursor_name), XtRString, NULL},
#ifdef ISW_INTERNATIONALIZATION
  {XtNinternational, XtCInternational, XtRBoolean, sizeof(Boolean),
     offset(international), XtRImmediate, (XtPointer) FALSE},
#endif
#undef offset
};

static void ClassPartInitialize(WidgetClass);
static void ClassInitialize(void);
static void Realize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static void SimpleRedisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void SimpleDestroy(Widget);
static void ConvertCursor(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean ChangeSensitive(Widget);

SimpleClassRec simpleClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &widgetClassRec,
    /* class_name		*/	"Simple",
    /* widget_size		*/	sizeof(SimpleRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	ClassPartInitialize,
    /* class_inited		*/	FALSE,
    /* initialize		*/	NULL,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	SimpleDestroy,
    /* resize			*/	NULL,
    /* expose			*/	SimpleRedisplay,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	NULL,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* simple fields */
    /* change_sensitive		*/	ChangeSensitive
  }
};

WidgetClass simpleWidgetClass = (WidgetClass)&simpleClassRec;

static void
ClassInitialize(void)
{
    static XtConvertArgRec convertArg[] = {
        {XtWidgetBaseOffset, (XtPointer) XtOffsetOf(WidgetRec, core.screen),
      sizeof(xcb_screen_t *)},
        /* Color resources removed for XCB compatibility
        {XtResourceString, (XtPointer) XtNpointerColor, sizeof(Pixel)},
        {XtResourceString, (XtPointer) XtNpointerColorBackground,
      sizeof(Pixel)},
        */
        {XtWidgetBaseOffset, (XtPointer) XtOffsetOf(WidgetRec, core.colormap),
      sizeof(xcb_colormap_t)}
    };

    IswInitializeWidgetSet();
    /* Color cursor converter removed - not available in XCB
    XtSetTypeConverter( XtRString, XtRColorCursor, XmuCvtStringToColorCursor,
         convertArg, XtNumber(convertArg),
         XtCacheByDisplay, (XtDestructor)NULL);
    */
}

static void
ClassPartInitialize(WidgetClass class)
{
    SimpleWidgetClass c     = (SimpleWidgetClass) class;
    SimpleWidgetClass super = (SimpleWidgetClass)
      c->core_class.superclass;

    if (c->simple_class.change_sensitive == NULL) {
	char buf[BUFSIZ];

	(void) sprintf(buf,
		"%s Widget: The Simple Widget class method 'change_sensitive' is undefined.\nA function must be defined or inherited.",
		c->core_class.class_name);
	XtWarning(buf);
	c->simple_class.change_sensitive = ChangeSensitive;
    }

    if (c->simple_class.change_sensitive == XtInheritChangeSensitive)
	c->simple_class.change_sensitive = super->simple_class.change_sensitive;
}

static void
Realize(xcb_connection_t *dpy, Widget w, XtValueMask *valueMask, uint32_t *attributes)
{
    xcb_pixmap_t border_pixmap = 0;
    
    if (!XtIsSensitive(w)) {
	/* change border to gray; have to remember the old one,
	 * so XtDestroyWidget deletes the proper one */
	if (((SimpleWidget)w)->simple.insensitive_border == None)
	    ((SimpleWidget)w)->simple.insensitive_border =
		IswCreateStippledPixmap(XtDisplay(w), XtWindow(w),
					w->core.border_pixel,
					w->core.background_pixel,
					w->core.depth);
        border_pixmap = w->core.border_pixmap;
	w->core.border_pixmap = ((SimpleWidget)w)->simple.insensitive_border;

	*valueMask |= XCB_CW_BORDER_PIXMAP;
	*valueMask &= ~XCB_CW_BORDER_PIXEL;
	attributes[__builtin_popcount(*valueMask & (XCB_CW_BORDER_PIXMAP - 1))] = w->core.border_pixmap;
    }

    ConvertCursor(w);

    if (((SimpleWidget)w)->simple.cursor != None &&
        ((SimpleWidget)w)->simple.cursor != (xcb_cursor_t)0xffffffff) {
	/* Only add cursor if it's a valid non-zero cursor ID */
	*valueMask |= XCB_CW_CURSOR;
	attributes[__builtin_popcount(*valueMask & (XCB_CW_CURSOR - 1))] = ((SimpleWidget)w)->simple.cursor;
    }

    /* attributes parameter is already in XCB uint32_t format */
    XtCreateWindow(XtDisplay(w), w, (unsigned int)XCB_WINDOW_CLASS_INPUT_OUTPUT,
                   (xcb_visualtype_t *)CopyFromParent, *valueMask, attributes);

    if (!XtIsSensitive(w))
	w->core.border_pixmap = border_pixmap;
}

static void
SimpleRedisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    SimpleWidget sw = (SimpleWidget) w;

    if (!XtIsRealized(w))
        return;
    if (w->core.border_width_top == 0 && w->core.border_width_right == 0 &&
        w->core.border_width_bottom == 0 && w->core.border_width_left == 0)
        return;

    ISWRenderContext *ctx = sw->simple.render_ctx;
    if (!ctx && w->core.width > 0 && w->core.height > 0) {
        ctx = sw->simple.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    }
    if (ctx) {
        ISWRenderBegin(ctx);
        ISWRenderDrawBorder(ctx,
            w->core.border_width_top, w->core.border_width_right,
            w->core.border_width_bottom, w->core.border_width_left,
            w->core.border_pixel_top, w->core.border_pixel_right,
            w->core.border_pixel_bottom, w->core.border_pixel_left,
            w->core.width, w->core.height);
        ISWRenderEnd(ctx);
    }
}

static void
SimpleDestroy(Widget w)
{
    SimpleWidget sw = (SimpleWidget) w;
    if (sw->simple.render_ctx) {
        ISWRenderDestroy(sw->simple.render_ctx);
        sw->simple.render_ctx = NULL;
    }
}

/*	Function Name: ConvertCursor
 *	Description: Converts a name to a new cursor.
 *	Arguments: w - the simple widget.
 *	Returns: none.
 */

static void
ConvertCursor(Widget w)
{
    SimpleWidget simple = (SimpleWidget) w;
    XrmValue from, to;
    xcb_cursor_t cursor;

    if (simple->simple.cursor_name == NULL)
	return;

    from.addr = (XtPointer) simple->simple.cursor_name;
    from.size = strlen((char *) from.addr) + 1;

    to.size = sizeof(xcb_cursor_t);
    to.addr = (XtPointer) &cursor;

    /* Changed XtRColorCursor to XtRCursor for XCB compatibility */
    if (XtConvertAndStore(w, XtRString, &from, XtRCursor, &to)) {
 if ( cursor !=  None)
     simple->simple.cursor = cursor;
    }
    else {
 XtAppErrorMsg(XtWidgetToApplicationContext(w),
        "convertFailed","ConvertCursor","IswError",
        "Simple: ConvertCursor failed.",
        (String *)NULL, (Cardinal *)NULL);
    }
}


/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SimpleWidget s_old = (SimpleWidget) current;
    SimpleWidget s_new = (SimpleWidget) new;
    Boolean new_cursor = FALSE;

    /* this disables user changes after creation*/
#ifdef ISW_INTERNATIONALIZATION
    s_new->simple.international = s_old->simple.international;
#endif

    if ( XtIsSensitive(current) != XtIsSensitive(new) )
	(*((SimpleWidgetClass)XtClass(new))->
	     simple_class.change_sensitive) ( new );

    if (s_old->simple.cursor != s_new->simple.cursor) {
	new_cursor = TRUE;
    }

/*
 * We are not handling the string cursor_name correctly here.
 */

    if ( (s_old->simple.pointer_fg != s_new->simple.pointer_fg) ||
	(s_old->simple.pointer_bg != s_new->simple.pointer_bg) ||
	(s_old->simple.cursor_name != s_new->simple.cursor_name) ) {
	ConvertCursor(new);
	new_cursor = TRUE;
    }

    if (new_cursor && XtIsRealized(new)) {
        /* XDefineCursor → xcb_change_window_attributes */
        uint32_t value = s_new->simple.cursor;
        xcb_change_window_attributes(XtDisplay(new), XtWindow(new),
                                     XCB_CW_CURSOR, &value);
    }

    return False;
}


static Boolean
ChangeSensitive(Widget w)
{
    if (XtIsRealized(w)) {
 if (XtIsSensitive(w)) {
     if (w->core.border_pixmap != XtUnspecifiedPixmap) {
  /* XSetWindowBorderPixmap → xcb_change_window_attributes */
  uint32_t value = w->core.border_pixmap;
  xcb_change_window_attributes(XtDisplay(w), XtWindow(w),
    	     XCB_CW_BORDER_PIXMAP, &value);
     } else {
  /* XSetWindowBorder → xcb_change_window_attributes */
  uint32_t value = w->core.border_pixel;
  xcb_change_window_attributes(XtDisplay(w), XtWindow(w),
    	     XCB_CW_BORDER_PIXEL, &value);
     }
 } else {
     if (((SimpleWidget)w)->simple.insensitive_border == None)
  ((SimpleWidget)w)->simple.insensitive_border =
      IswCreateStippledPixmap(XtDisplay(w), XtWindow(w),
         w->core.border_pixel,
         w->core.background_pixel,
        w->core.depth);
    /* XSetWindowBorderPixmap → xcb_change_window_attributes */
    uint32_t value = ((SimpleWidget)w)->simple.insensitive_border;
    xcb_change_window_attributes(XtDisplay(w), XtWindow(w),
     XCB_CW_BORDER_PIXMAP, &value);
}
    }
    return False;
}
