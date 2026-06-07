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
/*
 * Portions Copyright (c) 1996 Alfredo Kojima
 * Rights, permissions, and disclaimer per the above X Consortium license.
 */

/*
 * This is a copy of Xt/Vendor.c with an additional ClassInitialize
 * procedure to register Xmu resource type converters, and all the
 * monkey business associated with input methods...
 *
 */

/* Make sure all wm properties can make it out of the resource manager */

#include <ISW/ISWP.h>
#include <stdio.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ShellP.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

/* XCB_ATOM_COMPOUND_TEXT stub - simple atom value for compound text */
#ifndef XCB_ATOM_COMPOUND_TEXT
#define XCB_ATOM_COMPOUND_TEXT(dpy) XCB_ATOM_STRING
#endif
#include <ISW/VendorP.h>
#include <ISW/FocusMgrI.h>
#include <ISW/ISWPNG.h>
#include "ISWRenderPrivate.h"

/* The following two headers are for the input method. */
#include <ISW/VendorEP.h>
#include <ISW/ISWImP.h>
#include "ISWPlatformPrivate.h"


static IswResource resources[] = {
  {IswNinput, IswCInput, IswRBool, sizeof(Bool),
		IswOffsetOf(VendorShellRec, wm.wm_hints.input),
		IswRImmediate, (IswPointer)True}
};

/***************************************************************************
 *
 * Vendor shell class record
 *
 ***************************************************************************/

