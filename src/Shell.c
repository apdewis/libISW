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

#include <X11/cursorfont.h>
#include <ISW/IswDragDrop.h>
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
    { IswNvisual, IswCVisual, IswRVisual, sizeof(IswVisualId),
        Offset(shell.visual), IswRImmediate, (IswPointer)CopyFromParent}
};
/* *INDENT-ON* */

static void ClassPartInitialize(WidgetClass);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(IswDisplay, Widget, Mask *, uint32_t *); //IswSetWindowAttributes *);
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
        IswRImmediate, (IswPointer)IswWmStateNormalState},
    { IswNiconPixmap, IswCIconPixmap, IswRBitmap, sizeof(IswPixmap),
        Offset(wm.wm_hints.icon_pixmap), IswRPixmap, NULL},
    { IswNiconWindow, IswCIconWindow, IswRWindow, sizeof(IswWindow),
        Offset(wm.wm_hints.icon_window), IswRWindow,   (IswPointer) NULL},
    { IswNiconX, IswCIconX, IswRInt, sizeof(int),
        Offset(wm.wm_hints.icon_x),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNiconY, IswCIconY, IswRInt, sizeof(int),
        Offset(wm.wm_hints.icon_y),
        IswRInt, (IswPointer) &default_unspecified_shell_int},
    { IswNiconMask, IswCIconMask, IswRBitmap, sizeof(IswPixmap),
        Offset(wm.wm_hints.icon_mask), IswRPixmap, NULL},
    { IswNwindowGroup, IswCWindowGroup, IswRWindow, sizeof(IswWindow),
        Offset(wm.wm_hints.window_group),
        IswRImmediate, (IswPointer)IswUnspecifiedWindow},
    { IswNclientLeader, IswCClientLeader, IswRWidget, sizeof(Widget),
        Offset(wm.client_leader), IswRWidget, NULL},
    { IswNwindowRole, IswCWindowRole, IswRString, sizeof(String),
        Offset(wm.window_role), IswRString, (IswPointer) NULL},
    { IswNurgency, IswCUrgency, IswRBoolean, sizeof(Boolean),
        Offset(wm.urgency), IswRImmediate, (IswPointer) False},
    { IswNwindowType, IswCWindowType, IswRWindowType, sizeof(IswWindowType),
        Offset(wm.window_type), IswRImmediate, (IswPointer) ISW_WINDOW_TYPE_NORMAL},
    { IswNstrutLeft, IswCStrutLeft, IswRInt, sizeof(int),
        Offset(wm.strut_partial.left), IswRImmediate, (IswPointer) 0},
    { IswNstrutRight, IswCStrutRight, IswRInt, sizeof(int),
        Offset(wm.strut_partial.right), IswRImmediate, (IswPointer) 0},
    { IswNstrutTop, IswCStrutTop, IswRInt, sizeof(int),
        Offset(wm.strut_partial.top), IswRImmediate, (IswPointer) 0},
    { IswNstrutBottom, IswCStrutBottom, IswRInt, sizeof(int),
        Offset(wm.strut_partial.bottom), IswRImmediate, (IswPointer) 0}
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
TransientRealize(IswDisplay, Widget, Mask *, uint32_t *);
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
                     IswSizeHints *size_hints,
                     IswWmHints *wm_hints,
                     char *classhint_class, char *classhint_name) {
    
    // Set WM_NAME / _NET_WM_NAME (semantic title hint)
    if (window_name != NULL) {
        _IswPlatformSetWindowTitle(IswDisplayOf(w), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)), window_name);
    }

    // Set WM_ICON_NAME / _NET_WM_ICON_NAME (semantic icon-title hint)
    if (IswIsTopLevelShell((Widget) w) && icon_name != NULL) {
        _IswPlatformSetIconTitle(IswDisplayOf(w), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)), icon_name);
    }

    if (argc > 0 && argv != NULL) {
        _IswPlatformSetWmCommand(IswDisplayOf(w),
                                 _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
                                 (const char *const *) argv, argc);
    }

    // Set WM_CLASS (semantic class hint)
    if (classhint_name != NULL && classhint_class != NULL) {
        _IswPlatformSetWmClass(IswDisplayOf(w), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
                               classhint_name, classhint_class);
    }
}

/****************************************************************************
 * Whew!
 ****************************************************************************/

