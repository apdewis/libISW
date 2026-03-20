/*
 * Copyright (c) 1999 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo C�sar Pereira de Andrade
 */

/*
 * Portions Copyright (c) 2003 David J. Hawkey Jr.
 * Rights, permissions, and disclaimer per the above XFree86 license.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
#include <ISW/TipP.h>
#include <ISW/ISWInit.h>
#include "ISWXcbDraw.h"

#include <stdlib.h>

/* BackingStore resource definitions (stub - limited XCB support) */
#ifndef XtNbackingStore
#define XtNbackingStore "backingStore"
#endif
#ifndef XtCBackingStore
#define XtCBackingStore "BackingStore"
#endif
#ifndef XtRBackingStore
#define XtRBackingStore "BackingStore"
#endif
#ifndef XtEnotUseful
#define XtEnotUseful "notUseful"
#endif
#ifndef XtEwhenMapped
#define XtEwhenMapped "whenMapped"
#endif
#ifndef XtEalways
#define XtEalways "always"
#endif
#ifndef XtEdefault
#define XtEdefault "default"
#endif

#define	TIP_EVENT_MASK (ButtonPressMask	  |	\
			ButtonReleaseMask |	\
			PointerMotionMask |	\
			ButtonMotionMask  |	\
			KeyPressMask	  |	\
			KeyReleaseMask	  |	\
			EnterWindowMask	  |	\
			LeaveWindowMask)

/*
 * Types
 */
typedef struct _WidgetInfo {
    Widget widget;
    String label;
    struct _WidgetInfo *next;
} WidgetInfo;

typedef struct _XawTipInfo {
    xcb_screen_t *screen;
    TipWidget tip;
    Bool mapped;
    WidgetInfo *widgets;
    struct _XawTipInfo *next;
} XawTipInfo;

typedef struct {
    XawTipInfo *info;
    WidgetInfo *winfo;
} TimeoutInfo;

/*
 * Class Methods
 */