static void IswVendorShellClassInitialize(void);
static void IswVendorShellInitialize(Widget, Widget, ArgList, Cardinal *);
static Boolean IswVendorShellSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, IswValueMask *, uint32_t *);
static void ChangeManaged(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static void IswVendorShellClassPartInit(WidgetClass);
void IswVendorShellExtResize(Widget);

#if defined(__UNIXOS2__) || defined(__CYGWIN__) || defined(__MINGW32__)
/* to fix the EditRes problem because of wrong linker semantics */
extern WidgetClass vendorShellWidgetClass; /* from Xt/Vendor.c */
extern VendorShellClassRec _IswVendorShellClassRec;
void _IswFixupVendorShell(void);

#if defined(__UNIXOS2__)
unsigned long _DLL_InitTerm(unsigned long mod,unsigned long flag)
{
        switch (flag) {
        case 0: /*called on init*/
                _CRT_init();
                vendorShellWidgetClass = (WidgetClass)(&_IswVendorShellClassRec$
                _IswFixupVendorShell();
                return 1;
        case 1: /*called on exit*/
                return 1;
        default:
                return 0;
        }
}
#endif

#if defined(__CYGWIN__) || defined(__MINGW32__)
int __stdcall DllMain(unsigned long, unsigned long, void *);

int __stdcall
DllMain(unsigned long mod_handle, unsigned long flag, void *routine)
{
  switch (flag)
    {
    case 1: /* DLL_PROCESS_ATTACH - process attach */
      vendorShellWidgetClass = (WidgetClass)(&_IswVendorShellClassRec);
      _IswFixupVendorShell();
      break;
    case 0: /* DLL_PROCESS_DETACH - process detach */
      break;
    }
  return 1;
}
#endif

#define vendorShellClassRec _IswVendorShellClassRec

#endif

static CompositeClassExtensionRec vendorCompositeExt = {
    /* next_extension     */	NULL,
    /* record_type        */    NULLQUARK,
    /* version            */    IswCompositeExtensionVersion,
    /* record_size        */    sizeof (CompositeClassExtensionRec),
    /* accepts_objects    */    TRUE,
    /* allows_change_managed_set */ FALSE
};

#define SuperClass (&wmShellClassRec)
externaldef(vendorshellclassrec) VendorShellClassRec vendorShellClassRec = {
  {
    /* superclass	  */	(WidgetClass)SuperClass,
    /* class_name	  */	"VendorShell",
    /* size		  */	sizeof(VendorShellRec),
    /* class_initialize	  */	IswVendorShellClassInitialize,
    /* class_part_init	  */	IswVendorShellClassPartInit,
    /* Class init'ed ?	  */	FALSE,
    /* initialize         */	IswVendorShellInitialize,
    /* initialize_hook	  */	NULL,
    /* realize		  */	Realize,
    /* actions		  */	NULL,
    /* num_actions	  */	0,
    /* resources	  */	resources,
    /* resource_count	  */	IswNumber(resources),
    /* xrm_class	  */	NULLQUARK,
    /* compress_motion	  */	FALSE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave*/	FALSE,
    /* visible_interest	  */	FALSE,
    /* destroy		  */	NULL,
    /* resize		  */	IswVendorShellExtResize,
    /* expose		  */	NULL,
    /* set_values	  */	IswVendorShellSetValues,
    /* set_values_hook	  */	NULL,
    /* set_values_almost  */	IswInheritSetValuesAlmost,
    /* get_values_hook	  */	NULL,
    /* accept_focus	  */	NULL,
    /* intrinsics version */	IswVersion,
    /* callback offsets	  */	NULL,
    /* tm_table		  */	NULL,
    /* query_geometry	  */	NULL,
    /* display_accelerator*/	NULL,
    /* extension	  */	NULL
  },{
    /* geometry_manager	  */	GeometryManager,
    /* change_managed	  */	ChangeManaged,
    /* insert_child	  */	IswInheritInsertChild,
    /* delete_child	  */	IswInheritDeleteChild,
    /* extension	  */	(IswPointer) &vendorCompositeExt
  },{
    /* extension	  */	NULL
  },{
    /* extension	  */	NULL
  },{
    /* extension	  */	NULL
  }
};

externaldef(vendorshellwidgetclass) WidgetClass vendorShellWidgetClass =
	(WidgetClass) (&vendorShellClassRec);


/***************************************************************************
 *
 * The following section is for the Vendor shell Extension class record
 *
 ***************************************************************************/

static IswResource ext_resources[] = {
  {IswNinputMethod, IswCInputMethod, IswRString, sizeof(String),
		IswOffsetOf(IswVendorShellExtRec, vendor_ext.im.input_method),
		IswRString, (IswPointer)NULL},
  {IswNpreeditType, IswCPreeditType, IswRString, sizeof(String),
		IswOffsetOf(IswVendorShellExtRec, vendor_ext.im.preedit_type),
		IswRString, (IswPointer)"OverTheSpot,OffTheSpot,Root"},
  {IswNopenIm, IswCOpenIm, IswRBoolean, sizeof(Boolean),
		IswOffsetOf(IswVendorShellExtRec, vendor_ext.im.open_im),
		IswRImmediate, (IswPointer)TRUE},
  {IswNsharedIc, IswCSharedIc, IswRBoolean, sizeof(Boolean),
		IswOffsetOf(IswVendorShellExtRec, vendor_ext.ic.shared_ic),
		IswRImmediate, (IswPointer)FALSE}
};

static void IswVendorShellExtClassInitialize(void);
static void IswVendorShellExtInitialize(Widget, Widget, ArgList, Cardinal *);
static void IswVendorShellExtDestroy(Widget);
static Boolean IswVendorShellExtSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

externaldef(vendorshellextclassrec) IswVendorShellExtClassRec
       xawvendorShellExtClassRec = {
  {
    /* superclass	  */	(WidgetClass)&objectClassRec,
    /* class_name	  */	"VendorShellExt",
    /* size		  */	sizeof(IswVendorShellExtRec),
    /* class_initialize	  */	IswVendorShellExtClassInitialize,
    /* class_part_initialize*/	NULL,
    /* Class init'ed ?	  */	FALSE,
    /* initialize	  */	IswVendorShellExtInitialize,
    /* initialize_hook	  */	NULL,
    /* pad		  */	NULL,
    /* pad		  */	NULL,
    /* pad		  */	0,
    /* resources	  */	ext_resources,
    /* resource_count	  */	IswNumber(ext_resources),
    /* xrm_class	  */	NULLQUARK,
    /* pad		  */	FALSE,
    /* pad		  */	FALSE,
    /* pad		  */	FALSE,
    /* pad		  */	FALSE,
    /* destroy		  */	IswVendorShellExtDestroy,
    /* pad		  */	NULL,
    /* pad		  */	NULL,
    /* set_values	  */	IswVendorShellExtSetValues,
    /* set_values_hook	  */	NULL,
    /* pad		  */	NULL,
    /* get_values_hook	  */	NULL,
    /* pad		  */	NULL,
    /* version		  */	IswVersion,
    /* callback_offsets	  */	NULL,
    /* pad		  */	NULL,
    /* pad		  */	NULL,
    /* pad		  */	NULL,
    /* extension	  */	NULL
  },{
    /* extension	  */	NULL
  }
};

externaldef(xawvendorshellwidgetclass) WidgetClass
     xawvendorShellExtWidgetClass = (WidgetClass) (&xawvendorShellExtClassRec);


/* IswCvtCompoundTextToString - commented out for XCB port
 * Text property conversion is complex in XCB and rarely used */
#if 0
/*ARGSUSED*/
static Boolean
IswCvtCompoundTextToString(IswDisplay dpy, XrmValuePtr args, Cardinal *num_args,
                           XrmValue *fromVal, XrmValue *toVal, IswPointer *cvt_data)
{
    XTextProperty prop;
    char **list;
    int count;
    static char *mbs = NULL;
    int len;

    prop.value = (unsigned char *)fromVal->addr;
    prop.encoding = XCB_ATOM_COMPOUND_TEXT(dpy);
    prop.format = 8;
    prop.nitems = fromVal->size;

    if(XmbTextPropertyToTextList(dpy, &prop, &list, &count) < Success) {
	IswAppWarningMsg(IswDisplayToApplicationContext((IswDisplay) dpy),
	"converter", "XmbTextPropertyToTextList", "IswError",
	"conversion from CT to MB failed.", NULL, 0);
	return False;
    }
    len = strlen(*list);
    toVal->size = len;
    mbs = IswRealloc(mbs, len + 1); /* keep buffer because no one call free :( */
    strcpy(mbs, *list);
    XFreeStringList(list);
    toVal->addr = (IswPointer)mbs;
    return True;
}
#endif /* 0 */

#ifndef ParentRelative
#define ParentRelative 1L
#endif

#define DONE(type, address) \
	{to->size = sizeof(type); to->addr = (IswPointer)address;}

/* ARGSUSED */
static Boolean
_IswCvtStringToPixmap(IswDisplay dpy, XrmValuePtr args, Cardinal *nargs,
                      XrmValuePtr from, XrmValuePtr to, IswPointer *data)
{
    static xcb_pixmap_t pixmap;
    ISWPNGImage *png;
    xcb_screen_t *screen;
    unsigned int w, h;
    const unsigned char *rgba;

    if (*nargs != 3)
	IswAppErrorMsg(IswDisplayToApplicationContext((IswDisplay) dpy),
		"_IswCvtStringToPixmap", "wrongParameters", "IswToolkitError",
	"_IswCvtStringToPixmap needs screen, colormap, and background_pixel",
		      (String *) NULL, (Cardinal *) NULL);

    if (strcmp(from->addr, "None") == 0)
    {
	pixmap = None;
	DONE(xcb_pixmap_t, &pixmap);
	return (True);
    }
    if (strcmp(from->addr, "ParentRelative") == 0)
    {
	pixmap = ParentRelative;
	DONE(xcb_pixmap_t, &pixmap);
	return (True);
    }

    /* Load PNG via lodepng (with ISW path resolution) */
    png = ISWPNGLoadFile((String) from->addr);
    if (!png) {
	IswDisplayStringConversionWarning((IswDisplay) dpy, (String) from->addr, IswRPixmap);
	return (False);
    }

    w = ISWPNGGetWidth(png);
    h = ISWPNGGetHeight(png);
    rgba = ISWPNGGetRGBA(png);

    screen = *((xcb_screen_t **) args[0].addr);

    /* Create a server-side pixmap and paint the RGBA data onto it via Cairo */
    pixmap = xcb_generate_id(_IswXcbConn(dpy));
    xcb_create_pixmap(_IswXcbConn(dpy), screen->root_depth, pixmap,
		      screen->root, w, h);
    {
	cairo_surface_t *target, *source;
	cairo_t *cr;
	int stride;

	target = cairo_xcb_surface_create(_IswXcbConn(dpy), pixmap,
		    ISWRenderFindVisual(screen, screen->root_depth),
		    w, h);
	stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w);
	source = cairo_image_surface_create_for_data(
		    (unsigned char *)rgba, CAIRO_FORMAT_ARGB32, w, h, stride);
	/* lodepng gives RGBA, but Cairo wants pre-multiplied ARGB in native
	 * byte order.  Swizzle in-place before painting. */
	{
	    unsigned char *px = (unsigned char *)rgba;
	    unsigned int i, npx = w * h;
	    for (i = 0; i < npx; i++) {
		unsigned char r = px[0], g = px[1], b = px[2], a = px[3];
		float af = a / 255.0f;
		/* Cairo native order is BGRA on little-endian (ARGB32) */
		px[0] = (unsigned char)(b * af + 0.5f);
		px[1] = (unsigned char)(g * af + 0.5f);
		px[2] = (unsigned char)(r * af + 0.5f);
		px[3] = a;
		px += 4;
	    }
	}
	cairo_surface_mark_dirty(source);
	cr = cairo_create(target);
	cairo_set_source_surface(cr, source, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(source);
	cairo_surface_flush(target);
	cairo_surface_destroy(target);
    }

    ISWPNGDestroy(png);

    if (to->addr == NULL)
	to->addr = (IswPointer) & pixmap;
    else
    {
	if (to->size < sizeof(xcb_pixmap_t))
	{
	    to->size = sizeof(xcb_pixmap_t);
	    IswDisplayStringConversionWarning((IswDisplay) dpy, (String) from->addr,
					     IswRPixmap);
	    return (False);
	}

	*((xcb_pixmap_t *) to->addr) = pixmap;
    }
    to->size = sizeof(xcb_pixmap_t);
    return (True);
}

static void
_VendorFetchDisplayArg(Widget widget, Cardinal *size _X_UNUSED,
                       XrmValue *value)
{
    static xcb_connection_t *_fetch_dpy;
    _fetch_dpy = _IswXcbConn(IswDisplayOfObject(widget));
    value->size = sizeof(xcb_connection_t *);
    value->addr = (IswPointer) &_fetch_dpy;
}

static void
IswVendorShellClassInitialize(void)
{
    static IswConvertArgRec cursorConvertArgs[] = {
        {IswProcedureArg, (IswPointer)_VendorFetchDisplayArg, 0},
        {IswWidgetBaseOffset, (IswPointer) IswOffsetOf(WidgetRec, core.screen),
	     sizeof(xcb_screen_t *)}
    };
    static IswConvertArgRec _IswCvtStrToPix[] = {
	{IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.screen),
	     sizeof(xcb_screen_t *)},
	{IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.colormap),
	     sizeof(IswColormap)},
	{IswWidgetBaseOffset,
	     (IswPointer)IswOffsetOf(WidgetRec, core.background_pixel),
	     sizeof(Pixel)}
    };

    /* IswSetTypeConverter needs 7 args: from, to, converter, args, num_args, cache, destructor */
    IswSetTypeConverter(IswRString, IswRCursor, IswCvtStringToCursor,
     cursorConvertArgs, IswNumber(cursorConvertArgs),
     IswCacheNone, NULL);

    IswSetTypeConverter(IswRString, IswRBitmap,
		       (IswTypeConverter)_IswCvtStringToPixmap,
		       _IswCvtStrToPix, IswNumber(_IswCvtStrToPix),
		       IswCacheByDisplay, (IswDestructor)NULL);

    /* IswCvtCompoundTextToString commented out - complex text conversion not ported yet */
    /* IswSetTypeConverter("CompoundText", IswRString, IswCvtCompoundTextToString,
			NULL, 0, IswCacheNone, NULL); */
}