static void
ComputeWMSizeHints(WMShellWidget w, IswSizeHints *hints)
{
    register long flags;
    hints->flags = flags = w->wm.size_hints.flags;
    
#define copy(field) hints->field = w->wm.size_hints.field
    if (flags & (IswSizeHintUSPosition | IswSizeHintPPosition)) {
        copy(x);
        copy(y);
    }
    if (flags & (IswSizeHintUSSize | IswSizeHintPSize)) {
        copy(width);
        copy(height);
    }
    if (flags & IswSizeHintPMinSize) {
        copy(min_width);
        copy(min_height);
    }
    if (flags & IswSizeHintPMaxSize) {
        copy(max_width);
        copy(max_height);
    }
    if (flags & IswSizeHintPResizeInc) {
        copy(width_inc);
        copy(height_inc);
    }
    if (flags & IswSizeHintPAspect) {
        hints->min_aspect_num = w->wm.size_hints.min_aspect.x;
        hints->min_aspect_den = w->wm.size_hints.min_aspect.y;
        hints->max_aspect_num = w->wm.size_hints.max_aspect.x;
        hints->max_aspect_den = w->wm.size_hints.max_aspect.y;
    }
#undef copy
#define copy(field) hints->field = w->wm.field
    if (flags & IswSizeHintBaseSize) {
        copy(base_width);
        copy(base_height);
    }
    if (flags & IswSizeHintPWinGravity)
        copy(win_gravity);
#undef copy
}

static void
_SetWMSizeHints(WMShellWidget w)
{
    IswSizeHints *size_hints;
    size_hints = calloc(1, sizeof(IswSizeHints));
    if (size_hints == NULL)
        _IswAllocError("IswSizeHints");

    ComputeWMSizeHints(w, size_hints);
    _IswPlatformSetNormalHints(IswDisplayOf((Widget) w), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
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

    IswAddEventHandler(new, (EventMask) IswStructureNotifyMask,
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
        w->wm.wm_hints.initial_state = IswWmStateIconicState;
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
    (void) params; (void) num_params;

    if (iswev->kind != IswWindowClose)
	return;

    if (IswIsApplicationShell(w))
	IswAppSetExitFlag(IswWidgetToApplicationContext(w));
    else
	IswDestroyWidget(w);
}

static void
SetShellWMProtocolTranslations(Widget w)
{
    static IswTranslations compiled_table;	/* initially 0 */
    static IswAppContext *app_context_list;	/* initially 0 */
    static Cardinal list_size;			/* initially 0 */
    IswAppContext app_context;
    int i;

    app_context = IswWidgetToApplicationContext(w);

    /* parse translation table once */
    if (!compiled_table)
	compiled_table = IswParseTranslationTable(
	    "<WindowClose>: IswShellDeleteWindow()\n");

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

    {
        const char *protocols[] = { "WM_DELETE_WINDOW" };
        _IswPlatformSetWmProtocols(IswDisplayOf(w),
            _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
            protocols, 1);
    }
}

static void
Realize(IswDisplay dpy, Widget wid, Mask *vmask, uint32_t *attr)
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

    if (w->shell.save_under)
        mask |= IswCWSaveUnder;
    if (w->shell.override_redirect)
        mask |= IswCWOverrideRedirect;

    if (wid->core.width == 0 || wid->core.height == 0) {
        Cardinal count = 1;

        IswErrorMsg("invalidDimension", "shellRealize", IswCIswToolkitError,
                   "Shell widget %s has zero width and/or height",
                   &wid->core.name, &count);
    }

    /* Create the WM-managed top-level window through the platform root op —
       the shell no longer calls xcb_create_window directly.  The root surface
       (window + presentation) is owned by the platform layer; the shell holds
       the returned opaque IswWindow in core.window and never dereferences it.*/
    {
        double sf = _IswGetScaleFactor(IswDisplayOf(wid));
        IswWindowGeometry geom;
        IswWindowAttributes attrs;

        geom.x = (int32_t)(wid->core.x * sf + 0.5);
        geom.y = (int32_t)(wid->core.y * sf + 0.5);
        geom.width = (uint32_t)(wid->core.width * sf + 0.5);
        geom.height = (uint32_t)(wid->core.height * sf + 0.5);
        geom.border_width = (uint32_t)(wid->core.border_width * sf + 0.5);

        memset(&attrs, 0, sizeof(attrs));
        attrs.background_pixel = wid->core.background_pixel;
        attrs.border_pixel = wid->core.border_pixel;
        attrs.event_mask = _IswWindowSelectMask(wid);
        attrs.override_redirect = w->shell.override_redirect;
        attrs.save_under = w->shell.save_under;
        attrs.bit_gravity_nw = (mask & IswCWBitGravity) ? True : False;
        attrs.colormap = wid->core.colormap;
        attrs.depth = wid->core.depth;
        attrs.visual = w->shell.visual;

        /* Create the platform-owned top-level window and register the
           widget→window association.  The toolkit holds no window handle; the
           platform tracks it, and event routing resolves it back via
           _IswPlatformWidgetWindow.  Also register the window in the
           window→widget dispatch table so server events on it reach the shell. */
        {
            IswWindow win = _IswPlatformCreateRoot(IswDisplayOf(wid),
                                                   wid->core.screen,
                                                   &geom, &attrs);
            _IswPlatformSetWidgetWindow(IswDisplayOf(wid), wid, win);
            IswRegisterDrawable(IswDisplayOf(wid), win, wid);
        }

        /* Set a themed default cursor on the shell window so child
           widgets that don't set their own cursor inherit the theme's
           left_ptr instead of the X server's default glyph cursor. */
        {
            IswCursor cursor = _IswLoadThemedCursor(
                IswDisplayOf(wid), wid->core.screen, "left_ptr", XC_left_ptr);
            if (cursor != IswCursorNone)
                _IswSetWindowCursor(wid, cursor);
        }
    }
    _IswPlatformFlush(IswDisplayOf(wid));

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
    IswWindow window_group;

    if (w->wm.transient) {
        if (w->transient.transient_for != NULL
            && IswIsRealized(w->transient.transient_for))
            window_group = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w->transient.transient_for)), (Widget)(w->transient.transient_for));
        else if ((window_group = w->wm.wm_hints.window_group)
                 == IswUnspecifiedWindowGroup) {
            if (delete) {
                _IswPlatformDeleteTransientFor(IswDisplayOf((Widget) w),
                    _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)));
            }
            return;
        }

        _IswPlatformSetTransientFor(IswDisplayOf((Widget) w),
                                    _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
                                    window_group);
    }
}

