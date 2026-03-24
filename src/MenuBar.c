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
 * MenuBar.c - MenuBar composite widget
 *
 * A modern menubar widget that manages MenuButton children with
 * click-to-open/close behavior and hover-to-switch between menus.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/MenuBarP.h>
#include <ISW/MenuButtoP.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeBSBP.h>
#include <xcb/xcb.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"

#define XtNshadowWidth "shadowWidth"
#define XtNhighlightThickness "highlightThickness"

#define superclass (&boxClassRec)

/* Event mask for the toplevel shell dismiss handler */
#define DISMISS_MASK (ButtonPressMask | FocusChangeMask | \
                      StructureNotifyMask | VisibilityChangeMask)

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Destroy(Widget);
static void InsertChild(Widget);

/* Action procedures - registered globally so MenuButton children can use them */
static void MenuBarEnter(Widget, XEvent *, String *, Cardinal *);
static void MenuBarLeave(Widget, XEvent *, String *, Cardinal *);
static void MenuBarClick(Widget, XEvent *, String *, Cardinal *);
static void MenuBarDismiss(Widget, XEvent *, String *, Cardinal *);

/* Private functions */
static void OpenMenu(MenuBarWidget, Widget);
static void CloseMenu(MenuBarWidget);
static void SwitchMenu(MenuBarWidget, Widget);
static Widget FindMenuForButton(Widget);
static void MenuPopdownCB(Widget, XtPointer, XtPointer);
static void OutsideClickHandler(Widget, XtPointer, XEvent *, Boolean *);
static Widget FindToplevelShell(Widget);

/* Override SimpleMenu translations for click-to-select behavior.
 * <Motion> is needed because there's no active pointer grab after
 * the opening click is released -- <BtnMotion> only fires while
 * a button is held down.
 * <BtnUp> is explicitly overridden to prevent the original
 * spring-loaded "release to select" behavior from firing.
 */
static char menuBarMenuTranslations[] =
    "<EnterWindow>:     highlight()             \n\
     <LeaveWindow>:     unhighlight()           \n\
     <Motion>:          highlight()             \n\
     <BtnMotion>:       highlight()             \n\
     <Btn4Down>:        unhighlight() popdown() \n\
     <Btn5Down>:        unhighlight() popdown() \n\
     <BtnUp>:           highlight()             \n\
     <BtnDown>:         notify() unhighlight() popdown()";

/* Translations for MenuButton children inside the menubar */
static char menuBarChildTranslations[] =
    "<EnterWindow>:     menubar-enter()         \n\
     <LeaveWindow>:     menubar-leave()         \n\
     Any<BtnDown>:      menubar-click()";

static XtActionsRec actionsList[] = {
    {"menubar-dismiss",  MenuBarDismiss},
};

/* Actions registered globally (for use by MenuButton children) */
static XtActionsRec globalActionsList[] = {
    {"menubar-enter",    MenuBarEnter},
    {"menubar-leave",    MenuBarLeave},
    {"menubar-click",    MenuBarClick},
};

static char defaultTranslations[] =
    "<Key>Escape: menubar-dismiss()";

MenuBarClassRec menuBarClassRec = {
  { /* core */
    (WidgetClass) superclass,           /* superclass             */
    "MenuBar",                          /* class_name             */
    sizeof(MenuBarRec),                 /* size                   */
    ClassInitialize,                    /* class_initialize       */
    NULL,                               /* class_part_initialize  */
    FALSE,                              /* class_inited           */
    Initialize,                         /* initialize             */
    NULL,                               /* initialize_hook        */
    XtInheritRealize,                   /* realize                */
    actionsList,                        /* actions                */
    XtNumber(actionsList),              /* num_actions            */
    NULL,                               /* resources              */
    0,                                  /* resource_count         */
    NULLQUARK,                          /* xrm_class              */
    TRUE,                               /* compress_motion        */
    TRUE,                               /* compress_exposure      */
    TRUE,                               /* compress_enterleave    */
    FALSE,                              /* visible_interest       */
    Destroy,                            /* destroy                */
    XtInheritResize,                    /* resize                 */
    Redisplay,                          /* expose                 */
    NULL,                               /* set_values             */
    NULL,                               /* set_values_hook        */
    XtInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    XtVersion,                          /* version                */
    NULL,                               /* callback_private       */
    defaultTranslations,                /* tm_table               */
    XtInheritQueryGeometry,             /* query_geometry         */
    XtInheritDisplayAccelerator,        /* display_accelerator    */
    NULL                                /* extension              */
  },
  { /* composite */
    XtInheritGeometryManager,           /* geometry_manager       */
    XtInheritChangeManaged,             /* change_managed         */
    InsertChild,                        /* insert_child           */
    XtInheritDeleteChild,               /* delete_child           */
    NULL                                /* extension              */
  },
  { /* box */
    0                                   /* empty                  */
  },
  { /* menu_bar */
    0                                   /* empty                  */
  }
};

