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

Copyright 1987, 1988, 1994, 1998  The Open Group

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

#ifndef DEFAULT_WM_TIMEOUT
#define DEFAULT_WM_TIMEOUT 5000
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "StringDefs.h"
#include "Shell.h"
#include "ShellP.h"
#include "ShellI.h"
#include "Vendor.h"
#include "VendorP.h"
#include <ISW/FocusMgrI.h>
#include <ISW/SimpleP.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include <X11/cursorfont.h>
#include <ISW/IswDragDrop.h>
#include "ISWXcbDraw.h"
#include "ISWPlatformPrivate.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#ifdef WIN32
#include <process.h>		/* for getpid() */
#endif

/***************************************************************************
 *
 * Note: per the Xt spec, the Shell geometry management assumes in
 * several places that there is only one managed child.  This is
 * *not* a bug.  Any subclass that assumes otherwise is broken.
 *
 ***************************************************************************/

#define BIGSIZE ((Dimension)32767)

/***************************************************************************
 *
 * Default values for resource lists
 *
 ***************************************************************************/

//static void _IswShellDepth(Widget, int, XrmValue *);
//static void _IswShellColormap(Widget, int, XrmValue *);
//static void _IswTitleEncoding(Widget, int, XrmValue *);

/***************************************************************************
 *
 * Shell class record
 *
 ***************************************************************************/

#define Offset(x)       (IswOffsetOf(ShellRec, x))
/* *INDENT-OFF* */
static IswResource shellResources[]=
{
    {IswNx, IswCPosition, IswRPosition, sizeof(Position),
        Offset(core.x), IswRImmediate, (IswPointer)BIGSIZE},
    {IswNy, IswCPosition, IswRPosition, sizeof(Position),
        Offset(core.y), IswRImmediate, (IswPointer)BIGSIZE},
    { IswNallowShellResize, IswCAllowShellResize, IswRBoolean,
        sizeof(Boolean), Offset(shell.allow_shell_resize),
        IswRImmediate, (IswPointer)False},
    { IswNgeometry, IswCGeometry, IswRString, sizeof(String),
        Offset(shell.geometry), IswRString, (IswPointer)NULL},
    { IswNcreatePopupChildProc, IswCCreatePopupChildProc, IswRFunction,
        sizeof(IswCreatePopupChildProc), Offset(shell.create_popup_child_proc),
        IswRFunction, NULL},
    { IswNsaveUnder, IswCSaveUnder, IswRBoolean, sizeof(Boolean),
        Offset(shell.save_under), IswRImmediate, (IswPointer)False},
    { IswNpopupCallback, IswCCallback, IswRCallback, sizeof(IswCallbackList),
        Offset(shell.popup_callback), IswRCallback, (IswPointer) NULL},
    { IswNpopdownCallback, IswCCallback, IswRCallback, sizeof(IswCallbackList),
        Offset(shell.popdown_callback), IswRCallback, (IswPointer) NULL},
    { IswNoverrideRedirect, IswCOverrideRedirect,
        IswRBoolean, sizeof(Boolean), Offset(shell.override_redirect),
        IswRImmediate, (IswPointer)False},
    { IswNvisual, IswCVisual, IswRVisual, sizeof(xcb_visualtype_t*),
        Offset(shell.visual), IswRImmediate, (IswPointer)CopyFromParent}
};
/* *INDENT-ON* */