static void
TransientRealize(IswDisplay dpy, Widget w, Mask *vmask, uint32_t *attr)
{
    IswRealizeProc realize;

    LOCK_PROCESS;
    realize =
        transientShellWidgetClass->core_class.superclass->core_class.realize;
    UNLOCK_PROCESS;
    (*realize) (IswDisplayOf(w), w, vmask, attr);

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
    IswWmHints *hintp = &w->wm.wm_hints;

    hintp->flags = IswWmHintState | IswWmHintInput;

    if (hintp->icon_x == IswUnspecifiedShellInt)
        hintp->icon_x = -1;
    else
        hintp->flags |= IswWmHintIconPosition;

    if (hintp->icon_y == IswUnspecifiedShellInt)
        hintp->icon_y = -1;
    else
        hintp->flags |= IswWmHintIconPosition;

    if (hintp->icon_pixmap != None)
        hintp->flags |= IswWmHintIconPixmap;
    if (hintp->icon_mask != None)
        hintp->flags |= IswWmHintIconMask;
    if (hintp->icon_window != None)
        hintp->flags |= IswWmHintIconWindow;

    if (hintp->window_group == IswUnspecifiedWindow) {
        if (w->core.parent) {
            Widget p;

            for (p = w->core.parent; p->core.parent; p = p->core.parent);
            if (IswIsRealized(p)) {
                hintp->window_group = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(p)), (Widget)(p));
                hintp->flags |= IswWmHintWindowGroup;
            }
        }
    }
    else if (hintp->window_group != IswUnspecifiedWindowGroup)
        hintp->flags |= IswWmHintWindowGroup;

    if (w->wm.urgency)
        hintp->flags |= IswWmHintUrgency;
}

