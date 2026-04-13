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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <X11/Xos.h>
#include <ISW/TipP.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"

#include <stdlib.h>

extern double _IswGetScaleFactor(xcb_connection_t *dpy);

/* BackingStore resource definitions (stub - limited XCB support) */
#ifndef IswNbackingStore
#define IswNbackingStore "backingStore"
#endif
#ifndef IswCBackingStore
#define IswCBackingStore "BackingStore"
#endif
#ifndef IswRBackingStore
#define IswRBackingStore "BackingStore"
#endif
#ifndef IswEnotUseful
#define IswEnotUseful "notUseful"
#endif
#ifndef IswEwhenMapped
#define IswEwhenMapped "whenMapped"
#endif
#ifndef IswEalways
#define IswEalways "always"
#endif
#ifndef IswEdefault
#define IswEdefault "default"
#endif

#define	TIP_EVENT_MASK (XCB_EVENT_MASK_BUTTON_PRESS	  |	\
			XCB_EVENT_MASK_BUTTON_RELEASE |	\
			XCB_EVENT_MASK_POINTER_MOTION |	\
			XCB_EVENT_MASK_BUTTON_MOTION  |	\
			XCB_EVENT_MASK_KEY_PRESS	  |	\
			XCB_EVENT_MASK_KEY_RELEASE	  |	\
			XCB_EVENT_MASK_ENTER_WINDOW	  |	\
			XCB_EVENT_MASK_LEAVE_WINDOW)

/*
 * Types
 */
typedef struct _WidgetInfo {
    Widget widget;
    String label;
    struct _WidgetInfo *next;
} WidgetInfo;

typedef struct _IswTipInfo {
    xcb_screen_t *screen;
    TipWidget tip;
    Bool mapped;
    WidgetInfo *widgets;
    struct _IswTipInfo *next;
} IswTipInfo;

typedef struct {
    IswTipInfo *info;
    WidgetInfo *winfo;
} TimeoutInfo;

/*
 * Class Methods
 */
