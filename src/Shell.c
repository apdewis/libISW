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
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <ISW/ISWXdnd.h>
#include "ISWXcbDraw.h"
#include <stdio.h>
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

//static void _XtShellDepth(Widget, int, XrmValue *);
//static void _XtShellColormap(Widget, int, XrmValue *);
static void _XtShellAncestorSensitive(Widget, int, XrmValue *);
//static void _XtTitleEncoding(Widget, int, XrmValue *);

/***************************************************************************
 *
 * Shell class record
 *
 ***************************************************************************/

#define Offset(x)       (XtOffsetOf(ShellRec, x))
/* *INDENT-OFF* */
static XtResource shellResources[]=
{
    {XtNx, XtCPosition, XtRPosition, sizeof(Position),
        Offset(core.x), XtRImmediate, (XtPointer)BIGSIZE},
    {XtNy, XtCPosition, XtRPosition, sizeof(Position),
        Offset(core.y), XtRImmediate, (XtPointer)BIGSIZE},
    { XtNallowShellResize, XtCAllowShellResize, XtRBoolean,
        sizeof(Boolean), Offset(shell.allow_shell_resize),
        XtRImmediate, (XtPointer)False},
    { XtNgeometry, XtCGeometry, XtRString, sizeof(String),
        Offset(shell.geometry), XtRString, (XtPointer)NULL},
    { XtNcreatePopupChildProc, XtCCreatePopupChildProc, XtRFunction,
        sizeof(XtCreatePopupChildProc), Offset(shell.create_popup_child_proc),
        XtRFunction, NULL},
    { XtNsaveUnder, XtCSaveUnder, XtRBoolean, sizeof(Boolean),
        Offset(shell.save_under), XtRImmediate, (XtPointer)False},
    { XtNpopupCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
        Offset(shell.popup_callback), XtRCallback, (XtPointer) NULL},
    { XtNpopdownCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
        Offset(shell.popdown_callback), XtRCallback, (XtPointer) NULL},
    { XtNoverrideRedirect, XtCOverrideRedirect,
        XtRBoolean, sizeof(Boolean), Offset(shell.override_redirect),
        XtRImmediate, (XtPointer)False},
    { XtNvisual, XtCVisual, XtRVisual, sizeof(xcb_visualtype_t*),
        Offset(shell.visual), XtRImmediate, (XtPointer)CopyFromParent}
};
/* *INDENT-ON* */

static void ClassPartInitialize(WidgetClass);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, Mask *, uint32_t *); //XSetWindowAttributes *);
static void Resize(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void GetValuesHook(Widget, ArgList, Cardinal *);
static void ChangeManaged(Widget);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *,
                                        XtWidgetGeometry *);
static XtGeometryResult RootGeometryManager(Widget gw,
                                            XtWidgetGeometry *request,
                                            XtWidgetGeometry *reply);
static void Destroy(Widget);

