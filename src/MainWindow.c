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

#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/MainWindowP.h>
#include <ISW/MenuBarP.h>
#include <ISW/StatusBar.h>
#include <ISW/IswArgMacros.h>

#define superclass (&compositeClassRec)

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Resize(Widget);
static void InsertChild(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static void ChangeManaged(Widget);
static IswGeometryResult PreferredSize(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

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
    IswInheritRealize,                    /* realize                */
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
    IswInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    IswVersion,                          /* version                */
    NULL,                               /* callback_private       */
    NULL,                               /* tm_table               */
    PreferredSize,                      /* query_geometry         */
    IswInheritDisplayAccelerator,        /* display_accelerator    */
    NULL                                /* extension              */
  },
  { /* composite */
    GeometryManager,                    /* geometry_manager       */
    ChangeManaged,                      /* change_managed         */
    InsertChild,                        /* insert_child           */
    IswInheritDeleteChild,               /* delete_child           */
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
            IswIsManaged(child))
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
    IswWidgetGeometry pref;

    if (!mw->main_window.menubar || !IswIsManaged(mw->main_window.menubar))
        return 0;

    IswQueryGeometry(mw->main_window.menubar, NULL, &pref);
    return (pref.request_mode & XCB_CONFIG_WINDOW_HEIGHT) ? pref.height
                                          : mw->main_window.menubar->core.height;
}

static Dimension
StatusBarHeight(MainWindowWidget mw)
{
    IswWidgetGeometry pref;

    if (!mw->main_window.statusbar || !IswIsManaged(mw->main_window.statusbar))
        return 0;

    IswQueryGeometry(mw->main_window.statusbar, NULL, &pref);
    return (pref.request_mode & XCB_CONFIG_WINDOW_HEIGHT) ? pref.height
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
    if (mw->main_window.menubar && IswIsManaged(mw->main_window.menubar)) {
        IswConfigureWidget(mw->main_window.menubar, 0, 0, w, mb_h, 0);
    }

    /* StatusBar: bottom, full width */
    if (mw->main_window.statusbar && IswIsManaged(mw->main_window.statusbar)) {
        Position sb_y = (h > sb_h) ? (Position)(h - sb_h) : 0;
        IswConfigureWidget(mw->main_window.statusbar, 0, sb_y, w, sb_h, 0);
    }

    /* Content child: between menubar and statusbar */
    content = FindContentChild(mw);
    if (content) {
        Dimension chrome = mb_h + sb_h;
        Dimension content_h = (h > chrome) ? h - chrome : 1;
        IswConfigureWidget(content, 0, (Position)mb_h, w, content_h, content->core.border_width);
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
    IswArgBuilder ab = IswArgBuilderInit();

    (void)request; (void)args; (void)num_args;

    new->core.windowless = True;

    IswArgBorderWidth(&ab, 0);
    mw->main_window.menubar = IswCreateManagedWidget(
        "menubar", menuBarWidgetClass, new, ab.args, ab.count);
    mw->main_window.statusbar = NULL;
}

static void
InsertChild(Widget child)
{
    /* Call Composite's insert_child */
    (*compositeClassRec.composite_class.insert_child)(child);

    /* If the child is a StatusBar, claim it */
    if (IswIsSubclass(child, statusBarWidgetClass)) {
        MainWindowWidget mw = (MainWindowWidget) IswParent(child);
        if (mw->main_window.statusbar == NULL)
            mw->main_window.statusbar = child;
    }
}

static void
Resize(Widget w)
{
    DoLayout((MainWindowWidget) w);
}

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request, IswWidgetGeometry *reply)
{
    MainWindowWidget mw = (MainWindowWidget) IswParent(child);

    (void)reply;

    /* Deny position requests */
    if ((request->request_mode & XCB_CONFIG_WINDOW_X && request->x != child->core.x) ||
        (request->request_mode & XCB_CONFIG_WINDOW_Y && request->y != child->core.y))
        return IswGeometryNo;

    /* For the menubar or statusbar, allow height changes and relayout */
    if (child == mw->main_window.menubar || child == mw->main_window.statusbar) {
        if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) {
            child->core.height = request->height;
            DoLayout(mw);
        }
        return IswGeometryYes;
    }

    /* For the content child, allow height changes by negotiating with parent */
    if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) {
        Dimension mb_h = MenuBarHeight(mw);
        Dimension sb_h = StatusBarHeight(mw);
        Dimension new_total = mb_h + sb_h + request->height;
        Dimension proposed_w = mw->core.width;
        Dimension proposed_h = new_total;

        switch (IswMakeResizeRequest((Widget)mw, proposed_w, proposed_h,
                                    &proposed_w, &proposed_h)) {
        case IswGeometryYes:
            DoLayout(mw);
            return IswGeometryYes;
        case IswGeometryAlmost:
            (void) IswMakeResizeRequest((Widget)mw, proposed_w, proposed_h,
                                       NULL, NULL);
            DoLayout(mw);
            return IswGeometryYes;
        case IswGeometryNo:
        default:
            return IswGeometryNo;
        }
    }

    return IswGeometryYes;
}