WidgetClass menuBarWidgetClass = (WidgetClass) &menuBarClassRec;

/****************************************************************
 *
 * Class Methods
 *
 ****************************************************************/

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    (void)event; (void)region;

    if (!XtIsRealized(w) || w->core.width == 0 || w->core.height == 0)
        return;

    ISWRenderContext *ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    if (ctx) {
        int y = (int)w->core.height - 1;
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, w->core.border_pixel);
        ISWRenderDrawLine(ctx, 0, y, (int)w->core.width, y);
        ISWRenderEnd(ctx);
        ISWRenderDestroy(ctx);
    }
}

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
}

static Boolean globalActionsRegistered = FALSE;

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    MenuBarWidget mbw = (MenuBarWidget) new;

    mbw->menu_bar.active_button = NULL;
    mbw->menu_bar.active_menu = NULL;
    mbw->menu_bar.menu_is_open = FALSE;

    /* Force horizontal orientation, minimal horizontal spacing,
     * vertical padding to keep items clear of the bottom border */
    mbw->box.orientation = XtorientHorizontal;
    mbw->box.h_space = 0;
    mbw->box.v_space = 2;

    /* Register global actions once so MenuButton children can find them */
    if (!globalActionsRegistered) {
        XtAppAddActions(XtWidgetToApplicationContext(new),
                        globalActionsList, XtNumber(globalActionsList));
        globalActionsRegistered = TRUE;
    }
}

static void
Destroy(Widget w)
{
    MenuBarWidget mbw = (MenuBarWidget) w;

    if (mbw->menu_bar.menu_is_open)
        CloseMenu(mbw);
}

static void
InsertChild(Widget child)
{
    /* Call superclass insert_child (Box's) */
    (*boxClassRec.composite_class.insert_child)(child);

    /* If the child is a MenuButton, style it for menubar use */
    if (XtIsSubclass(child, menuButtonWidgetClass)) {
        Arg args[6];
        Cardinal n = 0;
        static XtTranslations parsed = NULL;

        /* Flat appearance: no border, no 3D shadow, no highlight frame */
        XtSetArg(args[n], XtNborderWidth, 0); n++;
        XtSetArg(args[n], XtNinternalWidth, 6); n++;
        XtSetArg(args[n], XtNinternalHeight, 2); n++;
        XtSetArg(args[n], XtNshadowWidth, 0); n++;
        XtSetArg(args[n], XtNhighlightThickness, 0); n++;
        XtSetValues(child, args, n);

        if (parsed == NULL)
            parsed = XtParseTranslationTable(menuBarChildTranslations);
        XtOverrideTranslations(child, parsed);
    }
}

/****************************************************************
 *
 * Action Procedures
 *
 ****************************************************************/

/*
 * Called when pointer enters a MenuButton child.
 * If a menu is already open and this is a different button, switch menus.
 */
static void
MenuBarEnter(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw;

    if (!XtIsSubclass(w, menuButtonWidgetClass))
        return;
    mbw = (MenuBarWidget) XtParent(w);
    if (!XtIsSubclass((Widget)mbw, menuBarWidgetClass))
        return;

    /* Highlight the button on hover */
    XtCallActionProc(w, "highlight", event, NULL, 0);

    /* If a menu is open and we entered a different button, switch */
    if (mbw->menu_bar.menu_is_open && w != mbw->menu_bar.active_button)
        SwitchMenu(mbw, w);
}