static void ClassPartInitialize(WidgetClass);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, Mask *, uint32_t *); //IswSetWindowAttributes *);
static void Resize(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void GetValuesHook(Widget, ArgList, Cardinal *);
static void ChangeManaged(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *,
                                        IswWidgetGeometry *);
static IswGeometryResult RootGeometryManager(Widget gw,
                                            IswWidgetGeometry *request,
                                            IswWidgetGeometry *reply);
static void Destroy(Widget);

/* *INDENT-OFF* */
static ShellClassExtensionRec shellClassExtRec = {
    NULL,
    NULLQUARK,
    IswShellExtensionVersion,
    sizeof(ShellClassExtensionRec),
    RootGeometryManager
};

externaldef(shellclassrec) ShellClassRec shellClassRec = {
  {   /* Core */
    /* superclass            */ (WidgetClass) &compositeClassRec,
    /* class_name            */ "Shell",
    /* size                  */ sizeof(ShellRec),
    /* Class Initializer     */ NULL,
    /* class_part_initialize */ ClassPartInitialize,
    /* Class init'ed ?       */ FALSE,
    /* initialize            */ Initialize,
    /* initialize_notify     */ NULL,
    /* realize               */ Realize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ shellResources,
    /* resource_count        */ IswNumber(shellResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ Destroy,
    /* resize                */ Resize,
    /* expose                */ NULL,
    /* set_values            */ SetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ IswInheritSetValuesAlmost,
    /* get_values_hook       */ GetValuesHook,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ IswVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{ /* Composite */
    /* geometry_manager      */ GeometryManager,
    /* change_managed        */ ChangeManaged,
    /* insert_child          */ IswInheritInsertChild,
    /* delete_child          */ IswInheritDeleteChild,
    /* extension             */ NULL
  },{ /* Shell */
    /* extension             */ (IswPointer)&shellClassExtRec
  }
};
/* *INDENT-ON* */

externaldef(shellwidgetclass)
WidgetClass shellWidgetClass = (WidgetClass) (&shellClassRec);

/***************************************************************************
 *
 * OverrideShell class record
 *
 ***************************************************************************/

/* *INDENT-OFF* */
static IswResource overrideResources[] =
{
    { IswNoverrideRedirect, IswCOverrideRedirect,
        IswRBoolean, sizeof(Boolean), Offset(shell.override_redirect),
        IswRImmediate, (IswPointer)True},
    { IswNsaveUnder, IswCSaveUnder, IswRBoolean, sizeof(Boolean),
        Offset(shell.save_under), IswRImmediate, (IswPointer)True},
};

externaldef(overrideshellclassrec) OverrideShellClassRec overrideShellClassRec = {
  {
    /* superclass            */ (WidgetClass) &shellClassRec,
    /* class_name            */ "OverrideShell",
    /* size                  */ sizeof(OverrideShellRec),
    /* Class Initializer     */ NULL,
    /* class_part_initialize */ NULL,
    /* Class init'ed ?       */ FALSE,
    /* initialize            */ NULL,
    /* initialize_notify     */ NULL,
    /* realize               */ IswInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ overrideResources,
    /* resource_count        */ IswNumber(overrideResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ NULL,
    /* resize                */ IswInheritResize,
    /* expose                */ NULL,
    /* set_values            */ NULL,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ IswInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ IswVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ IswInheritGeometryManager,
    /* change_managed        */ IswInheritChangeManaged,
    /* insert_child          */ IswInheritInsertChild,
    /* delete_child          */ IswInheritDeleteChild,
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  }
};
/* *INDENT-ON* */

externaldef(overrideshellwidgetclass)
WidgetClass overrideShellWidgetClass = (WidgetClass) (&overrideShellClassRec);

/***************************************************************************
 *
 * WMShell class record
 *
 ***************************************************************************/

#undef Offset
#define Offset(x)       (IswOffsetOf(WMShellRec, x))

static int default_unspecified_shell_int = IswUnspecifiedShellInt;

/*
 * Warning, casting IswUnspecifiedShellInt (which is -1) to an (IswPointer)
 * can result is loss of bits on some machines (i.e. crays)
 */

/* *INDENT-OFF* */
static IswResource wmResources[] =
{
    { IswNtitle, IswCTitle, IswRString, sizeof(String),
        Offset(wm.title), IswRString, NULL},
    { IswNwmTimeout, IswCWmTimeout, IswRInt, sizeof(int),
        Offset(wm.wm_timeout), IswRImmediate,(IswPointer)DEFAULT_WM_TIMEOUT},
    { IswNwaitForWm, IswCWaitForWm, IswRBoolean, sizeof(Boolean),
        Offset(wm.wait_for_wm), IswRImmediate, (IswPointer)True},
    { IswNtransient, IswCTransient, IswRBoolean, sizeof(Boolean),
        Offset(wm.transient), IswRImmediate, (IswPointer)False},
/* size_hints minus things stored in core */
    { IswNbaseWidth, IswCBaseWidth, IswRInt, sizeof(int),
        Offset(wm.base_width),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNbaseHeight, IswCBaseHeight, IswRInt, sizeof(int),
        Offset(wm.base_height),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNwinGravity, IswCWinGravity, IswRGravity, sizeof(int),
        Offset(wm.win_gravity),
        IswRGravity, (IswPointer) &default_unspecified_shell_int},
    { IswNminWidth, IswCMinWidth, IswRInt, sizeof(int),
        Offset(wm.size_hints.min_width),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNminHeight, IswCMinHeight, IswRInt, sizeof(int),
        Offset(wm.size_hints.min_height),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNmaxWidth, IswCMaxWidth, IswRInt, sizeof(int),
        Offset(wm.size_hints.max_width),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNmaxHeight, IswCMaxHeight, IswRInt, sizeof(int),
        Offset(wm.size_hints.max_height),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNwidthInc, IswCWidthInc, IswRInt, sizeof(int),
        Offset(wm.size_hints.width_inc),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNheightInc, IswCHeightInc, IswRInt, sizeof(int),
        Offset(wm.size_hints.height_inc),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNminAspectX, IswCMinAspectX, IswRInt, sizeof(int),
        Offset(wm.size_hints.min_aspect.x),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNminAspectY, IswCMinAspectY, IswRInt, sizeof(int),
        Offset(wm.size_hints.min_aspect.y),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNmaxAspectX, IswCMaxAspectX, IswRInt, sizeof(int),
        Offset(wm.size_hints.max_aspect.x),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNmaxAspectY, IswCMaxAspectY, IswRInt, sizeof(int),
        Offset(wm.size_hints.max_aspect.y),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
/* wm_hints */
    { IswNinput, IswCInput, IswRBool, sizeof(Bool),
        Offset(wm.wm_hints.input), IswRImmediate, (IswPointer)False},
    { IswNinitialState, IswCInitialState, IswRInitialState, sizeof(int),
        Offset(wm.wm_hints.initial_state),
        IswRImmediate, (IswPointer)XCB_ICCCM_WM_STATE_NORMAL},
    { IswNiconPixmap, IswCIconPixmap, IswRBitmap, sizeof(xcb_pixmap_t),
        Offset(wm.wm_hints.icon_pixmap), IswRPixmap, NULL},
    { IswNiconWindow, IswCIconWindow, IswRWindow, sizeof(xcb_window_t),
        Offset(wm.wm_hints.icon_window), IswRWindow,   (IswPointer) NULL},
    { IswNiconX, IswCIconX, IswRInt, sizeof(int),
        Offset(wm.wm_hints.icon_x),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNiconY, IswCIconY, IswRInt, sizeof(int),
        Offset(wm.wm_hints.icon_y),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNiconMask, IswCIconMask, IswRBitmap, sizeof(xcb_pixmap_t),
        Offset(wm.wm_hints.icon_mask), IswRPixmap, NULL},
    { IswNwindowGroup, IswCWindowGroup, IswRWindow, sizeof(xcb_window_t),
        Offset(wm.wm_hints.window_group),
        IswRImmediate, (IswPointer)IswUnspecifiedWindow},
    { IswNclientLeader, IswCClientLeader, IswRWidget, sizeof(Widget),
        Offset(wm.client_leader), IswRWidget, NULL},
    { IswNwindowRole, IswCWindowRole, IswRString, sizeof(String),
        Offset(wm.window_role), IswRString, (IswPointer) NULL},
    { IswNurgency, IswCUrgency, IswRBoolean, sizeof(Boolean),
        Offset(wm.urgency), IswRImmediate, (IswPointer) False}
};
/* *INDENT-ON* */

static void
WMInitialize(Widget, Widget, ArgList, Cardinal *);
static Boolean
WMSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void
WMDestroy(Widget);

/* *INDENT-OFF* */
externaldef(wmshellclassrec) WMShellClassRec wmShellClassRec = {
  {
    /* superclass            */ (WidgetClass) &shellClassRec,
    /* class_name            */ "WMShell",
    /* size                  */ sizeof(WMShellRec),
    /* Class Initializer     */ NULL,
    /* class_part_initialize */ NULL,
    /* Class init'ed ?       */ FALSE,
    /* initialize            */ WMInitialize,
    /* initialize_notify     */ NULL,
    /* realize               */ IswInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ wmResources,
    /* resource_count        */ IswNumber(wmResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ WMDestroy,
    /* resize                */ IswInheritResize,
    /* expose                */ NULL,
    /* set_values            */ WMSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ IswInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ IswVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ IswInheritGeometryManager,
    /* change_managed        */ IswInheritChangeManaged,
    /* insert_child          */ IswInheritInsertChild,
    /* delete_child          */ IswInheritDeleteChild,
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  }
};
/* *INDENT-ON* */

externaldef(wmshellwidgetclass)
WidgetClass wmShellWidgetClass = (WidgetClass) (&wmShellClassRec);

/***************************************************************************
 *
 * TransientShell class record
 *
 ***************************************************************************/

#undef Offset
#define Offset(x)       (IswOffsetOf(TransientShellRec, x))

/* *INDENT-OFF* */
static IswResource transientResources[]=
{
    { IswNtransient, IswCTransient, IswRBoolean, sizeof(Boolean),
        Offset(wm.transient), IswRImmediate, (IswPointer)True},
    { IswNtransientFor, IswCTransientFor, IswRWidget, sizeof(Widget),
        Offset(transient.transient_for), IswRWidget, NULL},
    { IswNsaveUnder, IswCSaveUnder, IswRBoolean, sizeof(Boolean),
        Offset(shell.save_under), IswRImmediate, (IswPointer)True},
};
/* *INDENT-ON* */

static void
TransientRealize(xcb_connection_t *, Widget, Mask *, uint32_t *);
static Boolean
TransientSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

/* *INDENT-OFF* */
externaldef(transientshellclassrec) TransientShellClassRec transientShellClassRec = {
  {
    /* superclass            */ (WidgetClass) &vendorShellClassRec,
    /* class_name            */ "TransientShell",
    /* size                  */ sizeof(TransientShellRec),
    /* Class Initializer     */ NULL,
    /* class_part_initialize */ NULL,
    /* Class init'ed ?       */ FALSE,
    /* initialize            */ NULL,
    /* initialize_notify     */ NULL,
    /* realize               */ TransientRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ transientResources,
    /* resource_count        */ IswNumber(transientResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ NULL,
    /* resize                */ IswInheritResize,
    /* expose                */ NULL,
    /* set_values            */ TransientSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ IswInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ IswVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ IswInheritTranslations,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ IswInheritGeometryManager,
    /* change_managed        */ IswInheritChangeManaged,
    /* insert_child          */ IswInheritInsertChild,
    /* delete_child          */ IswInheritDeleteChild,
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  }
};
/* *INDENT-ON* */

externaldef(transientshellwidgetclass)
WidgetClass transientShellWidgetClass = (WidgetClass) (&transientShellClassRec);

/***************************************************************************
 *
 * TopLevelShell class record
 *
 ***************************************************************************/

#undef Offset
#define Offset(x)       (IswOffsetOf(TopLevelShellRec, x))

/* *INDENT-OFF* */
static IswResource topLevelResources[]=
{
    { IswNiconName, IswCIconName, IswRString, sizeof(String),
        Offset(topLevel.icon_name), IswRString, (IswPointer) NULL},
    { IswNiconic, IswCIconic, IswRBoolean, sizeof(Boolean),
        Offset(topLevel.iconic), IswRImmediate, (IswPointer)False}
};
/* *INDENT-ON* */

static void
TopLevelInitialize(Widget, Widget, ArgList, Cardinal *);
static Boolean
TopLevelSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void
TopLevelDestroy(Widget);

/* *INDENT-OFF* */
externaldef(toplevelshellclassrec) TopLevelShellClassRec topLevelShellClassRec = {
  {
    /* superclass            */ (WidgetClass) &vendorShellClassRec,
    /* class_name            */ "TopLevelShell",
    /* size                  */ sizeof(TopLevelShellRec),
    /* Class Initializer     */ NULL,
    /* class_part_initialize */ NULL,
    /* Class init'ed ?       */ FALSE,
    /* initialize            */ TopLevelInitialize,
    /* initialize_notify     */ NULL,
    /* realize               */ IswInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ topLevelResources,
    /* resource_count        */ IswNumber(topLevelResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ TopLevelDestroy,
    /* resize                */ IswInheritResize,
    /* expose                */ NULL,
    /* set_values            */ TopLevelSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ IswInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ IswVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ IswInheritTranslations,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ IswInheritGeometryManager,
    /* change_managed        */ IswInheritChangeManaged,
    /* insert_child          */ IswInheritInsertChild,
    /* delete_child          */ IswInheritDeleteChild,
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  }
};
/* *INDENT-ON* */

externaldef(toplevelshellwidgetclass)
WidgetClass topLevelShellWidgetClass = (WidgetClass) (&topLevelShellClassRec);

/***************************************************************************
 *
 * ApplicationShell class record
 *
 ***************************************************************************/

#undef Offset
#define Offset(x)       (IswOffsetOf(ApplicationShellRec, x))

/* *INDENT-OFF* */
static IswResource applicationResources[]=
{
    {IswNargc, IswCArgc, IswRInt, sizeof(int),
          Offset(application.argc), IswRImmediate, (IswPointer)0},
    {IswNargv, IswCArgv, IswRStringArray, sizeof(String*),
          Offset(application.argv), IswRPointer, (IswPointer) NULL}
};
/* *INDENT-ON* */
#undef Offset

static void
ApplicationInitialize(Widget, Widget, ArgList, Cardinal *);
static void
ApplicationDestroy(Widget);
static Boolean
ApplicationSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void
ApplicationShellInsertChild(Widget);

/* *INDENT-OFF* */
static CompositeClassExtensionRec compositeClassExtension = {
    /* next_extension        */ NULL,
    /* record_type           */ NULLQUARK,
    /* version               */ IswCompositeExtensionVersion,
    /* record_size           */ sizeof(CompositeClassExtensionRec),
    /* accepts_objects       */ TRUE,
    /* allows_change_managed_set */ FALSE
};

externaldef(applicationshellclassrec) ApplicationShellClassRec applicationShellClassRec = {
  {
    /* superclass            */ (WidgetClass) &topLevelShellClassRec,
    /* class_name            */ "ApplicationShell",
    /* size                  */ sizeof(ApplicationShellRec),
    /* Class Initializer     */ NULL,
    /* class_part_initialize*/  NULL,
    /* Class init'ed ?       */ FALSE,
    /* initialize            */ ApplicationInitialize,
    /* initialize_notify     */ NULL,
    /* realize               */ IswInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ applicationResources,
    /* resource_count        */ IswNumber(applicationResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ ApplicationDestroy,
    /* resize                */ IswInheritResize,
    /* expose                */ NULL,
    /* set_values            */ ApplicationSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ IswInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ IswVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ IswInheritTranslations,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ IswInheritGeometryManager,
    /* change_managed        */ IswInheritChangeManaged,
    /* insert_child          */ ApplicationShellInsertChild,
    /* delete_child          */ IswInheritDeleteChild,
    /* extension             */ (IswPointer)&compositeClassExtension
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  },{
    /* extension             */ NULL
  }
};
/* *INDENT-ON* */

externaldef(applicationshellwidgetclass)
WidgetClass applicationShellWidgetClass =
    (WidgetClass) (&applicationShellClassRec);

static void SetWMProperties(Widget w, char *window_name, char *icon_name,
                     char **argv, int argc,
                     xcb_size_hints_t *size_hints,
                     xcb_icccm_wm_hints_t *wm_hints,
                     char *classhint_class, char *classhint_name) {
    
    // Set WM_NAME / _NET_WM_NAME (semantic title hint)
    if (window_name != NULL) {
        _IswPlatformSetWindowTitle(IswDisplayOf(w), IswWindowOf(w), window_name);
    }

    // Set WM_ICON_NAME / _NET_WM_ICON_NAME (semantic icon-title hint)
    if (IswIsTopLevelShell((Widget) w) && icon_name != NULL) {
        _IswPlatformSetIconTitle(IswDisplayOf(w), IswWindowOf(w), icon_name);
    }

    // Set WM_COMMAND property (niche ICCCM; generic property op)
    if (argc > 0 && argv != NULL) {
        Atom wm_command = _IswPlatformInternAtomOp(IswDisplayOf(w),
                                                   "WM_COMMAND", False);
        _IswPlatformChangeProperty(IswDisplayOf(w), IswWindowOf(w),
                                   wm_command, ISW_ATOM_STRING, 8,
                                   ISW_PROP_MODE_REPLACE, argv, (uint32_t) argc);
    }

    // Set WM_CLASS (semantic class hint)
    if (classhint_name != NULL && classhint_class != NULL) {
        _IswPlatformSetWmClass(IswDisplayOf(w), IswWindowOf(w),
                               classhint_name, classhint_class);
    }
}

/****************************************************************************
 * Whew!
 ****************************************************************************/

static void
ComputeWMSizeHints(WMShellWidget w, xcb_size_hints_t *hints)
{
    register long flags;
    hints->flags = flags = w->wm.size_hints.flags;
    
#define copy(field) hints->field = w->wm.size_hints.field
    if (flags & (XCB_ICCCM_SIZE_HINT_US_POSITION | XCB_ICCCM_SIZE_HINT_P_POSITION)) {
        copy(x);
        copy(y);
    }
    if (flags & (XCB_ICCCM_SIZE_HINT_US_SIZE | XCB_ICCCM_SIZE_HINT_P_SIZE)) {
        copy(width);
        copy(height);
    }
    if (flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
        copy(min_width);
        copy(min_height);
    }
    if (flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
        copy(max_width);
        copy(max_height);
    }
    if (flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
        copy(width_inc);
        copy(height_inc);
    }
    if (flags & XCB_ICCCM_SIZE_HINT_P_ASPECT) {
        hints->min_aspect_num = w->wm.size_hints.min_aspect.x;
        hints->min_aspect_den = w->wm.size_hints.min_aspect.y;
        hints->max_aspect_num = w->wm.size_hints.max_aspect.x;
        hints->max_aspect_den = w->wm.size_hints.max_aspect.y;
    }
#undef copy
#define copy(field) hints->field = w->wm.field
    if (flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
        copy(base_width);
        copy(base_height);
    }
    if (flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)
        copy(win_gravity);
#undef copy
}

static void
_SetWMSizeHints(WMShellWidget w)
{
    xcb_size_hints_t *size_hints;
    size_hints = calloc(1, sizeof(xcb_size_hints_t));
    if (size_hints == NULL)
        _IswAllocError("xcb_size_hints_t");

    ComputeWMSizeHints(w, size_hints);
    _IswPlatformSetNormalHints(IswDisplayOf((Widget) w), IswWindowOf((Widget) w),
                               size_hints->flags,
                               size_hints->x, size_hints->y,
                               size_hints->width, size_hints->height,
                               size_hints->min_width, size_hints->min_height,
                               size_hints->max_width, size_hints->max_height,
                               size_hints->width_inc, size_hints->height_inc,
                               size_hints->min_aspect_num, size_hints->min_aspect_den,
                               size_hints->max_aspect_num, size_hints->max_aspect_den,
                               size_hints->base_width, size_hints->base_height,
                               (int) size_hints->win_gravity);
    free(size_hints);
}

static ShellClassExtension
_FindClassExtension(WidgetClass widget_class)
{
    ShellClassExtension ext;

    for (ext = (ShellClassExtension) ((ShellWidgetClass) widget_class)
         ->shell_class.extension;
         ext != NULL && ext->record_type != NULLQUARK;
         ext = (ShellClassExtension) ext->next_extension);

    if (ext != NULL) {
        if (ext->version == IswShellExtensionVersion
            && ext->record_size == sizeof(ShellClassExtensionRec)) {
            /* continue */
        }
        else {
            String params[1];
            Cardinal num_params = 1;

            params[0] = widget_class->core_class.class_name;
            IswErrorMsg("invalidExtension", "shellClassPartInitialize",
                       IswCIswToolkitError,
                       "widget class %s has invalid ShellClassExtension record",
                       params, &num_params);
        }
    }
    return ext;
}

static void
ClassPartInitialize(WidgetClass widget_class)
{
    ShellClassExtension ext = _FindClassExtension(widget_class);
    ShellClassExtension super =  _FindClassExtension(widget_class->core_class.superclass);

    if (ext != NULL) {
        if (ext->root_geometry_manager == IswInheritRootGeometryManager) {
            ext->root_geometry_manager =
                _FindClassExtension(widget_class->core_class.superclass)
                ->root_geometry_manager;
        }
    }
    else {
        /* if not found, spec requires IswInheritRootGeometryManager */
        IswPointer *extP
            = &((ShellWidgetClass) widget_class)->shell_class.extension;
        ext = IswNew(ShellClassExtensionRec);
        if(super != NULL)
        (void) memcpy(ext,
                      super,
                      sizeof(ShellClassExtensionRec));
        ext->next_extension = *extP;
        *extP = (IswPointer) ext;
    }
}

static void EventHandler(Widget wid, IswPointer closure,IswEvent *event,
                         Boolean *continue_to_dispatch);
static void _popup_set_prop(ShellWidget);

static void
Initialize(Widget req _X_UNUSED,
           Widget new,
           ArgList args _X_UNUSED,
           Cardinal *num_args _X_UNUSED)
{
    ShellWidget w = (ShellWidget) new;

    w->shell.popped_up = FALSE;
    w->shell.client_specified = _IswShellNotReparented | _IswShellPositionValid;

    if (w->core.x == BIGSIZE) {
        w->core.x = 0;
        if (w->core.y == BIGSIZE)
            w->core.y = 0;
    }
    else {
        if (w->core.y == BIGSIZE)
            w->core.y = 0;
        else
            w->shell.client_specified |= _IswShellPPositionOK;
    }

    IswAddEventHandler(new, (EventMask) XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                      TRUE, EventHandler, (IswPointer) NULL);
}

static void
WMInitialize(Widget req _X_UNUSED,
             Widget new,
             ArgList args _X_UNUSED,
             Cardinal *num_args _X_UNUSED)
{
    WMShellWidget w = (WMShellWidget) new;
    TopLevelShellWidget tls = (TopLevelShellWidget) new;        /* maybe */

    if (w->wm.title == NULL) {
        if (IswIsTopLevelShell(new) &&
            tls->topLevel.icon_name != NULL &&
            strlen(tls->topLevel.icon_name) != 0) {
            w->wm.title = IswNewString(tls->topLevel.icon_name);
        }
        else {
            w->wm.title = IswNewString(w->core.name);
        }
    }
    else {
        w->wm.title = IswNewString(w->wm.title);
    }
    w->wm.size_hints.flags = 0;
    w->wm.wm_hints.flags = 0;
    if (w->wm.window_role)
        w->wm.window_role = IswNewString(w->wm.window_role);

    {
        const char *sid = getenv("DESKTOP_STARTUP_ID");
        if (sid && *sid) {
            w->wm.startup_id = IswNewString(sid);
            unsetenv("DESKTOP_STARTUP_ID");
        } else {
            w->wm.startup_id = NULL;
        }
    }
}

static void
TopLevelInitialize(Widget req _X_UNUSED,
                   Widget new,
                   ArgList args _X_UNUSED,
                   Cardinal *num_args _X_UNUSED)
{
    TopLevelShellWidget w = (TopLevelShellWidget) new;

    if (w->topLevel.icon_name == NULL) {
        w->topLevel.icon_name = IswNewString(w->core.name);
    }
    else {
        w->topLevel.icon_name = IswNewString(w->topLevel.icon_name);
    }

    if (w->topLevel.iconic)
        w->wm.wm_hints.initial_state = XCB_ICCCM_WM_STATE_ICONIC;
}

static _IswString *NewArgv(int, _IswString *);
static void FreeStringArray(_IswString *);

static void
ApplicationInitialize(Widget req _X_UNUSED,
                      Widget new,
                      ArgList args _X_UNUSED,
                      Cardinal *num_args _X_UNUSED)
{
    ApplicationShellWidget w = (ApplicationShellWidget) new;

    if (w->application.argc > 0)
        w->application.argv = NewArgv(w->application.argc, w->application.argv);
}

static void
Resize(Widget w)
{
    register ShellWidget sw = (ShellWidget) w;
    Widget childwid;
    Cardinal i;

    for (i = 0; i < sw->composite.num_children; i++) {
        if (IswIsManaged(sw->composite.children[i])) {
            childwid = sw->composite.children[i];
            IswResizeWidget(childwid, sw->core.width, sw->core.height,
                           childwid->core.border_width);
            break;              /* can only be one managed child */
        }
    }
}

static void GetGeometry(Widget, Widget);

/*
 * Default WM_DELETE_WINDOW handler for toplevel shells.
 * Destroys the shell widget, which triggers destroy callbacks and allows
 * clean shutdown.  Apps can override with IswOverrideTranslations.
 */
/* ARGSUSED */
static void
ShellWMDeleteWindow(Widget w, IswEvent *iswev, String *params,
		    Cardinal *num_params)
{
    ISW_NATIVE_EVENT(iswev);   /* WM_DELETE client-message: backend-internal */
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_window;

    if ((event->response_type & ~0x80) != XCB_CLIENT_MESSAGE)
	return;

    wm_protocols = IswXcbInternAtom(_IswXcbConn(IswDisplayOf(w)), "WM_PROTOCOLS", True);
    wm_delete_window = IswXcbInternAtom(_IswXcbConn(IswDisplayOf(w)), "WM_DELETE_WINDOW", True);

    if (wm_protocols == 0 || wm_delete_window == 0)
	return;

    {
	xcb_client_message_event_t *cm = (xcb_client_message_event_t *)event;
	if (cm->type == wm_protocols && cm->data.data32[0] == wm_delete_window) {
	    if (IswIsApplicationShell(w))
		IswAppSetExitFlag(IswWidgetToApplicationContext(w));
	    else
		IswDestroyWidget(w);
	}
    }
}

static void
SetShellWMProtocolTranslations(Widget w)
{
    static IswTranslations compiled_table;	/* initially 0 */
    static IswAppContext *app_context_list;	/* initially 0 */
    static Cardinal list_size;			/* initially 0 */
    IswAppContext app_context;
    xcb_atom_t wm_delete_window;
    int i;

    app_context = IswWidgetToApplicationContext(w);

    /* parse translation table once */
    if (!compiled_table)
	compiled_table = IswParseTranslationTable(
	    "<Message>WM_PROTOCOLS: IswShellDeleteWindow()\n");

    /* add actions once per application context */
    for (i = 0; i < list_size && app_context_list[i] != app_context; i++) ;
    if (i == list_size) {
	IswActionsRec actions[1];
	actions[0].string = "IswShellDeleteWindow";
	actions[0].proc = ShellWMDeleteWindow;
	list_size++;
	app_context_list = (IswAppContext *) IswRealloc(
	    (char *)app_context_list, list_size * sizeof(IswAppContext));
	IswAppAddActions(app_context, actions, 1);
	app_context_list[i] = app_context;
    }

    /* augment so apps can override with IswOverrideTranslations */
    IswAugmentTranslations(w, compiled_table);

    /* advertise WM_DELETE_WINDOW to the window manager */
    wm_delete_window = _IswPlatformInternAtomOp(IswDisplayOf(w), "WM_DELETE_WINDOW", False);
    {
        Atom protocols[1] = { wm_delete_window };
        _IswPlatformSetWmProtocols(IswDisplayOf(w), IswWindowOf(w), protocols, 1);
    }
}

static void
Realize(xcb_connection_t *dpy, Widget wid, Mask *vmask, uint32_t *attr)
{
    ShellWidget w = (ShellWidget) wid;
    Mask mask = *vmask;

    if (!(w->shell.client_specified & _IswShellGeometryParsed)) {
        /* we'll get here only if there was no child the first
           time we were realized.  If the shell was Unrealized
           and then re-Realized, we probably don't want to
           re-evaluate the defaults anyway.
         */
        GetGeometry(wid, (Widget) NULL);
    }
    else if (w->core.background_pixmap == IswUnspecifiedPixmap) {
        /* I attempt to inherit my child's background to avoid screen flash
         * if there is latency between when I get resized and when my child
         * is resized.  Background=None is not satisfactory, as I want the
         * user to get immediate feedback on the new dimensions (most
         * particularly in the case of a non-reparenting wm).  It is
         * especially important to have the server clear any old cruft
         * from the display when I am resized larger.
         */
        register Widget *childP = w->composite.children;
        int i;

        for (i = (int) w->composite.num_children; i; i--, childP++) {
            if (IswIsWidget(*childP) && IswIsManaged(*childP)) {
                if ((*childP)->core.background_pixmap != IswUnspecifiedPixmap) {
                    mask &= (unsigned long) (~(XCB_CW_BACK_PIXEL));
                    mask |= XCB_CW_BACK_PIXMAP;
                    //attr->background_pixmap =
                    //    w->core.background_pixmap =
                    //    (*childP)->core.background_pixmap;
                }
                else {
                    //attr->background_pixel =
                    //    w->core.background_pixel =
                    //    (*childP)->core.background_pixel;
                }
                break;
            }
        }
    }

    if (w->shell.save_under)
        mask |= XCB_CW_SAVE_UNDER;
    if (w->shell.override_redirect)
        mask |= XCB_CW_OVERRIDE_REDIRECT;

    if (wid->core.width == 0 || wid->core.height == 0) {
        Cardinal count = 1;

        IswErrorMsg("invalidDimension", "shellRealize", IswCIswToolkitError,
                   "Shell widget %s has zero width and/or height",
                   &wid->core.name, &count);
    }

    /* Rebuild the XCB value list from scratch.  Subclass Realize methods
       (e.g. SimpleMenu) may have corrupted the packed array inherited from
       ComputeWindowAttributes, because the original Xlib code used an
       IswSetWindowAttributes struct with named fields, while XCB requires
       values packed in ascending bit order.  Rebuilding here is the only
       safe approach. */
    {
        uint32_t vals[16];
        int vi = 0;

        /* CW_BACK_PIXMAP (bit 0) */
        if (mask & XCB_CW_BACK_PIXMAP)
            vals[vi++] = wid->core.background_pixmap;
        /* CW_BACK_PIXEL (bit 1) */
        if (mask & XCB_CW_BACK_PIXEL)
            vals[vi++] = wid->core.background_pixel;
        /* CW_BORDER_PIXMAP (bit 2) */
        if (mask & XCB_CW_BORDER_PIXMAP)
            vals[vi++] = wid->core.border_pixmap;
        /* CW_BORDER_PIXEL (bit 3) */
        if (mask & XCB_CW_BORDER_PIXEL)
            vals[vi++] = wid->core.border_pixel;
        /* CW_BIT_GRAVITY (bit 4) */
        if (mask & XCB_CW_BIT_GRAVITY)
            vals[vi++] = XCB_GRAVITY_NORTH_WEST;
        /* CW_WIN_GRAVITY (bit 5) */
        if (mask & XCB_CW_WIN_GRAVITY)
            vals[vi++] = XCB_GRAVITY_NORTH_WEST;
        /* Bits 6-10 and 14 (backing_store, override_redirect, save_under,
           cursor) are applied via xcb_change_window_attributes after
           creation — strip them from the create mask to keep things simple. */
        Mask create_mask = mask & ~(XCB_CW_BACKING_STORE | XCB_CW_OVERRIDE_REDIRECT
                                    | XCB_CW_SAVE_UNDER | XCB_CW_CURSOR);
        /* CW_EVENT_MASK (bit 11) */
        if (create_mask & XCB_CW_EVENT_MASK)
            vals[vi++] = IswBuildEventMask(wid);
        /* CW_DONT_PROPAGATE (bit 12) — not used */
        /* CW_COLORMAP (bit 13) */
        if (create_mask & XCB_CW_COLORMAP)
            vals[vi++] = _IswXcbColormap(wid->core.colormap);

        /* HiDPI: create window at physical pixel geometry */
        {
            double sf = _IswGetScaleFactor(IswDisplayOf(wid));
            wid->core.window = _IswXcbWindowWrap(xcb_generate_id(_IswXcbConn(IswDisplayOf(wid))));
            xcb_create_window(
                _IswXcbConn(IswDisplayOf(wid)),
                wid->core.depth,
                _IswXcbWindow(wid->core.window),
                _IswXcbScreen(wid->core.screen)->root,
                (int16_t)(wid->core.x * sf + 0.5),
                (int16_t)(wid->core.y * sf + 0.5),
                (uint16_t)(wid->core.width * sf + 0.5),
                (uint16_t)(wid->core.height * sf + 0.5),
                (uint16_t)(wid->core.border_width * sf + 0.5),
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                w->shell.visual,
                create_mask,
                vals
            );
        }

        /* Apply shell attributes that were stripped from the create mask */
        {
            uint32_t post_mask = 0;
            uint32_t post_vals[2];
            int pi = 0;
            if (w->shell.override_redirect) {
                post_mask |= XCB_CW_OVERRIDE_REDIRECT;
                post_vals[pi++] = 1;
            }
            if (w->shell.save_under) {
                post_mask |= XCB_CW_SAVE_UNDER;
                post_vals[pi++] = 1;
            }
            if (post_mask)
                xcb_change_window_attributes(_IswXcbConn(IswDisplayOf(wid)), _IswXcbWindow(wid->core.window),
                                             post_mask, post_vals);
        }
        /* Set a themed default cursor on the shell window so child
           widgets that don't set their own cursor inherit the theme's
           left_ptr instead of the X server's default glyph cursor. */
        {
            xcb_cursor_t cursor = _IswLoadThemedCursor(
                _IswXcbConn(IswDisplayOf(wid)), _IswXcbScreen(wid->core.screen), "left_ptr", XC_left_ptr);
            if (cursor != XCB_NONE)
                _IswSetWindowCursor(wid, cursor);
        }
    }
    xcb_flush(_IswXcbConn(IswDisplayOf(wid)));

    _popup_set_prop(w);

    /* Enable XDND drop target on WM-managed shell windows.
     * Override-redirect shells (WM frames, menus, tooltips) must NOT
     * advertise XdndAware — otherwise drag sources target the frame
     * instead of the client window inside it. */
    if (!w->shell.override_redirect)
        IswDndEnable(wid);

    /* Set up default WM_DELETE_WINDOW handling for WM-managed shells */
    if (!w->shell.override_redirect)
	SetShellWMProtocolTranslations(wid);
}

static void
_SetTransientForHint(TransientShellWidget w, Boolean delete)
{
    xcb_window_t window_group;

    if (w->wm.transient) {
        if (w->transient.transient_for != NULL
            && IswIsRealized(w->transient.transient_for))
            window_group = _IswXcbWindow(IswWindowOf(w->transient.transient_for));
        else if ((window_group = w->wm.wm_hints.window_group)
                 == IswUnspecifiedWindowGroup) {
            if (delete) {
                Atom transient_for = _IswPlatformInternAtomOp(
                    IswDisplayOf((Widget) w), "WM_TRANSIENT_FOR", False);
                _IswPlatformDeleteProperty(IswDisplayOf((Widget) w),
                                           IswWindowOf((Widget) w), transient_for);
            }
            return;
        }

        _IswPlatformSetTransientFor(IswDisplayOf((Widget) w),
                                    IswWindowOf((Widget) w),
                                    _IswXcbWindowWrap(window_group));
    }
}

static void
TransientRealize(xcb_connection_t *dpy, Widget w, Mask *vmask, uint32_t *attr)
{
    IswRealizeProc realize;

    LOCK_PROCESS;
    realize =
        transientShellWidgetClass->core_class.superclass->core_class.realize;
    UNLOCK_PROCESS;
    (*realize) (_IswXcbConn(IswDisplayOf(w)), w, vmask, attr);

    _SetTransientForHint((TransientShellWidget) w, False);
}

static Widget
GetClientLeader(Widget w)
{
    while ((!IswIsWMShell(w) || !((WMShellWidget) w)->wm.client_leader)
           && w->core.parent)
        w = w->core.parent;

    /* ASSERT: w is a WMshell with client_leader set, or w has no parent */

    if (IswIsWMShell(w) && ((WMShellWidget) w)->wm.client_leader)
        w = ((WMShellWidget) w)->wm.client_leader;
    return w;
}

static void
EvaluateWMHints(WMShellWidget w)
{
    xcb_icccm_wm_hints_t *hintp = &w->wm.wm_hints;

    hintp->flags = XCB_ICCCM_WM_HINT_STATE | XCB_ICCCM_WM_HINT_INPUT;

    if (hintp->icon_x == IswUnspecifiedShellInt)
        hintp->icon_x = -1;
    else
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_POSITION;

    if (hintp->icon_y == IswUnspecifiedShellInt)
        hintp->icon_y = -1;
    else
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_POSITION;

    if (hintp->icon_pixmap != None)
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_PIXMAP;
    if (hintp->icon_mask != None)
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_MASK;
    if (hintp->icon_window != None)
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_WINDOW;

    if (hintp->window_group == IswUnspecifiedWindow) {
        if (w->core.parent) {
            Widget p;

            for (p = w->core.parent; p->core.parent; p = p->core.parent);
            if (IswIsRealized(p)) {
                hintp->window_group = _IswXcbWindow(IswWindowOf(p));
                hintp->flags |= XCB_ICCCM_WM_HINT_WINDOW_GROUP;
            }
        }
    }
    else if (hintp->window_group != IswUnspecifiedWindowGroup)
        hintp->flags |= XCB_ICCCM_WM_HINT_WINDOW_GROUP;

    if (w->wm.urgency)
        hintp->flags |= XCB_ICCCM_WM_HINT_X_URGENCY;
}

static void
EvaluateSizeHints(WMShellWidget w)
{
    struct _OldXSizeHints *sizep = &w->wm.size_hints;

    sizep->x = w->core.x;
    sizep->y = w->core.y;
    sizep->width = w->core.width;
    sizep->height = w->core.height;

    if (sizep->flags & XCB_ICCCM_SIZE_HINT_US_SIZE) {
        if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_SIZE)
            sizep->flags &= ~XCB_ICCCM_SIZE_HINT_P_SIZE;
    }
    else
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_SIZE;

    if (sizep->flags & XCB_ICCCM_SIZE_HINT_US_POSITION) {
        if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_POSITION)
            sizep->flags &= ~XCB_ICCCM_SIZE_HINT_P_POSITION;
    }
    else if (w->shell.client_specified & _IswShellPPositionOK)
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_POSITION;

    if (sizep->min_aspect.x != IswUnspecifiedShellInt
        || sizep->min_aspect.y != IswUnspecifiedShellInt
        || sizep->max_aspect.x != IswUnspecifiedShellInt
        || sizep->max_aspect.y != IswUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_ASPECT;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE
        || w->wm.base_width != IswUnspecifiedShellInt
        || w->wm.base_height != IswUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_BASE_SIZE;
        if (w->wm.base_width == IswUnspecifiedShellInt)
            w->wm.base_width = 0;
        if (w->wm.base_height == IswUnspecifiedShellInt)
            w->wm.base_height = 0;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC
        || sizep->width_inc != IswUnspecifiedShellInt
        || sizep->height_inc != IswUnspecifiedShellInt) {
        if (sizep->width_inc < 1)
            sizep->width_inc = 1;
        if (sizep->height_inc < 1)
            sizep->height_inc = 1;
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_RESIZE_INC;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE
        || sizep->max_width != IswUnspecifiedShellInt
        || sizep->max_height != IswUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
        if (sizep->max_width == IswUnspecifiedShellInt)
            sizep->max_width = BIGSIZE;
        if (sizep->max_height == IswUnspecifiedShellInt)
            sizep->max_height = BIGSIZE;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE
        || sizep->min_width != IswUnspecifiedShellInt
        || sizep->min_height != IswUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_MIN_SIZE;
        if (sizep->min_width == IswUnspecifiedShellInt)
            sizep->min_width = 1;
        if (sizep->min_height == IswUnspecifiedShellInt)
            sizep->min_height = 1;
    }
}

static void
_popup_set_prop(ShellWidget w)
{
    Widget p;
    WMShellWidget wmshell = (WMShellWidget) w;
    TopLevelShellWidget tlshell = (TopLevelShellWidget) w;
    ApplicationShellWidget appshell = (ApplicationShellWidget) w;
    char * icon_name;
    char * window_name;
    char **argv;
    int argc;
    xcb_size_hints_t *size_hints;
    xcb_window_t window_group;

    if (!IswIsWMShell((Widget) w) || w->shell.override_redirect)
        return;

    size_hints = calloc(1, sizeof(xcb_size_hints_t));
    if (size_hints == NULL)
        _IswAllocError("xcb_size_hints_t");

    /* Use the title/icon_name strings directly.
     * The original code used XmbTextListToTextProperty for locale-aware
     * encoding, which is Xlib-specific and has no XCB equivalent.
     * For now, use the raw strings directly. */
    window_name = (char *) wmshell->wm.title;
    if (IswIsTopLevelShell((Widget) w))
        icon_name = (char *) tlshell->topLevel.icon_name;
    else
        icon_name = NULL;

    EvaluateWMHints(wmshell);
    EvaluateSizeHints(wmshell);
    ComputeWMSizeHints(wmshell, size_hints);

    if (wmshell->wm.transient && !IswIsTransientShell((Widget) w)
        && (window_group = wmshell->wm.wm_hints.window_group)
        != IswUnspecifiedWindowGroup) {

        _IswPlatformSetTransientFor(IswDisplayOf((Widget) w), IswWindowOf(w),
                                    _IswXcbWindowWrap(window_group));
    }

    XClassHint classhint;
    classhint.res_name = (_IswString) w->core.name;
    for (p = (Widget) w; p->core.parent != NULL; p = p->core.parent);
    if (IswIsApplicationShell(p)) {
        classhint.res_class = ((ApplicationShellWidget) p)->application.class;
    }
    else {
        LOCK_PROCESS;
        classhint.res_class = (_IswString) IswClass(p)->core_class.class_name;
        UNLOCK_PROCESS;
    }

    if (IswIsApplicationShell((Widget) w)
        && (argc = appshell->application.argc) != -1)
        argv = (char **) appshell->application.argv;
    else {
        argv = NULL;
        argc = 0;
    }

    SetWMProperties((Widget)w, window_name,
                (IswIsTopLevelShell((Widget) w)) ? icon_name : NULL,
                argv, argc, size_hints, &wmshell->wm.wm_hints,
                (char *)classhint.res_class, (char *)classhint.res_name);
    if(size_hints) free(size_hints);
    //IswFree((char *) size_hints);
    //if (copied_wname)
    //    IswFree((IswPointer) window_name.value);
    //if (copied_iname)
    //    IswFree((IswPointer) icon_name.value);

    LOCK_PROCESS;
    if (IswWidgetToApplicationContext((Widget) w)->langProcRec.proc) {
        const char *locale = "C"; //setlocale(LC_CTYPE, (char *) NULL);

        if (locale) {
            Atom wm_locale = _IswPlatformInternAtomOp(IswDisplayOf((Widget) w),
                                                      "WM_LOCALE_NAME", False);
            _IswPlatformChangeProperty(IswDisplayOf((Widget) w),
                                       IswWindowOf((Widget) w), wm_locale,
                                       ISW_ATOM_STRING, 8, ISW_PROP_MODE_REPLACE,
                                       locale, (uint32_t) strlen(locale));
        }
            //XChangeProperty(IswDisplayOf((Widget) w), IswWindowOf((Widget) w),
            //                XInternAtom(IswDisplayOf((Widget) w),
            //                            "WM_LOCALE_NAME", False),
            //                XCB_ATOM_STRING, 8, XCB_PROP_MODE_REPLACE,
            //                (unsigned char *) locale, (int) strlen(locale));
    }
    UNLOCK_PROCESS;

    p = GetClientLeader((Widget) w);
    if (_IswXcbWindow(IswWindowOf(p))) {
        xcb_window_t leader_win = _IswXcbWindow(p->core.window);
        Atom client_leader = _IswPlatformInternAtomOp(IswDisplayOf((Widget) w),
                                                      "WM_CLIENT_LEADER", False);
        _IswPlatformChangeProperty(IswDisplayOf((Widget) w),
                                   IswWindowOf((Widget) w), client_leader,
                                   ISW_ATOM_WINDOW, 32, ISW_PROP_MODE_REPLACE,
                                   &leader_win, 1);
    }
    if (wmshell->wm.window_role) {
        Atom window_role = _IswPlatformInternAtomOp(IswDisplayOf((Widget) w),
                                                    "WM_WINDOW_ROLE", False);
        _IswPlatformChangeProperty(IswDisplayOf((Widget) w),
                                   IswWindowOf((Widget) w), window_role,
                                   ISW_ATOM_STRING, 8, ISW_PROP_MODE_REPLACE,
                                   wmshell->wm.window_role,
                                   (uint32_t) strlen(wmshell->wm.window_role));
    }

    {
        xcb_connection_t *conn = _IswXcbConn(IswDisplayOf((Widget) w));
        xcb_window_t win = _IswXcbWindow(IswWindowOf((Widget) w));

        /* _NET_WM_PID */
        _IswPlatformSetPid(IswDisplayOf((Widget) w), IswWindowOf((Widget) w),
                           (uint32_t) getpid());

        /* _NET_WM_WINDOW_TYPE */
        _IswPlatformSetWindowType(IswDisplayOf((Widget) w),
                                  IswWindowOf((Widget) w),
                                  IswIsTransientShell((Widget) w)
                                      ? ISW_WINDOW_TYPE_DIALOG
                                      : ISW_WINDOW_TYPE_NORMAL);

        /* _NET_WM_USER_TIME_WINDOW */
        if (IswIsWMShell((Widget) w)) {
            IswPerDisplay pd = _IswGetPerDisplay(IswDisplayOf((Widget) w));
            if (!pd->net_wm_user_time) {
                pd->net_wm_user_time = _IswPlatformInternAtomOp(
                    IswDisplayOf((Widget) w), "_NET_WM_USER_TIME", False);
                pd->net_wm_user_time_window = _IswPlatformInternAtomOp(
                    IswDisplayOf((Widget) w), "_NET_WM_USER_TIME_WINDOW", False);
            }

            if (pd->net_wm_user_time && pd->net_wm_user_time_window) {
                xcb_window_t utwin = xcb_generate_id(conn);
                xcb_create_window(conn, XCB_COPY_FROM_PARENT, utwin, win,
                                  -1, -1, 1, 1, 0,
                                  XCB_WINDOW_CLASS_INPUT_ONLY,
                                  XCB_COPY_FROM_PARENT, 0, NULL);
                wmshell->wm.user_time_win = utwin;

                _IswPlatformChangeProperty(IswDisplayOf((Widget) w),
                                    IswWindowOf((Widget) w),
                                    pd->net_wm_user_time_window, ISW_ATOM_WINDOW, 32,
                                    ISW_PROP_MODE_REPLACE, &utwin, 1);

                uint32_t initial_time = pd->last_timestamp;
                _IswPlatformChangeProperty(IswDisplayOf((Widget) w),
                                    _IswXcbWindowWrap(utwin),
                                    pd->net_wm_user_time, ISW_ATOM_CARDINAL, 32,
                                    ISW_PROP_MODE_REPLACE, &initial_time, 1);
            }
        }

        /* _NET_STARTUP_ID — set property and send remove message.  The atoms
           come from the atom op + the property via the generic op; the
           broadcast client-message loop (xcb_send_event) stays on the seam. */
        if (wmshell->wm.startup_id) {
            IswDisplay sdpy = IswDisplayOf((Widget) w);
            Atom sid_atom  = _IswPlatformInternAtomOp(sdpy, "_NET_STARTUP_ID", False);
            Atom utf8_atom = _IswPlatformInternAtomOp(sdpy, "UTF8_STRING", False);
            Atom sib_atom  = _IswPlatformInternAtomOp(sdpy, "_NET_STARTUP_INFO_BEGIN", False);
            Atom si_atom   = _IswPlatformInternAtomOp(sdpy, "_NET_STARTUP_INFO", False);

            if (sid_atom && utf8_atom) {
                _IswPlatformChangeProperty(sdpy, IswWindowOf((Widget) w),
                                    sid_atom, utf8_atom, 8, ISW_PROP_MODE_REPLACE,
                                    wmshell->wm.startup_id,
                                    (uint32_t) strlen(wmshell->wm.startup_id));
            }

            if (sib_atom && si_atom) {
                char msg[256];
                int len = snprintf(msg, sizeof(msg), "remove: ID=%s",
                                   wmshell->wm.startup_id);
                if (len > 0 && (size_t)len < sizeof(msg)) {
                    len++;  /* include NUL terminator */
                    xcb_window_t root = _IswXcbScreen(w->core.screen)->root;
                    const char *mp = msg;
                    int remaining = len;

                    while (remaining > 0) {
                        xcb_client_message_event_t ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.response_type = XCB_CLIENT_MESSAGE;
                        ev.format = 8;
                        ev.window = win;
                        ev.type = (mp == msg) ? (xcb_atom_t) sib_atom
                                              : (xcb_atom_t) si_atom;

                        int chunk = remaining > 20 ? 20 : remaining;
                        memcpy(ev.data.data8, mp, chunk);

                        xcb_send_event(conn, FALSE, root,
                                       XCB_EVENT_MASK_PROPERTY_CHANGE,
                                       (const char *) &ev);
                        mp += chunk;
                        remaining -= chunk;
                    }
                }
            }

            IswFree(wmshell->wm.startup_id);
            wmshell->wm.startup_id = NULL;
        }
    }
}

static void
EventHandler(Widget wid,
             IswPointer closure _X_UNUSED,
             IswEvent *iswev,
             Boolean *continue_to_dispatch _X_UNUSED)
{
    ISW_NATIVE_EVENT(iswev);   /* WM configure/client-message: backend-internal */
    register ShellWidget w = (ShellWidget) wid;
    WMShellWidget wmshell = (WMShellWidget) w;
    Boolean sizechanged = FALSE;

    switch (event->response_type) {
    case XCB_CONFIGURE_NOTIFY: {
        xcb_configure_notify_event_t * cne = (xcb_configure_notify_event_t *)event;
        if (_IswXcbWindow(w->core.window) != cne->window)
            return;             /* in case of SubstructureNotify */
        /* ConfigureNotify values are already descaled to logical pixels
         * by _IswDescaleEventCoords in the event dispatcher. */
        if (w->core.width != cne->width || w->core.height != cne->height ||
            w->core.border_width != cne->border_width) {
            sizechanged = TRUE;
            w->core.width = (Dimension) cne->width;
            w->core.height = (Dimension) cne->height;
            w->core.border_width = (Dimension) cne->border_width;
        }
        if (w->shell.client_specified & _IswShellNotReparented) {
            w->core.x = (Position) cne->x;
            w->core.y = (Position) cne->y;
            w->shell.client_specified |= _IswShellPositionValid;
        }
        else
            w->shell.client_specified &= ~_IswShellPositionValid;
        if (IswIsWMShell(wid) && !wmshell->wm.wait_for_wm) {
            /* Consider trusting the wm again */
            register struct _OldXSizeHints *hintp = &wmshell->wm.size_hints;

#define EQ(x) (hintp->x == w->core.x)
            if (EQ(x) && EQ(y) && EQ(width) && EQ(height)) {
                wmshell->wm.wait_for_wm = TRUE;
            }
#undef EQ
        }
        break;
    }

    case XCB_REPARENT_NOTIFY:
        xcb_reparent_notify_event_t *rne = (xcb_reparent_notify_event_t *)event;
        if (rne->window == _IswXcbWindow(IswWindowOf(w))) {
            if (rne->parent != RootWindowOfScreen(_IswXcbScreen(IswScreenOf(w))))
                w->shell.client_specified &=
                    ~(_IswShellNotReparented | _IswShellPositionValid);
            else {
                w->core.x = (Position) rne->x;
                w->core.y = (Position) rne->y;
                w->shell.client_specified |=
                    (_IswShellNotReparented | _IswShellPositionValid);
            }
        }
        return;

    case XCB_MAP_NOTIFY:
        if (IswIsTopLevelShell(wid)) {
            ((TopLevelShellWidget) wid)->topLevel.iconic = FALSE;
        }
        return;

    case XCB_UNMAP_NOTIFY:
    {
        IswPerDisplayInput pdi;
        IswDevice device;
        Widget p;

        if (IswIsTopLevelShell(wid))
            ((TopLevelShellWidget) wid)->topLevel.iconic = TRUE;

        //getting display via Shell widget as the event 
        //doesn't carry the pointer in XCB
        //presumably a shellwidget per display and the events are routed appropriately
        //otherwise this may break
        pdi = _IswGetPerDisplayInput(IswDisplayOf(w));

        device = &pdi->pointer;

        if (device->grabType == IswPassiveServerGrab) {
            p = device->grab.widget;
            while (p && !(IswIsShell(p)))
                p = p->core.parent;
            if (p == wid)
                device->grabType = IswNoServerGrab;
        }

        device = &pdi->keyboard;
        if (IsEitherPassiveGrab(device->grabType)) {
            p = device->grab.widget;
            while (p && !(IswIsShell(p)))
                p = p->core.parent;
            if (p == wid) {
                device->grabType = IswNoServerGrab;
                pdi->activatingKey = 0;
            }
        }

        return;
    }
    default:
        return;
    }
    {
        IswWidgetProc resize;

        LOCK_PROCESS;
        resize = IswClass(wid)->core_class.resize;
        UNLOCK_PROCESS;

        if (sizechanged && resize) {
            CALLGEOTAT(_IswGeoTrace((Widget) w,
                                   "Shell \"%s\" is being resized to %d %d.\n",
                                   IswName(wid), wid->core.width,
                                   wid->core.height));
            (*resize) (wid);
        }
    }
}

static void
Destroy(Widget wid)
{
    if (IswIsRealized(wid))
        xcb_destroy_window(_IswXcbConn(IswDisplayOf(wid)), _IswXcbWindow(IswWindowOf(wid)));
}

static void
WMDestroy(Widget wid)
{
    WMShellWidget w = (WMShellWidget) wid;

    if (w->wm.user_time_win) {
        xcb_destroy_window(_IswXcbConn(IswDisplayOf(wid)), w->wm.user_time_win);
        w->wm.user_time_win = 0;
    }
    IswFree((char *) w->wm.title);
    IswFree((char *) w->wm.window_role);
}

static void
TopLevelDestroy(Widget wid)
{
    TopLevelShellWidget w = (TopLevelShellWidget) wid;

    IswFree((char *) w->topLevel.icon_name);
}

static void
ApplicationDestroy(Widget wid)
{
    ApplicationShellWidget w = (ApplicationShellWidget) wid;

    if (w->application.argc > 0)
        FreeStringArray(w->application.argv);
}

/* -----------------------------------------------------------------------
 * Geometry parse flags (same values as Xlib XValue/YValue/etc.)
 * ----------------------------------------------------------------------- */
#define IswNoValue     0x0000
#define IswXValue      0x0001
#define IswYValue      0x0002
#define IswWidthValue  0x0004
#define IswHeightValue 0x0008
#define IswAllValues   0x000F
#define IswNegative    0x0010

/* -----------------------------------------------------------------------
 * _IswParseGeometry: pure string parser for geometry strings like
 * "800x600+100+200" or "=800x600+100+200".
 * Returns a bitmask of which fields were specified.
 * ----------------------------------------------------------------------- */
static int
_IswParseGeometry(const char *string,
                 int *x, int *y,
                 unsigned int *width, unsigned int *height)
{
    int mask = IswNoValue;
    const char *s = string;
    char *end;
    long val;

    if (s == NULL || *s == '\0') return mask;

    /* Skip optional '=' */
    if (*s == '=') s++;

    /* Width */
    if (*s != '+' && *s != '-' && *s != '\0') {
        val = strtol(s, &end, 10);
        if (end != s) {
            *width = (unsigned int) val;
            mask |= IswWidthValue;
            s = end;
        }
        /* Height */
        if (*s == 'x' || *s == 'X') {
            s++;
            val = strtol(s, &end, 10);
            if (end != s) {
                *height = (unsigned int) val;
                mask |= IswHeightValue;
                s = end;
            }
        }
    }

    /* X offset */
    if (*s == '+' || *s == '-') {
        int negative = (*s == '-');
        s++;
        val = strtol(s, &end, 10);
        if (end != s) {
            *x = negative ? -(int) val : (int) val;
            mask |= IswXValue;
            if (negative) mask |= IswNegative;
            s = end;
        }
        /* Y offset */
        if (*s == '+' || *s == '-') {
            negative = (*s == '-');
            s++;
            val = strtol(s, &end, 10);
            if (end != s) {
                *y = negative ? -(int) val : (int) val;
                mask |= IswYValue;
                s = end;
            }
        }
    }

    return mask;
}

/* -----------------------------------------------------------------------
 * _IswWMGeometry: XWMGeometry equivalent using _IswParseGeometry.
 * Parses geometry strings and applies size hints.
 * ----------------------------------------------------------------------- */
static int
_IswWMGeometry(xcb_screen_t *screen _X_UNUSED,
              const char *user_geom,
              const char *def_geom,
              unsigned int border_width _X_UNUSED,
              xcb_size_hints_t *hints,
              int *x_return, int *y_return,
              int *width_return, int *height_return,
              int *gravity_return)
{
    int x = 0, y = 0;
    unsigned int width = 0, height = 0;
    int mask = 0;

    /* Parse default geometry first */
    if (def_geom != NULL)
        _IswParseGeometry(def_geom, &x, &y, &width, &height);

    /* Override with user geometry */
    if (user_geom != NULL) {
        int ux = 0, uy = 0;
        unsigned int uw = 0, uh = 0;
        int umask = _IswParseGeometry(user_geom, &ux, &uy, &uw, &uh);
        if (umask & IswXValue)      { x = ux; }
        if (umask & IswYValue)      { y = uy; }
        if (umask & IswWidthValue)  { width = uw; }
        if (umask & IswHeightValue) { height = uh; }
        mask = umask;
    }

    /* Apply size hints increments */
    if (hints != NULL && (hints->flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)) {
        width  *= hints->width_inc;
        height *= hints->height_inc;
    }
    if (hints != NULL && (hints->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE)) {
        width  += hints->base_width;
        height += hints->base_height;
    } else if (hints != NULL && (hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
        width  += hints->min_width;
        height += hints->min_height;
    }

    *x_return = x;
    *y_return = y;
    *width_return  = (int) width;
    *height_return = (int) height;
    *gravity_return = (hints != NULL && (hints->flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY))
                      ? hints->win_gravity : XCB_GRAVITY_NORTH_WEST;

    return mask;
}

/*
 * If the Shell has a width and a height which are zero, and as such
 * suspect, and it has not yet been realized then it will grow to
 * match the child before parsing the geometry resource.
 *
 */
static void
GetGeometry(Widget W, Widget child)
{
    register ShellWidget w = (ShellWidget) W;
    Boolean is_wmshell = IswIsWMShell(W);
    int x, y, width, height, win_gravity = -1, flag;
    xcb_size_hints_t hints;

    if (child != NULL) {
        /* we default to our child's size */
        if (is_wmshell && (w->core.width == 0 || w->core.height == 0))
            ((WMShellWidget) W)->wm.size_hints.flags |= XCB_ICCCM_SIZE_HINT_P_SIZE;
        if (w->core.width == 0)
            w->core.width = child->core.width;
        if (w->core.height == 0)
            w->core.height = child->core.height;
    }
    if (w->shell.geometry != NULL) {
        char def_geom[64];

        x = w->core.x;
        y = w->core.y;
        width = w->core.width;
        height = w->core.height;
        if (is_wmshell) {
            WMShellPart *wm = &((WMShellWidget) w)->wm;

            EvaluateSizeHints((WMShellWidget) w);
            (void) memcpy(&hints, &wm->size_hints,
                          sizeof(struct _OldXSizeHints));
            hints.win_gravity = wm->win_gravity;
            if (wm->size_hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
                width -= wm->base_width;
                height -= wm->base_height;
                hints.base_width = wm->base_width;
                hints.base_height = wm->base_height;
            }
            else if (wm->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
                width -= wm->size_hints.min_width;
                height -= wm->size_hints.min_height;
            }
            if (wm->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
                width /= wm->size_hints.width_inc;
                height /= wm->size_hints.height_inc;
            }
        }
        else
            hints.flags = 0;

        snprintf(def_geom, sizeof(def_geom), "%dx%d+%d+%d",
                 width, height, x, y);
        flag = _IswWMGeometry(_IswXcbScreen(IswScreenOf(W)),
                             w->shell.geometry, def_geom,
                             (unsigned int) w->core.border_width,
                             &hints, &x, &y, &width, &height, &win_gravity);
        if (flag) {
            if (flag & XValue)
                w->core.x = (Position) x;
            if (flag & YValue)
                w->core.y = (Position) y;
            if (flag & WidthValue)
                w->core.width = (Dimension) width;
            if (flag & HeightValue)
                w->core.height = (Dimension) height;
        }
        else {
            String params[2];
            Cardinal num_params = 2;

            params[0] = IswName(W);
            params[1] = w->shell.geometry;
            IswAppWarningMsg(IswWidgetToApplicationContext(W),
                            "badGeometry", "shellRealize", IswCIswToolkitError,
                            "Shell widget \"%s\" has an invalid geometry specification: \"%s\"",
                            params, &num_params);
        }
    }
    else
        flag = 0;

    if (is_wmshell) {
        WMShellWidget wmshell = (WMShellWidget) w;

        if (wmshell->wm.win_gravity == IswUnspecifiedShellInt) {
            if (win_gravity != -1)
                wmshell->wm.win_gravity = win_gravity;
            else
                wmshell->wm.win_gravity = XCB_GRAVITY_NORTH_WEST;
        }
        wmshell->wm.size_hints.flags |= XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY;
        if ((flag & (XValue | YValue)) == (XValue | YValue))
            wmshell->wm.size_hints.flags |= XCB_ICCCM_SIZE_HINT_US_POSITION;
        if ((flag & (WidthValue | HeightValue)) == (WidthValue | HeightValue))
            wmshell->wm.size_hints.flags |= XCB_ICCCM_SIZE_HINT_US_SIZE;
    }
    w->shell.client_specified |= _IswShellGeometryParsed;
}

static void
ChangeManaged(Widget wid)
{
    ShellWidget w = (ShellWidget) wid;
    Widget child = NULL;
    Cardinal i;

    for (i = 0; i < w->composite.num_children; i++) {
        if (IswIsManaged(w->composite.children[i])) {
            child = w->composite.children[i];
            break;              /* there can only be one of them! */
        }
    }

    if (!IswIsRealized(wid))     /* then we're about to be realized... */
        GetGeometry(wid, child);

    if (child != NULL)
        IswConfigureWidget(child, (Position) 0, (Position) 0,
                          w->core.width, w->core.height, (Dimension) 0);

    _IswFocusMgrEnsureInstalled(wid);
}

/*
 * This is gross, I can't wait to see if the change happened so I will ask
 * the window manager to change my size and do the appropriate X work.
 * I will then tell the requester that he can.  Care must be taken because
 * it is possible that some time in the future the request will be
 * asynchronusly denied and the window reverted to it's old size/shape.
 */

static IswGeometryResult
GeometryManager(Widget wid,
                IswWidgetGeometry *request,
                IswWidgetGeometry *reply _X_UNUSED)
{
    ShellWidget shell = (ShellWidget) (wid->core.parent);
    IswWidgetGeometry my_request;

    if (shell->shell.allow_shell_resize == FALSE && IswIsRealized(wid))
        return (IswGeometryNo);

    if (request->request_mode & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y))
        return (IswGeometryNo);

    my_request.request_mode = (request->request_mode & IswCWQueryOnly);
    if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH) {
        my_request.width = request->width;
        my_request.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
    }
    if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) {
        my_request.height = request->height;
        my_request.request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
    }
    if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        my_request.border_width = request->border_width;
        my_request.request_mode |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    }
    if (IswMakeGeometryRequest((Widget) shell, &my_request, NULL)
        == IswGeometryYes) {
        /* assert: if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH) then
         *            shell->core.width == request->width
         * assert: if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) then
         *            shell->core.height == request->height
         *
         * so, whatever the WM sized us to (if the Shell requested
         * only one of the two) is now the correct child size
         */

        if (!(request->request_mode & IswCWQueryOnly)) {
            wid->core.width = shell->core.width;
            wid->core.height = shell->core.height;
            if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
                wid->core.x = wid->core.y = (Position) (-request->border_width);
            }
        }
        return IswGeometryYes;
    }
    else
        return IswGeometryNo;
}

typedef struct {
    Widget w;
    unsigned long request_num;
    Boolean done;
} QueryStruct;

//static Bool
//isMine(xcb_connection_t *dpy, register xcb_generic_event_t *event, char *arg)
//{
//    QueryStruct *q = (QueryStruct *) arg;
//    register Widget w = q->w;
//
//    if ((dpy != IswDisplayOf(w)) || (rne->window != IswWindowOf(w))) {
//        return FALSE;
//    }
//
//    if (event->sequence >= q->request_num) {
//        if (event->response_type == XCB_CONFIGURE_NOTIFY) {
//            q->done = TRUE;
//            return TRUE;
//        }
//    }
//    else if (event->response_type == XCB_CONFIGURE_NOTIFY)
//        return TRUE;            /* flush old events */
//
//    if (event->response_type == XCB_REPARENT_NOTIFY) {
//        xcb_reparent_notify_event_t * rne = (xcb_reparent_notify_event_t *)event;
//        if (rne->window == IswWindowOf(w)) {
//        /* we might get ahead of this event, so just in case someone
//         * asks for coordinates before this event is dispatched...
//         */
//        register ShellWidget s = (ShellWidget) w;
//
//        if (rne->parent != RootWindowOfScreen(IswScreenOf(w)))
//            s->shell.client_specified &= ~_IswShellNotReparented;
//        else
//            s->shell.client_specified |= _IswShellNotReparented;
//        }
//    }
//
//    return FALSE;
//}

static Boolean
_wait_for_response(ShellWidget w, xcb_generic_event_t **event_out,
                   unsigned long request_num)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(w));
    xcb_window_t win = _IswXcbWindow(IswWindowOf(w));
    unsigned long timeout;
    struct timespec start, now;

    if (IswIsWMShell((Widget) w))
        timeout = (unsigned long) ((WMShellWidget) w)->wm.wm_timeout;
    else
        timeout = DEFAULT_WM_TIMEOUT;

    xcb_flush(conn);
    clock_gettime(CLOCK_MONOTONIC, &start);

    *event_out = NULL;

    for (;;) {
        xcb_generic_event_t *ev = (xcb_generic_event_t *)
            _IswPlatformPollEvent((IswDisplay) conn);
        if (ev) {
            uint8_t type = ev->response_type & ~0x80;
            if (type == XCB_CONFIGURE_NOTIFY) {
                xcb_configure_notify_event_t *cne =
                    (xcb_configure_notify_event_t *) ev;
                if (cne->window == win && ev->sequence >= request_num) {
                    *event_out = ev;
                    return TRUE;
                }
            }
            /* Handle reparent events inline — track reparenting state */
            if (type == XCB_REPARENT_NOTIFY) {
                xcb_reparent_notify_event_t *rne =
                    (xcb_reparent_notify_event_t *) ev;
                if (rne->window == win) {
                    if (rne->parent != RootWindowOfScreen(_IswXcbScreen(IswScreenOf(w))))
                        w->shell.client_specified &= ~_IswShellNotReparented;
                    else
                        w->shell.client_specified |= _IswShellNotReparented;
                }
            }
            free(ev);
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        unsigned long elapsed_ms =
            (unsigned long)(now.tv_sec - start.tv_sec) * 1000 +
            (unsigned long)(now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed_ms >= timeout)
            return FALSE;

        /* Brief sleep to avoid busy-spinning */
        struct timespec sleep_ts = { 0, 1000000 }; /* 1ms */
        nanosleep(&sleep_ts, NULL);
    }
}

static IswGeometryResult
RootGeometryManager(Widget gw,
                    IswWidgetGeometry *request,
                    IswWidgetGeometry *reply _X_UNUSED)
{
    register ShellWidget w = (ShellWidget) gw;
    xcb_configure_window_value_list_t values;
    unsigned int mask = request->request_mode;
    xcb_generic_event_t *event = NULL;
    Boolean wm;
    register struct _OldXSizeHints *hintp = NULL;
    int oldx, oldy, oldwidth, oldheight, oldborder_width;
    unsigned long request_num;

    CALLGEOTAT(_IswGeoTab(1));

    if (IswIsWMShell(gw)) {
        wm = True;
        hintp = &((WMShellWidget) w)->wm.size_hints;
        /* for draft-ICCCM wm's, need to make sure hints reflect
           (current) reality so client can move and size separately. */
        hintp->x = w->core.x;
        hintp->y = w->core.y;
        hintp->width = w->core.width;
        hintp->height = w->core.height;
    }
    else
        wm = False;

    oldx = w->core.x;
    oldy = w->core.y;
    oldwidth = w->core.width;
    oldheight = w->core.height;
    oldborder_width = w->core.border_width;

#define PutBackGeometry() \
        { w->core.x = (Position) (oldx); \
          w->core.y = (Position) (oldy); \
          w->core.width = (Dimension) (oldwidth); \
          w->core.height = (Dimension) (oldheight); \
          w->core.border_width = (Dimension) (oldborder_width); }

    memset(&values, 0, sizeof(values));
    if (mask & XCB_CONFIG_WINDOW_X) {
        if (w->core.x == request->x)
            mask &= (unsigned int) (~XCB_CONFIG_WINDOW_X);
        else {
            w->core.x = (Position) (values.x = request->x);
            if (wm) {
                hintp->flags &= ~XCB_ICCCM_SIZE_HINT_US_POSITION;
                hintp->flags |= XCB_ICCCM_SIZE_HINT_P_POSITION;
                hintp->x = values.x;
            }
        }
    }
    if (mask & XCB_CONFIG_WINDOW_Y) {
        if (w->core.y == request->y)
            mask &= (unsigned int) (~XCB_CONFIG_WINDOW_Y);
        else {
            w->core.y = (Position) (values.y = request->y);
            if (wm) {
                hintp->flags &= ~XCB_ICCCM_SIZE_HINT_US_POSITION;
                hintp->flags |= XCB_ICCCM_SIZE_HINT_P_POSITION;
                hintp->y = values.y;
            }
        }
    }
    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        if (w->core.border_width == request->border_width) {
            mask &= (unsigned int) (~XCB_CONFIG_WINDOW_BORDER_WIDTH);
        }
        else
            w->core.border_width =
                (Dimension) (values.border_width = request->border_width);
    }
    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        if (w->core.width == request->width)
            mask &= (unsigned int) (~XCB_CONFIG_WINDOW_WIDTH);
        else {
            w->core.width = (Dimension) (values.width = request->width);
            if (wm) {
                hintp->flags &= ~XCB_ICCCM_SIZE_HINT_US_SIZE;
                hintp->flags |= XCB_ICCCM_SIZE_HINT_P_SIZE;
                hintp->width = values.width;
            }
        }
    }
    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        if (w->core.height == request->height)
            mask &= (unsigned int) (~XCB_CONFIG_WINDOW_HEIGHT);
        else {
            w->core.height = (Dimension) (values.height = request->height);
            if (wm) {
                hintp->flags &= ~XCB_ICCCM_SIZE_HINT_US_SIZE;
                hintp->flags |= XCB_ICCCM_SIZE_HINT_P_SIZE;
                hintp->height = values.height;
            }
        }
    }
    if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        values.stack_mode = request->stack_mode;
        if (mask & XCB_CONFIG_WINDOW_SIBLING)
            values.sibling = _IswXcbWindow(IswWindowOf(request->sibling));
    }

    if (!IswIsRealized((Widget) w)) {
        CALLGEOTAT(_IswGeoTrace((Widget) w,
                               "Shell \"%s\" is not realized, return IswGeometryYes.\n",
                               IswName((Widget) w)));
        CALLGEOTAT(_IswGeoTab(-1));
        return IswGeometryYes;
    }

    CALLGEOTAT(_IswGeoTrace((Widget) w, "XConfiguring the Shell X window :\n"));
    CALLGEOTAT(_IswGeoTab(1));