static void
EvaluateSizeHints(WMShellWidget w)
{
    struct _OldXSizeHints *sizep = &w->wm.size_hints;

    sizep->x = w->core.x;
    sizep->y = w->core.y;
    sizep->width = w->core.width;
    sizep->height = w->core.height;

    if (sizep->flags & IswSizeHintUSSize) {
        if (sizep->flags & IswSizeHintPSize)
            sizep->flags &= ~IswSizeHintPSize;
    }
    else
        sizep->flags |= IswSizeHintPSize;

    if (sizep->flags & IswSizeHintUSPosition) {
        if (sizep->flags & IswSizeHintPPosition)
            sizep->flags &= ~IswSizeHintPPosition;
    }
    else if (w->shell.client_specified & _IswShellPPositionOK)
        sizep->flags |= IswSizeHintPPosition;

    if (sizep->min_aspect.x != IswUnspecifiedShellInt
        || sizep->min_aspect.y != IswUnspecifiedShellInt
        || sizep->max_aspect.x != IswUnspecifiedShellInt
        || sizep->max_aspect.y != IswUnspecifiedShellInt) {
        sizep->flags |= IswSizeHintPAspect;
    }
    if (sizep->flags & IswSizeHintBaseSize
        || w->wm.base_width != IswUnspecifiedShellInt
        || w->wm.base_height != IswUnspecifiedShellInt) {
        sizep->flags |= IswSizeHintBaseSize;
        if (w->wm.base_width == IswUnspecifiedShellInt)
            w->wm.base_width = 0;
        if (w->wm.base_height == IswUnspecifiedShellInt)
            w->wm.base_height = 0;
    }
    if (sizep->flags & IswSizeHintPResizeInc
        || sizep->width_inc != IswUnspecifiedShellInt
        || sizep->height_inc != IswUnspecifiedShellInt) {
        if (sizep->width_inc < 1)
            sizep->width_inc = 1;
        if (sizep->height_inc < 1)
            sizep->height_inc = 1;
        sizep->flags |= IswSizeHintPResizeInc;
    }
    if (sizep->flags & IswSizeHintPMaxSize
        || sizep->max_width != IswUnspecifiedShellInt
        || sizep->max_height != IswUnspecifiedShellInt) {
        sizep->flags |= IswSizeHintPMaxSize;
        if (sizep->max_width == IswUnspecifiedShellInt)
            sizep->max_width = BIGSIZE;
        if (sizep->max_height == IswUnspecifiedShellInt)
            sizep->max_height = BIGSIZE;
    }
    if (sizep->flags & IswSizeHintPMinSize
        || sizep->min_width != IswUnspecifiedShellInt
        || sizep->min_height != IswUnspecifiedShellInt) {
        sizep->flags |= IswSizeHintPMinSize;
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
    IswSizeHints *size_hints;
    IswWindow window_group;

    if (!IswIsWMShell((Widget) w) || w->shell.override_redirect)
        return;

    size_hints = calloc(1, sizeof(IswSizeHints));
    if (size_hints == NULL)
        _IswAllocError("IswSizeHints");

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

        _IswPlatformSetTransientFor(IswDisplayOf((Widget) w), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
                                    window_group);
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
            _IswPlatformSetLocaleName(IswDisplayOf((Widget) w),
                _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
                locale);
        }
    }
    UNLOCK_PROCESS;

    p = GetClientLeader((Widget) w);

        IswWindow leader = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(p)), (Widget)(p));
        if (leader) {
            _IswPlatformSetClientLeader(IswDisplayOf((Widget) w),
                _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
                leader);
        }

    if (wmshell->wm.window_role) {
        _IswPlatformSetWindowRole(IswDisplayOf((Widget) w),
            _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
            wmshell->wm.window_role);
    }

    IswDisplay wdpy = IswDisplayOf((Widget) w);
    IswWindow win = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w));

    /* _NET_WM_PID */
    _IswPlatformSetPid(IswDisplayOf((Widget) w), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
                        (uint32_t) getpid());

    /* _NET_WM_WINDOW_TYPE */
    IswWindowType wt = ((WMShellWidget) w)->wm.window_type;
    if (wt == ISW_WINDOW_TYPE_NORMAL && IswIsTransientShell((Widget) w))
        wt = ISW_WINDOW_TYPE_DIALOG;
    _IswPlatformSetWindowType(wdpy, win, wt);

    /* _NET_WM_STRUT_PARTIAL */
    const IswStrutPartial *sp = &((WMShellWidget) w)->wm.strut_partial;
    if (sp->left || sp->right || sp->top || sp->bottom)
        _IswPlatformSetStrutPartial(wdpy, win, sp);

    /* _NET_WM_USER_TIME_WINDOW */
    if (IswIsWMShell((Widget) w)) {
        IswPerDisplay pd = _IswGetPerDisplay(IswDisplayOf((Widget) w));
        IswWindow utwin = _IswPlatformCreateInputOnly(wdpy, win);
        wmshell->wm.user_time_win = utwin;
        _IswPlatformSetUserTime(wdpy, win, utwin, pd->last_timestamp);
    }

    if (wmshell->wm.startup_id) {
        IswWindow root = _IswDefaultRootWindow(wdpy);
        _IswPlatformSetStartupId(wdpy, win, root, wmshell->wm.startup_id);
        IswFree(wmshell->wm.startup_id);
        wmshell->wm.startup_id = NULL;
    }
}