/* *INDENT-OFF* */
static ShellClassExtensionRec shellClassExtRec = {
    NULL,
    NULLQUARK,
    XtShellExtensionVersion,
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
    /* resource_count        */ XtNumber(shellResources),
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
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ GetValuesHook,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{ /* Composite */
    /* geometry_manager      */ GeometryManager,
    /* change_managed        */ ChangeManaged,
    /* insert_child          */ XtInheritInsertChild,
    /* delete_child          */ XtInheritDeleteChild,
    /* extension             */ NULL
  },{ /* Shell */
    /* extension             */ (XtPointer)&shellClassExtRec
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
static XtResource overrideResources[] =
{
    { XtNoverrideRedirect, XtCOverrideRedirect,
        XtRBoolean, sizeof(Boolean), Offset(shell.override_redirect),
        XtRImmediate, (XtPointer)True},
    { XtNsaveUnder, XtCSaveUnder, XtRBoolean, sizeof(Boolean),
        Offset(shell.save_under), XtRImmediate, (XtPointer)True},
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
    /* realize               */ XtInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ overrideResources,
    /* resource_count        */ XtNumber(overrideResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ NULL,
    /* resize                */ XtInheritResize,
    /* expose                */ NULL,
    /* set_values            */ NULL,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ XtInheritGeometryManager,
    /* change_managed        */ XtInheritChangeManaged,
    /* insert_child          */ XtInheritInsertChild,
    /* delete_child          */ XtInheritDeleteChild,
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
#define Offset(x)       (XtOffsetOf(WMShellRec, x))

static int default_unspecified_shell_int = XtUnspecifiedShellInt;

/*
 * Warning, casting XtUnspecifiedShellInt (which is -1) to an (XtPointer)
 * can result is loss of bits on some machines (i.e. crays)
 */

/* *INDENT-OFF* */
static XtResource wmResources[] =
{
    { XtNtitle, XtCTitle, XtRString, sizeof(String),
        Offset(wm.title), XtRString, NULL},
    { XtNwmTimeout, XtCWmTimeout, XtRInt, sizeof(int),
        Offset(wm.wm_timeout), XtRImmediate,(XtPointer)DEFAULT_WM_TIMEOUT},
    { XtNwaitForWm, XtCWaitForWm, XtRBoolean, sizeof(Boolean),
        Offset(wm.wait_for_wm), XtRImmediate, (XtPointer)True},
    { XtNtransient, XtCTransient, XtRBoolean, sizeof(Boolean),
        Offset(wm.transient), XtRImmediate, (XtPointer)False},
/* size_hints minus things stored in core */
    { XtNbaseWidth, XtCBaseWidth, XtRInt, sizeof(int),
        Offset(wm.base_width),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNbaseHeight, XtCBaseHeight, XtRInt, sizeof(int),
        Offset(wm.base_height),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNwinGravity, XtCWinGravity, XtRGravity, sizeof(int),
        Offset(wm.win_gravity),
        XtRGravity, (XtPointer) &default_unspecified_shell_int},
    { XtNminWidth, XtCMinWidth, XtRInt, sizeof(int),
        Offset(wm.size_hints.min_width),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNminHeight, XtCMinHeight, XtRInt, sizeof(int),
        Offset(wm.size_hints.min_height),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNmaxWidth, XtCMaxWidth, XtRInt, sizeof(int),
        Offset(wm.size_hints.max_width),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNmaxHeight, XtCMaxHeight, XtRInt, sizeof(int),
        Offset(wm.size_hints.max_height),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNwidthInc, XtCWidthInc, XtRInt, sizeof(int),
        Offset(wm.size_hints.width_inc),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNheightInc, XtCHeightInc, XtRInt, sizeof(int),
        Offset(wm.size_hints.height_inc),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNminAspectX, XtCMinAspectX, XtRInt, sizeof(int),
        Offset(wm.size_hints.min_aspect.x),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNminAspectY, XtCMinAspectY, XtRInt, sizeof(int),
        Offset(wm.size_hints.min_aspect.y),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNmaxAspectX, XtCMaxAspectX, XtRInt, sizeof(int),
        Offset(wm.size_hints.max_aspect.x),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNmaxAspectY, XtCMaxAspectY, XtRInt, sizeof(int),
        Offset(wm.size_hints.max_aspect.y),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
/* wm_hints */
    { XtNinput, XtCInput, XtRBool, sizeof(Bool),
        Offset(wm.wm_hints.input), XtRImmediate, (XtPointer)False},
    { XtNinitialState, XtCInitialState, XtRInitialState, sizeof(int),
        Offset(wm.wm_hints.initial_state),
        XtRImmediate, (XtPointer)XCB_ICCCM_WM_STATE_NORMAL},
    { XtNiconPixmap, XtCIconPixmap, XtRBitmap, sizeof(xcb_pixmap_t),
        Offset(wm.wm_hints.icon_pixmap), XtRPixmap, NULL},
    { XtNiconWindow, XtCIconWindow, XtRWindow, sizeof(xcb_window_t),
        Offset(wm.wm_hints.icon_window), XtRWindow,   (XtPointer) NULL},
    { XtNiconX, XtCIconX, XtRInt, sizeof(int),
        Offset(wm.wm_hints.icon_x),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNiconY, XtCIconY, XtRInt, sizeof(int),
        Offset(wm.wm_hints.icon_y),
        XtRInt, (XtPointer) &default_unspecified_shell_int},
    { XtNiconMask, XtCIconMask, XtRBitmap, sizeof(xcb_pixmap_t),
        Offset(wm.wm_hints.icon_mask), XtRPixmap, NULL},
    { XtNwindowGroup, XtCWindowGroup, XtRWindow, sizeof(xcb_window_t),
        Offset(wm.wm_hints.window_group),
        XtRImmediate, (XtPointer)XtUnspecifiedWindow},
    { XtNclientLeader, XtCClientLeader, XtRWidget, sizeof(Widget),
        Offset(wm.client_leader), XtRWidget, NULL},
    { XtNwindowRole, XtCWindowRole, XtRString, sizeof(String),
        Offset(wm.window_role), XtRString, (XtPointer) NULL},
    { XtNurgency, XtCUrgency, XtRBoolean, sizeof(Boolean),
        Offset(wm.urgency), XtRImmediate, (XtPointer) False}
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
    /* realize               */ XtInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ wmResources,
    /* resource_count        */ XtNumber(wmResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ WMDestroy,
    /* resize                */ XtInheritResize,
    /* expose                */ NULL,
    /* set_values            */ WMSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ XtInheritGeometryManager,
    /* change_managed        */ XtInheritChangeManaged,
    /* insert_child          */ XtInheritInsertChild,
    /* delete_child          */ XtInheritDeleteChild,
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
#define Offset(x)       (XtOffsetOf(TransientShellRec, x))

/* *INDENT-OFF* */
static XtResource transientResources[]=
{
    { XtNtransient, XtCTransient, XtRBoolean, sizeof(Boolean),
        Offset(wm.transient), XtRImmediate, (XtPointer)True},
    { XtNtransientFor, XtCTransientFor, XtRWidget, sizeof(Widget),
        Offset(transient.transient_for), XtRWidget, NULL},
    { XtNsaveUnder, XtCSaveUnder, XtRBoolean, sizeof(Boolean),
        Offset(shell.save_under), XtRImmediate, (XtPointer)True},
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
    /* resource_count        */ XtNumber(transientResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ NULL,
    /* resize                */ XtInheritResize,
    /* expose                */ NULL,
    /* set_values            */ TransientSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ XtInheritTranslations,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ XtInheritGeometryManager,
    /* change_managed        */ XtInheritChangeManaged,
    /* insert_child          */ XtInheritInsertChild,
    /* delete_child          */ XtInheritDeleteChild,
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
#define Offset(x)       (XtOffsetOf(TopLevelShellRec, x))

/* *INDENT-OFF* */
static XtResource topLevelResources[]=
{
    { XtNiconName, XtCIconName, XtRString, sizeof(String),
        Offset(topLevel.icon_name), XtRString, (XtPointer) NULL},
    { XtNiconic, XtCIconic, XtRBoolean, sizeof(Boolean),
        Offset(topLevel.iconic), XtRImmediate, (XtPointer)False}
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
    /* realize               */ XtInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ topLevelResources,
    /* resource_count        */ XtNumber(topLevelResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ TopLevelDestroy,
    /* resize                */ XtInheritResize,
    /* expose                */ NULL,
    /* set_values            */ TopLevelSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ XtInheritTranslations,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ XtInheritGeometryManager,
    /* change_managed        */ XtInheritChangeManaged,
    /* insert_child          */ XtInheritInsertChild,
    /* delete_child          */ XtInheritDeleteChild,
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
#define Offset(x)       (XtOffsetOf(ApplicationShellRec, x))

/* *INDENT-OFF* */
static XtResource applicationResources[]=
{
    {XtNargc, XtCArgc, XtRInt, sizeof(int),
          Offset(application.argc), XtRImmediate, (XtPointer)0},
    {XtNargv, XtCArgv, XtRStringArray, sizeof(String*),
          Offset(application.argv), XtRPointer, (XtPointer) NULL}
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
    /* version               */ XtCompositeExtensionVersion,
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
    /* realize               */ XtInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ applicationResources,
    /* resource_count        */ XtNumber(applicationResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ ApplicationDestroy,
    /* resize                */ XtInheritResize,
    /* expose                */ NULL,
    /* set_values            */ ApplicationSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ XtInheritTranslations,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ XtInheritGeometryManager,
    /* change_managed        */ XtInheritChangeManaged,
    /* insert_child          */ ApplicationShellInsertChild,
    /* delete_child          */ XtInheritDeleteChild,
    /* extension             */ (XtPointer)&compositeClassExtension
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

/***************************************************************************
 *
 * SessionShell class record
 *
 ***************************************************************************/

#undef Offset
#define Offset(x)       (XtOffsetOf(SessionShellRec, x))

/* *INDENT-OFF* */
static XtResource sessionResources[]=
{
#ifndef XT_NO_SM
 {XtNconnection, XtCConnection, XtRSmcConn, sizeof(SmcConn),
       Offset(session.connection), XtRSmcConn, (XtPointer) NULL},
#endif
 {XtNsessionID, XtCSessionID, XtRString, sizeof(String),
       Offset(session.session_id), XtRString, (XtPointer) NULL},
 {XtNrestartCommand, XtCRestartCommand, XtRCommandArgArray, sizeof(String*),
       Offset(session.restart_command), XtRPointer, (XtPointer) NULL},
 {XtNcloneCommand, XtCCloneCommand, XtRCommandArgArray, sizeof(String*),
       Offset(session.clone_command), XtRPointer, (XtPointer) NULL},
 {XtNdiscardCommand, XtCDiscardCommand, XtRCommandArgArray, sizeof(String*),
       Offset(session.discard_command), XtRPointer, (XtPointer) NULL},
 {XtNresignCommand, XtCResignCommand, XtRCommandArgArray, sizeof(String*),
       Offset(session.resign_command), XtRPointer, (XtPointer) NULL},
 {XtNshutdownCommand, XtCShutdownCommand, XtRCommandArgArray, sizeof(String*),
       Offset(session.shutdown_command), XtRPointer, (XtPointer) NULL},
 {XtNenvironment, XtCEnvironment, XtREnvironmentArray, sizeof(String*),
       Offset(session.environment), XtRPointer, (XtPointer) NULL},
 {XtNcurrentDirectory, XtCCurrentDirectory, XtRDirectoryString, sizeof(String),
       Offset(session.current_dir), XtRString, (XtPointer) NULL},
 {XtNprogramPath, XtCProgramPath, XtRString, sizeof(String),
      Offset(session.program_path), XtRString, (XtPointer) NULL},
 {XtNrestartStyle, XtCRestartStyle, XtRRestartStyle, sizeof(unsigned char),
      Offset(session.restart_style), XtRImmediate,
      (XtPointer) SmRestartIfRunning},
 {XtNjoinSession, XtCJoinSession, XtRBoolean, sizeof(Boolean),
       Offset(session.join_session), XtRImmediate, (XtPointer) True},
 {XtNsaveCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(session.save_callbacks), XtRCallback, (XtPointer) NULL},
 {XtNinteractCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(session.interact_callbacks), XtRCallback, (XtPointer)NULL},
 {XtNcancelCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(session.cancel_callbacks), XtRCallback, (XtPointer) NULL},
 {XtNsaveCompleteCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(session.save_complete_callbacks), XtRCallback, (XtPointer) NULL},
 {XtNdieCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(session.die_callbacks), XtRCallback, (XtPointer) NULL},
 {XtNerrorCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(session.error_callbacks), XtRCallback, (XtPointer) NULL}
};
/* *INDENT-ON* */
#undef Offset

static void
SessionInitialize(Widget, Widget, ArgList, Cardinal *);
static void
SessionDestroy(Widget);
static Boolean
SessionSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

/* *INDENT-OFF* */
static CompositeClassExtensionRec sessionCompositeClassExtension = {
    /* next_extension        */ NULL,
    /* record_type           */ NULLQUARK,
    /* version               */ XtCompositeExtensionVersion,
    /* record_size           */ sizeof(CompositeClassExtensionRec),
    /* accepts_objects       */ TRUE,
    /* allows_change_managed_set */ FALSE
};

externaldef(sessionshellclassrec) SessionShellClassRec sessionShellClassRec = {
  {
    /* superclass            */ (WidgetClass) &applicationShellClassRec,
    /* class_name            */ "SessionShell",
    /* size                  */ sizeof(SessionShellRec),
    /* Class Initializer     */ NULL,
    /* class_part_initialize */ NULL,
    /* Class init'ed ?       */ FALSE,
    /* initialize            */ SessionInitialize,
    /* initialize_notify     */ NULL,
    /* realize               */ XtInheritRealize,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ sessionResources,
    /* resource_count        */ XtNumber(sessionResources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ SessionDestroy,
    /* resize                */ XtInheritResize,
    /* expose                */ NULL,
    /* set_values            */ SessionSetValues,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* intrinsics version    */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ XtInheritTranslations,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  },{
    /* geometry_manager      */ XtInheritGeometryManager,
    /* change_managed        */ XtInheritChangeManaged,
    /* insert_child          */ XtInheritInsertChild,
    /* delete_child          */ XtInheritDeleteChild,
    /* extension             */ (XtPointer)&sessionCompositeClassExtension
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
  },{
    /* extension             */ NULL
  }
};
/* *INDENT-ON* */

externaldef(sessionshellwidgetclass)
WidgetClass sessionShellWidgetClass = (WidgetClass) (&sessionShellClassRec);

void SetWMProperties(Widget w, char *window_name, char *icon_name, 
                     char **argv, int argc, 
                     xcb_size_hints_t *size_hints,
                     xcb_icccm_wm_hints_t *wm_hints,
                     char *classhint_class, char *classhint_name) {
    
    // Set WM_NAME property
    if (window_name != NULL) {
        xcb_intern_atom_cookie_t name_atom_cookie = xcb_intern_atom(XtDisplay(w), FALSE, strlen("WM_NAME"), "WM_NAME");
        xcb_intern_atom_reply_t *name_atom_reply = xcb_intern_atom_reply(XtDisplay(w), name_atom_cookie, NULL);
        if (name_atom_reply) {
            xcb_change_property(XtDisplay(w), XCB_PROP_MODE_REPLACE, XtWindow(w),
                                name_atom_reply->atom, XCB_ATOM_STRING, 8,
                                strlen(window_name), window_name);
            free(name_atom_reply);
        }
    }

    // Set WM_ICON_NAME property (if needed)
    if (XtIsTopLevelShell((Widget) w) && icon_name != NULL) {
        xcb_intern_atom_cookie_t icon_atom_cookie = xcb_intern_atom(XtDisplay(w), FALSE, strlen("WM_ICON_NAME"), "WM_ICON_NAME");
        xcb_intern_atom_reply_t *icon_atom_reply = xcb_intern_atom_reply(XtDisplay(w), icon_atom_cookie, NULL);
        if (icon_atom_reply) {
            xcb_change_property(XtDisplay(w), XCB_PROP_MODE_REPLACE, XtWindow(w),
                                icon_atom_reply->atom, XCB_ATOM_STRING, 8,
                                strlen(icon_name), icon_name);
            free(icon_atom_reply);
        }
    }

    // Set WM_COMMAND property (if argv/argc are provided)
    if (argc > 0 && argv != NULL) {
        xcb_intern_atom_cookie_t cmd_atom_cookie = xcb_intern_atom(XtDisplay(w), FALSE, strlen("WM_COMMAND"), "WM_COMMAND");
        xcb_intern_atom_reply_t *cmd_atom_reply = xcb_intern_atom_reply(XtDisplay(w), cmd_atom_cookie, NULL);
        if (cmd_atom_reply) {
            xcb_change_property(XtDisplay(w), XCB_PROP_MODE_REPLACE, XtWindow(w),
                                cmd_atom_reply->atom, XCB_ATOM_STRING, 8,
                                argc, argv);
            free(cmd_atom_reply);
        }
    }

    // Set WM_NORMAL_HINTS property (size_hints)
    // Note: This requires proper packing of the size hints structure
    // Implementation depends on your specific size_hints structure
    
    // Set WM_HINTS property (wm_hints)
    // Note: This requires proper packing of the WM hints structure
    // Implementation depends on your specific wm_hints structure
    
    // Set WM_CLASS property
    // Note: This requires proper packing of the class hint structure
    // Implementation depends on your specific class hint structure
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
        _XtAllocError("xcb_size_hints_t");

    ComputeWMSizeHints(w, size_hints);
    xcb_icccm_set_wm_normal_hints(XtDisplay((Widget) w), XtWindow((Widget) w), size_hints);
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
        if (ext->version == XtShellExtensionVersion
            && ext->record_size == sizeof(ShellClassExtensionRec)) {
            /* continue */
        }
        else {
            String params[1];
            Cardinal num_params = 1;

            params[0] = widget_class->core_class.class_name;
            XtErrorMsg("invalidExtension", "shellClassPartInitialize",
                       XtCXtToolkitError,
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
        if (ext->root_geometry_manager == XtInheritRootGeometryManager) {
            ext->root_geometry_manager =
                _FindClassExtension(widget_class->core_class.superclass)
                ->root_geometry_manager;
        }
    }
    else {
        /* if not found, spec requires XtInheritRootGeometryManager */
        XtPointer *extP
            = &((ShellWidgetClass) widget_class)->shell_class.extension;
        ext = XtNew(ShellClassExtensionRec);
        if(super != NULL)
        (void) memcpy(ext,
                      super,
                      sizeof(ShellClassExtensionRec));
        ext->next_extension = *extP;
        *extP = (XtPointer) ext;
    }
}

static void EventHandler(Widget wid, XtPointer closure,xcb_generic_event_t *event,
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
    w->shell.client_specified = _XtShellNotReparented | _XtShellPositionValid;

    if (w->core.x == BIGSIZE) {
        w->core.x = 0;
        if (w->core.y == BIGSIZE)
            w->core.y = 0;
    }
    else {
        if (w->core.y == BIGSIZE)
            w->core.y = 0;
        else
            w->shell.client_specified |= _XtShellPPositionOK;
    }

    XtAddEventHandler(new, (EventMask) XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                      TRUE, EventHandler, (XtPointer) NULL);
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
        if (XtIsTopLevelShell(new) &&
            tls->topLevel.icon_name != NULL &&
            strlen(tls->topLevel.icon_name) != 0) {
            w->wm.title = XtNewString(tls->topLevel.icon_name);
        }
        else {
            w->wm.title = XtNewString(w->core.name);
        }
    }
    else {
        w->wm.title = XtNewString(w->wm.title);
    }
    w->wm.size_hints.flags = 0;
    w->wm.wm_hints.flags = 0;
    if (w->wm.window_role)
        w->wm.window_role = XtNewString(w->wm.window_role);
}

static void
TopLevelInitialize(Widget req _X_UNUSED,
                   Widget new,
                   ArgList args _X_UNUSED,
                   Cardinal *num_args _X_UNUSED)
{
    TopLevelShellWidget w = (TopLevelShellWidget) new;

    if (w->topLevel.icon_name == NULL) {
        w->topLevel.icon_name = XtNewString(w->core.name);
    }
    else {
        w->topLevel.icon_name = XtNewString(w->topLevel.icon_name);
    }

    if (w->topLevel.iconic)
        w->wm.wm_hints.initial_state = XCB_ICCCM_WM_STATE_ICONIC;
}

static _XtString *NewArgv(int, _XtString *);
static _XtString *NewStringArray(_XtString *);
static void FreeStringArray(_XtString *);

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

#define XtSaveInactive 0
#define XtSaveActive   1
#define XtInteractPending    2
#define XtInteractActive     3

#define XtCloneCommandMask      (1L<<0)
#define XtCurrentDirectoryMask  (1L<<1)
#define XtDiscardCommandMask    (1L<<2)
#define XtEnvironmentMask       (1L<<3)
#define XtProgramMask           (1L<<4)
#define XtResignCommandMask     (1L<<5)
#define XtRestartCommandMask    (1L<<6)
#define XtRestartStyleHintMask  (1L<<7)
#define XtShutdownCommandMask   (1L<<8)

static void JoinSession(SessionShellWidget);
static void SetSessionProperties(SessionShellWidget, Boolean, unsigned long,
                                 unsigned long);
static void StopManagingSession(SessionShellWidget, SmcConn);

typedef struct _XtSaveYourselfRec {
    XtSaveYourself next;
    int save_type;
    int interact_style;
    Boolean shutdown;
    Boolean fast;
    Boolean cancel_shutdown;
    int phase;
    int interact_dialog_type;
    Boolean request_cancel;
    Boolean request_next_phase;
    Boolean save_success;
    int save_tokens;
    int interact_tokens;
} XtSaveYourselfRec;

static void
SessionInitialize(Widget req _X_UNUSED,
                  Widget new,
                  ArgList args _X_UNUSED,
                  Cardinal *num_args _X_UNUSED)
{
#ifndef XT_NO_SM
    SessionShellWidget w = (SessionShellWidget) new;

    if (w->session.session_id)
        w->session.session_id = XtNewString(w->session.session_id);
    if (w->session.restart_command)
        w->session.restart_command = NewStringArray(w->session.restart_command);
    if (w->session.clone_command)
        w->session.clone_command = NewStringArray(w->session.clone_command);
    if (w->session.discard_command)
        w->session.discard_command = NewStringArray(w->session.discard_command);
    if (w->session.resign_command)
        w->session.resign_command = NewStringArray(w->session.resign_command);
    if (w->session.shutdown_command)
        w->session.shutdown_command =
            NewStringArray(w->session.shutdown_command);
    if (w->session.environment)
        w->session.environment = NewStringArray(w->session.environment);
    if (w->session.current_dir)
        w->session.current_dir = XtNewString(w->session.current_dir);
    if (w->session.program_path)
        w->session.program_path = XtNewString(w->session.program_path);

    w->session.checkpoint_state = XtSaveInactive;
    w->session.input_id = 0;
    w->session.save = NULL;

    if ((w->session.join_session) &&
        (w->application.argv || w->session.restart_command))
        JoinSession(w);

    if (w->session.connection)
        SetSessionProperties(w, True, 0L, 0L);
#endif                          /* !XT_NO_SM */
}

static void
Resize(Widget w)
{
    register ShellWidget sw = (ShellWidget) w;
    Widget childwid;
    Cardinal i;

    for (i = 0; i < sw->composite.num_children; i++) {
        if (XtIsManaged(sw->composite.children[i])) {
            childwid = sw->composite.children[i];
            XtResizeWidget(childwid, sw->core.width, sw->core.height,
                           childwid->core.border_width);
            break;              /* can only be one managed child */
        }
    }
}

static void GetGeometry(Widget, Widget);

/*
 * Default WM_DELETE_WINDOW handler for toplevel shells.
 * Destroys the shell widget, which triggers destroy callbacks and allows
 * clean shutdown.  Apps can override with XtOverrideTranslations.
 */
/* ARGSUSED */
static void
ShellWMDeleteWindow(Widget w, xcb_generic_event_t *event, String *params,
		    Cardinal *num_params)
{
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_window;

    if ((event->response_type & ~0x80) != XCB_CLIENT_MESSAGE)
	return;

    wm_protocols = IswXcbInternAtom(XtDisplay(w), "WM_PROTOCOLS", True);
    wm_delete_window = IswXcbInternAtom(XtDisplay(w), "WM_DELETE_WINDOW", True);

    if (wm_protocols == 0 || wm_delete_window == 0)
	return;

    {
	xcb_client_message_event_t *cm = (xcb_client_message_event_t *)event;
	if (cm->type == wm_protocols && cm->data.data32[0] == wm_delete_window) {
	    XtAppSetExitFlag(XtWidgetToApplicationContext(w));
	}
    }
}

static void
SetShellWMProtocolTranslations(Widget w)
{
    static XtTranslations compiled_table;	/* initially 0 */
    static XtAppContext *app_context_list;	/* initially 0 */
    static Cardinal list_size;			/* initially 0 */
    XtAppContext app_context;
    xcb_atom_t wm_delete_window;
    int i;

    app_context = XtWidgetToApplicationContext(w);

    /* parse translation table once */
    if (!compiled_table)
	compiled_table = XtParseTranslationTable(
	    "<Message>WM_PROTOCOLS: IswShellDeleteWindow()\n");

    /* add actions once per application context */
    for (i = 0; i < list_size && app_context_list[i] != app_context; i++) ;
    if (i == list_size) {
	XtActionsRec actions[1];
	actions[0].string = "IswShellDeleteWindow";
	actions[0].proc = ShellWMDeleteWindow;
	list_size++;
	app_context_list = (XtAppContext *) XtRealloc(
	    (char *)app_context_list, list_size * sizeof(XtAppContext));
	XtAppAddActions(app_context, actions, 1);
	app_context_list[i] = app_context;
    }

    /* augment so apps can override with XtOverrideTranslations */
    XtAugmentTranslations(w, compiled_table);

    /* advertise WM_DELETE_WINDOW to the window manager */
    wm_delete_window = IswXcbInternAtom(XtDisplay(w), "WM_DELETE_WINDOW", False);
    xcb_icccm_set_wm_protocols(XtDisplay(w), XtWindow(w),
			       IswXcbInternAtom(XtDisplay(w), "WM_PROTOCOLS", False),
			       1, &wm_delete_window);
}

static void
Realize(xcb_connection_t *dpy, Widget wid, Mask *vmask, uint32_t *attr)
{
    ShellWidget w = (ShellWidget) wid;
    Mask mask = *vmask;

    if (!(w->shell.client_specified & _XtShellGeometryParsed)) {
        /* we'll get here only if there was no child the first
           time we were realized.  If the shell was Unrealized
           and then re-Realized, we probably don't want to
           re-evaluate the defaults anyway.
         */
        GetGeometry(wid, (Widget) NULL);
    }
    else if (w->core.background_pixmap == XtUnspecifiedPixmap) {
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
            if (XtIsWidget(*childP) && XtIsManaged(*childP)) {
                if ((*childP)->core.background_pixmap != XtUnspecifiedPixmap) {
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

        XtErrorMsg("invalidDimension", "shellRealize", XtCXtToolkitError,
                   "Shell widget %s has zero width and/or height",
                   &wid->core.name, &count);
    }

    /* Rebuild the XCB value list from scratch.  Subclass Realize methods
       (e.g. SimpleMenu) may have corrupted the packed array inherited from
       ComputeWindowAttributes, because the original Xlib code used an
       XSetWindowAttributes struct with named fields, while XCB requires
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
            vals[vi++] = XtBuildEventMask(wid);
        /* CW_DONT_PROPAGATE (bit 12) — not used */
        /* CW_COLORMAP (bit 13) */
        if (create_mask & XCB_CW_COLORMAP)
            vals[vi++] = wid->core.colormap;

        wid->core.window = xcb_generate_id(XtDisplay(wid));
        xcb_create_window(
            XtDisplay(wid),
            wid->core.depth,
            wid->core.window,
            wid->core.screen->root,
            wid->core.x,
            wid->core.y,
            wid->core.width,
            wid->core.height,
            wid->core.border_width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            w->shell.visual,
            create_mask,
            vals
        );

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
                xcb_change_window_attributes(XtDisplay(wid), wid->core.window,
                                             post_mask, post_vals);
        }
        /* Note: CW_CURSOR and CW_BACKING_STORE are also stripped from
           create_mask.  Cursor is handled by subclass callbacks (e.g.
           SimpleMenu's ChangeCursorOnGrab).  Backing store defaults to
           XCB_BACKING_STORE_NOT_USEFUL which is the server default. */
    }
    xcb_flush(XtDisplay(wid));

    _popup_set_prop(w);

    /* Enable XDND drop target on all shell windows */
    ISWXdndEnable(wid);

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
            && XtIsRealized(w->transient.transient_for))
            window_group = XtWindow(w->transient.transient_for);
        else if ((window_group = w->wm.wm_hints.window_group)
                 == XtUnspecifiedWindowGroup) {
            if (delete) {
                // Get atom for WM_TRANSIENT_FOR
                xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay((Widget) w), FALSE, strlen("WM_TRANSIENT_FOR"), "WM_TRANSIENT_FOR");
                xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay((Widget) w), atom_cookie, NULL);
                if (atom_reply) {
                    xcb_delete_property(XtDisplay((Widget) w), XtWindow((Widget) w), atom_reply->atom);
                    free(atom_reply);
                }
            }
                //XDeleteProperty(XtDisplay((Widget) w),
                //                XtWindow((Widget) w), XCB_ATOM_WM_TRANSIENT_FOR);
            return;
        }

        //XSetTransientForHint(XtDisplay((Widget) w),
        //                     XtWindow((Widget) w), window_group);
        // Get atom for WM_TRANSIENT_FOR
        xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay((Widget) w), FALSE, strlen("WM_TRANSIENT_FOR"), "WM_TRANSIENT_FOR");
        xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay((Widget) w), atom_cookie, NULL);
        if (atom_reply) {
            xcb_change_property(XtDisplay((Widget) w), XCB_PROP_MODE_REPLACE, XtWindow((Widget) w),
                                atom_reply->atom, XCB_ATOM_WINDOW, 32,
                                1, &window_group);
            free(atom_reply);
        }
    }
}

static void
TransientRealize(xcb_connection_t *dpy, Widget w, Mask *vmask, uint32_t *attr)
{
    XtRealizeProc realize;

    LOCK_PROCESS;
    realize =
        transientShellWidgetClass->core_class.superclass->core_class.realize;
    UNLOCK_PROCESS;
    (*realize) (XtDisplay(w), w, vmask, attr);

    _SetTransientForHint((TransientShellWidget) w, False);
}

static Widget
GetClientLeader(Widget w)
{
    while ((!XtIsWMShell(w) || !((WMShellWidget) w)->wm.client_leader)
           && w->core.parent)
        w = w->core.parent;

    /* ASSERT: w is a WMshell with client_leader set, or w has no parent */

    if (XtIsWMShell(w) && ((WMShellWidget) w)->wm.client_leader)
        w = ((WMShellWidget) w)->wm.client_leader;
    return w;
}

static void
EvaluateWMHints(WMShellWidget w)
{
    xcb_icccm_wm_hints_t *hintp = &w->wm.wm_hints;

    hintp->flags = XCB_ICCCM_WM_HINT_STATE | XCB_ICCCM_WM_HINT_INPUT;

    if (hintp->icon_x == XtUnspecifiedShellInt)
        hintp->icon_x = -1;
    else
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_POSITION;

    if (hintp->icon_y == XtUnspecifiedShellInt)
        hintp->icon_y = -1;
    else
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_POSITION;

    if (hintp->icon_pixmap != None)
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_PIXMAP;
    if (hintp->icon_mask != None)
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_MASK;
    if (hintp->icon_window != None)
        hintp->flags |= XCB_ICCCM_WM_HINT_ICON_WINDOW;

    if (hintp->window_group == XtUnspecifiedWindow) {
        if (w->core.parent) {
            Widget p;

            for (p = w->core.parent; p->core.parent; p = p->core.parent);
            if (XtIsRealized(p)) {
                hintp->window_group = XtWindow(p);
                hintp->flags |= XCB_ICCCM_WM_HINT_WINDOW_GROUP;
            }
        }
    }
    else if (hintp->window_group != XtUnspecifiedWindowGroup)
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
    else if (w->shell.client_specified & _XtShellPPositionOK)
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_POSITION;

    if (sizep->min_aspect.x != XtUnspecifiedShellInt
        || sizep->min_aspect.y != XtUnspecifiedShellInt
        || sizep->max_aspect.x != XtUnspecifiedShellInt
        || sizep->max_aspect.y != XtUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_ASPECT;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE
        || w->wm.base_width != XtUnspecifiedShellInt
        || w->wm.base_height != XtUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_BASE_SIZE;
        if (w->wm.base_width == XtUnspecifiedShellInt)
            w->wm.base_width = 0;
        if (w->wm.base_height == XtUnspecifiedShellInt)
            w->wm.base_height = 0;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC
        || sizep->width_inc != XtUnspecifiedShellInt
        || sizep->height_inc != XtUnspecifiedShellInt) {
        if (sizep->width_inc < 1)
            sizep->width_inc = 1;
        if (sizep->height_inc < 1)
            sizep->height_inc = 1;
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_RESIZE_INC;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE
        || sizep->max_width != XtUnspecifiedShellInt
        || sizep->max_height != XtUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
        if (sizep->max_width == XtUnspecifiedShellInt)
            sizep->max_width = BIGSIZE;
        if (sizep->max_height == XtUnspecifiedShellInt)
            sizep->max_height = BIGSIZE;
    }
    if (sizep->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE
        || sizep->min_width != XtUnspecifiedShellInt
        || sizep->min_height != XtUnspecifiedShellInt) {
        sizep->flags |= XCB_ICCCM_SIZE_HINT_P_MIN_SIZE;
        if (sizep->min_width == XtUnspecifiedShellInt)
            sizep->min_width = 1;
        if (sizep->min_height == XtUnspecifiedShellInt)
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
    XClassHint classhint;
    Boolean copied_iname, copied_wname;

    if (!XtIsWMShell((Widget) w) || w->shell.override_redirect)
        return;

    size_hints = calloc(1, sizeof(xcb_size_hints_t));
    if (size_hints == NULL)
        _XtAllocError("xcb_size_hints_t");

    copied_iname = copied_wname = False;
    /* Use the title/icon_name strings directly.
     * The original code used XmbTextListToTextProperty for locale-aware
     * encoding, which is Xlib-specific and has no XCB equivalent.
     * For now, use the raw strings directly. */
    window_name = (char *) wmshell->wm.title;
    if (XtIsTopLevelShell((Widget) w))
        icon_name = (char *) tlshell->topLevel.icon_name;
    else
        icon_name = NULL;

    EvaluateWMHints(wmshell);
    EvaluateSizeHints(wmshell);
    ComputeWMSizeHints(wmshell, size_hints);

    if (wmshell->wm.transient && !XtIsTransientShell((Widget) w)
        && (window_group = wmshell->wm.wm_hints.window_group)
        != XtUnspecifiedWindowGroup) {

        //XSetTransientForHint(XtDisplay((Widget) w),
        //                     XtWindow((Widget) w), window_group);
        xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay((Widget) w), FALSE, strlen("WM_TRANSIENT_FOR"), "WM_TRANSIENT_FOR");
        xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay((Widget) w), atom_cookie, NULL);
        if (atom_reply) {
            xcb_change_property(XtDisplay((Widget) w), XCB_PROP_MODE_REPLACE, XtWindow(w),
                                atom_reply->atom, XCB_ATOM_WINDOW, 32,
                                1, &window_group);
            free(atom_reply);
        }
    }

    classhint.res_name = (_XtString) w->core.name;
    /* For the class, look up to the top of the tree */
    for (p = (Widget) w; p->core.parent != NULL; p = p->core.parent);
    if (XtIsApplicationShell(p)) {
        classhint.res_class = ((ApplicationShellWidget) p)->application.class;
    }
    else {
        LOCK_PROCESS;
        classhint.res_class = (_XtString) XtClass(p)->core_class.class_name;
        UNLOCK_PROCESS;
    }

    if (XtIsApplicationShell((Widget) w)
        && (argc = appshell->application.argc) != -1)
        argv = (char **) appshell->application.argv;
    else {
        argv = NULL;
        argc = 0;
    }

    //XSetWMProperties(XtDisplay((Widget) w), XtWindow((Widget) w),
    //                 &window_name,
    //                 (XtIsTopLevelShell((Widget) w)) ? &icon_name : NULL,
    //                 argv, argc, size_hints, &wmshell->wm.wm_hints, &classhint);
    SetWMProperties((Widget)w, window_name, 
                (XtIsTopLevelShell((Widget) w)) ? icon_name : NULL,
                argv, argc, size_hints, &wmshell->wm.wm_hints, "", "");// classhint_class, classhint_name);
    if(size_hints) free(size_hints);
    //XtFree((char *) size_hints);
    //if (copied_wname)
    //    XtFree((XtPointer) window_name.value);
    //if (copied_iname)
    //    XtFree((XtPointer) icon_name.value);

    LOCK_PROCESS;
    if (XtWidgetToApplicationContext((Widget) w)->langProcRec.proc) {
        char *locale = "C"; //setlocale(LC_CTYPE, (char *) NULL);

        if (locale) {
            // Get atom for WM_LOCALE_NAME
            xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay((Widget) w), FALSE, strlen("WM_LOCALE_NAME"), "WM_LOCALE_NAME");
            xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay((Widget) w), atom_cookie, NULL);
            if (atom_reply) {
                xcb_change_property(XtDisplay((Widget) w), XCB_PROP_MODE_REPLACE, XtWindow((Widget) w),
                                    atom_reply->atom, XCB_ATOM_STRING, 8,
                                    strlen(locale), locale);
                free(atom_reply);
            }
        }
            //XChangeProperty(XtDisplay((Widget) w), XtWindow((Widget) w),
            //                XInternAtom(XtDisplay((Widget) w),
            //                            "WM_LOCALE_NAME", False),
            //                XCB_ATOM_STRING, 8, XCB_PROP_MODE_REPLACE,
            //                (unsigned char *) locale, (int) strlen(locale));
    }
    UNLOCK_PROCESS;

    p = GetClientLeader((Widget) w);
    if (XtWindow(p)) {
        xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay((Widget) w), FALSE, strlen("WM_CLIENT_LEADER"), "WM_CLIENT_LEADER");
        xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay((Widget) w), atom_cookie, NULL);
        if (atom_reply) {
            xcb_change_property(XtDisplay((Widget) w), XCB_PROP_MODE_REPLACE, XtWindow((Widget) w),
                                atom_reply->atom, XCB_ATOM_WINDOW, 32,
                                1, &(p->core.window));
            free(atom_reply);
        }
    }
        //XChangeProperty(XtDisplay((Widget) w), XtWindow((Widget) w),
        //                XInternAtom(XtDisplay((Widget) w),
        //                            "WM_CLIENT_LEADER", False),
        //                XCB_ATOM_WINDOW, 32, XCB_PROP_MODE_REPLACE,
        //                (unsigned char *) (&(p->core.window)), 1);
        
#ifndef XT_NO_SM
    if (p == (Widget) w) {
        for (; p->core.parent != NULL; p = p->core.parent);
        if (XtIsSubclass(p, sessionShellWidgetClass)) {
            String sm_client_id = ((SessionShellWidget) p)->session.session_id;

            //if (sm_client_id != NULL) {
            //    XChangeProperty(XtDisplay((Widget) w), XtWindow((Widget) w),
            //                    XInternAtom(XtDisplay((Widget) w),
            //                                "SM_CLIENT_ID", False),
            //                    XCB_ATOM_STRING, 8, XCB_PROP_MODE_REPLACE,
            //                    (unsigned char *) sm_client_id,
            //                    (int) strlen(sm_client_id));
            //}
            if (sm_client_id != NULL) {
                // Get atom for SM_CLIENT_ID
                xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay((Widget) w), FALSE, strlen("SM_CLIENT_ID"), "SM_CLIENT_ID");
                xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay((Widget) w), atom_cookie, NULL);
                if (atom_reply) {
                    xcb_change_property(XtDisplay((Widget) w), XCB_PROP_MODE_REPLACE, XtWindow((Widget) w),
                                        atom_reply->atom, XCB_ATOM_STRING, 8,
                                        strlen(sm_client_id),
                                        sm_client_id);
                    free(atom_reply);
                }
            }
        }
    }
#endif                          /* !XT_NO_SM */

    if (wmshell->wm.window_role) {
        //XChangeProperty(XtDisplay((Widget) w), XtWindow((Widget) w),
        //                XInternAtom(XtDisplay((Widget) w),
        //                            "WM_WINDOW_ROLE", False),
        //                XCB_ATOM_STRING, 8, XCB_PROP_MODE_REPLACE,
        //                (unsigned char *) wmshell->wm.window_role,
        //                (int) strlen(wmshell->wm.window_role));
        // Get atom for WM_WINDOW_ROLE
        xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay((Widget) w), FALSE, strlen("WM_WINDOW_ROLE"), "WM_WINDOW_ROLE");
        xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay((Widget) w), atom_cookie, NULL);
        if (atom_reply) {
            xcb_change_property(XtDisplay((Widget) w), XCB_PROP_MODE_REPLACE, XtWindow((Widget) w),
                                atom_reply->atom, XCB_ATOM_STRING, 8,
                                strlen(wmshell->wm.window_role),
                                wmshell->wm.window_role);
            free(atom_reply);
        }
    }
}