static void
IswVendorShellClassPartInit(WidgetClass class)
{
    CompositeClassExtension ext;
    VendorShellWidgetClass vsclass = (VendorShellWidgetClass) class;

    if ((ext = (CompositeClassExtension)
	    IswGetClassExtension (class,
				 IswOffsetOf(CompositeClassRec,
					    composite_class.extension),
				 NULLQUARK, 1L, (Cardinal) 0)) == NULL) {
	ext = (CompositeClassExtension) IswNew (CompositeClassExtensionRec);
	if (ext != NULL) {
	    ext->next_extension = vsclass->composite_class.extension;
	    ext->record_type = NULLQUARK;
	    ext->version = IswCompositeExtensionVersion;
	    ext->record_size = sizeof (CompositeClassExtensionRec);
	    ext->accepts_objects = TRUE;
	    ext->allows_change_managed_set = FALSE;
	    vsclass->composite_class.extension = (IswPointer) ext;
	}
    }
}

#if defined(__osf__) || defined(__UNIXOS2__) || defined(__CYGWIN__) || defined(__MINGW32__)
/* stupid OSF/1 shared libraries have the wrong semantics */
/* symbols do not get resolved external to the shared library */
void
_IswFixupVendorShell(void)
{
    transientShellWidgetClass->core_class.superclass =
        (WidgetClass) &vendorShellClassRec;
    topLevelShellWidgetClass->core_class.superclass =
        (WidgetClass) &vendorShellClassRec;
}
#endif