static void
EventHandler(Widget wid,
             IswPointer closure _X_UNUSED,
             IswEvent *iswev,
             Boolean *continue_to_dispatch _X_UNUSED)
{
    register ShellWidget w = (ShellWidget) wid;
    WMShellWidget wmshell = (WMShellWidget) w;
    Boolean sizechanged = FALSE;

    switch (iswev->kind) {
    case IswGeometry: {
        /* The event is routed to this shell; its target is this widget.  Guard
           against a configure for some other widget (substructure). */
        if (iswev->any.target != (IswEventTarget) (void *) wid)
            return;
        /* geometry.* are logical pixels (already descaled by the dispatcher). */
        if (w->core.width != iswev->geometry.width ||
            w->core.height != iswev->geometry.height ||
            w->core.border_width != iswev->geometry.border_width) {
            sizechanged = TRUE;
            w->core.width = (Dimension) iswev->geometry.width;
            w->core.height = (Dimension) iswev->geometry.height;
            w->core.border_width = (Dimension) iswev->geometry.border_width;
        }
        if (w->shell.client_specified & _IswShellNotReparented) {
            w->core.x = (Position) iswev->geometry.x;
            w->core.y = (Position) iswev->geometry.y;
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

    case IswReparent:
        if (iswev->any.target == (IswEventTarget) (void *) wid) {
            if (!iswev->reparent.to_root)
                w->shell.client_specified &=
                    ~(_IswShellNotReparented | _IswShellPositionValid);
            else {
                w->core.x = (Position) iswev->reparent.x;
                w->core.y = (Position) iswev->reparent.y;
                w->shell.client_specified |=
                    (_IswShellNotReparented | _IswShellPositionValid);
            }
        }
        return;

    case IswMap:
        if (IswIsTopLevelShell(wid)) {
            ((TopLevelShellWidget) wid)->topLevel.iconic = FALSE;
        }
        return;

    case IswUnmap:
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
        _IswPlatformDestroyWindow(IswDisplayOf(wid), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(wid)), (Widget)(wid)));
}

static void
WMDestroy(Widget wid)
{
    WMShellWidget w = (WMShellWidget) wid;

    if (w->wm.user_time_win) {
        _IswPlatformDestroyWindow(IswDisplayOf(wid), w->wm.user_time_win);
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
_IswWMGeometry(IswScreen screen _X_UNUSED,
              const char *user_geom,
              const char *def_geom,
              unsigned int border_width _X_UNUSED,
              IswSizeHints *hints,
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
    if (hints != NULL && (hints->flags & IswSizeHintPResizeInc)) {
        width  *= hints->width_inc;
        height *= hints->height_inc;
    }
    if (hints != NULL && (hints->flags & IswSizeHintBaseSize)) {
        width  += hints->base_width;
        height += hints->base_height;
    } else if (hints != NULL && (hints->flags & IswSizeHintPMinSize)) {
        width  += hints->min_width;
        height += hints->min_height;
    }

    *x_return = x;
    *y_return = y;
    *width_return  = (int) width;
    *height_return = (int) height;
    *gravity_return = (hints != NULL && (hints->flags & IswSizeHintPWinGravity))
                      ? hints->win_gravity : IswGravityNorthWest;

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
    IswSizeHints hints;

    if (child != NULL) {
        /* we default to our child's size */
        if (is_wmshell && (w->core.width == 0 || w->core.height == 0))
            ((WMShellWidget) W)->wm.size_hints.flags |= IswSizeHintPSize;
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
            if (wm->size_hints.flags & IswSizeHintBaseSize) {
                width -= wm->base_width;
                height -= wm->base_height;
                hints.base_width = wm->base_width;
                hints.base_height = wm->base_height;
            }
            else if (wm->size_hints.flags & IswSizeHintPMinSize) {
                width -= wm->size_hints.min_width;
                height -= wm->size_hints.min_height;
            }
            if (wm->size_hints.flags & IswSizeHintPResizeInc) {
                width /= wm->size_hints.width_inc;
                height /= wm->size_hints.height_inc;
            }
        }
        else
            hints.flags = 0;

        snprintf(def_geom, sizeof(def_geom), "%dx%d+%d+%d",
                 width, height, x, y);
        flag = _IswWMGeometry(IswScreenOf(W),
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
                wmshell->wm.win_gravity = IswGravityNorthWest;
        }
        wmshell->wm.size_hints.flags |= IswSizeHintPWinGravity;
        if ((flag & (XValue | YValue)) == (XValue | YValue))
            wmshell->wm.size_hints.flags |= IswSizeHintUSPosition;
        if ((flag & (WidthValue | HeightValue)) == (WidthValue | HeightValue))
            wmshell->wm.size_hints.flags |= IswSizeHintUSSize;
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

    if (request->request_mode & (IswCWX | IswCWY))
        return (IswGeometryNo);

    my_request.request_mode = (request->request_mode & IswCWQueryOnly);
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
    if (IswMakeGeometryRequest((Widget) shell, &my_request, NULL)
        == IswGeometryYes) {
        /* assert: if (request->request_mode & IswCWWidth) then
         *            shell->core.width == request->width
         * assert: if (request->request_mode & IswCWHeight) then
         *            shell->core.height == request->height
         *
         * so, whatever the WM sized us to (if the Shell requested
         * only one of the two) is now the correct child size
         */

        if (!(request->request_mode & IswCWQueryOnly)) {
            wid->core.width = shell->core.width;
            wid->core.height = shell->core.height;
            if (request->request_mode & IswCWBorderWidth) {
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


static IswGeometryResult
RootGeometryManager(Widget gw,
                    IswWidgetGeometry *request,
                    IswWidgetGeometry *reply _X_UNUSED)
{
    register ShellWidget w = (ShellWidget) gw;
    IswWindowGeometry values;
    IswStackMode stack = ISW_STACK_NONE;
    IswWindow sibling = NULL;
    unsigned int mask = request->request_mode;
    int cfg_x = 0, cfg_y = 0, cfg_w = 0, cfg_h = 0, cfg_border = 0;
    Boolean reparented = (w->shell.client_specified & _IswShellNotReparented) == 0;
    Boolean wm;
    register struct _OldXSizeHints *hintp = NULL;
    int oldx, oldy, oldwidth, oldheight, oldborder_width;

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
    if (mask & IswCWX) {
        if (w->core.x == request->x)
            mask &= (unsigned int) (~IswCWX);
        else {
            w->core.x = (Position) (values.x = request->x);
            if (wm) {
                hintp->flags &= ~IswSizeHintUSPosition;
                hintp->flags |= IswSizeHintPPosition;
                hintp->x = values.x;
            }
        }
    }
    if (mask & IswCWY) {
        if (w->core.y == request->y)
            mask &= (unsigned int) (~IswCWY);
        else {
            w->core.y = (Position) (values.y = request->y);
            if (wm) {
                hintp->flags &= ~IswSizeHintUSPosition;
                hintp->flags |= IswSizeHintPPosition;
                hintp->y = values.y;
            }
        }
    }
    if (mask & IswCWBorderWidth) {
        if (w->core.border_width == request->border_width) {
            mask &= (unsigned int) (~IswCWBorderWidth);
        }
        else
            w->core.border_width =
                (Dimension) (values.border_width = request->border_width);
    }
    if (mask & IswCWWidth) {
        if (w->core.width == request->width)
            mask &= (unsigned int) (~IswCWWidth);
        else {
            w->core.width = (Dimension) (values.width = request->width);
            if (wm) {
                hintp->flags &= ~IswSizeHintUSSize;
                hintp->flags |= IswSizeHintPSize;
                hintp->width = values.width;
            }
        }
    }
    if (mask & IswCWHeight) {
        if (w->core.height == request->height)
            mask &= (unsigned int) (~IswCWHeight);
        else {
            w->core.height = (Dimension) (values.height = request->height);
            if (wm) {
                hintp->flags &= ~IswSizeHintUSSize;
                hintp->flags |= IswSizeHintPSize;
                hintp->height = values.height;
            }
        }
    }
    if (mask & IswCWStackMode) {
        stack = (request->stack_mode == IswSMAbove) ? ISW_STACK_ABOVE
              : (request->stack_mode == IswSMBelow) ? ISW_STACK_BELOW
              : ISW_STACK_NONE;
        if (mask & IswCWSibling)
            sibling = _IswPlatformWidgetWindow(IswDisplayOf((Widget)request->sibling), (Widget)request->sibling);
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
    if (mask & IswCWX) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "x = %d\n", values.x));
    }
    if (mask & IswCWY) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "y = %d\n", values.y));
    }
    if (mask & IswCWWidth) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "width = %d\n", values.width));
    }
    if (mask & IswCWHeight) {
        CALLGEOTAT(_IswGeoTrace((Widget) w, "height = %d\n", values.height));
    }
    if (mask & IswCWBorderWidth) {
        CALLGEOTAT(_IswGeoTrace((Widget) w,
                               "border_width = %d\n", values.border_width));
    }