static void
EventHandler(Widget wid,
             XtPointer closure _X_UNUSED,
             xcb_generic_event_t *event,
             Boolean *continue_to_dispatch _X_UNUSED)
{
    register ShellWidget w = (ShellWidget) wid;
    WMShellWidget wmshell = (WMShellWidget) w;
    Boolean sizechanged = FALSE;

    switch (event->response_type) {
    case XCB_CONFIGURE_NOTIFY:
        xcb_configure_notify_event_t * cne = (xcb_configure_notify_event_t *)event;
        if (w->core.window != cne->window)
            return;             /* in case of SubstructureNotify */
#define NEQ(x)  ( w->core.x != cne->x )
        if (NEQ(width) || NEQ(height) || NEQ(border_width)) {
            sizechanged = TRUE;
#undef NEQ
            w->core.width = (Dimension) cne->width;
            w->core.height = (Dimension) cne->height;
            w->core.border_width = (Dimension) cne->border_width;
        }
        if (w->shell.client_specified & _XtShellNotReparented) {
            w->core.x = (Position) cne->x;
            w->core.y = (Position) cne->y;
            w->shell.client_specified |= _XtShellPositionValid;
        }
        else
            w->shell.client_specified &= ~_XtShellPositionValid;
        if (XtIsWMShell(wid) && !wmshell->wm.wait_for_wm) {
            /* Consider trusting the wm again */
            register struct _OldXSizeHints *hintp = &wmshell->wm.size_hints;

#define EQ(x) (hintp->x == w->core.x)
            if (EQ(x) && EQ(y) && EQ(width) && EQ(height)) {
                wmshell->wm.wait_for_wm = TRUE;
            }
#undef EQ
        }
        break;

    case XCB_REPARENT_NOTIFY:
        xcb_reparent_notify_event_t *rne = (xcb_reparent_notify_event_t *)event;
        if (rne->window == XtWindow(w)) {
            if (rne->parent != RootWindowOfScreen(XtScreen(w)))
                w->shell.client_specified &=
                    ~(_XtShellNotReparented | _XtShellPositionValid);
            else {
                w->core.x = (Position) rne->x;
                w->core.y = (Position) rne->y;
                w->shell.client_specified |=
                    (_XtShellNotReparented | _XtShellPositionValid);
            }
        }
        return;

    case XCB_MAP_NOTIFY:
        if (XtIsTopLevelShell(wid)) {
            ((TopLevelShellWidget) wid)->topLevel.iconic = FALSE;
        }
        return;

    case XCB_UNMAP_NOTIFY:
    {
        xcb_unmap_notify_event_t *une = (xcb_unmap_notify_event_t *)event; 
        XtPerDisplayInput pdi;
        XtDevice device;
        Widget p;

        if (XtIsTopLevelShell(wid))
            ((TopLevelShellWidget) wid)->topLevel.iconic = TRUE;

        //getting display via Shell widget as the event 
        //doesn't carry the pointer in XCB
        //presumably a shellwidget per display and the events are routed appropriately
        //otherwise this may break
        pdi = _XtGetPerDisplayInput(XtDisplay(w));

        device = &pdi->pointer;

        if (device->grabType == XtPassiveServerGrab) {
            p = device->grab.widget;
            while (p && !(XtIsShell(p)))
                p = p->core.parent;
            if (p == wid)
                device->grabType = XtNoServerGrab;
        }

        device = &pdi->keyboard;
        if (IsEitherPassiveGrab(device->grabType)) {
            p = device->grab.widget;
            while (p && !(XtIsShell(p)))
                p = p->core.parent;
            if (p == wid) {
                device->grabType = XtNoServerGrab;
                pdi->activatingKey = 0;
            }
        }

        return;
    }
    default:
        return;
    }
    {
        XtWidgetProc resize;

        LOCK_PROCESS;
        resize = XtClass(wid)->core_class.resize;
        UNLOCK_PROCESS;

        if (sizechanged && resize) {
            CALLGEOTAT(_XtGeoTrace((Widget) w,
                                   "Shell \"%s\" is being resized to %d %d.\n",
                                   XtName(wid), wid->core.width,
                                   wid->core.height));
            (*resize) (wid);
        }
    }
}