static void XawTipClassInitialize(void);
static void XawTipInitialize(Widget, Widget, ArgList, Cardinal *);
static void XawTipDestroy(Widget);
static void XawTipExpose(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void XawTipRealize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static Boolean XawTipSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

/*
 * Prototypes
 */
static void TipEventHandler(Widget, XtPointer, XEvent *, Boolean *);
static void TipShellEventHandler(Widget, XtPointer, XEvent *, Boolean *);
static WidgetInfo *CreateWidgetInfo(Widget);
static WidgetInfo *FindWidgetInfo(XawTipInfo *, Widget);
static XawTipInfo *CreateTipInfo(Widget);
static XawTipInfo *FindTipInfo(Widget);
static void ResetTip(XawTipInfo *, WidgetInfo *, Bool);
static void TipTimeoutCallback(XtPointer, XtIntervalId *);
static void TipLayout(XawTipInfo *);
static void TipPosition(XawTipInfo *);

/*
 * Initialization
 */
#define offset(field) XtOffsetOf(TipRec, tip.field)
static XtResource resources[] = {
  {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
    offset(foreground), XtRString, XtDefaultForeground},
  {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct*),
    offset(font), XtRString, XtDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
  {XtNfontSet, XtCFontSet, XtRFontSet, sizeof(ISWFontSet*),
    offset(fontset), XtRString, XtDefaultFontSet},
#endif
  {XtNlabel, XtCLabel, XtRString, sizeof(String),
    offset(label), XtRString, NULL},
  {XtNencoding, XtCEncoding, XtRUnsignedChar, sizeof(unsigned char),
    offset(encoding), XtRImmediate, (XtPointer)XawTextEncoding8bit},
  {XtNinternalHeight, XtCHeight, XtRDimension, sizeof(Dimension),
    offset(internal_height), XtRImmediate, (XtPointer)2},
  {XtNinternalWidth, XtCWidth, XtRDimension, sizeof(Dimension),
    offset(internal_width), XtRImmediate, (XtPointer)2},
  {XtNbackingStore, XtCBackingStore, XtRBackingStore, sizeof(int),
    offset(backing_store), XtRImmediate,
    (XtPointer)(Always + WhenMapped + NotUseful)},
  {XtNtimeout, XtCTimeout, XtRInt, sizeof(int),
    offset(timeout), XtRImmediate, (XtPointer)500},
};
#undef offset

TipClassRec tipClassRec = {
  /* core */
  {
    (WidgetClass)&widgetClassRec,	/* superclass */
    "Tip",				/* class_name */
    sizeof(TipRec),			/* widget_size */
    XawTipClassInitialize,		/* class_initialize */
    NULL,				/* class_part_initialize */
    False,				/* class_inited */
    XawTipInitialize,			/* initialize */
    NULL,				/* initialize_hook */
    XawTipRealize,			/* realize */
    NULL,				/* actions */
    0,					/* num_actions */
    resources,				/* resources */
    XtNumber(resources),		/* num_resources */
    NULLQUARK,				/* xrm_class */
    True,				/* compress_motion */
    True,				/* compress_exposure */
    True,				/* compress_enterleave */
    False,				/* visible_interest */
    XawTipDestroy,			/* destroy */
    NULL,				/* resize */
    XawTipExpose,			/* expose */
    XawTipSetValues,			/* set_values */
    NULL,				/* set_values_hook */
    XtInheritSetValuesAlmost,		/* set_values_almost */
    NULL,				/* get_values_hook */
    NULL,				/* accept_focus */
    XtVersion,				/* version */
    NULL,				/* callback_private */
    NULL,				/* tm_table */
    XtInheritQueryGeometry,		/* query_geometry */
    XtInheritDisplayAccelerator,	/* display_accelerator */
    NULL,				/* extension */
  },
  /* tip */
  {
    NULL,				/* extension */
  },
};

WidgetClass tipWidgetClass = (WidgetClass)&tipClassRec;

static XawTipInfo *TipInfoList = NULL;
static TimeoutInfo TimeoutData;

/*
 * Implementation
 */

/*
 * XmuCvtStringToBackingStore() stub - XCB has limited BackingStore support
 * Always returns NotUseful since XCB backing store is not well supported
 */
/*ARGSUSED*/
static Boolean
XmuCvtStringToBackingStore(xcb_connection_t *dpy, XrmValuePtr args, Cardinal *num_args,
                           XrmValuePtr fromVal, XrmValuePtr toVal, XtPointer *data)
{
  static int backingStore = NotUseful;
  
  /* Stub: Always return NotUseful - XCB has limited backing store support */
  if (toVal->addr != NULL)
  {
    if (toVal->size < sizeof(int))
    {
      toVal->size = sizeof(int);
      return False;
    }
    *(int *)(toVal->addr) = NotUseful;
  }
  else
    toVal->addr = (XtPointer)&backingStore;
  
  toVal->size = sizeof(int);
  return True;
}

/*
 * XmuCvtBackingStoreToString() from XFree86's distribution, because
 * X.Org's distribution doesn't have it.
 */

/*ARGSUSED*/
static Boolean
XawCvtBackingStoreToString(xcb_connection_t *dpy, XrmValuePtr args, Cardinal *num_args,
                           XrmValuePtr fromVal, XrmValuePtr toVal, XtPointer *data)
{
  static String buffer;
  Cardinal size;

  switch (*(int *)fromVal->addr)
  {
    case NotUseful:
      buffer = XtEnotUseful;
      break;
    case WhenMapped:
      buffer = XtEwhenMapped;
      break;
    case Always:
      buffer = XtEalways;
      break;
    case (Always + WhenMapped + NotUseful):
      buffer = XtEdefault;
      break;
    default:
      XtWarning("Cannot convert BackingStore to String");
      toVal->addr = NULL;
      toVal->size = 0;
      return (False);
  }

  size = strlen(buffer) + 1;
  if (toVal->addr != NULL)
  {
      if (toVal->size < size)
      {
	  toVal->size = size;
	  return (False);
      }
      strcpy((char *)toVal->addr, buffer);
  }
  else
    toVal->addr = (XtPointer)buffer;
  toVal->size = sizeof(String);

  return (True);
}

static void
XawTipClassInitialize(void)
{
    XawInitializeWidgetSet();
    /* BackingStore converters - XCB has limited BackingStore support, but keep for compatibility */
    XtSetTypeConverter(XtRString, XtRBackingStore, XmuCvtStringToBackingStore,
		       NULL, 0, XtCacheNone, NULL);
    XtSetTypeConverter(XtRBackingStore, XtRString, XawCvtBackingStoreToString,
		       NULL, 0, XtCacheNone, NULL);
}

/*ARGSUSED*/
static void
XawTipInitialize(Widget req, Widget w, ArgList args, Cardinal *num_args)
{
    TipWidget tip = (TipWidget)w;
    xcb_create_gc_value_list_t values;

    /* XCB Fix: XtRFontStruct converter may fail in XCB mode, leaving font NULL.
     * If font is NULL but fontset is available, create a minimal XFontStruct
     * using the fontset's font_id (similar to Label.c approach). */
    if (tip->tip.font == NULL) {
#ifdef ISW_INTERNATIONALIZATION
	if (tip->tip.fontset != NULL) {
	    /* Allocate and initialize a minimal XFontStruct from fontset */
	    tip->tip.font = (XFontStruct *)XtMalloc(sizeof(XFontStruct));
	    memset(tip->tip.font, 0, sizeof(XFontStruct));
	    tip->tip.font->fid = tip->tip.fontset->font_id;
	    tip->tip.font->min_char_or_byte2 = 0;
	    tip->tip.font->max_char_or_byte2 = 255;
	} else
#endif
	{
	    XtAppWarning(XtWidgetToApplicationContext(w),
			 "Tip widget: font and fontset are both NULL - text rendering will fail");
	}
    }

    tip->tip.timer = 0;

    values.foreground = tip->tip.foreground;
    values.background = tip->core.background_pixel;
    values.graphics_exposures = 0;

    /* XCB Fix: Add NULL check for font before accessing fid */
    XtGCMask gc_mask = GCForeground | GCBackground | GCGraphicsExposures;
    if (tip->tip.font != NULL) {
	values.font = tip->tip.font->fid;
	gc_mask |= GCFont;
    }

    tip->tip.gc = XtAllocateGC(w, 0, gc_mask & ~GCFont, &values, GCFont, 0);
}

static void
XawTipDestroy(Widget w)
{
    XawTipInfo *info = FindTipInfo(w);
    WidgetInfo *winfo;
    TipWidget tip = (TipWidget)w;

    if (tip->tip.timer)
	XtRemoveTimeOut(tip->tip.timer);

    XtReleaseGC(w, tip->tip.gc);

    XtRemoveEventHandler(XtParent(w), KeyPressMask, False,
			 TipShellEventHandler, (XtPointer)NULL);

    while (info->widgets) {
	winfo = info->widgets->next;
	XtFree((char *)info->widgets->label);
	XtFree((char *)info->widgets);
	info->widgets = winfo;
    }

    if (info == TipInfoList)
	TipInfoList = TipInfoList->next;
    else {
	XawTipInfo *p = TipInfoList;

	while (p && p->next != info)
	    p = p->next;
	if (p)
	    p->next = info->next;
    }

    XtFree((char *)info);
}

static void
XawTipRealize(xcb_connection_t *conn, Widget w, XtValueMask *mask, uint32_t *values)
{
    TipWidget tip = (TipWidget)w;
    xcb_screen_t *screen = XtScreen(w);
    xcb_window_t window;
    uint32_t value_mask = 0;
    uint32_t value_list[32];
    int value_idx = 0;

    /* BackingStore is X11 feature with limited XCB support - stub it out for now */
    /* Modern compositors typically don't use BackingStore anyway */

    /* Override redirect (required for tooltips to bypass window manager) */
    value_mask |= XCB_CW_OVERRIDE_REDIRECT;
    value_list[value_idx++] = 1;  /* True */

    /* Create window using XCB */
    window = xcb_generate_id(conn);
    
    xcb_create_window(
        conn,
        screen->root_depth,                 /* depth */
        window,                             /* window ID */
        screen->root,                       /* parent */
        XtX(w), XtY(w),                    /* x, y */
        XtWidth(w) ? XtWidth(w) : 1,       /* width */
        XtHeight(w) ? XtHeight(w) : 1,     /* height */
        XtBorderWidth(w),                  /* border_width */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,     /* class */
        screen->root_visual,               /* visual */
        value_mask,                        /* value_mask */
        value_list                         /* value_list */
    );
    
    XtWindow(w) = window;
}

static void
XawTipExpose(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    TipWidget tip = (TipWidget)w;
    xcb_gcontext_t gc = tip->tip.gc;
    char *nl, *label = tip->tip.label;
    /* XCB Fix: Add NULL check for font before accessing ascent */
    Position y = tip->tip.internal_height + (tip->tip.font ? tip->tip.font->ascent : 11);
    int len;

#ifdef ISW_INTERNATIONALIZATION
    if (tip->tip.international == True) {
	Position ksy = tip->tip.internal_height;

	ksy += tip->tip.fontset->ascent;

	while ((nl = index(label, '\n')) != NULL) {
	    XawDrawString(XtDisplay(w), XtWindow(w), tip->tip.fontset,
			  gc, tip->tip.internal_width, ksy, label,
			  (int)(nl - label));
	    ksy += tip->tip.fontset->height;
	    label = nl + 1;
	}
	len = strlen(label);
	if (len)
	    XawDrawString(XtDisplay(w), XtWindow(w), tip->tip.fontset, gc,
			  tip->tip.internal_width, ksy, label, len);
    }
    else
#endif
    {
	while ((nl = index(label, '\n')) != NULL) {
	    if (tip->tip.encoding)
		XDrawString16(XtDisplay(w), XtWindow(w), gc,
			      tip->tip.internal_width, y,
			      (const xcb_char2b_t*)label, (int)(nl - label) >> 1);
		   else
		XDrawString(XtDisplay(w), XtWindow(w), gc,
			    tip->tip.internal_width, y,
			    label, (int)(nl - label));
	    /* XCB Fix: Add NULL check for font before accessing fields */
	    if (tip->tip.font != NULL) {
		y += tip->tip.font->ascent + tip->tip.font->descent;
	    } else {
		y += 14;  /* Default line height */
	    }
	    label = nl + 1;
	}
	len = strlen(label);
	if (len) {
		   if (tip->tip.encoding)
		XDrawString16(XtDisplay(w), XtWindow(w), gc,
			      tip->tip.internal_width, y,
			      (const xcb_char2b_t*)label, len >> 1);
		   else
		XDrawString(XtDisplay(w), XtWindow(w), gc,
			    tip->tip.internal_width, y, label, len);
	}
    }
}

/*ARGSUSED*/
static Boolean
XawTipSetValues(Widget current, Widget request, Widget cnew, ArgList args, Cardinal *num_args)
{
    TipWidget curtip = (TipWidget)current;
    TipWidget newtip = (TipWidget)cnew;
    Boolean redisplay = False;

    /* XCB Fix: Add NULL checks before comparing font->fid */
    Bool font_changed = False;
    if (curtip->tip.font != NULL && newtip->tip.font != NULL) {
	font_changed = (curtip->tip.font->fid != newtip->tip.font->fid);
    } else if (curtip->tip.font != newtip->tip.font) {
	/* One is NULL and the other isn't */
	font_changed = True;
    }

    if (font_changed || curtip->tip.foreground != newtip->tip.foreground) {
	xcb_create_gc_value_list_t values;

	values.foreground = newtip->tip.foreground;
	values.background = newtip->core.background_pixel;
	values.graphics_exposures = 0;

	/* XCB Fix: Add NULL check for font before accessing fid */
	XtGCMask gc_mask = GCForeground | GCBackground | GCGraphicsExposures;
	if (newtip->tip.font != NULL) {
	    values.font = newtip->tip.font->fid;
	    gc_mask |= GCFont;
	}

	XtReleaseGC(cnew, curtip->tip.gc);
	newtip->tip.gc = XtAllocateGC(cnew, 0, gc_mask & ~GCFont, &values,
				      GCFont, 0);
	redisplay = True;
    }

    return (redisplay);
}

static void
TipLayout(XawTipInfo *info)
{
    XFontStruct	*fs = info->tip->tip.font;
    int width = 0, height;
    char *nl, *label = info->tip->tip.label;

#ifdef ISW_INTERNATIONALIZATION
    if (info->tip->tip.international == True) {
	ISWFontSet *fset = info->tip->tip.fontset;

	height = fset->height;
	if ((nl = index(label, '\n')) != NULL) {
	    /*CONSTCOND*/
	    while (True) {
		int w = XawTextWidth(fset, label, (int)(nl - label));

		if (w > width)
		    width = w;
		if (*nl == '\0')
		    break;
		label = nl + 1;
		if (*label)
		    height += fset->height;
		if ((nl = index(label, '\n')) == NULL)
		    nl = index(label, '\0');
	    }
	}
	else
	    width = XawTextWidth(fset, label, strlen(label));
    }
    else
#endif
    {
 /* XCB Fix: Add NULL check for font before accessing fields */
 if (fs != NULL) {
     height = fs->ascent + fs->descent;
     if ((nl = index(label, '\n')) != NULL) {
  /*CONSTCOND*/
  while (True) {
      int w = info->tip->tip.encoding ?
   XTextWidth16(fs, (const xcb_char2b_t*)label,
    (int)(nl - label) >> 1) :
   XTextWidth(fs, label, (int)(nl - label));

      if (w > width)
   width = w;
      if (*nl == '\0')
   break;
      label = nl + 1;
      if (*label)
   height += fs->ascent + fs->descent;
	 if ((nl = index(label, '\n')) == NULL)
	     nl = index(label, '\0');
	    }
	}
	else
	    width = info->tip->tip.encoding ?
	 XTextWidth16(fs, (const xcb_char2b_t*)label, strlen(label) >> 1) :
	 XTextWidth(fs, label, strlen(label));
	} else {
	    /* No font available - use defaults */
	    height = 14;  /* Default height */
	    width = label ? strlen(label) * 8 : 0;  /* Default width */
	}
	   }
    XtWidth(info->tip) = width + info->tip->tip.internal_width * 2;
    XtHeight(info->tip) = height + info->tip->tip.internal_height * 2;
}

#define	DEFAULT_TIP_OFFSET	12

static void
TipPosition(XawTipInfo *info)
{
    xcb_window_t r, c;
    int rx, ry, wx, wy;
    unsigned mask;
    Position x, y;
    int bw2 = XtBorderWidth(info->tip) * 2;
    int scr_width = WidthOfScreen(XtScreen(info->tip));
    int scr_height = HeightOfScreen(XtScreen(info->tip));
    int win_width = XtWidth(info->tip) + bw2;
    int win_height = XtHeight(info->tip) + bw2;

    XQueryPointer(XtDisplay((Widget)info->tip), XtWindow((Widget)info->tip),
		  &r, &c, &rx, &ry, &wx, &wy, &mask);
    x = rx + DEFAULT_TIP_OFFSET;
    y = ry + DEFAULT_TIP_OFFSET;

    if (x + win_width > scr_width)
	x = scr_width - win_width;
    if (x < 0)
	x = 0;

    if (y + win_height > scr_height)
	y -= win_height + (DEFAULT_TIP_OFFSET << 1);
    if (y < 0)
	y = 0;

    XMoveResizeWindow(XtDisplay(info->tip), XtWindow(info->tip),
		      (int)(XtX(info->tip) = x), (int)(XtY(info->tip) = y),
		      (unsigned)XtWidth(info->tip),
		      (unsigned)XtHeight(info->tip));
}

static WidgetInfo *
CreateWidgetInfo(Widget w)
{
    WidgetInfo *winfo = XtNew(WidgetInfo);

    winfo->widget = w;
    winfo->label = NULL;
    winfo->next = NULL;

    return (winfo);
}

static WidgetInfo *
FindWidgetInfo(XawTipInfo *info, Widget w)
{
    WidgetInfo *winfo, *wlist = info->widgets;

    if (wlist == NULL)
	return (info->widgets = CreateWidgetInfo(w));

    for (winfo = wlist; wlist; winfo = wlist, wlist = wlist->next)
	if (wlist->widget == w)
	    return (wlist);

    return (winfo->next = CreateWidgetInfo(w));
}

static XawTipInfo *
CreateTipInfo(Widget w)
{
    XawTipInfo *info = XtNew(XawTipInfo);
    Widget shell = w;

    while (XtParent(shell))
	shell = XtParent(shell);

    info->tip = (TipWidget)XtCreateWidget("tip", tipWidgetClass,
					  shell, NULL, 0);
    XtRealizeWidget((Widget)info->tip);
    info->screen = XtScreen(w);
    info->mapped = False;
    info->widgets = NULL;
    info->next = NULL;
    XtAddEventHandler(shell, KeyPressMask, False, TipShellEventHandler,
		      (XtPointer)NULL);

    return (info);
}

static XawTipInfo *
FindTipInfo(Widget w)
{
    XawTipInfo *info, *list = TipInfoList;
    xcb_screen_t *screen;

    if (list == NULL)
	return (TipInfoList = CreateTipInfo(w));

    screen = XtScreen(w);
    for (info = list; list; info = list, list = list->next)
	if (list->screen == screen)
	    return (list);

    return (info->next = CreateTipInfo(w));
}

static void
ResetTip(XawTipInfo *info, WidgetInfo *winfo, Bool add_timeout)
{
    if (info->tip->tip.timer) {
	XtRemoveTimeOut(info->tip->tip.timer);
	info->tip->tip.timer = 0;
    }
    if (info->mapped) {
	XtRemoveGrab(XtParent((Widget)info->tip));
	XUnmapWindow(XtDisplay((Widget)info->tip), XtWindow((Widget)info->tip));
	info->mapped = False;
    }
    if (add_timeout) {
	TimeoutData.info = info;
	TimeoutData.winfo = winfo;
	info->tip->tip.timer =
	    XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)info->tip),
			    info->tip->tip.timeout, TipTimeoutCallback,
			    (XtPointer)&TimeoutData);
    }
}

