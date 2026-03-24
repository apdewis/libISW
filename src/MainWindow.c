/*
Copyright (c) 2024  Infi Systems

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
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
INFI SYSTEMS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * MainWindow.c - MainWindow composite widget
 *
 * Manages a fixed MenuBar at the top and a single content child below.
 * The menubar is always full width and never scrolls.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/MainWindowP.h>
#include <ISW/MenuBarP.h>
#include <ISW/StatusBar.h>

#define superclass (&compositeClassRec)

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Resize(Widget);
static void InsertChild(Widget);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *, XtWidgetGeometry *);
static void ChangeManaged(Widget);
static XtGeometryResult PreferredSize(Widget, XtWidgetGeometry *, XtWidgetGeometry *);

MainWindowClassRec mainWindowClassRec = {
  { /* core */
    (WidgetClass) superclass,           /* superclass             */
    "MainWindow",                       /* class_name             */
    sizeof(MainWindowRec),              /* size                   */
    ClassInitialize,                    /* class_initialize       */
    NULL,                               /* class_part_initialize  */
    FALSE,                              /* class_inited           */
    Initialize,                         /* initialize             */
    NULL,                               /* initialize_hook        */
    XtInheritRealize,                    /* realize                */
    NULL,                               /* actions                */
    0,                                  /* num_actions            */
    NULL,                               /* resources              */
    0,                                  /* resource_count         */
    NULLQUARK,                          /* xrm_class              */
    TRUE,                               /* compress_motion        */
    TRUE,                               /* compress_exposure      */
    TRUE,                               /* compress_enterleave    */
    FALSE,                              /* visible_interest       */
    NULL,                               /* destroy                */
    Resize,                             /* resize                 */
    NULL,                               /* expose                 */
    NULL,                               /* set_values             */
    NULL,                               /* set_values_hook        */
    XtInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    XtVersion,                          /* version                */
    NULL,                               /* callback_private       */
    NULL,                               /* tm_table               */
    PreferredSize,                      /* query_geometry         */
    XtInheritDisplayAccelerator,        /* display_accelerator    */
    NULL                                /* extension              */
  },
  { /* composite */
    GeometryManager,                    /* geometry_manager       */
    ChangeManaged,                      /* change_managed         */
    InsertChild,                        /* insert_child           */
    XtInheritDeleteChild,               /* delete_child           */
    NULL                                /* extension              */
  },
  { /* main_window */
    0                                   /* empty                  */
  }
};

WidgetClass mainWindowWidgetClass = (WidgetClass) &mainWindowClassRec;

/****************************************************************
 *
 * Private Routines
 *
 ****************************************************************/

/*
 * Find the content child: first managed child that is not the menubar.
 */
static Widget
FindContentChild(MainWindowWidget mw)
{
    Cardinal i;

    for (i = 0; i < mw->composite.num_children; i++) {
        Widget child = mw->composite.children[i];
        if (child != mw->main_window.menubar &&
            child != mw->main_window.statusbar &&
            XtIsManaged(child))
            return child;
    }
    return NULL;
}

/*
 * Get the menubar's preferred height.
 */
static Dimension
MenuBarHeight(MainWindowWidget mw)
{
    XtWidgetGeometry pref;

    if (!mw->main_window.menubar || !XtIsManaged(mw->main_window.menubar))
        return 0;

    XtQueryGeometry(mw->main_window.menubar, NULL, &pref);
    return (pref.request_mode & CWHeight) ? pref.height
                                          : mw->main_window.menubar->core.height;
}

static Dimension
StatusBarHeight(MainWindowWidget mw)
{
    XtWidgetGeometry pref;

    if (!mw->main_window.statusbar || !XtIsManaged(mw->main_window.statusbar))
        return 0;

    XtQueryGeometry(mw->main_window.statusbar, NULL, &pref);
    return (pref.request_mode & CWHeight) ? pref.height
                                          : mw->main_window.statusbar->core.height;
}

/*
 * Position the menubar at the top, statusbar at the bottom (both full-width),
 * content child fills the remaining space between them.
 */
static void
DoLayout(MainWindowWidget mw)
{
    Dimension mb_h = MenuBarHeight(mw);
    Dimension sb_h = StatusBarHeight(mw);
    Dimension w = mw->core.width;
    Dimension h = mw->core.height;
    Widget content;

    /* Menubar: top, full width */
    if (mw->main_window.menubar && XtIsManaged(mw->main_window.menubar)) {
        XtConfigureWidget(mw->main_window.menubar, 0, 0, w, mb_h, 0);
    }

    /* StatusBar: bottom, full width */
    if (mw->main_window.statusbar && XtIsManaged(mw->main_window.statusbar)) {
        Position sb_y = (h > sb_h) ? (Position)(h - sb_h) : 0;
        XtConfigureWidget(mw->main_window.statusbar, 0, sb_y, w, sb_h, 0);
    }

    /* Content child: between menubar and statusbar */
    content = FindContentChild(mw);
    if (content) {
        Dimension chrome = mb_h + sb_h;
        Dimension content_h = (h > chrome) ? h - chrome : 1;
        XtConfigureWidget(content, 0, (Position)mb_h, w, content_h, 0);
    }
}