static void
Destroy(Widget wid)
{
    if (XtIsRealized(wid))
        xcb_destroy_window(XtDisplay(wid), XtWindow(wid));
}

static void
WMDestroy(Widget wid)
{
    WMShellWidget w = (WMShellWidget) wid;

    XtFree((char *) w->wm.title);
    XtFree((char *) w->wm.window_role);
}

static void
TopLevelDestroy(Widget wid)
{
    TopLevelShellWidget w = (TopLevelShellWidget) wid;

    XtFree((char *) w->topLevel.icon_name);
}

static void
ApplicationDestroy(Widget wid)
{
    ApplicationShellWidget w = (ApplicationShellWidget) wid;

    if (w->application.argc > 0)
        FreeStringArray(w->application.argv);
}

static void
SessionDestroy(Widget wid)
{
#ifndef XT_NO_SM
    SessionShellWidget w = (SessionShellWidget) wid;

    StopManagingSession(w, w->session.connection);
    XtFree(w->session.session_id);
    FreeStringArray(w->session.restart_command);
    FreeStringArray(w->session.clone_command);
    FreeStringArray(w->session.discard_command);
    FreeStringArray(w->session.resign_command);
    FreeStringArray(w->session.shutdown_command);
    FreeStringArray(w->session.environment);
    XtFree(w->session.current_dir);
    XtFree((_XtString) w->session.program_path);
#endif                          /* !XT_NO_SM */
}

/* -----------------------------------------------------------------------
 * Geometry parse flags (same values as Xlib XValue/YValue/etc.)
 * ----------------------------------------------------------------------- */
#define XtNoValue     0x0000
#define XtXValue      0x0001
#define XtYValue      0x0002
#define XtWidthValue  0x0004
#define XtHeightValue 0x0008
#define XtAllValues   0x000F
#define XtNegative    0x0010

/* -----------------------------------------------------------------------
 * _XtParseGeometry: pure string parser for geometry strings like
 * "800x600+100+200" or "=800x600+100+200".
 * Returns a bitmask of which fields were specified.
 * ----------------------------------------------------------------------- */
static int
_XtParseGeometry(const char *string,
                 int *x, int *y,
                 unsigned int *width, unsigned int *height)
{
    int mask = XtNoValue;
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
            mask |= XtWidthValue;
            s = end;
        }
        /* Height */
        if (*s == 'x' || *s == 'X') {
            s++;
            val = strtol(s, &end, 10);
            if (end != s) {
                *height = (unsigned int) val;
                mask |= XtHeightValue;
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
            mask |= XtXValue;
            if (negative) mask |= XtNegative;
            s = end;
        }
        /* Y offset */
        if (*s == '+' || *s == '-') {
            negative = (*s == '-');
            s++;
            val = strtol(s, &end, 10);
            if (end != s) {
                *y = negative ? -(int) val : (int) val;
                mask |= XtYValue;
                s = end;
            }
        }
    }

    return mask;
}

/* -----------------------------------------------------------------------
 * _XtWMGeometry: XWMGeometry equivalent using _XtParseGeometry.
 * Parses geometry strings and applies size hints.
 * ----------------------------------------------------------------------- */