static void IswTipClassInitialize(void);
static void IswTipInitialize(Widget, Widget, ArgList, Cardinal *);
static void IswTipDestroy(Widget);
static void IswTipExpose(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void IswTipRealize(xcb_connection_t *, Widget, IswValueMask *, uint32_t *);
static Boolean IswTipSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

/*
 * Prototypes
 */
static void TipEventHandler(Widget, IswPointer, xcb_generic_event_t *, Boolean *);
static void TipShellEventHandler(Widget, IswPointer, xcb_generic_event_t *, Boolean *);
static WidgetInfo *CreateWidgetInfo(Widget);
static WidgetInfo *FindWidgetInfo(IswTipInfo *, Widget);
static IswTipInfo *CreateTipInfo(Widget);
static IswTipInfo *FindTipInfo(Widget);
static void ResetTip(IswTipInfo *, WidgetInfo *, Bool);
static void TipTimeoutCallback(IswPointer, IswIntervalId *);
static void TipLayout(IswTipInfo *);
static void TipPosition(IswTipInfo *);

/*
 * Initialization
 */
#define offset(field) IswOffsetOf(TipRec, tip.field)
static IswResource resources[] = {
  {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
    offset(foreground), IswRString, IswDefaultForeground},
  {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct*),
    offset(font), IswRString, IswDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
  {IswNfontSet, IswCFontSet, IswRFontSet, sizeof(ISWFontSet*),
    offset(fontset), IswRString, IswDefaultFontSet},
#endif
  {IswNlabel, IswCLabel, IswRString, sizeof(String),
    offset(label), IswRString, NULL},
  {IswNencoding, IswCEncoding, IswRUnsignedChar, sizeof(unsigned char),
    offset(encoding), IswRImmediate, (IswPointer)IswTextEncoding8bit},
  {IswNinternalHeight, IswCHeight, IswRDimension, sizeof(Dimension),
    offset(internal_height), IswRImmediate, (IswPointer)2},
  {IswNinternalWidth, IswCWidth, IswRDimension, sizeof(Dimension),
    offset(internal_width), IswRImmediate, (IswPointer)2},
  {IswNbackingStore, IswCBackingStore, IswRBackingStore, sizeof(int),
    offset(backing_store), IswRImmediate,
    (IswPointer)(XCB_BACKING_STORE_ALWAYS + XCB_BACKING_STORE_WHEN_MAPPED + XCB_BACKING_STORE_NOT_USEFUL)},
  {IswNtimeout, IswCTimeout, IswRInt, sizeof(int),
    offset(timeout), IswRImmediate, (IswPointer)500},
};
#undef offset

TipClassRec tipClassRec = {
  /* core */
  {
    (WidgetClass)&widgetClassRec,	/* superclass */
    "Tip",				/* class_name */
    sizeof(TipRec),			/* widget_size */
    IswTipClassInitialize,		/* class_initialize */
    NULL,				/* class_part_initialize */
    False,				/* class_inited */
    IswTipInitialize,			/* initialize */
    NULL,				/* initialize_hook */
    IswTipRealize,			/* realize */
    NULL,				/* actions */
    0,					/* num_actions */
    resources,				/* resources */
    IswNumber(resources),		/* num_resources */
    NULLQUARK,				/* xrm_class */
    True,				/* compress_motion */
    True,				/* compress_exposure */
    True,				/* compress_enterleave */
    False,				/* visible_interest */
    IswTipDestroy,			/* destroy */
    NULL,				/* resize */
    IswTipExpose,			/* expose */
    IswTipSetValues,			/* set_values */
    NULL,				/* set_values_hook */
    IswInheritSetValuesAlmost,		/* set_values_almost */
    NULL,				/* get_values_hook */
    NULL,				/* accept_focus */
    IswVersion,				/* version */
    NULL,				/* callback_private */
    NULL,				/* tm_table */
    IswInheritQueryGeometry,		/* query_geometry */
    IswInheritDisplayAccelerator,	/* display_accelerator */
    NULL,				/* extension */
  },
  /* tip */
  {
    NULL,				/* extension */
  },
};

WidgetClass tipWidgetClass = (WidgetClass)&tipClassRec;

static IswTipInfo *TipInfoList = NULL;
static TimeoutInfo TimeoutData;

/*
 * Implementation
 */

/*
 * XmuCvtStringToBackingStore() stub - XCB has limited BackingStore support
 * Always returns XCB_BACKING_STORE_NOT_USEFUL since XCB backing store is not well supported
 */
/*ARGSUSED*/
static Boolean
XmuCvtStringToBackingStore(xcb_connection_t *dpy, XrmValuePtr args, Cardinal *num_args,
                           XrmValuePtr fromVal, XrmValuePtr toVal, IswPointer *data)
{
  static int backingStore = XCB_BACKING_STORE_NOT_USEFUL;
  
  /* Stub: Always return XCB_BACKING_STORE_NOT_USEFUL - XCB has limited backing store support */
  if (toVal->addr != NULL)
  {
    if (toVal->size < sizeof(int))
    {
      toVal->size = sizeof(int);
      return False;
    }
    *(int *)(toVal->addr) = XCB_BACKING_STORE_NOT_USEFUL;
  }
  else
    toVal->addr = (IswPointer)&backingStore;
  
  toVal->size = sizeof(int);
  return True;
}

/*
 * XmuCvtBackingStoreToString() from XFree86's distribution, because
 * X.Org's distribution doesn't have it.
 */

/*ARGSUSED*/
static Boolean
IswCvtBackingStoreToString(xcb_connection_t *dpy, XrmValuePtr args, Cardinal *num_args,
                           XrmValuePtr fromVal, XrmValuePtr toVal, IswPointer *data)
{
  static String buffer;
  Cardinal size;

  switch (*(int *)fromVal->addr)
  {
    case XCB_BACKING_STORE_NOT_USEFUL:
      buffer = IswEnotUseful;
      break;
    case XCB_BACKING_STORE_WHEN_MAPPED:
      buffer = IswEwhenMapped;
      break;
    case XCB_BACKING_STORE_ALWAYS:
      buffer = IswEalways;
      break;
    case (XCB_BACKING_STORE_ALWAYS + XCB_BACKING_STORE_WHEN_MAPPED + XCB_BACKING_STORE_NOT_USEFUL):
      buffer = IswEdefault;
      break;
    default:
      IswWarning("Cannot convert BackingStore to String");
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
    toVal->addr = (IswPointer)buffer;
  toVal->size = sizeof(String);

  return (True);
}

static void
IswTipClassInitialize(void)
{
    IswInitializeWidgetSet();
    /* BackingStore converters - XCB has limited BackingStore support, but keep for compatibility */
    IswSetTypeConverter(IswRString, IswRBackingStore, XmuCvtStringToBackingStore,
		       NULL, 0, IswCacheNone, NULL);
    IswSetTypeConverter(IswRBackingStore, IswRString, IswCvtBackingStoreToString,
		       NULL, 0, IswCacheNone, NULL);
}

/*ARGSUSED*/
static void
IswTipInitialize(Widget req, Widget w, ArgList args, Cardinal *num_args)
{
    TipWidget tip = (TipWidget)w;

    /* HiDPI: scale dimension resources */
    tip->tip.internal_width = (tip->tip.internal_width);
    tip->tip.internal_height = (tip->tip.internal_height);

    /* XCB Fix: IswRFontStruct converter may fail in XCB mode, leaving font NULL.
     * If font is NULL but fontset is available, create a minimal IswFontStruct
     * using the fontset's font_id (similar to Label.c approach). */
    if (tip->tip.font == NULL) {
#ifdef ISW_INTERNATIONALIZATION
	if (tip->tip.fontset != NULL) {
	    /* Allocate and initialize a minimal IswFontStruct from fontset */
	    tip->tip.font = (IswFontStruct *)IswMalloc(sizeof(IswFontStruct));
	    memset(tip->tip.font, 0, sizeof(IswFontStruct));
	    tip->tip.font->fid = tip->tip.fontset->font_id;
	    tip->tip.font->ascent = tip->tip.fontset->ascent;
	    tip->tip.font->descent = tip->tip.fontset->descent;
	    tip->tip.font->min_char_or_byte2 = 0;
	    tip->tip.font->max_char_or_byte2 = 255;
	} else
#endif
	{
	    IswAppWarning(IswWidgetToApplicationContext(w),
			 "Tip widget: font and fontset are both NULL - text rendering will fail");
	}
    }

    tip->tip.timer = 0;
    tip->tip.render_ctx = NULL;
}

static void
IswTipDestroy(Widget w)
{
    IswTipInfo *info = FindTipInfo(w);
    WidgetInfo *winfo;
    TipWidget tip = (TipWidget)w;

    if (tip->tip.timer)
	IswRemoveTimeOut(tip->tip.timer);

    if (tip->tip.render_ctx) {
	ISWRenderDestroy(tip->tip.render_ctx);
	tip->tip.render_ctx = NULL;
    }

    IswRemoveEventHandler(IswParent(w), XCB_EVENT_MASK_KEY_PRESS, False,
			 TipShellEventHandler, (IswPointer)NULL);

    while (info->widgets) {
	winfo = info->widgets->next;
	IswFree((char *)info->widgets->label);
	IswFree((char *)info->widgets);
	info->widgets = winfo;
    }

    if (info == TipInfoList)
	TipInfoList = TipInfoList->next;
    else {
	IswTipInfo *p = TipInfoList;

	while (p && p->next != info)
	    p = p->next;
	if (p)
	    p->next = info->next;
    }

    IswFree((char *)info);
}

static void
IswTipRealize(xcb_connection_t *conn, Widget w, IswValueMask *mask, uint32_t *values)
{
    xcb_screen_t *screen = IswScreen(w);
    xcb_window_t window;
    uint32_t value_mask = 0;
    uint32_t value_list[32];
    int value_idx = 0;

    TipWidget tip = (TipWidget)w;

    /* CW values must be in enum order */
    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    value_list[value_idx++] = tip->core.background_pixel;  /* back_pixel */
    value_list[value_idx++] = 1;                           /* override_redirect */
    value_list[value_idx++] = XCB_EVENT_MASK_EXPOSURE;     /* event_mask */

    window = xcb_generate_id(conn);

    /* HiDPI: create window at physical pixel geometry */
    {
        double _sf = _IswGetScaleFactor(conn);
        xcb_create_window(
            conn,
            screen->root_depth,
            window,
            screen->root,
            (int16_t)(IswX(w) * _sf + 0.5),
            (int16_t)(IswY(w) * _sf + 0.5),
            (uint16_t)((IswWidth(w) ? IswWidth(w) : 1) * _sf + 0.5),
            (uint16_t)((IswHeight(w) ? IswHeight(w) : 1) * _sf + 0.5),
            (uint16_t)(IswBorderWidth(w) * _sf + 0.5),
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            screen->root_visual,
            value_mask,
            value_list
        );
    }
    
    IswWindow(w) = window;
}

static void
IswTipExpose(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    TipWidget tip = (TipWidget)w;
    char *nl, *label = tip->tip.label;
    int len, line_height;
    Position y;
    ISWRenderContext *ctx;

    if (!label || !*label)
	return;

    ctx = tip->tip.render_ctx;
    if (!ctx && w->core.width > 0 && w->core.height > 0) {
	ctx = tip->tip.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    }
    if (!ctx)
	return;

    ISWRenderBegin(ctx);

    /* Fill background */
    ISWRenderSetColor(ctx, tip->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);

    /* Draw text */
    ISWRenderSetFont(ctx, tip->tip.font);
    ISWRenderSetColor(ctx, tip->tip.foreground);

    line_height = ISWScaledFontHeight(w, tip->tip.font);
    y = tip->tip.internal_height + ISWScaledFontAscent(w, tip->tip.font);

    while ((nl = index(label, '\n')) != NULL) {
	ISWRenderDrawString(ctx, label, (int)(nl - label),
			    tip->tip.internal_width, y);
	y += line_height;
	label = nl + 1;
    }
    len = strlen(label);
    if (len)
	ISWRenderDrawString(ctx, label, len,
			    tip->tip.internal_width, y);

    ISWRenderEnd(ctx);
}

/*ARGSUSED*/
static Boolean
IswTipSetValues(Widget current, Widget request, Widget cnew, ArgList args, Cardinal *num_args)
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

    if (font_changed || curtip->tip.foreground != newtip->tip.foreground)
	redisplay = True;

    return (redisplay);
}

static void
TipLayout(IswTipInfo *info)
{
    IswFontStruct	*fs = info->tip->tip.font;
    Widget w = (Widget)info->tip;
    int width = 0, height;
    char *nl, *label = info->tip->tip.label;

    if (!label || !*label) {
	IswWidth(info->tip) = 1;
	IswHeight(info->tip) = 1;
	return;
    }

    height = ISWScaledFontHeight(w, fs);

    if ((nl = index(label, '\n')) != NULL) {
	/*CONSTCOND*/
	while (True) {
	    int lw = ISWScaledTextWidth(w, fs, label, (int)(nl - label));

	    if (lw > width)
		width = lw;
	    if (*nl == '\0')
		break;
	    label = nl + 1;
	    if (*label)
		height += ISWScaledFontHeight(w, fs);
	    if ((nl = index(label, '\n')) == NULL)
		nl = index(label, '\0');
	}
    }
    else
	width = ISWScaledTextWidth(w, fs, label, strlen(label));

    IswWidth(info->tip) = width + info->tip->tip.internal_width * 2;
    IswHeight(info->tip) = height + info->tip->tip.internal_height * 2;
}

#define	DEFAULT_TIP_OFFSET	12

static void
TipPosition(IswTipInfo *info)
{
    xcb_window_t r, c;
    int rx, ry, wx, wy;
    unsigned mask;
    Position x, y;
    int bw2 = IswBorderWidth(info->tip) * 2;
    int scr_width = WidthOfScreen(IswScreen(info->tip));
    int scr_height = HeightOfScreen(IswScreen(info->tip));
    int win_width = IswWidth(info->tip) + bw2;
    int win_height = IswHeight(info->tip) + bw2;

    XQueryPointer(IswDisplay((Widget)info->tip),
		  IswScreen(info->tip)->root,
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

    {
        double _sf = _IswGetScaleFactor(IswDisplay(info->tip));
        IswX(info->tip) = x;
        IswY(info->tip) = y;
        uint32_t mv[4];
        mv[0] = (uint32_t)(int32_t)(x * _sf + 0.5);
        mv[1] = (uint32_t)(int32_t)(y * _sf + 0.5);
        mv[2] = (uint32_t)(IswWidth(info->tip) * _sf + 0.5);
        mv[3] = (uint32_t)(IswHeight(info->tip) * _sf + 0.5);
        xcb_configure_window(IswDisplay(info->tip), IswWindow(info->tip),
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, mv);
    }
}

static WidgetInfo *
CreateWidgetInfo(Widget w)
{
    WidgetInfo *winfo = IswNew(WidgetInfo);

    winfo->widget = w;
    winfo->label = NULL;
    winfo->next = NULL;

    return (winfo);
}

static WidgetInfo *
FindWidgetInfo(IswTipInfo *info, Widget w)
{
    WidgetInfo *winfo, *wlist = info->widgets;

    if (wlist == NULL)
	return (info->widgets = CreateWidgetInfo(w));

    for (winfo = wlist; wlist; winfo = wlist, wlist = wlist->next)
	if (wlist->widget == w)
	    return (wlist);

    return (winfo->next = CreateWidgetInfo(w));
}

static IswTipInfo *
CreateTipInfo(Widget w)
{
    IswTipInfo *info = IswNew(IswTipInfo);
    Widget shell = w;

    while (IswParent(shell))
	shell = IswParent(shell);

    info->tip = (TipWidget)IswCreateWidget("tip", tipWidgetClass,
					  shell, NULL, 0);
    IswRealizeWidget((Widget)info->tip);
    info->screen = IswScreen(w);
    info->mapped = False;
    info->widgets = NULL;
    info->next = NULL;
    IswAddEventHandler(shell, XCB_EVENT_MASK_KEY_PRESS, False, TipShellEventHandler,
		      (IswPointer)NULL);

    return (info);
}

static IswTipInfo *
FindTipInfo(Widget w)
{
    IswTipInfo *info, *list = TipInfoList;
    xcb_screen_t *screen;

    if (list == NULL)
	return (TipInfoList = CreateTipInfo(w));

    screen = IswScreen(w);
    for (info = list; list; info = list, list = list->next)
	if (list->screen == screen)
	    return (list);

    return (info->next = CreateTipInfo(w));
}

static void
ResetTip(IswTipInfo *info, WidgetInfo *winfo, Bool add_timeout)
{
    if (info->tip->tip.timer) {
	IswRemoveTimeOut(info->tip->tip.timer);
	info->tip->tip.timer = 0;
    }
    if (info->mapped) {
	IswRemoveGrab(IswParent((Widget)info->tip));
	xcb_unmap_window(IswDisplay((Widget)info->tip), IswWindow((Widget)info->tip));
	xcb_flush(IswDisplay((Widget)info->tip));
	info->mapped = False;
    }
    if (add_timeout) {
	TimeoutData.info = info;
	TimeoutData.winfo = winfo;
	info->tip->tip.timer =
	    IswAppAddTimeOut(IswWidgetToApplicationContext((Widget)info->tip),
			    info->tip->tip.timeout, TipTimeoutCallback,
			    (IswPointer)&TimeoutData);
    }
}

static void
TipTimeoutCallback(IswPointer closure, IswIntervalId *id)
{
    TimeoutInfo *cinfo = (TimeoutInfo *)closure;
    IswTipInfo *info = cinfo->info;
    WidgetInfo *winfo = cinfo->winfo;
    Arg args[2];

    info->tip->tip.label = winfo->label;
    info->tip->tip.encoding = 0;
    IswSetArg(args[0], IswNencoding, &info->tip->tip.encoding);
#ifdef ISW_INTERNATIONALIZATION
    info->tip->tip.international = False;
    IswSetArg(args[1], IswNinternational, &info->tip->tip.international);
    IswGetValues(winfo->widget, args, 2);
#else
    IswGetValues(winfo->widget, args, 1);
#endif

    TipLayout(info);
    TipPosition(info);

    /* Invalidate render context — tooltip size changes per label */
    if (info->tip->tip.render_ctx) {
	ISWRenderDestroy(info->tip->tip.render_ctx);
	info->tip->tip.render_ctx = NULL;
    }

    {
	xcb_connection_t *conn = IswDisplay((Widget)info->tip);
	xcb_window_t win = IswWindow((Widget)info->tip);
	uint32_t stack_above = XCB_STACK_MODE_ABOVE;

	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, &stack_above);
	xcb_map_window(conn, win);
	xcb_flush(conn);
    }
    IswAddGrab(IswParent((Widget)info->tip), True, True);
    info->mapped = True;
}

/*ARGSUSED*/
static void
TipShellEventHandler(Widget w, IswPointer client_data, xcb_generic_event_t *event, Boolean *continue_to_dispatch)
{
    IswTipInfo *info = FindTipInfo(w);

    ResetTip(info, FindWidgetInfo(info, w), False);
}

/*ARGSUSED*/
static void
TipEventHandler(Widget w, IswPointer client_data, xcb_generic_event_t *event, Boolean *continue_to_dispatch)
{
    IswTipInfo *info = FindTipInfo(w);
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
IswTipEnable(Widget w, String label)
{
    if (IswIsWidget(w) && label && *label) {
	IswTipInfo *info = FindTipInfo(w);
	WidgetInfo *winfo = FindWidgetInfo(info, w);

	if (winfo->label)
	    IswFree((char *)winfo->label);
	winfo->label = IswNewString(label);

	IswAddEventHandler(w, TIP_EVENT_MASK, False, TipEventHandler,
			  (IswPointer)NULL);
    }
}

void
IswTipDisable(Widget w)
{
    if (IswIsWidget(w)) {
	IswTipInfo *info = FindTipInfo(w);

	IswRemoveEventHandler(w, TIP_EVENT_MASK, False, TipEventHandler,
			     (IswPointer)NULL);
	ResetTip(info, FindWidgetInfo(info, w), False);
    }
}