#ifdef ISW_GEO_TATTLER
    if (mask & XCB_CONFIG_WINDOW_X) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "x = %d\n", values.x));
    }
    if (mask & XCB_CONFIG_WINDOW_Y) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "y = %d\n", values.y));
    }
    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "width = %d\n", values.width));
    }
    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "height = %d\n", values.height));
    }
    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        CALLGEOTAT(_IswGeoTrace((Widget) w,
                               "border_width = %d\n", values.border_width));
    }
#endif
    CALLGEOTAT(_IswGeoTab(-1));
    /* HiDPI: scale logical values to physical for the X server */
    {
        double sf = _IswGetScaleFactor(IswDisplayOf((Widget)w));
        if (sf > 1.0) {
            if (mask & XCB_CONFIG_WINDOW_X)
                values.x = (int32_t)(values.x * sf + 0.5);
            if (mask & XCB_CONFIG_WINDOW_Y)
                values.y = (int32_t)(values.y * sf + 0.5);
            if (mask & XCB_CONFIG_WINDOW_WIDTH)
                values.width = (uint32_t)(values.width * sf + 0.5);
            if (mask & XCB_CONFIG_WINDOW_HEIGHT)
                values.height = (uint32_t)(values.height * sf + 0.5);
            if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                values.border_width = (uint32_t)(values.border_width * sf + 0.5);
        }
    }
    xcb_void_cookie_t cookie = xcb_configure_window_aux(_IswXcbConn(IswDisplayOf((Widget) w)),
        _IswXcbWindow(IswWindowOf((Widget) w)), mask, &values);
    request_num = cookie.sequence;
    
    if (wm && !w->shell.override_redirect
        && mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
        _SetWMSizeHints((WMShellWidget) w);
    }

    if (w->shell.override_redirect) {
        CALLGEOTAT(_IswGeoTrace
                   ((Widget) w,
                    "Shell \"%s\" is override redirect, return IswGeometryYes.\n",
                    IswName((Widget) w)));
        CALLGEOTAT(_IswGeoTab(-1));
        return IswGeometryYes;
    }

    /* If no non-stacking bits are set, there's no way to tell whether
       or not this worked, so assume it did */

    if (!(mask & (unsigned) (~(XCB_CONFIG_WINDOW_STACK_MODE | XCB_CONFIG_WINDOW_SIBLING))))
        return IswGeometryYes;

    if (wm && ((WMShellWidget) w)->wm.wait_for_wm == FALSE) {
        /* the window manager is sick
         * so I will do the work and
         * say no so if a new WM starts up,
         * or the current one recovers
         * my size requests will be visible
         */
        CALLGEOTAT(_IswGeoTrace
                   ((Widget) w,
                    "Shell \"%s\" has wait_for_wm == FALSE, return IswGeometryNo.\n",
                    IswName((Widget) w)));
        CALLGEOTAT(_IswGeoTab(-1));

        PutBackGeometry();
        return IswGeometryNo;
    }


    //#TODO this seems like it would be better served with a callback mechanism in XCB
    if (_wait_for_response(w, &event, request_num)) {
        /* got an event */
        if (event->response_type == XCB_CONFIGURE_NOTIFY) {
            xcb_configure_notify_event_t * cne = (xcb_configure_notify_event_t *)event;

#define NEQ(x, msk) ((mask & msk) && (values.x != cne->x))
            if (NEQ(x, XCB_CONFIG_WINDOW_X) ||
                NEQ(y, XCB_CONFIG_WINDOW_Y) ||
                NEQ(width, XCB_CONFIG_WINDOW_WIDTH) ||
                NEQ(height, XCB_CONFIG_WINDOW_HEIGHT) || NEQ(border_width, XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
#ifdef ISW_GEO_TATTLER
                if (NEQ(x, XCB_CONFIG_WINDOW_X)) {
                    CALLGEOTAT(_IswGeoTrace((Widget) w,
                                           "received Configure X %d\n",
                                           event.xconfigure.x));
                }
                if (NEQ(y, XCB_CONFIG_WINDOW_Y)) {
                    CALLGEOTAT(_IswGeoTrace((Widget) w,
                                           "received Configure Y %d\n",
                                           event.xconfigure.y));
                }
                if (NEQ(width, XCB_CONFIG_WINDOW_WIDTH)) {
                    CALLGEOTAT(_IswGeoTrace((Widget) w,
                                           "received Configure Width %d\n",
                                           event.xconfigure.width));
                }
                if (NEQ(height, XCB_CONFIG_WINDOW_HEIGHT)) {
                    CALLGEOTAT(_IswGeoTrace((Widget) w,
                                           "received Configure Height %d\n",
                                           event.xconfigure.height));
                }
                if (NEQ(border_width, XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
                    CALLGEOTAT(_IswGeoTrace((Widget) w,
                                           "received Configure BorderWidth %d\n",
                                           event.xconfigure.border_width));
                }
#endif
#undef NEQ
                //#TODO just push the event back into the AppContext queue
                //XPutBackEvent(IswDisplayOf(w), &event);
                PutBackGeometry();
                /*
                 * We just potentially re-ordered the event queue
                 * w.r.t. ConfigureNotifies with some trepidation.
                 * But this is probably a Good Thing because we
                 * will know the new true state of the world sooner
                 * this way.
                 */
                CALLGEOTAT(_IswGeoTrace((Widget) w,
                                       "ConfigureNotify failed, return IswGeometryNo.\n"));
                CALLGEOTAT(_IswGeoTab(-1));
                free(event);
                return IswGeometryNo;
            }
            else {
                /* HiDPI: ConfigureNotify values are physical pixels;
                 * convert back to logical for widget internals. */
                double inv = 1.0 / _IswGetScaleFactor(IswDisplayOf((Widget)w));
                w->core.width = (Dimension)(cne->width * inv + 0.5);
                w->core.height = (Dimension)(cne->height * inv + 0.5);
                w->core.border_width =
                    (Dimension)(cne->border_width * inv + 0.5);
                if (w->shell.client_specified & _IswShellNotReparented) {

                    w->core.x = (Position)(cne->x * inv + 0.5);
                    w->core.y = (Position)(cne->y * inv + 0.5);
                    w->shell.client_specified |= _IswShellPositionValid;
                }
                else
                    w->shell.client_specified &= ~_IswShellPositionValid;
                CALLGEOTAT(_IswGeoTrace((Widget) w,
                                       "ConfigureNotify succeed, return IswGeometryYes.\n"));
                CALLGEOTAT(_IswGeoTab(-1));
                free(event);
                return IswGeometryYes;
            }
        }
        else if (!wm) {
            PutBackGeometry();
            CALLGEOTAT(_IswGeoTrace((Widget) w,
                                   "Not wm, return IswGeometryNo.\n"));
            CALLGEOTAT(_IswGeoTab(-1));
            free(event);
            return IswGeometryNo;
        }
        else
            IswAppWarningMsg(IswWidgetToApplicationContext((Widget) w),
                            "internalError", "shell", IswCIswToolkitError,
                            "Shell's window manager interaction is broken",
                            NULL, NULL);
    }
    else if (wm) {              /* no event */
        ((WMShellWidget) w)->wm.wait_for_wm = FALSE;    /* timed out; must be broken */
    }
    PutBackGeometry();
#undef PutBackGeometry
    CALLGEOTAT(_IswGeoTrace((Widget) w,
                           "Timeout passed?, return IswGeometryNo.\n"));
    CALLGEOTAT(_IswGeoTab(-1));
    return IswGeometryNo;
}

static Boolean
SetValues(Widget old,
          Widget ref _X_UNUSED,
          Widget new,
          ArgList args,
          Cardinal *num_args)
{
    ShellWidget nw = (ShellWidget) new;
    ShellWidget ow = (ShellWidget) old;
    Mask mask = 0;
    IswSetWindowAttributes attr;

    if (!IswIsRealized(new))
        return False;

    if (ow->shell.save_under != nw->shell.save_under) {
        mask = XCB_CW_SAVE_UNDER;
        attr.save_under = nw->shell.save_under;
    }

    if (ow->shell.override_redirect != nw->shell.override_redirect) {
        mask |= XCB_CW_OVERRIDE_REDIRECT;
        attr.override_redirect = nw->shell.override_redirect;
    }

    if (mask) {
        //XChangeWindowAttributes(IswDisplayOf(new), IswWindowOf(new), mask, &attr);
        xcb_change_window_attributes(_IswXcbConn(IswDisplayOf(new)), _IswXcbWindow(IswWindowOf(new)), mask, &attr);
        if ((mask & XCB_CW_OVERRIDE_REDIRECT) && !nw->shell.override_redirect)
            _popup_set_prop(nw);
    }

    if (!(ow->shell.client_specified & _IswShellPositionValid)) {
        Cardinal n;

        for (n = *num_args; n; n--, args++) {
            if (strcmp(IswNx, args->name) == 0) {
                _IswShellGetCoordinates((Widget) ow, &ow->core.x, &ow->core.y);
            }
            else if (strcmp(IswNy, args->name) == 0) {
                _IswShellGetCoordinates((Widget) ow, &ow->core.x, &ow->core.y);
            }
        }
    }
    return FALSE;
}

static Boolean
WMSetValues(Widget old,
            Widget ref _X_UNUSED,
            Widget new,
            ArgList args _X_UNUSED,
            Cardinal *num_args _X_UNUSED)
{
    WMShellWidget nwmshell = (WMShellWidget) new;
    WMShellWidget owmshell = (WMShellWidget) old;
    Boolean set_prop = IswIsRealized(new) && !nwmshell->shell.override_redirect;
    Boolean title_changed;

    EvaluateSizeHints(nwmshell);

#define NEQ(f) (nwmshell->wm.size_hints.f != owmshell->wm.size_hints.f)

    if (set_prop && (NEQ(flags) || NEQ(min_width) || NEQ(min_height)
                     || NEQ(max_width) || NEQ(max_height)
                     || NEQ(width_inc) || NEQ(height_inc)
                     || NEQ(min_aspect.x) || NEQ(min_aspect.y)
                     || NEQ(max_aspect.x) || NEQ(max_aspect.y)
#undef NEQ
#define NEQ(f) (nwmshell->wm.f != owmshell->wm.f)
                     || NEQ(base_width) || NEQ(base_height) ||
                     NEQ(win_gravity))) {
        _SetWMSizeHints(nwmshell);
    }
#undef NEQ

    if (nwmshell->wm.title != owmshell->wm.title) {
        IswFree(owmshell->wm.title);
        if (!nwmshell->wm.title)
            nwmshell->wm.title = (_IswString) "";
        nwmshell->wm.title = IswNewString(nwmshell->wm.title);
        title_changed = True;
    }
    else
        title_changed = False;

    if (set_prop
        && (title_changed ||
            nwmshell->wm.title_encoding != owmshell->wm.title_encoding)) {

        //XTextProperty title;

        //if (nwmshell->wm.title_encoding == None &&
        //    XmbTextListToTextProperty(IswDisplayOf(new),
        //                              (char **) &nwmshell->wm.title,
        //                              1, XStdICCTextStyle, &title) >= Success) {
        //    copied = True;
        //}
        //else {
        //    title.value = (unsigned char *) nwmshell->wm.title;
        //    title.encoding = nwmshell->wm.title_encoding ?
        //        nwmshell->wm.title_encoding : XCB_ATOM_STRING;
        //    title.format = 8;
        //    title.nitems = strlen(nwmshell->wm.title);
        //}
        //XSetWMName(IswDisplayOf(new), IswWindowOf(new), &title);
        _IswPlatformSetWindowTitle(IswDisplayOf(new), IswWindowOf(new),
                                   (const char *) nwmshell->wm.title);
    }

    EvaluateWMHints(nwmshell);

#define NEQ(f)  (nwmshell->wm.wm_hints.f != owmshell->wm.wm_hints.f)

    if (set_prop && (NEQ(flags) || NEQ(input) || NEQ(initial_state)
                     || NEQ(icon_x) || NEQ(icon_y)
                     || NEQ(icon_pixmap) || NEQ(icon_mask) || NEQ(icon_window)
                     || NEQ(window_group))) {

        /* WM_HINTS carries icon pixmap/mask/urgency in a private xcb_icccm
           struct field (WMShellPart.wm_hints) the toolkit has not neutralised;
           a lossy semantic op would drop those.  Stays on the seam until that
           struct is abstracted (not part of Phase 6's atom/property scope). */
        xcb_icccm_set_wm_hints(_IswXcbConn(IswDisplayOf(new)),
                               _IswXcbWindow(IswWindowOf(new)),
                               &nwmshell->wm.wm_hints);
    }
#undef NEQ

    //if (IswIsRealized(new) && nwmshell->wm.transient != owmshell->wm.transient) {
    //    if (nwmshell->wm.transient) {
    //        if (!IswIsTransientShell(new) &&
    //            !nwmshell->shell.override_redirect &&
    //            nwmshell->wm.wm_hints.window_group != IswUnspecifiedWindowGroup)
    //            XSetTransientForHint(IswDisplayOf(new), IswWindowOf(new),
    //                                 nwmshell->wm.wm_hints.window_group);
    //    }
    //    else
    //        XDeleteProperty(IswDisplayOf(new), IswWindowOf(new), XCB_ATOM_WM_TRANSIENT_FOR);
    //}

    if (IswIsRealized(new) && nwmshell->wm.transient != owmshell->wm.transient) {
        if (nwmshell->wm.transient) {
            if (!IswIsTransientShell(new) &&
                !nwmshell->shell.override_redirect &&
                nwmshell->wm.wm_hints.window_group != IswUnspecifiedWindowGroup) {
                
                _IswPlatformSetTransientFor(IswDisplayOf(new), IswWindowOf(new),
                    _IswXcbWindowWrap(nwmshell->wm.wm_hints.window_group));
            }
        }
        else {
            Atom transient_for = _IswPlatformInternAtomOp(IswDisplayOf(new),
                                                          "WM_TRANSIENT_FOR", False);
            _IswPlatformDeleteProperty(IswDisplayOf(new), IswWindowOf(new),
                                       transient_for);
        }
    }

    if (nwmshell->wm.client_leader != owmshell->wm.client_leader
        && _IswXcbWindow(IswWindowOf(new)) && !nwmshell->shell.override_redirect) {
        Widget leader = GetClientLeader(new);

        if (_IswXcbWindow(IswWindowOf(leader))) {
            xcb_window_t leader_win = _IswXcbWindow(leader->core.window);
            Atom client_leader = _IswPlatformInternAtomOp(IswDisplayOf(new),
                                                          "WM_CLIENT_LEADER", False);
            _IswPlatformChangeProperty(IswDisplayOf(new), IswWindowOf(new),
                                       client_leader, ISW_ATOM_WINDOW, 32,
                                       ISW_PROP_MODE_REPLACE, &leader_win, 1);
        }
            //XChangeProperty(IswDisplayOf(new), IswWindowOf(new),
            //                XInternAtom(IswDisplayOf(new),
            //                            "WM_CLIENT_LEADER", False),
            //                XCB_ATOM_WINDOW, 32, XCB_PROP_MODE_REPLACE,
            //                (unsigned char *) &(leader->core.window), 1);
    }//

    //if (nwmshell->wm.window_role != owmshell->wm.window_role) {
    //    IswFree((_IswString) owmshell->wm.window_role);
    //    if (set_prop && nwmshell->wm.window_role) {
    //        XChangeProperty(IswDisplayOf(new), IswWindowOf(new),
    //                        XInternAtom(IswDisplayOf(new), "WM_WINDOW_ROLE",
    //                                    False),
    //                        XCB_ATOM_STRING, 8, XCB_PROP_MODE_REPLACE,
    //                        (unsigned char *) nwmshell->wm.window_role,
    //                        (int) strlen(nwmshell->wm.window_role));
    //    }
    //    else if (IswIsRealized(new) && !nwmshell->wm.window_role) {
    //        XDeleteProperty(IswDisplayOf(new), IswWindowOf(new),
    //                        XInternAtom(IswDisplayOf(new), "WM_WINDOW_ROLE",
    //                                    False));
    //    }
    //}
    if (nwmshell->wm.window_role != owmshell->wm.window_role) {
        IswFree((_IswString) owmshell->wm.window_role);

        if (set_prop && nwmshell->wm.window_role) {
            Atom window_role = _IswPlatformInternAtomOp(IswDisplayOf(new),
                                                        "WM_WINDOW_ROLE", False);
            _IswPlatformChangeProperty(IswDisplayOf(new), IswWindowOf(new),
                                       window_role, ISW_ATOM_STRING, 8,
                                       ISW_PROP_MODE_REPLACE,
                                       nwmshell->wm.window_role,
                                       (uint32_t) strlen(nwmshell->wm.window_role));
        }
        else if (IswIsRealized(new) && !nwmshell->wm.window_role) {
            Atom window_role = _IswPlatformInternAtomOp(IswDisplayOf(new),
                                                        "WM_WINDOW_ROLE", False);
            _IswPlatformDeleteProperty(IswDisplayOf(new), IswWindowOf(new),
                                       window_role);
        }
    }
    return FALSE;
}

static Boolean
TransientSetValues(Widget oldW,
                   Widget refW _X_UNUSED,
                   Widget newW,
                   ArgList args _X_UNUSED,
                   Cardinal *num_args _X_UNUSED)
{
    TransientShellWidget old = (TransientShellWidget) oldW;
    TransientShellWidget new = (TransientShellWidget) newW;

    if (IswIsRealized(newW)
        && ((new->wm.transient && !old->wm.transient)
            || ((new->transient.transient_for != old->transient.transient_for)
                || (new->transient.transient_for == NULL
                    && (new->wm.wm_hints.window_group
                        != old->wm.wm_hints.window_group))))) {

        _SetTransientForHint(new, True);
    }
    return False;
}

static Boolean
TopLevelSetValues(Widget oldW,
                  Widget refW _X_UNUSED,
                  Widget newW,
                  ArgList args _X_UNUSED,
                  Cardinal *num_args _X_UNUSED)
{
    TopLevelShellWidget old = (TopLevelShellWidget) oldW;
    TopLevelShellWidget new = (TopLevelShellWidget) newW;
    Boolean name_changed;

    if (old->topLevel.icon_name != new->topLevel.icon_name) {
        IswFree((IswPointer) old->topLevel.icon_name);
        if (!new->topLevel.icon_name)
            new->topLevel.icon_name = (_IswString) "";
        new->topLevel.icon_name = IswNewString(new->topLevel.icon_name);
        name_changed = True;
    }
    else
        name_changed = False;

    if (IswIsRealized(newW)) {
        if (new->topLevel.iconic != old->topLevel.iconic) {
            if (new->topLevel.iconic) {
                //XIconifyWindow(IswDisplayOf(newW),
                //               IswWindowOf(newW),
                //               XScreenNumberOfScreen(IswScreenOf(newW))
                //    );
                // XCB doesn't have a direct equivalent to XIconifyWindow

                /* Atoms via the atom op; the iconify request is a broadcast
                   client-message (xcb_send_event), which stays on the seam. */
                Atom net_wm_state = _IswPlatformInternAtomOp(IswDisplayOf(newW),
                                                             "_NET_WM_STATE", False);
                Atom net_wm_state_hidden = _IswPlatformInternAtomOp(
                    IswDisplayOf(newW), "_NET_WM_STATE_HIDDEN", False);

                if (net_wm_state && net_wm_state_hidden) {
                    // Send the client message
                    xcb_client_message_event_t *event = malloc(sizeof(xcb_client_message_event_t));
                    event->response_type = XCB_CLIENT_MESSAGE;
                    event->format = 32;
                    event->window = _IswXcbWindow(IswWindowOf(newW));
                    event->type = (xcb_atom_t) net_wm_state;
                    event->data.data32[0] = 1; // _NET_WM_STATE_ADD
                    event->data.data32[1] = (xcb_atom_t) net_wm_state_hidden;
                    event->data.data32[2] = 0;
                    event->data.data32[3] = 0;
                    event->data.data32[4] = 0;

                    xcb_send_event(_IswXcbConn(IswDisplayOf(newW)), 0, _IswXcbWindow(IswWindowOf(newW)), XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (char*)event);
                    free(event);
                }
            }
            else {
                Boolean map = new->shell.popped_up;

                IswPopup(newW, IswGrabNone);
                if (map) {
                    xcb_map_window(_IswXcbConn(IswDisplayOf(newW)), _IswXcbWindow(IswWindowOf(newW)));
                    xcb_flush(_IswXcbConn(IswDisplayOf(newW)));
                }
            }
        }

        if (!new->shell.override_redirect &&
            (name_changed ||
             (old->topLevel.icon_name_encoding
              != new->topLevel.icon_name_encoding))) {

            //XTextProperty icon_name;

            //if (new->topLevel.icon_name_encoding == None &&
            //    XmbTextListToTextProperty(IswDisplayOf(newW),
            //                              (char **) &new->topLevel.icon_name,
            //                              1, XStdICCTextStyle,
            //                              &icon_name) >= Success) {
            //    copied = True;
            //}
            //else {
                //icon_name.value = (unsigned char *) new->topLevel.icon_name;
                //icon_name.encoding = new->topLevel.icon_name_encoding ?
                //    new->topLevel.icon_name_encoding : XCB_ATOM_STRING;
                //icon_name.format = 8;
                //icon_name.nitems = strlen((char *) icon_name.value);
            //}
            // First, get the atom ID for WM_ICON_NAME
            _IswPlatformSetIconTitle(IswDisplayOf(newW), IswWindowOf(newW),
                                     (const char *) new->topLevel.icon_name);
        }
    }
    else if (new->topLevel.iconic != old->topLevel.iconic) {
        if (new->topLevel.iconic)
            new->wm.wm_hints.initial_state = XCB_ICCCM_WM_STATE_ICONIC;
    }
    return False;
}

/* do not assume it's terminated by a NULL element */
static _IswString *
NewArgv(int count, _IswString *str)
{
    Cardinal nbytes = 0;
    Cardinal num = 0;
    _IswString *newarray;
    _IswString *new;
    _IswString *strarray = str;
    _IswString sptr;

    if (count <= 0 || !str)
        return NULL;

    for (num = (Cardinal) count; num--; str++) {
        nbytes = (nbytes + (Cardinal) strlen(*str));
        nbytes++;
    }
    num = (Cardinal) ((size_t) (count + 1) * sizeof(_IswString));
    new = newarray = (_IswString *) __XtMalloc(num + nbytes);
    sptr = ((char *) new) + num;

    for (str = strarray; count--; str++) {
        *new = sptr;
        strcpy(*new, *str);
        new++;
        sptr = strchr(sptr, '\0');
        sptr++;
    }
    *new = NULL;
    return newarray;
}

static Boolean
ApplicationSetValues(Widget current,
                     Widget request _X_UNUSED,
                     Widget new,
                     ArgList args _X_UNUSED,
                     Cardinal *num_args _X_UNUSED)
{
    ApplicationShellWidget nw = (ApplicationShellWidget) new;
    ApplicationShellWidget cw = (ApplicationShellWidget) current;

    if (cw->application.argc != nw->application.argc ||
        cw->application.argv != nw->application.argv) {

        if (nw->application.argc > 0)
            nw->application.argv = NewArgv(nw->application.argc,
                                           nw->application.argv);
        if (cw->application.argc > 0)
            FreeStringArray(cw->application.argv);

        //if (IswIsRealized(new) && !nw->shell.override_redirect) {
        //    if (nw->application.argc >= 0 && nw->application.argv)
        //        XSetCommand(IswDisplayOf(new), IswWindowOf(new),
        //                    nw->application.argv, nw->application.argc);
        //    else
        //        XDeleteProperty(IswDisplayOf(new), IswWindowOf(new), XCB_ATOM_WM_COMMAND);
        //}
        if (IswIsRealized(new) && !nw->shell.override_redirect) {
            Atom wm_command = _IswPlatformInternAtomOp(IswDisplayOf(new),
                                                       "WM_COMMAND", False);
            if (nw->application.argc >= 0 && nw->application.argv) {
                _IswPlatformChangeProperty(IswDisplayOf(new), IswWindowOf(new),
                                           wm_command, ISW_ATOM_STRING, 8,
                                           ISW_PROP_MODE_REPLACE,
                                           nw->application.argv,
                                           (uint32_t) nw->application.argc);
            } else {
                _IswPlatformDeleteProperty(IswDisplayOf(new), IswWindowOf(new),
                                           wm_command);
            }
        }
    }
    return False;
}

void
_IswShellGetCoordinates(Widget widget, Position *x, Position *y)
{
    ShellWidget w = (ShellWidget) widget;

    if (IswIsRealized(widget)) {
        int tmpx, tmpy;

        /* HiDPI: border_width is logical; X server expects physical pixels. */
        double sf = _IswGetScaleFactor(IswDisplayOf(widget));
        int bw_phys = (int)lrint((double)w->core.border_width * sf);

        xcb_translate_coordinates_cookie_t cookie = xcb_translate_coordinates(_IswXcbConn(IswDisplayOf(w)),
            _IswXcbWindow(IswWindowOf(w)),
            RootWindowOfScreen(_IswXcbScreen(IswScreenOf(w))),
            -bw_phys,
            -bw_phys);

        xcb_translate_coordinates_reply_t *reply = xcb_translate_coordinates_reply(_IswXcbConn(IswDisplayOf(w)), cookie, NULL);
        if (reply) {
            tmpx = reply->dst_x;
            tmpy = reply->dst_y;
            free(reply);
        }
        /* HiDPI: X server returns physical pixels; convert to logical. */
        double inv = 1.0 / sf;
        w->core.x = (Position)lrint((double)tmpx * inv);
        w->core.y = (Position)lrint((double)tmpy * inv);
    }
    *x = w->core.x;
    *y = w->core.y;
}

static void
GetValuesHook(Widget widget, ArgList args, Cardinal *num_args)
{
    ShellWidget w = (ShellWidget) widget;

    /* x and y resource values may be invalid after a shell resize */
    if (IswIsRealized(widget) &&
        !(w->shell.client_specified & _IswShellPositionValid)) {
        Cardinal n;
        Position x, y;

        for (n = *num_args; n; n--, args++) {
            if (strcmp(IswNx, args->name) == 0) {
                _IswShellGetCoordinates(widget, &x, &y);
                _IswCopyToArg((char *) &x, &args->value, sizeof(Position));
            }
            else if (strcmp(IswNy, args->name) == 0) {
                _IswShellGetCoordinates(widget, &x, &y);
                _IswCopyToArg((char *) &y, &args->value, sizeof(Position));
            }
        }
    }
}

static void
ApplicationShellInsertChild(Widget widget)
{
    if (!IswIsWidget(widget) && IswIsRectObj(widget)) {
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "invalidClass", "applicationShellInsertChild",
                        IswCIswToolkitError,
                        "ApplicationShell does not accept RectObj children; ignored",
                        NULL, NULL);
    }
    else {
        IswWidgetProc insert_child;

        LOCK_PROCESS;
        insert_child =
            ((CompositeWidgetClass) applicationShellClassRec.core_class.
             superclass)->composite_class.insert_child;
        UNLOCK_PROCESS;
        (*insert_child) (widget);
    }
}


