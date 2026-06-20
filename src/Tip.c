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
#include <ISW/TipP.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/IswArgMacros.h>
#include <ISW/ISWPlatform.h>
#include "IntrinsicI.h"

#include <string.h>

#include <stdlib.h>

/*
 * Types
 */
typedef struct _WidgetInfo {
    Widget widget;
    String label;
    struct _WidgetInfo *next;
} WidgetInfo;

typedef struct _IswTipInfo {
    IswScreen screen;
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
static void IswTipExpose(Widget, IswEvent *, IswRegion);
static void IswTipRealize(IswDisplay, Widget, IswValueMask *, uint32_t *);
static Boolean IswTipSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

/*
 * Prototypes
 */
static void TipEventHandler(Widget, IswPointer, IswEvent *, Boolean *);
static void TipShellEventHandler(Widget, IswPointer, IswEvent *, Boolean *);
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
  {IswNlabel, IswCLabel, IswRString, sizeof(String),
    offset(label), IswRString, NULL},
  {IswNencoding, IswCEncoding, IswRUnsignedChar, sizeof(unsigned char),
    offset(encoding), IswRImmediate, (IswPointer)IswTextEncoding8bit},
  {IswNinternalHeight, IswCHeight, IswRDimension, sizeof(Dimension),
    offset(internal_height), IswRImmediate, (IswPointer)2},
  {IswNinternalWidth, IswCWidth, IswRDimension, sizeof(Dimension),
    offset(internal_width), IswRImmediate, (IswPointer)2},
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
    ISW_NULLQUARK,				/* xrm_class */
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

static void
IswTipClassInitialize(void)
{
    IswInitializeWidgetSet();
}

/*ARGSUSED*/
static void
IswTipInitialize(Widget req, Widget w, ArgList args, Cardinal *num_args)
{
    TipWidget tip = (TipWidget)w;

    /* HiDPI: scale dimension resources */
    tip->tip.internal_width = (tip->tip.internal_width);
    tip->tip.internal_height = (tip->tip.internal_height);

    if (tip->tip.font == NULL) {
	IswAppWarning(IswWidgetToApplicationContext(w),
		     "Tip widget: font is NULL - text rendering will fail");
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

    IswRemoveEventHandler(IswParent(w), IswKeyPressMask, False,
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
IswTipRealize(IswDisplay dpy, Widget w, IswValueMask *mask _X_UNUSED,
              uint32_t *values _X_UNUSED)
{
    double sf = _IswGetScaleFactor(dpy);
    IswWindowGeometry geom;
    IswWindowAttributes attrs;

    /* Override-redirect popup child of the screen root, at physical-pixel
       geometry.  Created through the platform window op — no native calls. */
    geom.x = (int32_t)(IswX(w) * sf + 0.5);
    geom.y = (int32_t)(IswY(w) * sf + 0.5);
    geom.width = (uint32_t)((IswWidth(w) ? IswWidth(w) : 1) * sf + 0.5);
    geom.height = (uint32_t)((IswHeight(w) ? IswHeight(w) : 1) * sf + 0.5);
    geom.border_width = (uint32_t)(IswBorderWidth(w) * sf + 0.5);

    memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel = w->core.background_pixel;
    attrs.override_redirect = True;
    attrs.event_mask = IswBuildEventMask(w);

    {
        IswWindow win = _IswPlatformCreateWindow(dpy, _IswDefaultRootWindow(dpy),
                                                 &geom, &attrs,
                                                 ISW_WINDOW_CLASS_INPUT_OUTPUT);
        /* The tooltip backs its own override-redirect popup window; register
           it with the platform so IswWindowOf-style resolution finds it.  The
           toolkit stores no window handle. */
        _IswPlatformSetWidgetWindow(dpy, w, win);

        /* _NET_WM_WINDOW_TYPE = TOOLTIP */
        _IswPlatformSetWindowType(IswDisplayOf(w), win, ISW_WINDOW_TYPE_TOOLTIP);
    }
}

static void
IswTipExpose(Widget w, IswEvent *event, IswRegion region)
{
    TipWidget tip = (TipWidget)w;
    const char *nl, *label = tip->tip.label;
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

    while ((nl = strchr(label, '\n')) != NULL) {
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
    const char *nl, *label = info->tip->tip.label;

    if (!label || !*label) {
	IswWidth(info->tip) = 1;
	IswHeight(info->tip) = 1;
	return;
    }

    height = ISWScaledFontHeight(w, fs);

    if ((nl = strchr(label, '\n')) != NULL) {
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
	    if ((nl = strchr(label, '\n')) == NULL)
		nl = strchr(label, '\0');
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
    IswDisplay dpy = IswDisplayOf((Widget)info->tip);
    int rx = 0, ry = 0;
    Position x, y;
    int bw2 = IswBorderWidth(info->tip) * 2;
    int scr_width = (int) _IswPlatformScreenWidth(dpy, IswScreenOf(info->tip));
    int scr_height = (int) _IswPlatformScreenHeight(dpy, IswScreenOf(info->tip));
    int win_width = IswWidth(info->tip) + bw2;
    int win_height = IswHeight(info->tip) + bw2;

    (void) _IswPlatformQueryPointer(dpy, _IswDefaultRootWindow(dpy),
				    &rx, &ry, NULL, NULL, NULL, NULL);
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
        double _sf = _IswGetScaleFactor(dpy);
        IswWindowGeometry geom;
        IswX(info->tip) = x;
        IswY(info->tip) = y;
        geom.x = (int32_t)(x * _sf + 0.5);
        geom.y = (int32_t)(y * _sf + 0.5);
        geom.width = (uint32_t)(IswWidth(info->tip) * _sf + 0.5);
        geom.height = (uint32_t)(IswHeight(info->tip) * _sf + 0.5);
        geom.border_width = 0;
        _IswPlatformConfigureWindow(dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(info->tip)), (Widget)(info->tip)), &geom,
                                    ISW_CONFIG_X | ISW_CONFIG_Y |
                                    ISW_CONFIG_WIDTH | ISW_CONFIG_HEIGHT,
                                    ISW_STACK_NONE, NULL);
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
    info->screen = IswScreenOf(w);
    info->mapped = False;
    info->widgets = NULL;
    info->next = NULL;
    IswAddEventHandler(shell, IswKeyPressMask, False, TipShellEventHandler,
		      (IswPointer)NULL);

    return (info);
}

static IswTipInfo *
FindTipInfo(Widget w)
{
    IswTipInfo *info, *list = TipInfoList;
    IswScreen screen;

    if (list == NULL)
	return (TipInfoList = CreateTipInfo(w));

    screen = IswScreenOf(w);
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
	IswDisplay dpy = IswDisplayOf((Widget)info->tip);
	IswRemoveGrab(IswParent((Widget)info->tip));
	_IswPlatformUnmapWindow(dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)info->tip), (Widget)info->tip));
	_IswPlatformFlush(dpy);
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
    info->tip->tip.label = winfo->label;
    info->tip->tip.encoding = 0;
    info->tip->tip.international = False;
    {
	IswArgBuilder ab = IswArgBuilderInit();
	IswArgEncoding(&ab, (IswArgVal)&info->tip->tip.encoding);
	IswArgInternational(&ab, (IswArgVal)&info->tip->tip.international);
	IswGetValues(winfo->widget, ab.args, ab.count);
    }

    TipLayout(info);
    TipPosition(info);

    /* Invalidate render context — tooltip size changes per label */
    if (info->tip->tip.render_ctx) {
	ISWRenderDestroy(info->tip->tip.render_ctx);
	info->tip->tip.render_ctx = NULL;
    }

    {
	IswDisplay dpy = IswDisplayOf((Widget)info->tip);
	IswWindow win = _IswPlatformWidgetWindow(IswDisplayOf((Widget)info->tip), (Widget)info->tip);
	IswWindowGeometry geom;

	memset(&geom, 0, sizeof(geom));
	_IswPlatformConfigureWindow(dpy, win, &geom, ISW_CONFIG_STACK,
				    ISW_STACK_ABOVE, NULL);
	_IswPlatformMapWindow(dpy, win);
	_IswPlatformFlush(dpy);
    }
    IswAddGrab(IswParent((Widget)info->tip), True);
    info->mapped = True;
}

/*ARGSUSED*/
static void
TipShellEventHandler(Widget w, IswPointer client_data, IswEvent *iswev, Boolean *continue_to_dispatch)
{
    IswTipInfo *info = FindTipInfo(w);

    ResetTip(info, FindWidgetInfo(info, w), False);
}

/*ARGSUSED*/
static void
TipEventHandler(Widget w, IswPointer client_data, IswEvent *iswev, Boolean *continue_to_dispatch)
{
    IswTipInfo *info = FindTipInfo(w);
    Boolean add_timeout;

    switch (iswev->kind) {
 case IswEnter:
     add_timeout = True;
     break;
 case IswMotion:
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

	IswAddEventHandler(w,
			  IswButtonPressMask | IswButtonReleaseMask |
			  IswPointerMotionMask | IswButtonMotionMask |
			  IswKeyPressMask | IswKeyReleaseMask |
			  IswEnterWindowMask | IswLeaveWindowMask,
			  False, TipEventHandler, (IswPointer)NULL);
    }
}

void
IswTipDisable(Widget w)
{
    if (IswIsWidget(w)) {
	IswTipInfo *info = FindTipInfo(w);

	IswRemoveEventHandler(w,
			     IswButtonPressMask | IswButtonReleaseMask |
			     IswPointerMotionMask | IswButtonMotionMask |
			     IswKeyPressMask | IswKeyReleaseMask |
			     IswEnterWindowMask | IswLeaveWindowMask,
			     False, TipEventHandler, (IswPointer)NULL);
	ResetTip(info, FindWidgetInfo(info, w), False);
    }
}