#endif
    CALLGEOTAT(_IswGeoTab(-1));
    /* HiDPI: scale logical values to physical for the present target */
    {
        double sf = _IswGetScaleFactor(IswDisplayOf((Widget)w));
        if (sf > 1.0) {
            if (mask & IswCWX)
                values.x = (int32_t)(values.x * sf + 0.5);
            if (mask & IswCWY)
                values.y = (int32_t)(values.y * sf + 0.5);
            if (mask & IswCWWidth)
                values.width = (uint32_t)(values.width * sf + 0.5);
            if (mask & IswCWHeight)
                values.height = (uint32_t)(values.height * sf + 0.5);
            if (mask & IswCWBorderWidth)
                values.border_width = (uint32_t)(values.border_width * sf + 0.5);
        }
    }
    {
        unsigned int cmask = 0;
        if (mask & IswCWX)           cmask |= ISW_CONFIG_X;
        if (mask & IswCWY)           cmask |= ISW_CONFIG_Y;
        if (mask & IswCWWidth)       cmask |= ISW_CONFIG_WIDTH;
        if (mask & IswCWHeight)      cmask |= ISW_CONFIG_HEIGHT;
        if (mask & IswCWBorderWidth) cmask |= ISW_CONFIG_BORDER;
        if (mask & IswCWStackMode)   cmask |= ISW_CONFIG_STACK;
        _IswPlatformConfigureWindow(IswDisplayOf((Widget) w),
                                    _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)), &values,
                                    cmask, stack, sibling);
    }

    if (wm && !w->shell.override_redirect
        && mask & (IswCWX | IswCWY | IswCWWidth | IswCWHeight | IswCWBorderWidth)) {
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

    if (!(mask & (unsigned) (~(IswCWStackMode | IswCWSibling))))
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
    {
        unsigned long wm_timeout = IswIsWMShell((Widget) w)
            ? (unsigned long) ((WMShellWidget) w)->wm.wm_timeout
            : (unsigned long) DEFAULT_WM_TIMEOUT;
        Boolean got = _IswPlatformWaitForConfigure(
            IswDisplayOf((Widget) w),
            _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
            wm_timeout, &cfg_x, &cfg_y, &cfg_w, &cfg_h, &cfg_border, &reparented);

        /* Track reparent state observed during the wait. */
        if (reparented)
            w->shell.client_specified &= ~_IswShellNotReparented;
        else
            w->shell.client_specified |= _IswShellNotReparented;

    if (got) {
        /* The WM acknowledged with a ConfigureNotify.  Adopt the ACTUAL window
         * geometry it reports (which may differ from the request due to WM
         * rounding, size increments, or decorations) and report success so the
         * shell's child layout runs against the real window size — this is what
         * makes the top-level viewport fill/shrink to the resized window.
         *
         * HiDPI: ConfigureNotify values are physical pixels; convert back to
         * logical for widget internals. */
        double inv = 1.0 / _IswGetScaleFactor(IswDisplayOf((Widget)w));
        w->core.width = (Dimension)(cfg_w * inv + 0.5);
        w->core.height = (Dimension)(cfg_h * inv + 0.5);
        w->core.border_width = (Dimension)(cfg_border * inv + 0.5);
        if (w->shell.client_specified & _IswShellNotReparented) {
            w->core.x = (Position)(cfg_x * inv + 0.5);
            w->core.y = (Position)(cfg_y * inv + 0.5);
            w->shell.client_specified |= _IswShellPositionValid;
        }
        else
            w->shell.client_specified &= ~_IswShellPositionValid;
        CALLGEOTAT(_IswGeoTrace((Widget) w,
                               "ConfigureNotify succeed, return IswGeometryYes.\n"));
        CALLGEOTAT(_IswGeoTab(-1));
        return IswGeometryYes;
    }
    else if (wm) {              /* no response */
        ((WMShellWidget) w)->wm.wait_for_wm = FALSE;    /* timed out; must be broken */
    }
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
    unsigned int mask = 0;
    IswWindowAttributes attr;

    if (!IswIsRealized(new))
        return False;

    memset(&attr, 0, sizeof(attr));

    if (ow->shell.save_under != nw->shell.save_under) {
        mask = ISW_ATTR_SAVE_UNDER;
        attr.save_under = nw->shell.save_under;
    }

    if (ow->shell.override_redirect != nw->shell.override_redirect) {
        mask |= ISW_ATTR_OVERRIDE;
        attr.override_redirect = nw->shell.override_redirect;
    }

    if (mask) {
        _IswPlatformChangeAttributes(IswDisplayOf(new),
                                     _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)), &attr, mask);
        if ((mask & ISW_ATTR_OVERRIDE) && !nw->shell.override_redirect)
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

    if (set_prop && title_changed) {

        _IswPlatformSetWindowTitle(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)),
                                   (const char *) nwmshell->wm.title);
    }

    EvaluateWMHints(nwmshell);