/*
 * Called when pointer leaves a MenuButton child.
 * Don't unhighlight the active button while its menu is open.
 */
static void
MenuBarLeave(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw;

    if (!XtIsSubclass(w, menuButtonWidgetClass))
        return;
    mbw = (MenuBarWidget) XtParent(w);
    if (!XtIsSubclass((Widget)mbw, menuBarWidgetClass))
        return;

    /* Keep the active button highlighted while its menu is open */
    if (mbw->menu_bar.menu_is_open && w == mbw->menu_bar.active_button)
        return;

    XtCallActionProc(w, "reset", event, NULL, 0);
}

/*
 * Called when a MenuButton child is clicked.
 * Toggle: if this button's menu is open, close it; otherwise open it.
 */
static void
MenuBarClick(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw;

    if (!XtIsSubclass(w, menuButtonWidgetClass))
        return;
    mbw = (MenuBarWidget) XtParent(w);
    if (!XtIsSubclass((Widget)mbw, menuBarWidgetClass))
        return;

    if (mbw->menu_bar.menu_is_open) {
        if (w == mbw->menu_bar.active_button) {
            /* Toggle off: close the menu */
            CloseMenu(mbw);
        } else {
            /* Different button: switch to its menu */
            SwitchMenu(mbw, w);
        }
    } else {
        /* No menu open: open this button's menu */
        OpenMenu(mbw, w);
    }
}

/*
 * Called on Escape key to dismiss any open menu.
 */
static void
MenuBarDismiss(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw = (MenuBarWidget) w;

    if (!XtIsSubclass(w, menuBarWidgetClass))
        return;

    if (mbw->menu_bar.menu_is_open)
        CloseMenu(mbw);
}

/****************************************************************
 *
 * Private Functions
 *
 ****************************************************************/

/*
 * Find the SimpleMenu popup shell associated with a MenuButton.
 * Replicates the lookup logic from MenuButton.c's PopupMenu.
 */
static Widget
FindMenuForButton(Widget button)
{
    MenuButtonWidget mbtn = (MenuButtonWidget) button;
    Widget menu = NULL, temp;

    temp = button;
    while (temp != NULL) {
        menu = XtNameToWidget(temp, mbtn->menu_button.menu_name);
        if (menu == NULL)
            temp = XtParent(temp);
        else
            break;
    }
    return menu;
}

/*
 * Open the dropdown menu for the given MenuButton.
 */
