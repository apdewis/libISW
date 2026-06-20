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

#include <ISW/VendorP.h>
#include <ISW/FocusMgrI.h>

/* The following two headers are for the input method. */
#include <ISW/VendorEP.h>

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
static void Realize(IswDisplay, Widget, IswValueMask *, uint32_t *);
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
    /* record_type        */    ISW_NULLQUARK,
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
    /* xrm_class	  */	ISW_NULLQUARK,
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
    /* xrm_class	  */	ISW_NULLQUARK,
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

#ifndef ParentRelative
#define ParentRelative 1L
#endif

#define DONE(type, address) \
	{to->size = sizeof(type); to->addr = (IswPointer)address;}

static void
_VendorFetchDisplayArg(Widget widget, Cardinal *size _X_UNUSED,
                       IswValueRec *value)
{
    static IswDisplay _fetch_dpy;
    _fetch_dpy = IswDisplayOfObject(widget);
    value->size = sizeof(IswDisplay);
    value->addr = (IswPointer) &_fetch_dpy;
}

static void
IswVendorShellClassInitialize(void)
{
    static IswConvertArgRec cursorConvertArgs[] = {
        {IswProcedureArg, (IswPointer)_VendorFetchDisplayArg, 0},
        {IswWidgetBaseOffset, (IswPointer) IswOffsetOf(WidgetRec, core.screen),
	     sizeof(IswScreen)}
    };
    static IswConvertArgRec _IswCvtStrToPix[] = {
	{IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.screen),
	     sizeof(IswScreen)},
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
				 ISW_NULLQUARK, 1L, (Cardinal) 0)) == NULL) {
	ext = (CompositeClassExtension) IswNew (CompositeClassExtensionRec);
	if (ext != NULL) {
	    ext->next_extension = vsclass->composite_class.extension;
	    ext->record_type = ISW_NULLQUARK;
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
Realize(IswDisplay dpy, Widget wid, IswValueMask *vmask, uint32_t *attr)
{
	WidgetClass super = wmShellWidgetClass;

	/* Make my superclass do all the dirty work */

	/* Call superclass realize - XCB custom libXt uses 4-parameter signature */
	(*super->core_class.realize) (dpy, wid, vmask, attr);
}


static void
IswVendorShellExtClassInitialize(void)
{
}

/* ARGSUSED */
static void
IswVendorShellExtInitialize(Widget req, Widget new, ArgList args, Cardinal *num_args)
{
}

/* ARGSUSED */
static void
IswVendorShellExtDestroy(Widget w)
{
}

/* ARGSUSED */
static Boolean
IswVendorShellExtSetValues(Widget old, Widget ref, Widget new, ArgList args, Cardinal *num_args)
{
	return FALSE;
}

//#TODO does this even have a need to exist without XIM?
void
IswVendorShellExtResize(Widget w)
{
	ShellWidget sw = (ShellWidget) w;
	Widget childwid;
	Cardinal i;

	/* Check if children array is allocated before accessing it */
	if (sw->composite.children == NULL) {
		return;
	}

	for( i = 0; i < sw->composite.num_children; i++ ) {
	    if( IswIsManaged( sw->composite.children[ i ] ) ) {
		childwid = sw->composite.children[ i ];
		IswResizeWidget( childwid, sw->core.width, sw->core.height,
			       childwid->core.border_width );
		break;
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

	if (request->request_mode & (IswCWX | IswCWY))
	    return(IswGeometryNo);

	/* %%% worry about IswCWQueryOnly */
	my_request.request_mode = 0;
	if (request->request_mode & IswCWWidth) {
	    my_request.width = request->width;
	    my_request.request_mode |= IswCWWidth;
	}
	if (request->request_mode & IswCWHeight) {
	    my_request.height = request->height;
	    my_request.request_mode |= IswCWHeight;
	}
	if (request->request_mode & IswCWBorderWidth) {
	    my_request.border_width = request->border_width;
	    my_request.request_mode |= IswCWBorderWidth;
	}
	if (IswMakeGeometryRequest((Widget)shell, &my_request, NULL)
		== IswGeometryYes) {
	    /* assert: if (request->request_mode & IswCWWidth) then
	     * 		  shell->core.width == request->width
	     * assert: if (request->request_mode & IswCWHeight) then
	     * 		  shell->core.height == request->height
	     *
	     * so, whatever the WM sized us to (if the Shell requested
	     * only one of the two) is now the correct child size
	     */

	    wid->core.width = shell->core.width;
	    wid->core.height = shell->core.height;
	    if (request->request_mode & IswCWBorderWidth) {
		wid->core.x = wid->core.y = -request->border_width;
	    }
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