static void
ChangeManaged(Widget w)
{
    DoLayout((MainWindowWidget) w);
}

static IswGeometryResult
PreferredSize(Widget w, IswWidgetGeometry *constraint, IswWidgetGeometry *preferred)
{
    MainWindowWidget mw = (MainWindowWidget) w;
    IswWidgetGeometry mb_pref, sb_pref, content_pref;
    Dimension pref_w = 0, pref_h = 0;
    Widget content;

    (void)constraint;

    /* Query menubar preferred size */
    if (mw->main_window.menubar && IswIsManaged(mw->main_window.menubar)) {
        IswQueryGeometry(mw->main_window.menubar, NULL, &mb_pref);
        if (mb_pref.request_mode & XCB_CONFIG_WINDOW_WIDTH)
            pref_w = mb_pref.width;
        if (mb_pref.request_mode & XCB_CONFIG_WINDOW_HEIGHT)
            pref_h = mb_pref.height;
    }

    /* Query statusbar preferred size */
    if (mw->main_window.statusbar && IswIsManaged(mw->main_window.statusbar)) {
        IswQueryGeometry(mw->main_window.statusbar, NULL, &sb_pref);
        if ((sb_pref.request_mode & XCB_CONFIG_WINDOW_WIDTH) && sb_pref.width > pref_w)
            pref_w = sb_pref.width;
        if (sb_pref.request_mode & XCB_CONFIG_WINDOW_HEIGHT)
            pref_h += sb_pref.height;
    }

    /* Query content child preferred size */
    content = FindContentChild(mw);
    if (content) {
        IswQueryGeometry(content, NULL, &content_pref);
        if ((content_pref.request_mode & XCB_CONFIG_WINDOW_WIDTH) &&
            content_pref.width > pref_w)
            pref_w = content_pref.width;
        if (content_pref.request_mode & XCB_CONFIG_WINDOW_HEIGHT)
            pref_h += content_pref.height;
    }

    preferred->request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    preferred->width = pref_w > 0 ? pref_w : mw->core.width;
    preferred->height = pref_h > 0 ? pref_h : mw->core.height;

    if (constraint &&
        (constraint->request_mode & XCB_CONFIG_WINDOW_WIDTH) &&
        constraint->width == preferred->width &&
        (constraint->request_mode & XCB_CONFIG_WINDOW_HEIGHT) &&
        constraint->height == preferred->height)
        return IswGeometryYes;

    return IswGeometryAlmost;
}

/****************************************************************
 *
 * Public Functions
 *
 ****************************************************************/

Widget
IswMainWindowGetMenuBar(Widget w)
{
    if (!IswIsSubclass(w, mainWindowWidgetClass))
        return NULL;
    return ((MainWindowWidget) w)->main_window.menubar;
}

Widget
IswMainWindowGetStatusBar(Widget w)
{
    if (!IswIsSubclass(w, mainWindowWidgetClass))
        return NULL;
    return ((MainWindowWidget) w)->main_window.statusbar;
}