static void
OpenMenu(MenuBarWidget mbw, Widget button)
{
    Widget menu;
    int menu_x, menu_y, menu_width, menu_height, button_height;
    Position button_x, button_y;
    Arg arglist[2];
    Cardinal num_args;
    static XtTranslations menu_translations = NULL;

    menu = FindMenuForButton(button);
    if (menu == NULL)
        return;

    /* Remove 3D shadows from menu and its entries */
    {
        Arg flat[2];
        Cardinal i;
        SimpleMenuWidget smw = (SimpleMenuWidget) menu;

        XtSetArg(flat[0], XtNshadowWidth, 0);
        XtSetArg(flat[1], XtNborderWidth, 0);
        XtSetValues(menu, flat, 2);

        for (i = 0; i < smw->composite.num_children; i++) {
            XtSetArg(flat[0], XtNshadowWidth, 0);
            XtSetValues(smw->composite.children[i], flat, 1);
        }
    }

    if (!XtIsRealized(menu))
        XtRealizeWidget(menu);

    /* Position menu below the button */
    menu_width = menu->core.width + 2 * menu->core.border_width;
    button_height = button->core.height + 2 * button->core.border_width;
    menu_height = menu->core.height + 2 * menu->core.border_width;

    XtTranslateCoords(button, 0, 0, &button_x, &button_y);
    menu_x = button_x;
    menu_y = button_y + button_height;

    /* Clamp to screen edges */
    if (menu_x >= 0) {
        int scr_width = WidthOfScreen(XtScreen(menu));
        if (menu_x + menu_width > scr_width)
            menu_x = scr_width - menu_width;
    }
    if (menu_x < 0)
        menu_x = 0;

    if (menu_y >= 0) {
        int scr_height = HeightOfScreen(XtScreen(menu));
        if (menu_y + menu_height > scr_height)
            menu_y = scr_height - menu_height;
    }
    if (menu_y < 0)
        menu_y = 0;

    num_args = 0;
    XtSetArg(arglist[num_args], XtNx, menu_x); num_args++;
    XtSetArg(arglist[num_args], XtNy, menu_y); num_args++;
    XtSetValues(menu, arglist, num_args);

    /* Override SimpleMenu translations for click-to-select behavior */
    if (menu_translations == NULL)
        menu_translations = XtParseTranslationTable(menuBarMenuTranslations);
    XtOverrideTranslations(menu, menu_translations);

    /* Pop up without grab -- we handle dismissal ourselves */
    XtPopup(menu, XtGrabNone);

    /* X server pointer grab — delivers all button events (scroll, outside
     * clicks) to the menu window. Same technique as GTK/Motif popups. */
    if (XtIsRealized(menu)) {
        xcb_grab_pointer(XtDisplay(menu), True, XtWindow(menu),
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION |
            XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
            XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
        xcb_flush(XtDisplay(menu));
    }

    /* Visually activate the button (inverted/set state) */
    XtCallActionProc(button, "set", NULL, NULL, 0);
    XtCallActionProc(button, "highlight", NULL, (String[]){"Always"}, 1);

    /* Register popdown callback to clean up state */
    XtAddCallback(menu, XtNpopdownCallback, MenuPopdownCB, (XtPointer)mbw);

    /* Install click-outside handler on toplevel shell */
    {
        Widget toplevel = FindToplevelShell((Widget)mbw);
        if (toplevel)
            XtAddEventHandler(toplevel, DISMISS_MASK, False,
                              OutsideClickHandler, (XtPointer)mbw);
    }

    mbw->menu_bar.active_button = button;
    mbw->menu_bar.active_menu = menu;
    mbw->menu_bar.menu_is_open = TRUE;
}

/*
 * Close the currently open dropdown menu.
 */
static void
CloseMenu(MenuBarWidget mbw)
{
    Widget menu, button, toplevel;

    if (!mbw->menu_bar.menu_is_open)
        return;

    menu = mbw->menu_bar.active_menu;
    button = mbw->menu_bar.active_button;

    xcb_ungrab_pointer(XtDisplay((Widget)mbw), XCB_CURRENT_TIME);
    xcb_flush(XtDisplay((Widget)mbw));

    /* Remove click-outside handler */
    toplevel = FindToplevelShell((Widget)mbw);
    if (toplevel)
        XtRemoveEventHandler(toplevel, DISMISS_MASK, False,
                             OutsideClickHandler, (XtPointer)mbw);

    if (menu) {
        XtRemoveCallback(menu, XtNpopdownCallback, MenuPopdownCB, (XtPointer)mbw);
        XtPopdown(menu);
    }

    /* Reset the button visual state */
    if (button) {
        XtCallActionProc(button, "unset", NULL, NULL, 0);
        XtCallActionProc(button, "unhighlight", NULL, NULL, 0);
    }

    mbw->menu_bar.active_button = NULL;
    mbw->menu_bar.active_menu = NULL;
    mbw->menu_bar.menu_is_open = FALSE;
}

/*
 * Switch from the current open menu to a different button's menu.
 */
static void
SwitchMenu(MenuBarWidget mbw, Widget new_button)
{
    Widget old_button = mbw->menu_bar.active_button;
    Widget old_menu = mbw->menu_bar.active_menu;
    Widget toplevel;

    xcb_ungrab_pointer(XtDisplay((Widget)mbw), XCB_CURRENT_TIME);
    xcb_flush(XtDisplay((Widget)mbw));

    /* Remove popdown callback and click-outside handler from old menu */
    toplevel = FindToplevelShell((Widget)mbw);
    if (toplevel)
        XtRemoveEventHandler(toplevel, DISMISS_MASK, False,
                             OutsideClickHandler, (XtPointer)mbw);

    if (old_menu) {
        XtRemoveCallback(old_menu, XtNpopdownCallback, MenuPopdownCB, (XtPointer)mbw);
        XtPopdown(old_menu);
    }

    /* Reset old button visual state */
    if (old_button) {
        XtCallActionProc(old_button, "unset", NULL, NULL, 0);
        XtCallActionProc(old_button, "unhighlight", NULL, NULL, 0);
    }

    mbw->menu_bar.active_button = NULL;
    mbw->menu_bar.active_menu = NULL;
    mbw->menu_bar.menu_is_open = FALSE;

    /* Open the new button's menu */
    OpenMenu(mbw, new_button);
}

/*
 * Callback invoked when a SimpleMenu pops down (e.g. after item selection).
 * Cleans up MenuBar state.
 */
static void
MenuPopdownCB(Widget menu, XtPointer client_data, XtPointer call_data)
{
    MenuBarWidget mbw = (MenuBarWidget) client_data;
    Widget button, toplevel;

    if (!mbw->menu_bar.menu_is_open)
        return;

    button = mbw->menu_bar.active_button;

    xcb_ungrab_pointer(XtDisplay((Widget)mbw), XCB_CURRENT_TIME);
    xcb_flush(XtDisplay((Widget)mbw));

    /* Remove click-outside handler */
    toplevel = FindToplevelShell((Widget)mbw);
    if (toplevel)
        XtRemoveEventHandler(toplevel, DISMISS_MASK, False,
                             OutsideClickHandler, (XtPointer)mbw);

    XtRemoveCallback(menu, XtNpopdownCallback, MenuPopdownCB, (XtPointer)mbw);

    /* Reset button visual state */
    if (button) {
        XtCallActionProc(button, "unset", NULL, NULL, 0);
        XtCallActionProc(button, "unhighlight", NULL, NULL, 0);
    }

    mbw->menu_bar.active_button = NULL;
    mbw->menu_bar.active_menu = NULL;
    mbw->menu_bar.menu_is_open = FALSE;
}

/*
 * Event handler on the toplevel shell to dismiss the menu on outside
 * clicks, focus loss, minimize, or visibility changes.
 */
static void
OutsideClickHandler(Widget w, XtPointer client_data, XEvent *event, Boolean *cont)
{
    MenuBarWidget mbw = (MenuBarWidget) client_data;
    uint8_t type;

    if (!mbw->menu_bar.menu_is_open)
        return;

    type = event->response_type & 0x7f;

    /* Focus loss, minimize, or visibility change — always dismiss */
    if (type == XCB_FOCUS_OUT || type == XCB_UNMAP_NOTIFY ||
        type == XCB_VISIBILITY_NOTIFY || type == XCB_CONFIGURE_NOTIFY) {
        CloseMenu(mbw);
        return;
    }

    if (type != XCB_BUTTON_PRESS)
        return;

    {
        xcb_button_press_event_t *bev = (xcb_button_press_event_t *)event;
        Widget target;

        /* Check if the click is on the menubar itself or any of its children */
        target = XtWindowToWidget(XtDisplay((Widget)mbw), bev->event);
        if (target == NULL) {
            CloseMenu(mbw);
            return;
        }

        /* If click is on a MenuButton child of this menubar, let MenuBarClick handle it */
        if (XtParent(target) == (Widget)mbw && XtIsSubclass(target, menuButtonWidgetClass))
            return;

        /* If click is on the menubar itself, ignore */
        if (target == (Widget)mbw)
            return;

        /* If click is on the active menu or its children, let the menu handle it */
        if (mbw->menu_bar.active_menu) {
            Widget check = target;
            while (check != NULL) {
                if (check == mbw->menu_bar.active_menu)
                    return;
                check = XtParent(check);
            }
        }

        /* Click is outside -- dismiss */
        CloseMenu(mbw);
    }
}

/*
 * Walk up the widget tree to find the toplevel shell.
 */
static Widget
FindToplevelShell(Widget w)
{
    while (w != NULL && !XtIsShell(w))
        w = XtParent(w);
    return w;
}