static int
_XtWMGeometry(xcb_screen_t *screen _X_UNUSED,
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
        _XtParseGeometry(def_geom, &x, &y, &width, &height);

    /* Override with user geometry */
    if (user_geom != NULL) {
        int ux = 0, uy = 0;
        unsigned int uw = 0, uh = 0;
        int umask = _XtParseGeometry(user_geom, &ux, &uy, &uw, &uh);
        if (umask & XtXValue)      { x = ux; }
        if (umask & XtYValue)      { y = uy; }
        if (umask & XtWidthValue)  { width = uw; }
        if (umask & XtHeightValue) { height = uh; }
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
    Boolean is_wmshell = XtIsWMShell(W);
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
        flag = _XtWMGeometry(XtScreen(W),
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

            params[0] = XtName(W);
            params[1] = w->shell.geometry;
            XtAppWarningMsg(XtWidgetToApplicationContext(W),
                            "badGeometry", "shellRealize", XtCXtToolkitError,
                            "Shell widget \"%s\" has an invalid geometry specification: \"%s\"",
                            params, &num_params);
        }
    }
    else
        flag = 0;

    if (is_wmshell) {
        WMShellWidget wmshell = (WMShellWidget) w;

        if (wmshell->wm.win_gravity == XtUnspecifiedShellInt) {
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
    w->shell.client_specified |= _XtShellGeometryParsed;
}

static void
ChangeManaged(Widget wid)
{
    ShellWidget w = (ShellWidget) wid;
    Widget child = NULL;
    Cardinal i;

    for (i = 0; i < w->composite.num_children; i++) {
        if (XtIsManaged(w->composite.children[i])) {
            child = w->composite.children[i];
            break;              /* there can only be one of them! */
        }
    }

    if (!XtIsRealized(wid))     /* then we're about to be realized... */
        GetGeometry(wid, child);

    if (child != NULL)
        XtConfigureWidget(child, (Position) 0, (Position) 0,
                          w->core.width, w->core.height, (Dimension) 0);
}

/*
 * This is gross, I can't wait to see if the change happened so I will ask
 * the window manager to change my size and do the appropriate X work.
 * I will then tell the requester that he can.  Care must be taken because
 * it is possible that some time in the future the request will be
 * asynchronusly denied and the window reverted to it's old size/shape.
 */

static XtGeometryResult
GeometryManager(Widget wid,
                XtWidgetGeometry *request,
                XtWidgetGeometry *reply _X_UNUSED)
{
    ShellWidget shell = (ShellWidget) (wid->core.parent);
    XtWidgetGeometry my_request;

    if (shell->shell.allow_shell_resize == FALSE && XtIsRealized(wid))
        return (XtGeometryNo);

    if (request->request_mode & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y))
        return (XtGeometryNo);

    my_request.request_mode = (request->request_mode & XtCWQueryOnly);
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
    if (XtMakeGeometryRequest((Widget) shell, &my_request, NULL)
        == XtGeometryYes) {
        /* assert: if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH) then
         *            shell->core.width == request->width
         * assert: if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) then
         *            shell->core.height == request->height
         *
         * so, whatever the WM sized us to (if the Shell requested
         * only one of the two) is now the correct child size
         */

        if (!(request->request_mode & XtCWQueryOnly)) {
            wid->core.width = shell->core.width;
            wid->core.height = shell->core.height;
            if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
                wid->core.x = wid->core.y = (Position) (-request->border_width);
            }
        }
        return XtGeometryYes;
    }
    else
        return XtGeometryNo;
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
//    if ((dpy != XtDisplay(w)) || (rne->window != XtWindow(w))) {
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
//        if (rne->window == XtWindow(w)) {
//        /* we might get ahead of this event, so just in case someone
//         * asks for coordinates before this event is dispatched...
//         */
//        register ShellWidget s = (ShellWidget) w;
//
//        if (rne->parent != RootWindowOfScreen(XtScreen(w)))
//            s->shell.client_specified &= ~_XtShellNotReparented;
//        else
//            s->shell.client_specified |= _XtShellNotReparented;
//        }
//    }
//
//    return FALSE;
//}

static Boolean
_wait_for_response(ShellWidget w, xcb_generic_event_t **event_out,
                   unsigned long request_num)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    unsigned long timeout;
    struct timespec start, now;

    if (XtIsWMShell((Widget) w))
        timeout = (unsigned long) ((WMShellWidget) w)->wm.wm_timeout;
    else
        timeout = DEFAULT_WM_TIMEOUT;

    xcb_flush(conn);
    clock_gettime(CLOCK_MONOTONIC, &start);

    *event_out = NULL;

    for (;;) {
        xcb_generic_event_t *ev = xcb_poll_for_event(conn);
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
                    if (rne->parent != RootWindowOfScreen(XtScreen(w)))
                        w->shell.client_specified &= ~_XtShellNotReparented;
                    else
                        w->shell.client_specified |= _XtShellNotReparented;
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

static XtGeometryResult
RootGeometryManager(Widget gw,
                    XtWidgetGeometry *request,
                    XtWidgetGeometry *reply _X_UNUSED)
{
    register ShellWidget w = (ShellWidget) gw;
    xcb_configure_window_value_list_t values;
    unsigned int mask = request->request_mode;
    xcb_generic_event_t *event = NULL;
    Boolean wm;
    register struct _OldXSizeHints *hintp = NULL;
    int oldx, oldy, oldwidth, oldheight, oldborder_width;
    unsigned long request_num;

    CALLGEOTAT(_XtGeoTab(1));

    if (XtIsWMShell(gw)) {
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
            values.sibling = XtWindow(request->sibling);
    }

    if (!XtIsRealized((Widget) w)) {
        CALLGEOTAT(_XtGeoTrace((Widget) w,
                               "Shell \"%s\" is not realized, return XtGeometryYes.\n",
                               XtName((Widget) w)));
        CALLGEOTAT(_XtGeoTab(-1));
        return XtGeometryYes;
    }

    CALLGEOTAT(_XtGeoTrace((Widget) w, "XConfiguring the Shell X window :\n"));
    CALLGEOTAT(_XtGeoTab(1));
#ifdef XT_GEO_TATTLER
    if (mask & XCB_CONFIG_WINDOW_X) {
        CALLGEOTAT(_XtGeoTrace((Widget) w, "x = %d\n", values.x));
    }
    if (mask & XCB_CONFIG_WINDOW_Y) {
        CALLGEOTAT(_XtGeoTrace((Widget) w, "y = %d\n", values.y));
    }
    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        CALLGEOTAT(_XtGeoTrace((Widget) w, "width = %d\n", values.width));
    }
    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        CALLGEOTAT(_XtGeoTrace((Widget) w, "height = %d\n", values.height));
    }
    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        CALLGEOTAT(_XtGeoTrace((Widget) w,
                               "border_width = %d\n", values.border_width));
    }
#endif
    CALLGEOTAT(_XtGeoTab(-1));
    xcb_void_cookie_t cookie = xcb_configure_window(XtDisplay((Widget) w),
        XtWindow((Widget) w), mask, (const void *)&values);
    request_num = cookie.sequence;
    
    if (wm && !w->shell.override_redirect
        && mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
        _SetWMSizeHints((WMShellWidget) w);
    }

    if (w->shell.override_redirect) {
        CALLGEOTAT(_XtGeoTrace
                   ((Widget) w,
                    "Shell \"%s\" is override redirect, return XtGeometryYes.\n",
                    XtName((Widget) w)));
        CALLGEOTAT(_XtGeoTab(-1));
        return XtGeometryYes;
    }

    /* If no non-stacking bits are set, there's no way to tell whether
       or not this worked, so assume it did */

    if (!(mask & (unsigned) (~(XCB_CONFIG_WINDOW_STACK_MODE | XCB_CONFIG_WINDOW_SIBLING))))
        return XtGeometryYes;

    if (wm && ((WMShellWidget) w)->wm.wait_for_wm == FALSE) {
        /* the window manager is sick
         * so I will do the work and
         * say no so if a new WM starts up,
         * or the current one recovers
         * my size requests will be visible
         */
        CALLGEOTAT(_XtGeoTrace
                   ((Widget) w,
                    "Shell \"%s\" has wait_for_wm == FALSE, return XtGeometryNo.\n",
                    XtName((Widget) w)));
        CALLGEOTAT(_XtGeoTab(-1));

        PutBackGeometry();
        return XtGeometryNo;
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
#ifdef XT_GEO_TATTLER
                if (NEQ(x, XCB_CONFIG_WINDOW_X)) {
                    CALLGEOTAT(_XtGeoTrace((Widget) w,
                                           "received Configure X %d\n",
                                           event.xconfigure.x));
                }
                if (NEQ(y, XCB_CONFIG_WINDOW_Y)) {
                    CALLGEOTAT(_XtGeoTrace((Widget) w,
                                           "received Configure Y %d\n",
                                           event.xconfigure.y));
                }
                if (NEQ(width, XCB_CONFIG_WINDOW_WIDTH)) {
                    CALLGEOTAT(_XtGeoTrace((Widget) w,
                                           "received Configure Width %d\n",
                                           event.xconfigure.width));
                }
                if (NEQ(height, XCB_CONFIG_WINDOW_HEIGHT)) {
                    CALLGEOTAT(_XtGeoTrace((Widget) w,
                                           "received Configure Height %d\n",
                                           event.xconfigure.height));
                }
                if (NEQ(border_width, XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
                    CALLGEOTAT(_XtGeoTrace((Widget) w,
                                           "received Configure BorderWidth %d\n",
                                           event.xconfigure.border_width));
                }
#endif
#undef NEQ
                //#TODO just push the event back into the AppContext queue
                //XPutBackEvent(XtDisplay(w), &event);
                PutBackGeometry();
                /*
                 * We just potentially re-ordered the event queue
                 * w.r.t. ConfigureNotifies with some trepidation.
                 * But this is probably a Good Thing because we
                 * will know the new true state of the world sooner
                 * this way.
                 */
                CALLGEOTAT(_XtGeoTrace((Widget) w,
                                       "ConfigureNotify failed, return XtGeometryNo.\n"));
                CALLGEOTAT(_XtGeoTab(-1));
                free(event);
                return XtGeometryNo;
            }
            else {
                w->core.width = (Dimension) cne->width;
                w->core.height = (Dimension) cne->height;
                w->core.border_width =
                    (Dimension) cne->border_width;
                if (w->shell.client_specified & _XtShellNotReparented) {

                    w->core.x = (Position) cne->x;
                    w->core.y = (Position) cne->y;
                    w->shell.client_specified |= _XtShellPositionValid;
                }
                else
                    w->shell.client_specified &= ~_XtShellPositionValid;
                CALLGEOTAT(_XtGeoTrace((Widget) w,
                                       "ConfigureNotify succeed, return XtGeometryYes.\n"));
                CALLGEOTAT(_XtGeoTab(-1));
                free(event);
                return XtGeometryYes;
            }
        }
        else if (!wm) {
            PutBackGeometry();
            CALLGEOTAT(_XtGeoTrace((Widget) w,
                                   "Not wm, return XtGeometryNo.\n"));
            CALLGEOTAT(_XtGeoTab(-1));
            free(event);
            return XtGeometryNo;
        }
        else
            XtAppWarningMsg(XtWidgetToApplicationContext((Widget) w),
                            "internalError", "shell", XtCXtToolkitError,
                            "Shell's window manager interaction is broken",
                            NULL, NULL);
    }
    else if (wm) {              /* no event */
        ((WMShellWidget) w)->wm.wait_for_wm = FALSE;    /* timed out; must be broken */
    }
    PutBackGeometry();