/****************************************************************
 *
 * Class Methods
 *
 ****************************************************************/

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    MainWindowWidget mw = (MainWindowWidget) new;
    Arg mbar_args[2];
    Cardinal n = 0;

    (void)request; (void)args; (void)num_args;

    XtSetArg(mbar_args[n], XtNborderWidth, 0); n++;
    mw->main_window.menubar = XtCreateManagedWidget(
        "menubar", menuBarWidgetClass, new, mbar_args, n);
    mw->main_window.statusbar = NULL;
}

static void
InsertChild(Widget child)
{
    /* Call Composite's insert_child */
    (*compositeClassRec.composite_class.insert_child)(child);

    /* If the child is a StatusBar, claim it */
    if (XtIsSubclass(child, statusBarWidgetClass)) {
        MainWindowWidget mw = (MainWindowWidget) XtParent(child);
        if (mw->main_window.statusbar == NULL)
            mw->main_window.statusbar = child;
    }
}

static void
Resize(Widget w)
{
    DoLayout((MainWindowWidget) w);
}

static XtGeometryResult
GeometryManager(Widget child, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
    MainWindowWidget mw = (MainWindowWidget) XtParent(child);

    (void)reply;

    /* Deny position requests */
    if ((request->request_mode & CWX && request->x != child->core.x) ||
        (request->request_mode & CWY && request->y != child->core.y))
        return XtGeometryNo;

    /* For the menubar or statusbar, allow height changes and relayout */
    if (child == mw->main_window.menubar || child == mw->main_window.statusbar) {
        if (request->request_mode & CWHeight) {
            child->core.height = request->height;
            DoLayout(mw);
        }
        return XtGeometryYes;
    }

    /* For the content child, allow height changes by negotiating with parent */
    if (request->request_mode & CWHeight) {
        Dimension mb_h = MenuBarHeight(mw);
        Dimension sb_h = StatusBarHeight(mw);
        Dimension new_total = mb_h + sb_h + request->height;
        Dimension proposed_w = mw->core.width;
        Dimension proposed_h = new_total;

        switch (XtMakeResizeRequest((Widget)mw, proposed_w, proposed_h,
                                    &proposed_w, &proposed_h)) {
        case XtGeometryYes:
            DoLayout(mw);
            return XtGeometryYes;
        case XtGeometryAlmost:
            (void) XtMakeResizeRequest((Widget)mw, proposed_w, proposed_h,
                                       NULL, NULL);
            DoLayout(mw);
            return XtGeometryYes;
        case XtGeometryNo:
        default:
            return XtGeometryNo;
        }
    }

    return XtGeometryYes;
}

static void
ChangeManaged(Widget w)
{
    DoLayout((MainWindowWidget) w);
}

static XtGeometryResult
PreferredSize(Widget w, XtWidgetGeometry *constraint, XtWidgetGeometry *preferred)
{
    MainWindowWidget mw = (MainWindowWidget) w;
    XtWidgetGeometry mb_pref, sb_pref, content_pref;
    Dimension pref_w = 0, pref_h = 0;
    Widget content;

    (void)constraint;

    /* Query menubar preferred size */
    if (mw->main_window.menubar && XtIsManaged(mw->main_window.menubar)) {
        XtQueryGeometry(mw->main_window.menubar, NULL, &mb_pref);
        if (mb_pref.request_mode & CWWidth)
            pref_w = mb_pref.width;
        if (mb_pref.request_mode & CWHeight)
            pref_h = mb_pref.height;
    }

    /* Query statusbar preferred size */
    if (mw->main_window.statusbar && XtIsManaged(mw->main_window.statusbar)) {
        XtQueryGeometry(mw->main_window.statusbar, NULL, &sb_pref);
        if ((sb_pref.request_mode & CWWidth) && sb_pref.width > pref_w)
            pref_w = sb_pref.width;
        if (sb_pref.request_mode & CWHeight)
            pref_h += sb_pref.height;
    }

    /* Query content child preferred size */
    content = FindContentChild(mw);
    if (content) {
        XtQueryGeometry(content, NULL, &content_pref);
        if ((content_pref.request_mode & CWWidth) &&
            content_pref.width > pref_w)
            pref_w = content_pref.width;
        if (content_pref.request_mode & CWHeight)
            pref_h += content_pref.height;
    }

    preferred->request_mode = CWWidth | CWHeight;
    preferred->width = pref_w > 0 ? pref_w : mw->core.width;
    preferred->height = pref_h > 0 ? pref_h : mw->core.height;

    if (constraint &&
        (constraint->request_mode & CWWidth) &&
        constraint->width == preferred->width &&
        (constraint->request_mode & CWHeight) &&
        constraint->height == preferred->height)
        return XtGeometryYes;

    return XtGeometryAlmost;
}

/****************************************************************
 *
 * Public Functions
 *
 ****************************************************************/

Widget
IswMainWindowGetMenuBar(Widget w)
{
    if (!XtIsSubclass(w, mainWindowWidgetClass))
        return NULL;
    return ((MainWindowWidget) w)->main_window.menubar;
}

Widget
IswMainWindowGetStatusBar(Widget w)
{
    if (!XtIsSubclass(w, mainWindowWidgetClass))
        return NULL;
    return ((MainWindowWidget) w)->main_window.statusbar;
}