/* Removed: Session Protocol (XSMP/ICE) support — dead code.
 * See commit history for the original SessionShell implementation.
 *
 * FreeStringArray remains below — used by
 * ApplicationShell for argv handling.
 */

static void
FreeStringArray(_IswString *str)
{
    if (str)
        IswFree((_IswString) str);
}

void
IswSetWindowIconRGBA(Widget shell, const unsigned char *rgba,
                     unsigned int width, unsigned int height)
{
    uint32_t *data;
    unsigned int npixels = width * height;
    unsigned int n_entries = 2 + npixels;

    if (!IswIsRealized(shell) || !rgba || width == 0 || height == 0)
        return;

    data = (uint32_t *)malloc(n_entries * sizeof(uint32_t));
    if (!data)
        return;

    data[0] = width;
    data[1] = height;

    for (unsigned int i = 0; i < npixels; i++) {
        unsigned int si = i * 4;
        unsigned char r = rgba[si + 0];
        unsigned char g = rgba[si + 1];
        unsigned char b = rgba[si + 2];
        unsigned char a = rgba[si + 3];
        data[2 + i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                       ((uint32_t)g << 8) | (uint32_t)b;
    }

    IswSetWindowIconARGB(shell, data, n_entries);
    free(data);
}

void
IswSetWindowIconARGB(Widget shell, const uint32_t *argb_data,
                     unsigned int n_entries)
{
    Atom icon_atom;

    if (!IswIsRealized(shell) || !argb_data || n_entries == 0)
        return;

    icon_atom = _IswPlatformInternAtomOp(IswDisplayOf(shell), "_NET_WM_ICON", False);
    _IswPlatformChangeProperty(IswDisplayOf(shell), IswWindowOf(shell),
                               icon_atom, ISW_ATOM_CARDINAL, 32,
                               ISW_PROP_MODE_REPLACE, argb_data, n_entries);
}

void
IswClearWindowIcon(Widget shell)
{
    Atom icon_atom;

    if (!IswIsRealized(shell))
        return;

    icon_atom = _IswPlatformInternAtomOp(IswDisplayOf(shell), "_NET_WM_ICON", False);
    _IswPlatformDeleteProperty(IswDisplayOf(shell), IswWindowOf(shell), icon_atom);
}

static void
SetNetWmString(Widget shell, const char *prop_name, unsigned int prop_len,
               const char *value)
{
    Atom prop_atom, utf8_atom;

    if (!IswIsRealized(shell) || !value)
        return;

    (void) prop_len;
    prop_atom = _IswPlatformInternAtomOp(IswDisplayOf(shell), prop_name, False);
    utf8_atom = _IswPlatformInternAtomOp(IswDisplayOf(shell), "UTF8_STRING", False);
    _IswPlatformChangeProperty(IswDisplayOf(shell), IswWindowOf(shell),
                               prop_atom, utf8_atom, 8, ISW_PROP_MODE_REPLACE,
                               value, (uint32_t) strlen(value));
}

void
IswSetWindowName(Widget shell, const char *name)
{
    SetNetWmString(shell, "_NET_WM_NAME", 12, name);
}

void
IswSetWindowIconName(Widget shell, const char *name)
{
    SetNetWmString(shell, "_NET_WM_ICON_NAME", 17, name);
}

void
IswSetWindowState(Widget shell, const char *state, Boolean set)
{
    xcb_connection_t *conn;
    Atom wm_state, state_atom;

    if (!IswIsRealized(shell) || !state)
        return;

    conn = _IswXcbConn(IswDisplayOf(shell));

    /* Atoms via the atom op; the state-change is a broadcast client-message
       (xcb_send_event), which stays on the seam. */
    wm_state = _IswPlatformInternAtomOp(IswDisplayOf(shell), "_NET_WM_STATE", False);
    state_atom = _IswPlatformInternAtomOp(IswDisplayOf(shell), state, False);

    if (wm_state && state_atom) {
        xcb_client_message_event_t ev = {0};
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = _IswXcbWindow(IswWindowOf(shell));
        ev.type = (xcb_atom_t) wm_state;
        ev.data.data32[0] = set ? 1 : 0;
        ev.data.data32[1] = (xcb_atom_t) state_atom;

        xcb_send_event(conn, 0, _IswXcbScreen(shell->core.screen)->root,
                       XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                       (const char *) &ev);
    }
}

void
_IswShellUpdateUserTime(xcb_connection_t *dpy, xcb_window_t event_window,
                        xcb_timestamp_t time)
{
    Widget widget = IswWindowToWidget((IswDisplay) dpy, _IswXcbWindowWrap(event_window));
    if (!widget)
        return;

    while (widget && !IswIsWMShell(widget))
        widget = IswParent(widget);
    if (!widget)
        return;

    WMShellWidget wmshell = (WMShellWidget) widget;
    if (!wmshell->wm.user_time_win)
        return;

    IswPerDisplay pd = _IswGetPerDisplay(IswDisplayOf(widget));
    if (!pd->net_wm_user_time)
        return;

    _IswPlatformChangeProperty((IswDisplay) dpy,
                               _IswXcbWindowWrap(wmshell->wm.user_time_win),
                               pd->net_wm_user_time, ISW_ATOM_CARDINAL, 32,
                               ISW_PROP_MODE_REPLACE, &time, 1);
}