/* ARGSUSED */
static void
IswVendorShellInitialize(Widget req, Widget new, ArgList args, Cardinal *num_args)
{
    /* EditRes support commented out for XCB port - optional feature */
    /* IswAddEventHandler(new, (EventMask) 0, TRUE, _XEditResCheckMessages, NULL); */
    /* IswRegisterExternalAgent stub - XCB does not support XIM */
    /* IswAddEventHandler(new, (EventMask) 0, TRUE, IswRegisterExternalAgent, NULL); */
    IswCreateWidget("shellext", xawvendorShellExtWidgetClass,
		   new, args, *num_args);
}

/* ARGSUSED */
static Boolean
IswVendorShellSetValues(Widget old, Widget ref, Widget new, ArgList args, Cardinal *num_args)
{
	return FALSE;
}

static void
Realize(xcb_connection_t *dpy, Widget wid, IswValueMask *vmask, uint32_t *attr)
{
	WidgetClass super = wmShellWidgetClass;

	/* Make my superclass do all the dirty work */

	/* Call superclass realize - XCB custom libXt uses 4-parameter signature */
	(*super->core_class.realize) (dpy, wid, vmask, attr);
	_IswImRealize(wid);
}


static void
IswVendorShellExtClassInitialize(void)
{
}

/* ARGSUSED */
static void
IswVendorShellExtInitialize(Widget req, Widget new, ArgList args, Cardinal *num_args)
{
    _IswImInitialize(new->core.parent, new);
}