#define NEQ(f)  (nwmshell->wm.wm_hints.f != owmshell->wm.wm_hints.f)

    if (set_prop && (NEQ(flags) || NEQ(input) || NEQ(initial_state)
                     || NEQ(icon_x) || NEQ(icon_y)
                     || NEQ(icon_pixmap) || NEQ(icon_mask) || NEQ(icon_window)
                     || NEQ(window_group))) {

        _IswPlatformSetWmHints(IswDisplayOf(new),
                               _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)),
                               &nwmshell->wm.wm_hints);
    }
#undef NEQ

    if (IswIsRealized(new) && nwmshell->wm.transient != owmshell->wm.transient) {
        if (nwmshell->wm.transient) {
            if (!IswIsTransientShell(new) &&
                !nwmshell->shell.override_redirect &&
                nwmshell->wm.wm_hints.window_group != IswUnspecifiedWindowGroup) {
                
                _IswPlatformSetTransientFor(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)),
                    nwmshell->wm.wm_hints.window_group);
            }
        }
        else {
            _IswPlatformDeleteTransientFor(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)));
        }
    }

    if (nwmshell->wm.client_leader != owmshell->wm.client_leader
        && _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)) && !nwmshell->shell.override_redirect) {
        Widget leader = GetClientLeader(new);
        IswWindow leader_win = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(leader)), (Widget)(leader));

        if (leader_win) {
            _IswPlatformSetClientLeader(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)),
                                        leader_win);
        }
    }

    if (nwmshell->wm.window_role != owmshell->wm.window_role) {
        IswFree((_IswString) owmshell->wm.window_role);

        if (set_prop && nwmshell->wm.window_role) {
            _IswPlatformSetWindowRole(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)),
                                      nwmshell->wm.window_role);
        }
        else if (IswIsRealized(new) && !nwmshell->wm.window_role) {
            _IswPlatformDeleteWindowRole(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)));
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
                //               _IswPlatformWidgetWindow(IswDisplayOf((Widget)(newW)), (Widget)(newW)),
                //               XScreenNumberOfScreen(IswScreenOf(newW))
                //    );
                // XCB doesn't have a direct equivalent to XIconifyWindow

                _IswPlatformSetIconic(IswDisplayOf(newW), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(newW)), (Widget)(newW)));
            }
            else {
                Boolean map = new->shell.popped_up;

                IswPopup(newW, IswGrabNone);
                if (map) {
                    _IswPlatformMapWindow(IswDisplayOf(newW), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(newW)), (Widget)(newW)));
                    _IswPlatformFlush(IswDisplayOf(newW));
                }
            }
        }

        if (!new->shell.override_redirect && name_changed) {

            _IswPlatformSetIconTitle(IswDisplayOf(newW), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(newW)), (Widget)(newW)),
                                     (const char *) new->topLevel.icon_name);
        }
    }
    else if (new->topLevel.iconic != old->topLevel.iconic) {
        if (new->topLevel.iconic)
            new->wm.wm_hints.initial_state = IswWmStateIconicState;
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

        if (IswIsRealized(new) && !nw->shell.override_redirect) {
            if (nw->application.argc >= 0 && nw->application.argv) {
                _IswPlatformSetWmCommand(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)),
                                         (const char *const *) nw->application.argv,
                                         nw->application.argc);
            } else {
                _IswPlatformDeleteWmCommand(IswDisplayOf(new), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)));
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
        int tmpx = 0, tmpy = 0;

        /* HiDPI: border_width is logical; X server expects physical pixels. */
        double sf = _IswGetScaleFactor(IswDisplayOf(widget));
        int bw_phys = (int)lrint((double)w->core.border_width * sf);

        _IswPlatformTranslateToRoot(IswDisplayOf(w),
            _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)),
            -bw_phys, -bw_phys, &tmpx, &tmpy);

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
    if (!IswIsRealized(shell) || !argb_data || n_entries == 0)
        return;

    _IswPlatformSetIconData(IswDisplayOf(shell), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(shell)), (Widget)(shell)),
                            argb_data, n_entries);
}