#undef PutBackGeometry
    CALLGEOTAT(_XtGeoTrace((Widget) w,
                           "Timeout passed?, return XtGeometryNo.\n"));
    CALLGEOTAT(_XtGeoTab(-1));
    return XtGeometryNo;
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
    XSetWindowAttributes attr;

    if (!XtIsRealized(new))
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
        //XChangeWindowAttributes(XtDisplay(new), XtWindow(new), mask, &attr);
        xcb_change_window_attributes(XtDisplay(new), XtWindow(new), mask, &attr);
        if ((mask & XCB_CW_OVERRIDE_REDIRECT) && !nw->shell.override_redirect)
            _popup_set_prop(nw);
    }

    if (!(ow->shell.client_specified & _XtShellPositionValid)) {
        Cardinal n;

        for (n = *num_args; n; n--, args++) {
            if (strcmp(XtNx, args->name) == 0) {
                _XtShellGetCoordinates((Widget) ow, &ow->core.x, &ow->core.y);
            }
            else if (strcmp(XtNy, args->name) == 0) {
                _XtShellGetCoordinates((Widget) ow, &ow->core.x, &ow->core.y);
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
    Boolean set_prop = XtIsRealized(new) && !nwmshell->shell.override_redirect;
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
        XtFree(owmshell->wm.title);
        if (!nwmshell->wm.title)
            nwmshell->wm.title = (_XtString) "";
        nwmshell->wm.title = XtNewString(nwmshell->wm.title);
        title_changed = True;
    }
    else
        title_changed = False;

    if (set_prop
        && (title_changed ||
            nwmshell->wm.title_encoding != owmshell->wm.title_encoding)) {

        //XTextProperty title;
        Boolean copied = False;

        //if (nwmshell->wm.title_encoding == None &&
        //    XmbTextListToTextProperty(XtDisplay(new),
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
        //XSetWMName(XtDisplay(new), XtWindow(new), &title);
        // First, get the atom for WM_NAME
        xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_NAME"), "WM_NAME");
        xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
        if (atom_reply) {
            xcb_change_property(XtDisplay(new), XCB_PROP_MODE_REPLACE, XtWindow(new),
                                atom_reply->atom, XCB_ATOM_STRING, 8,
                                strlen((unsigned char *) nwmshell->wm.title), (unsigned char *) nwmshell->wm.title);
            free(atom_reply);
        }
        //if (copied)
        //    XtFree((XtPointer) title.value);
    }

    EvaluateWMHints(nwmshell);

#define NEQ(f)  (nwmshell->wm.wm_hints.f != owmshell->wm.wm_hints.f)

    if (set_prop && (NEQ(flags) || NEQ(input) || NEQ(initial_state)
                     || NEQ(icon_x) || NEQ(icon_y)
                     || NEQ(icon_pixmap) || NEQ(icon_mask) || NEQ(icon_window)
                     || NEQ(window_group))) {

        //XSetWMHints(XtDisplay(new), XtWindow(new), &nwmshell->wm.wm_hints);
        xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_HINTS"), "WM_HINTS");
        xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
        if (atom_reply) {
            xcb_icccm_set_wm_hints(XtDisplay(new), XtWindow(new), &nwmshell->wm.wm_hints);

            free(atom_reply);
        }
    }
#undef NEQ

    //if (XtIsRealized(new) && nwmshell->wm.transient != owmshell->wm.transient) {
    //    if (nwmshell->wm.transient) {
    //        if (!XtIsTransientShell(new) &&
    //            !nwmshell->shell.override_redirect &&
    //            nwmshell->wm.wm_hints.window_group != XtUnspecifiedWindowGroup)
    //            XSetTransientForHint(XtDisplay(new), XtWindow(new),
    //                                 nwmshell->wm.wm_hints.window_group);
    //    }
    //    else
    //        XDeleteProperty(XtDisplay(new), XtWindow(new), XCB_ATOM_WM_TRANSIENT_FOR);
    //}

    if (XtIsRealized(new) && nwmshell->wm.transient != owmshell->wm.transient) {
        if (nwmshell->wm.transient) {
            if (!XtIsTransientShell(new) &&
                !nwmshell->shell.override_redirect &&
                nwmshell->wm.wm_hints.window_group != XtUnspecifiedWindowGroup) {
                
                // Set WM_TRANSIENT_FOR property
                xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_TRANSIENT_FOR"), "WM_TRANSIENT_FOR");
                xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
                if (atom_reply) {
                    xcb_change_property(XtDisplay(new), XCB_PROP_MODE_REPLACE, XtWindow(new),
                                        atom_reply->atom, XCB_ATOM_WINDOW, 32,
                                        1, &nwmshell->wm.wm_hints.window_group);
                    free(atom_reply);
                }
            }
        }
        else {
            // Delete WM_TRANSIENT_FOR property
            xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_TRANSIENT_FOR"), "WM_TRANSIENT_FOR");
            xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
            if (atom_reply) {
                xcb_delete_property(XtDisplay(new), XtWindow(new), atom_reply->atom);
                free(atom_reply);
            }
        }
    }

    if (nwmshell->wm.client_leader != owmshell->wm.client_leader
        && XtWindow(new) && !nwmshell->shell.override_redirect) {
        Widget leader = GetClientLeader(new);

        if (XtWindow(leader)) {
            // Get atom for WM_CLIENT_LEADER
            xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_CLIENT_LEADER"), "WM_CLIENT_LEADER");
            xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
            if (atom_reply) {
                xcb_change_property(XtDisplay(new), XCB_PROP_MODE_REPLACE, XtWindow(new),
                                    atom_reply->atom, XCB_ATOM_WINDOW, 32,
                                    1, &(leader->core.window));
                free(atom_reply);
            }
        }
            //XChangeProperty(XtDisplay(new), XtWindow(new),
            //                XInternAtom(XtDisplay(new),
            //                            "WM_CLIENT_LEADER", False),
            //                XCB_ATOM_WINDOW, 32, XCB_PROP_MODE_REPLACE,
            //                (unsigned char *) &(leader->core.window), 1);
    }//

    //if (nwmshell->wm.window_role != owmshell->wm.window_role) {
    //    XtFree((_XtString) owmshell->wm.window_role);
    //    if (set_prop && nwmshell->wm.window_role) {
    //        XChangeProperty(XtDisplay(new), XtWindow(new),
    //                        XInternAtom(XtDisplay(new), "WM_WINDOW_ROLE",
    //                                    False),
    //                        XCB_ATOM_STRING, 8, XCB_PROP_MODE_REPLACE,
    //                        (unsigned char *) nwmshell->wm.window_role,
    //                        (int) strlen(nwmshell->wm.window_role));
    //    }
    //    else if (XtIsRealized(new) && !nwmshell->wm.window_role) {
    //        XDeleteProperty(XtDisplay(new), XtWindow(new),
    //                        XInternAtom(XtDisplay(new), "WM_WINDOW_ROLE",
    //                                    False));
    //    }
    //}
    if (nwmshell->wm.window_role != owmshell->wm.window_role) {
        XtFree((_XtString) owmshell->wm.window_role);

        if (set_prop && nwmshell->wm.window_role) {
            // Get atom for WM_WINDOW_ROLE
            xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_WINDOW_ROLE"), "WM_WINDOW_ROLE");
            xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
            if (atom_reply) {
                xcb_change_property(XtDisplay(new), XCB_PROP_MODE_REPLACE, XtWindow(new),
                                    atom_reply->atom, XCB_ATOM_STRING, 8,
                                    strlen(nwmshell->wm.window_role),
                                    nwmshell->wm.window_role);
                free(atom_reply);
            }
        }
        else if (XtIsRealized(new) && !nwmshell->wm.window_role) {
            // Get atom for WM_WINDOW_ROLE
            xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_WINDOW_ROLE"), "WM_WINDOW_ROLE");
            xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
            if (atom_reply) {
                xcb_delete_property(XtDisplay(new), XtWindow(new), atom_reply->atom);
                free(atom_reply);
            }
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

    if (XtIsRealized(newW)
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
        XtFree((XtPointer) old->topLevel.icon_name);
        if (!new->topLevel.icon_name)
            new->topLevel.icon_name = (_XtString) "";
        new->topLevel.icon_name = XtNewString(new->topLevel.icon_name);
        name_changed = True;
    }
    else
        name_changed = False;

    if (XtIsRealized(newW)) {
        if (new->topLevel.iconic != old->topLevel.iconic) {
            if (new->topLevel.iconic) {
                //XIconifyWindow(XtDisplay(newW),
                //               XtWindow(newW),
                //               XScreenNumberOfScreen(XtScreen(newW))
                //    );
                // XCB doesn't have a direct equivalent to XIconifyWindow

                // First, get the necessary atoms
                xcb_intern_atom_cookie_t net_wm_state_cookie = xcb_intern_atom(XtDisplay(newW), FALSE, strlen("_NET_WM_STATE"), "_NET_WM_STATE");
                xcb_intern_atom_cookie_t net_wm_state_hidden_cookie = xcb_intern_atom(XtDisplay(newW), FALSE, strlen("_NET_WM_STATE_HIDDEN"), "_NET_WM_STATE_HIDDEN");

                xcb_intern_atom_reply_t *net_wm_state_reply = xcb_intern_atom_reply(XtDisplay(newW), net_wm_state_cookie, NULL);
                xcb_intern_atom_reply_t *net_wm_state_hidden_reply = xcb_intern_atom_reply(XtDisplay(newW), net_wm_state_hidden_cookie, NULL);

                if (net_wm_state_reply && net_wm_state_hidden_reply) {
                    // Send the client message
                    xcb_client_message_event_t *event = malloc(sizeof(xcb_client_message_event_t));
                    event->response_type = XCB_CLIENT_MESSAGE;
                    event->format = 32;
                    event->window = XtWindow(newW);
                    event->type = net_wm_state_reply->atom;
                    event->data.data32[0] = 1; // _NET_WM_STATE_ADD
                    event->data.data32[1] = net_wm_state_hidden_reply->atom;
                    event->data.data32[2] = 0;
                    event->data.data32[3] = 0;
                    event->data.data32[4] = 0;

                    xcb_send_event(XtDisplay(newW), 0, XtWindow(newW), XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (char*)event);
                    free(event);
                }

                free(net_wm_state_reply);
                free(net_wm_state_hidden_reply);
            }
            else {
                Boolean map = new->shell.popped_up;

                XtPopup(newW, XtGrabNone);
                if (map) {
                    xcb_map_window(XtDisplay(newW), XtWindow(newW)); 
                    xcb_flush(XtDisplay(newW));
                }
            }
        }

        if (!new->shell.override_redirect &&
            (name_changed ||
             (old->topLevel.icon_name_encoding
              != new->topLevel.icon_name_encoding))) {

            //XTextProperty icon_name;
            Boolean copied = False;

            //if (new->topLevel.icon_name_encoding == None &&
            //    XmbTextListToTextProperty(XtDisplay(newW),
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
            xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(newW), FALSE, strlen("WM_ICON_NAME"), "WM_ICON_NAME");
            xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(newW), atom_cookie, NULL);
            if (atom_reply) {
                // Set the icon name property
                xcb_change_property(XtDisplay(newW), XCB_PROP_MODE_REPLACE, XtWindow(newW),
                                    atom_reply->atom, XCB_ATOM_STRING, 8,
                                    strlen((unsigned char *) new->topLevel.icon_name), (unsigned char *) new->topLevel.icon_name);
                free(atom_reply);
            }

            //if (copied)
            //    XtFree((XtPointer) icon_name.value);
        }
    }
    else if (new->topLevel.iconic != old->topLevel.iconic) {
        if (new->topLevel.iconic)
            new->wm.wm_hints.initial_state = XCB_ICCCM_WM_STATE_ICONIC;
    }
    return False;
}

/* do not assume it's terminated by a NULL element */
static _XtString *
NewArgv(int count, _XtString *str)
{
    Cardinal nbytes = 0;
    Cardinal num = 0;
    _XtString *newarray;
    _XtString *new;
    _XtString *strarray = str;
    _XtString sptr;

    if (count <= 0 || !str)
        return NULL;

    for (num = (Cardinal) count; num--; str++) {
        nbytes = (nbytes + (Cardinal) strlen(*str));
        nbytes++;
    }
    num = (Cardinal) ((size_t) (count + 1) * sizeof(_XtString));
    new = newarray = (_XtString *) __XtMalloc(num + nbytes);
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

        //if (XtIsRealized(new) && !nw->shell.override_redirect) {
        //    if (nw->application.argc >= 0 && nw->application.argv)
        //        XSetCommand(XtDisplay(new), XtWindow(new),
        //                    nw->application.argv, nw->application.argc);
        //    else
        //        XDeleteProperty(XtDisplay(new), XtWindow(new), XCB_ATOM_WM_COMMAND);
        //}
        if (XtIsRealized(new) && !nw->shell.override_redirect) {
            if (nw->application.argc >= 0 && nw->application.argv) {
                // First, get the atom ID for WM_COMMAND
                xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_COMMAND"), "WM_COMMAND");
                xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
                if (atom_reply) {
                    // Set the command property
                    xcb_change_property(XtDisplay(new), XCB_PROP_MODE_REPLACE, XtWindow(new),
                                        atom_reply->atom, XCB_ATOM_STRING, 8,
                                        nw->application.argc, nw->application.argv);
                    free(atom_reply);
                }
            } else {
                // First, get the atom ID for WM_COMMAND
                xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(new), FALSE, strlen("WM_COMMAND"), "WM_COMMAND");
                xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(new), atom_cookie, NULL);
                if (atom_reply) {
                    // Delete the property
                    xcb_delete_property(XtDisplay(new), XtWindow(new), atom_reply->atom);
                    free(atom_reply);
                }
            }
        }
    }
    return False;
}

static Boolean
SessionSetValues(Widget current,
                 Widget request _X_UNUSED,
                 Widget new,
                 ArgList args _X_UNUSED,
                 Cardinal *num_args _X_UNUSED)
{
#ifndef XT_NO_SM
    SessionShellWidget nw = (SessionShellWidget) new;
    SessionShellWidget cw = (SessionShellWidget) current;
    unsigned long set_mask = 0UL;
    unsigned long unset_mask = 0UL;
    Boolean initialize = False;

    if (cw->session.session_id != nw->session.session_id) {
        nw->session.session_id = XtNewString(nw->session.session_id);
        XtFree(cw->session.session_id);
    }

    if (cw->session.clone_command != nw->session.clone_command) {
        if (nw->session.clone_command) {
            nw->session.clone_command =
                NewStringArray(nw->session.clone_command);
            set_mask |= XtCloneCommandMask;
        }
        else
            unset_mask |= XtCloneCommandMask;
        FreeStringArray(cw->session.clone_command);
    }

    if (cw->session.current_dir != nw->session.current_dir) {
        if (nw->session.current_dir) {
            nw->session.current_dir = XtNewString(nw->session.current_dir);
            set_mask |= XtCurrentDirectoryMask;
        }
        else
            unset_mask |= XtCurrentDirectoryMask;
        XtFree((char *) cw->session.current_dir);
    }

    if (cw->session.discard_command != nw->session.discard_command) {
        if (nw->session.discard_command) {
            nw->session.discard_command =
                NewStringArray(nw->session.discard_command);
            set_mask |= XtDiscardCommandMask;
        }
        else
            unset_mask |= XtDiscardCommandMask;
        FreeStringArray(cw->session.discard_command);
    }

    if (cw->session.environment != nw->session.environment) {
        if (nw->session.environment) {
            nw->session.environment = NewStringArray(nw->session.environment);
            set_mask |= XtEnvironmentMask;
        }
        else
            unset_mask |= XtEnvironmentMask;
        FreeStringArray(cw->session.environment);
    }

    if (cw->session.program_path != nw->session.program_path) {
        if (nw->session.program_path) {
            nw->session.program_path = XtNewString(nw->session.program_path);
            set_mask |= XtProgramMask;
        }
        else
            unset_mask |= XtProgramMask;
        XtFree((char *) cw->session.program_path);
    }

    if (cw->session.resign_command != nw->session.resign_command) {
        if (nw->session.resign_command) {
            nw->session.resign_command =
                NewStringArray(nw->session.resign_command);
            set_mask |= XtResignCommandMask;
        }
        else
            set_mask |= XtResignCommandMask;
        FreeStringArray(cw->session.resign_command);
    }

    if (cw->session.restart_command != nw->session.restart_command) {
        if (nw->session.restart_command) {
            nw->session.restart_command =
                NewStringArray(nw->session.restart_command);
            set_mask |= XtRestartCommandMask;
        }
        else
            unset_mask |= XtRestartCommandMask;
        FreeStringArray(cw->session.restart_command);
    }

    if (cw->session.restart_style != nw->session.restart_style)
        set_mask |= XtRestartStyleHintMask;

    if (cw->session.shutdown_command != nw->session.shutdown_command) {
        if (nw->session.shutdown_command) {
            nw->session.shutdown_command =
                NewStringArray(nw->session.shutdown_command);
            set_mask |= XtShutdownCommandMask;
        }
        else
            unset_mask |= XtShutdownCommandMask;
        FreeStringArray(cw->session.shutdown_command);
    }

    if ((!cw->session.join_session && nw->session.join_session) ||
        (!cw->session.connection && nw->session.connection)) {
        JoinSession(nw);
        initialize = True;
    }

    if (nw->session.connection && (set_mask || unset_mask || initialize))
        SetSessionProperties((SessionShellWidget) new, initialize, set_mask,
                             unset_mask);

    if ((cw->session.join_session && !nw->session.join_session) ||
        (cw->session.connection && !nw->session.connection))
        StopManagingSession(nw, nw->session.connection);
#endif                          /* !XT_NO_SM */

    if (cw->wm.client_leader != nw->wm.client_leader ||
        cw->session.session_id != nw->session.session_id) {
        Widget leader;

        if (cw->session.session_id) {
            leader = GetClientLeader(current);
            //if (XtWindow(leader))
            //    XDeleteProperty(XtDisplay(leader), XtWindow(leader),
            //                    XInternAtom(XtDisplay(leader), "SM_CLIENT_ID",
            //                                False));
            if (XtWindow(leader)) {
                xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(leader), FALSE, strlen("SM_CLIENT_ID"), "SM_CLIENT_ID");
                xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(leader), atom_cookie, NULL);
                if (atom_reply) {
                    xcb_delete_property(XtDisplay(leader), XtWindow(leader), atom_reply->atom);
                    free(atom_reply);
                }
            }
        }
        if (nw->session.session_id) {
            leader = GetClientLeader(new);
            if (XtWindow(leader)) {
                //XChangeProperty(XtDisplay(leader), XtWindow(leader),
                //                XInternAtom(XtDisplay(leader), "SM_CLIENT_ID",
                //                            False),
                //                XCB_ATOM_STRING, 8, XCB_PROP_MODE_REPLACE,
                //                (unsigned char *) nw->session.session_id,
                //                (int) strlen(nw->session.session_id));
                // First, get the atom ID
                xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(XtDisplay(leader), FALSE, strlen("SM_CLIENT_ID"), "SM_CLIENT_ID");
                xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(XtDisplay(leader), atom_cookie, NULL);
                if (atom_reply) {
                    xcb_change_property(XtDisplay(leader), XCB_PROP_MODE_REPLACE, XtWindow(leader),
                                        atom_reply->atom, XCB_ATOM_STRING, 8,
                                        strlen(nw->session.session_id),
                                        nw->session.session_id);
                    free(atom_reply);
                }
            }
        }
    }
    return False;
}