/* ARGSUSED */
static void
IswVendorShellExtDestroy(Widget w)
{
    _IswImDestroy( w->core.parent, w );
}

/* ARGSUSED */
static Boolean
IswVendorShellExtSetValues(Widget old, Widget ref, Widget new, ArgList args, Cardinal *num_args)
{
	return FALSE;
}

void
IswVendorShellExtResize(Widget w)
{
	ShellWidget sw = (ShellWidget) w;
	Widget childwid;
	int i;
	int core_height;

	_IswImResizeVendorShell( w );
	core_height = _IswImGetShellHeight( w );
	
	/* Check if children array is allocated before accessing it */
	if (sw->composite.children == NULL) {
		return;
	}
	
	for( i = 0; i < sw->composite.num_children; i++ ) {
	    if( IswIsManaged( sw->composite.children[ i ] ) ) {
		childwid = sw->composite.children[ i ];
		IswResizeWidget( childwid, sw->core.width, core_height,
			       childwid->core.border_width );
	    }
	}
}

/*ARGSUSED*/
static IswGeometryResult
GeometryManager(Widget wid, IswWidgetGeometry *request, IswWidgetGeometry *reply)
{
	ShellWidget shell = (ShellWidget)(wid->core.parent);
	IswWidgetGeometry my_request;

	if(shell->shell.allow_shell_resize == FALSE && IswIsRealized(wid))
		return(IswGeometryNo);

	if (request->request_mode & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y))
	    return(IswGeometryNo);

	/* %%% worry about IswCWQueryOnly */
	my_request.request_mode = 0;
	if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH) {
	    my_request.width = request->width;
	    my_request.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
	}
	if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) {
	    my_request.height = request->height
			      + _IswImGetImAreaHeight( wid )
			      ;
	    my_request.request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
	}
	if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
	    my_request.border_width = request->border_width;
	    my_request.request_mode |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
	}
	if (IswMakeGeometryRequest((Widget)shell, &my_request, NULL)
		== IswGeometryYes) {
	    /* assert: if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH) then
	     * 		  shell->core.width == request->width
	     * assert: if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) then
	     * 		  shell->core.height == request->height
	     *
	     * so, whatever the WM sized us to (if the Shell requested
	     * only one of the two) is now the correct child size
	     */

	    wid->core.width = shell->core.width;
	    wid->core.height = shell->core.height;
	    if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
		wid->core.x = wid->core.y = -request->border_width;
	    }
	    _IswImCallVendorShellExtResize(wid);
	    return IswGeometryYes;
	} else return IswGeometryNo;
}

static void
ChangeManaged(Widget wid)
{
	ShellWidget w = (ShellWidget) wid;
	Widget* childP;
	int i;

	(*SuperClass->composite_class.change_managed)(wid);
	for (i = w->composite.num_children, childP = w->composite.children;
	     i; i--, childP++) {
	    if (IswIsManaged(*childP)) {
		IswSetKeyboardFocus(wid, *childP);
		break;
	    }
	}
	_IswFocusMgrEnsureInstalled(wid);
}