void
IswClearWindowIcon(Widget shell)
{
    if (!IswIsRealized(shell))
        return;

    _IswPlatformDeleteIconData(IswDisplayOf(shell), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(shell)), (Widget)(shell)));
}

static void
SetNetWmString(Widget shell, const char *prop_name _X_UNUSED,
               unsigned int prop_len _X_UNUSED,
               const char *value)
{
    if (!IswIsRealized(shell) || !value)
        return;

    /* TODO: route through a platform hint op */
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
    if (!IswIsRealized(shell) || !state)
        return;

    IswDisplay dpy = IswDisplayOf(shell);
    IswWindow swin = _IswPlatformWidgetWindow(dpy, (Widget) shell);
    _IswPlatformToggleWmState(dpy, swin, state, set);
}

void
_IswShellUpdateUserTime(IswDisplay dpy, Widget widget, IswTime time)
{
    if (!widget)
        return;

    while (widget && !IswIsWMShell(widget))
        widget = IswParent(widget);
    if (!widget)
        return;

    WMShellWidget wmshell = (WMShellWidget) widget;
    if (!wmshell->wm.user_time_win)
        return;

    IswWindow win = _IswPlatformWidgetWindow(dpy, widget);
    _IswPlatformSetUserTime(dpy, win, wmshell->wm.user_time_win, (uint32_t) time);
}