void
_XtShellGetCoordinates(Widget widget, Position *x, Position *y)
{
    ShellWidget w = (ShellWidget) widget;

    if (XtIsRealized(widget) &&
        !(w->shell.client_specified & _XtShellPositionValid)) {
        int tmpx, tmpy;
        xcb_window_t tmpchild;

        //(void) XTranslateCoordinates(XtDisplay(w), XtWindow(w),
        //                             RootWindowOfScreen(XtScreen(w)),
        //                             (int) -w->core.border_width,
        //                             (int) -w->core.border_width,
        //                             &tmpx, &tmpy, &tmpchild);
                                     

        xcb_translate_coordinates_cookie_t cookie = xcb_translate_coordinates(XtDisplay(w),
            XtWindow(w),
            RootWindowOfScreen(XtScreen(w)),
            -w->core.border_width,
            -w->core.border_width);

        xcb_translate_coordinates_reply_t *reply = xcb_translate_coordinates_reply(XtDisplay(w), cookie, NULL);
        if (reply) {
            tmpx = reply->dst_x;
            tmpy = reply->dst_y;
            tmpchild = reply->child;
            free(reply);
        }
        w->core.x = (Position) tmpx;
        w->core.y = (Position) tmpy;
        w->shell.client_specified |= _XtShellPositionValid;
    }
    *x = w->core.x;
    *y = w->core.y;
}

static void
GetValuesHook(Widget widget, ArgList args, Cardinal *num_args)
{
    ShellWidget w = (ShellWidget) widget;

    /* x and y resource values may be invalid after a shell resize */
    if (XtIsRealized(widget) &&
        !(w->shell.client_specified & _XtShellPositionValid)) {
        Cardinal n;
        Position x, y;

        for (n = *num_args; n; n--, args++) {
            if (strcmp(XtNx, args->name) == 0) {
                _XtShellGetCoordinates(widget, &x, &y);
                _XtCopyToArg((char *) &x, &args->value, sizeof(Position));
            }
            else if (strcmp(XtNy, args->name) == 0) {
                _XtShellGetCoordinates(widget, &x, &y);
                _XtCopyToArg((char *) &y, &args->value, sizeof(Position));
            }
        }
    }
}

static void
ApplicationShellInsertChild(Widget widget)
{
    if (!XtIsWidget(widget) && XtIsRectObj(widget)) {
        XtAppWarningMsg(XtWidgetToApplicationContext(widget),
                        "invalidClass", "applicationShellInsertChild",
                        XtCXtToolkitError,
                        "ApplicationShell does not accept RectObj children; ignored",
                        NULL, NULL);
    }
    else {
        XtWidgetProc insert_child;

        LOCK_PROCESS;
        insert_child =
            ((CompositeWidgetClass) applicationShellClassRec.core_class.
             superclass)->composite_class.insert_child;
        UNLOCK_PROCESS;
        (*insert_child) (widget);
    }
}

/**************************************************************************

  Session Protocol Participation

 *************************************************************************/

#define XtSessionCheckpoint     0
#define XtSessionInteract       1

static void CallSaveCallbacks(SessionShellWidget);
static _XtString *EditCommand(_XtString, _XtString *, _XtString *);
static Boolean ExamineToken(XtPointer);
static void GetIceEvent(XtPointer, int *, XtInputId *);
static XtCheckpointToken GetToken(Widget, int);
static void XtCallCancelCallbacks(SmcConn, SmPointer);
static void XtCallDieCallbacks(SmcConn, SmPointer);
static void XtCallSaveCallbacks(SmcConn, SmPointer, int, Bool, int, Bool);
static void XtCallSaveCompleteCallbacks(SmcConn, SmPointer);

#ifndef XT_NO_SM
static void
StopManagingSession(SessionShellWidget w, SmcConn connection)
{                               /* connection to close, if any */
    if (connection)
        SmcCloseConnection(connection, 0, NULL);

    if (w->session.input_id) {
        XtRemoveInput(w->session.input_id);
        w->session.input_id = 0;
    }
    w->session.connection = NULL;
}

#define XT_MSG_LENGTH 256
static void
JoinSession(SessionShellWidget w)
{
    IceConn ice_conn;
    SmcCallbacks smcb;
    char *sm_client_id;
    unsigned long mask;
    static char context;        /* used to guarantee the connection isn't shared */

    smcb.save_yourself.callback = XtCallSaveCallbacks;
    smcb.die.callback = XtCallDieCallbacks;
    smcb.save_complete.callback = XtCallSaveCompleteCallbacks;
    smcb.shutdown_cancelled.callback = XtCallCancelCallbacks;
    smcb.save_yourself.client_data = smcb.die.client_data =
        smcb.save_complete.client_data =
        smcb.shutdown_cancelled.client_data = (SmPointer) w;
    mask = SmcSaveYourselfProcMask | SmcDieProcMask |
        SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

    if (w->session.connection) {
        SmcModifyCallbacks(w->session.connection, mask, &smcb);
        sm_client_id = SmcClientID(w->session.connection);
    }
    else if (getenv("SESSION_MANAGER")) {
        char error_msg[XT_MSG_LENGTH];

        error_msg[0] = '\0';
        w->session.connection =
            SmcOpenConnection(NULL, &context, SmProtoMajor, SmProtoMinor,
                              mask, &smcb, w->session.session_id,
                              &sm_client_id, XT_MSG_LENGTH, error_msg);
        if (error_msg[0]) {
            String params[1];
            Cardinal num_params = 1;

            params[0] = error_msg;
            XtAppWarningMsg(XtWidgetToApplicationContext((Widget) w),
                            "sessionManagement", "SmcOpenConnection",
                            XtCXtToolkitError,
                            "Tried to connect to session manager, %s",
                            params, &num_params);
        }
    }

    if (w->session.connection) {
        if (w->session.session_id == NULL
            || (strcmp(w->session.session_id, sm_client_id) != 0)) {
            XtFree(w->session.session_id);
            w->session.session_id = XtNewString(sm_client_id);
        }
        free(sm_client_id);
        ice_conn = SmcGetIceConnection(w->session.connection);
        w->session.input_id =
            XtAppAddInput(XtWidgetToApplicationContext((Widget) w),
                          IceConnectionNumber(ice_conn),
                          (XtPointer) XtInputReadMask,
                          GetIceEvent, (XtPointer) w);

        w->session.restart_command =
            EditCommand(w->session.session_id, w->session.restart_command,
                        w->application.argv);

        if (!w->session.clone_command)
            w->session.clone_command =
                EditCommand(NULL, NULL, w->session.restart_command);

        if (!w->session.program_path)
            w->session.program_path = w->session.restart_command
                ? XtNewString(w->session.restart_command[0]) : NULL;
    }
}

#undef XT_MSG_LENGTH

#endif                          /* !XT_NO_SM */

static _XtString *
NewStringArray(_XtString *str)
{
    Cardinal nbytes = 0;
    Cardinal num = 0;
    _XtString *newarray;
    _XtString *new;
    _XtString *strarray = str;
    _XtString sptr;

    if (!str)
        return NULL;

    for (num = 0; *str; num++, str++) {
        nbytes = nbytes + (Cardinal) strlen(*str);
        nbytes++;
    }
    num = (Cardinal) ((size_t) (num + 1) * sizeof(_XtString));
    new = newarray = (_XtString *) __XtMalloc(num + nbytes);
    sptr = ((char *) new) + num;

    for (str = strarray; *str; str++) {
        *new = sptr;
        strcpy(*new, *str);
        new++;
        sptr = strchr(sptr, '\0');
        sptr++;
    }
    *new = NULL;
    return newarray;
}

static void
FreeStringArray(_XtString *str)
{
    if (str)
        XtFree((_XtString) str);
}

#ifndef XT_NO_SM
static SmProp *
CardPack(_Xconst _XtString name, XtPointer closure)
{
    unsigned char *prop = (unsigned char *) closure;
    SmProp *p;

    p = (SmProp *) __XtMalloc(sizeof(SmProp) + sizeof(SmPropValue));
    p->vals = (SmPropValue *) (((char *) p) + sizeof(SmProp));
    p->num_vals = 1;
    p->type = (char *) SmCARD8;
    p->name = (char *) name;
    p->vals->length = 1;
    p->vals->value = (SmPointer) prop;
    return p;
}

static SmProp *
ArrayPack(_Xconst _XtString name, XtPointer closure)
{
    _XtString prop = *(_XtString *) closure;
    SmProp *p;

    p = (SmProp *) __XtMalloc(sizeof(SmProp) + sizeof(SmPropValue));
    p->vals = (SmPropValue *) (((char *) p) + sizeof(SmProp));
    p->num_vals = 1;
    p->type = (char *) SmARRAY8;
    p->name = (char *) name;
    p->vals->length = (int) strlen(prop) + 1;
    p->vals->value = prop;
    return p;
}

static SmProp *
ListPack(_Xconst _XtString name, XtPointer closure)
{
    _XtString *prop = *(_XtString **) closure;
    SmProp *p;
    _XtString *ptr;
    SmPropValue *vals;
    int n = 0;

    for (ptr = prop; *ptr; ptr++)
        n++;
    p = (SmProp *)
        __XtMalloc((Cardinal)
                   (sizeof(SmProp) + (size_t) n * sizeof(SmPropValue)));
    p->vals = (SmPropValue *) (((char *) p) + sizeof(SmProp));
    p->num_vals = n;
    p->type = (char *) SmLISTofARRAY8;
    p->name = (char *) name;
    for (ptr = prop, vals = p->vals; *ptr; ptr++, vals++) {
        vals->length = (int) strlen(*ptr) + 1;
        vals->value = *ptr;
    }
    return p;
}

static void
FreePacks(SmProp ** props, int num_props)
{
    while (--num_props >= 0)
        XtFree((char *) props[num_props]);
}

typedef SmProp *(*PackProc) (_Xconst _XtString, XtPointer);

typedef struct PropertyRec {
    String name;
    int offset;
    PackProc proc;
} PropertyRec, *PropertyTable;

#define Offset(x) (XtOffsetOf(SessionShellRec, x))
/* *INDENT-OFF* */
static PropertyRec propertyTable[] = {
  {SmCloneCommand,     Offset(session.clone_command),    ListPack},
  {SmCurrentDirectory, Offset(session.current_dir),      ArrayPack},
  {SmDiscardCommand,   Offset(session.discard_command),  ListPack},
  {SmEnvironment,      Offset(session.environment),      ListPack},
  {SmProgram,          Offset(session.program_path),     ArrayPack},
  {SmResignCommand,    Offset(session.resign_command),   ListPack},
  {SmRestartCommand,   Offset(session.restart_command),  ListPack},
  {SmRestartStyleHint, Offset(session.restart_style),    CardPack},
  {SmShutdownCommand,  Offset(session.shutdown_command), ListPack}
};
/* *INDENT-ON* */
#undef Offset

#define XT_NUM_SM_PROPS 11