static void
TipTimeoutCallback(XtPointer closure, XtIntervalId *id)
{
    TimeoutInfo *cinfo = (TimeoutInfo *)closure;
    XawTipInfo *info = cinfo->info;
    WidgetInfo *winfo = cinfo->winfo;
    Arg args[2];

    info->tip->tip.label = winfo->label;
    info->tip->tip.encoding = 0;
    XtSetArg(args[0], XtNencoding, &info->tip->tip.encoding);
#ifdef ISW_INTERNATIONALIZATION
    info->tip->tip.international = False;
    XtSetArg(args[1], XtNinternational, &info->tip->tip.international);
    XtGetValues(winfo->widget, args, 2);
#else
    XtGetValues(winfo->widget, args, 1);
#endif

    TipLayout(info);
    TipPosition(info);
    XMapRaised(XtDisplay((Widget)info->tip), XtWindow((Widget)info->tip));
    XtAddGrab(XtParent((Widget)info->tip), True, True);
    info->mapped = True;
}

/*ARGSUSED*/
static void
TipShellEventHandler(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch)
{
    XawTipInfo *info = FindTipInfo(w);

    ResetTip(info, FindWidgetInfo(info, w), False);
}

/*ARGSUSED*/
static void
TipEventHandler(Widget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch)
{
    XawTipInfo *info = FindTipInfo(w);
    Boolean add_timeout;

    switch (event->response_type & ~0x80) {
 case XCB_ENTER_NOTIFY:
     add_timeout = True;
     break;
 case XCB_MOTION_NOTIFY:
     /* If any button is pressed, timer is 0 */
     if (info->mapped)
  return;
     add_timeout = info->tip->tip.timer != 0;
     break;
 default:
     add_timeout = False;
     break;
    }
    ResetTip(info, FindWidgetInfo(info, w), add_timeout);
}

/*
 * Public routines
 */
void
XawTipEnable(Widget w, String label)
{
    if (XtIsWidget(w) && label && *label) {
	XawTipInfo *info = FindTipInfo(w);
	WidgetInfo *winfo = FindWidgetInfo(info, w);

	if (winfo->label)
	    XtFree((char *)winfo->label);
	winfo->label = XtNewString(label);

	XtAddEventHandler(w, TIP_EVENT_MASK, False, TipEventHandler,
			  (XtPointer)NULL);
    }
}

void
XawTipDisable(Widget w)
{
    if (XtIsWidget(w)) {
	XawTipInfo *info = FindTipInfo(w);

	XtRemoveEventHandler(w, TIP_EVENT_MASK, False, TipEventHandler,
			     (XtPointer)NULL);
	ResetTip(info, FindWidgetInfo(info, w), False);
    }
}