static void
SetSessionProperties(SessionShellWidget w,
                     Boolean initialize,
                     unsigned long set_mask,
                     unsigned long unset_mask)
{
    PropertyTable p = propertyTable;
    int n;
    int num_props = 0;
    XtPointer *addr;
    unsigned long mask;
    SmProp *props[XT_NUM_SM_PROPS];

    if (w->session.connection == NULL)
        return;

    if (initialize) {
        char nam_buf[32];
        char pid[12];
        String user_name;
        String pidp = pid;

        /* set all non-NULL session properties, the UserID and the ProcessID */
        for (n = XtNumber(propertyTable); n; n--, p++) {
            addr = (XtPointer *) ((char *) w + p->offset);
            if (p->proc == CardPack) {
                if (*(unsigned char *) addr)
                    props[num_props++] =
                        (*(p->proc)) (p->name, (XtPointer) addr);
            }
            else if (*addr)
                props[num_props++] = (*(p->proc)) (p->name, (XtPointer) addr);

        }
        user_name = _XtGetUserName(nam_buf, sizeof nam_buf);
        if (user_name)
            props[num_props++] = ArrayPack(SmUserID, &user_name);
        snprintf(pid, sizeof(pid), "%ld", (long) getpid());
        props[num_props++] = ArrayPack(SmProcessID, &pidp);

        if (num_props) {
            SmcSetProperties(w->session.connection, num_props, props);
            FreePacks(props, num_props);
        }
        return;
    }

    if (set_mask) {
        mask = 1L;
        for (n = XtNumber(propertyTable); n; n--, p++, mask <<= 1)
            if (mask & set_mask) {
                addr = (XtPointer *) ((char *) w + p->offset);
                props[num_props++] = (*(p->proc)) (p->name, (XtPointer) addr);
            }
        SmcSetProperties(w->session.connection, num_props, props);
        FreePacks(props, num_props);
    }

    if (unset_mask) {
        char *pnames[XT_NUM_SM_PROPS];

        mask = 1L;
        num_props = 0;
        for (n = XtNumber(propertyTable); n; n--, p++, mask <<= 1)
            if (mask & unset_mask)
                pnames[num_props++] = (char *) p->name;
        SmcDeleteProperties(w->session.connection, num_props, pnames);
    }
}

static void
GetIceEvent(XtPointer client_data,
            int *source _X_UNUSED,
            XtInputId *id _X_UNUSED)
{
    SessionShellWidget w = (SessionShellWidget) client_data;
    IceProcessMessagesStatus status;

    status = IceProcessMessages(SmcGetIceConnection(w->session.connection),
                                NULL, NULL);

    if (status == IceProcessMessagesIOError) {
        StopManagingSession(w, w->session.connection);
        XtCallCallbackList((Widget) w, w->session.error_callbacks,
                           (XtPointer) NULL);
    }
}

static void
CleanUpSave(SessionShellWidget w)
{
    XtSaveYourself next = w->session.save->next;

    XtFree((char *) w->session.save);
    w->session.save = next;
    if (w->session.save)
        CallSaveCallbacks(w);
}

static void
CallSaveCallbacks(SessionShellWidget w)
{
    if (XtHasCallbacks((Widget) w, XtNsaveCallback) != XtCallbackHasSome) {
        /* if the application makes no attempt to save state, report failure */
        SmcSaveYourselfDone(w->session.connection, False);
        CleanUpSave(w);
    }
    else {
        XtCheckpointToken token;

        w->session.checkpoint_state = XtSaveActive;
        token = GetToken((Widget) w, XtSessionCheckpoint);
        _XtCallConditionalCallbackList((Widget) w, w->session.save_callbacks,
                                       (XtPointer) token, ExamineToken);
        XtSessionReturnToken(token);
    }
}

static void
XtCallSaveCallbacks(SmcConn connection _X_UNUSED,
                    SmPointer client_data,
                    int save_type,
                    Bool shutdown,
                    int interact,
                    Bool fast)
{
    SessionShellWidget w = (SessionShellWidget) client_data;
    XtSaveYourself save;
    XtSaveYourself prev;

    save = XtNew(XtSaveYourselfRec);
    save->next = NULL;
    save->save_type = save_type;
    save->interact_style = interact;
    save->shutdown = (Boolean) shutdown;
    save->fast = (Boolean) fast;
    save->cancel_shutdown = False;
    save->phase = 1;
    save->interact_dialog_type = SmDialogNormal;
    save->request_cancel = save->request_next_phase = False;
    save->save_success = True;
    save->save_tokens = save->interact_tokens = 0;

    prev = (XtSaveYourself) &w->session.save;
    while (prev->next)
        prev = prev->next;
    prev->next = save;

    if (w->session.checkpoint_state == XtSaveInactive)
        CallSaveCallbacks(w);
}

static void
XtInteractPermission(SmcConn connection, SmPointer data)
{
    Widget w = (Widget) data;
    SessionShellWidget sw = (SessionShellWidget) data;
    XtCallbackProc callback;
    XtPointer client_data;

    _XtPeekCallback(w, sw->session.interact_callbacks, &callback, &client_data);
    if (callback) {
        XtCheckpointToken token;

        sw->session.checkpoint_state = XtInteractActive;
        token = GetToken(w, XtSessionInteract);
        XtRemoveCallback(w, XtNinteractCallback, callback, client_data);
        (*callback) (w, client_data, (XtPointer) token);
    }
    else if (!sw->session.save->cancel_shutdown) {
        SmcInteractDone(connection, False);
    }
}

static void
XtCallSaveCompleteCallbacks(SmcConn connection _X_UNUSED, SmPointer client_data)
{
    SessionShellWidget w = (SessionShellWidget) client_data;

    XtCallCallbackList((Widget) w, w->session.save_complete_callbacks,
                       (XtPointer) NULL);
}

static void
XtCallNextPhaseCallbacks(SmcConn connection _X_UNUSED, SmPointer client_data)
{
    SessionShellWidget w = (SessionShellWidget) client_data;

    w->session.save->phase = 2;
    CallSaveCallbacks(w);
}

static void
XtCallDieCallbacks(SmcConn connection _X_UNUSED, SmPointer client_data)
{
    SessionShellWidget w = (SessionShellWidget) client_data;

    StopManagingSession(w, w->session.connection);
    XtCallCallbackList((Widget) w, w->session.die_callbacks, (XtPointer) NULL);
}

static void
XtCallCancelCallbacks(SmcConn connection _X_UNUSED, SmPointer client_data)
{
    SessionShellWidget w = (SessionShellWidget) client_data;
    Boolean call_interacts = False;

    if (w->session.checkpoint_state != XtSaveInactive) {
        w->session.save->cancel_shutdown = True;
        call_interacts = (w->session.save->interact_style !=
                          SmInteractStyleNone);
    }

    XtCallCallbackList((Widget) w, w->session.cancel_callbacks,
                       (XtPointer) NULL);

    if (call_interacts) {
        w->session.save->interact_style = SmInteractStyleNone;
        XtInteractPermission(w->session.connection, (SmPointer) w);
    }

    if (w->session.checkpoint_state != XtSaveInactive) {
        if (w->session.save->save_tokens == 0 &&
            w->session.checkpoint_state == XtSaveActive) {
            w->session.checkpoint_state = XtSaveInactive;
            SmcSaveYourselfDone(w->session.connection,
                                w->session.save->save_success);
            CleanUpSave(w);
        }
    }
}

static XtCheckpointToken
GetToken(Widget widget, int type)
{
    SessionShellWidget w = (SessionShellWidget) widget;
    XtCheckpointToken token;
    XtSaveYourself save = w->session.save;

    if (type == XtSessionCheckpoint)
        w->session.save->save_tokens++;
    else if (type == XtSessionInteract)
        w->session.save->interact_tokens++;
    else
        return (XtCheckpointToken) NULL;

    token = (XtCheckpointToken) __XtMalloc(sizeof(XtCheckpointTokenRec));
    token->save_type = save->save_type;
    token->interact_style = save->interact_style;
    token->shutdown = save->shutdown;
    token->fast = save->fast;
    token->cancel_shutdown = save->cancel_shutdown;
    token->phase = save->phase;
    token->interact_dialog_type = save->interact_dialog_type;
    token->request_cancel = save->request_cancel;
    token->request_next_phase = save->request_next_phase;
    token->save_success = save->save_success;
    token->type = type;
    token->widget = widget;
    return token;
}

XtCheckpointToken
XtSessionGetToken(Widget widget)
{
    SessionShellWidget w = (SessionShellWidget) widget;
    XtCheckpointToken token = NULL;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (w->session.checkpoint_state)
        token = GetToken(widget, XtSessionCheckpoint);

    UNLOCK_APP(app);
    return token;
}

static Boolean
ExamineToken(XtPointer call_data)
{
    XtCheckpointToken token = (XtCheckpointToken) call_data;
    SessionShellWidget w = (SessionShellWidget) token->widget;

    if (token->interact_dialog_type == SmDialogError)
        w->session.save->interact_dialog_type = SmDialogError;
    if (token->request_next_phase)
        w->session.save->request_next_phase = True;
    if (!token->save_success)
        w->session.save->save_success = False;

    token->interact_dialog_type = w->session.save->interact_dialog_type;
    token->request_next_phase = w->session.save->request_next_phase;
    token->save_success = w->session.save->save_success;
    token->cancel_shutdown = w->session.save->cancel_shutdown;

    return True;
}

void
XtSessionReturnToken(XtCheckpointToken token)
{
    SessionShellWidget w = (SessionShellWidget) token->widget;
    Boolean has_some;
    Boolean phase_done;
    XtCallbackProc callback;
    XtPointer client_data;

    WIDGET_TO_APPCON((Widget) w);

    LOCK_APP(app);

    has_some = (XtHasCallbacks(token->widget, XtNinteractCallback)
                == XtCallbackHasSome);

    (void) ExamineToken((XtPointer) token);

    if (token->type == XtSessionCheckpoint) {
        w->session.save->save_tokens--;
        if (has_some && w->session.checkpoint_state == XtSaveActive) {
            w->session.checkpoint_state = XtInteractPending;
            SmcInteractRequest(w->session.connection,
                               w->session.save->interact_dialog_type,
                               XtInteractPermission, (SmPointer) w);
        }
        XtFree((char *) token);
    }
    else {
        if (token->request_cancel)
            w->session.save->request_cancel = True;
        token->request_cancel = w->session.save->request_cancel;
        if (has_some) {
            _XtPeekCallback((Widget) w, w->session.interact_callbacks,
                            &callback, &client_data);
            XtRemoveCallback((Widget) w, XtNinteractCallback,
                             callback, client_data);
            (*callback) ((Widget) w, client_data, (XtPointer) token);
        }
        else {
            w->session.save->interact_tokens--;
            if (w->session.save->interact_tokens == 0) {
                w->session.checkpoint_state = XtSaveActive;
                if (!w->session.save->cancel_shutdown)
                    SmcInteractDone(w->session.connection,
                                    w->session.save->request_cancel);
            }
            XtFree((char *) token);
        }
    }

    phase_done = (w->session.save->save_tokens == 0 &&
                  w->session.checkpoint_state == XtSaveActive);

    if (phase_done) {
        if (w->session.save->request_next_phase && w->session.save->phase == 1) {
            SmcRequestSaveYourselfPhase2(w->session.connection,
                                         XtCallNextPhaseCallbacks,
                                         (SmPointer) w);
        }
        else {
            w->session.checkpoint_state = XtSaveInactive;
            SmcSaveYourselfDone(w->session.connection,
                                w->session.save->save_success);
            CleanUpSave(w);
        }
    }

    UNLOCK_APP(app);
}

static Boolean
IsInArray(String str, _XtString *sarray)
{
    if (str == NULL || sarray == NULL)
        return False;
    for (; *sarray; sarray++) {
        if (strcmp(*sarray, str) == 0)
            return True;
    }
    return False;
}

static _XtString *
EditCommand(_XtString str,      /* if not NULL, the sm_client_id */
            _XtString *src1,   /* first choice */
            _XtString *src2)   /* alternate */
{
    Boolean have;
    Boolean want;
    int count;
    _XtString *sarray;
    _XtString *s;
    _XtString *new;

    want = (str != NULL);
    sarray = (src1 ? src1 : src2);
    if (!sarray)
        return NULL;
    have = IsInArray("-xtsessionID", sarray);
    if ((want && have) || (!want && !have)) {
        if (sarray == src1)
            return src1;
        else
            return NewStringArray(sarray);
    }

    count = 0;
    for (s = sarray; *s; s++)
        count++;

    if (want) {
        s = new = XtMallocArray((Cardinal) count + 3,
                                (Cardinal) sizeof(_XtString *));
        *s = *sarray;
        s++;
        sarray++;
        *s = (_XtString) "-xtsessionID";
        s++;
        *s = str;
        s++;
        for (; --count > 0; s++, sarray++)
            *s = *sarray;
        *s = NULL;
    }
    else {
        if (count < 3)
            return NewStringArray(sarray);
        s = new = XtMallocArray((Cardinal) count - 1,
                                (Cardinal) sizeof(_XtString *));
        for (; --count >= 0; sarray++) {
            if (strcmp(*sarray, "-xtsessionID") == 0) {
                sarray++;
                count--;
            }
            else {
                *s = *sarray;
                s++;
            }
        }
        *s = NULL;
    }
    s = new;
    new = NewStringArray(new);
    XtFree((char *) s);
    return new;
}

#endif                          /* !XT_NO_SM */
