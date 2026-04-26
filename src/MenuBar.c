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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/MenuBarP.h>
#include <ISW/MenuButtoP.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeBSBP.h>
#include <xcb/xcb.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"

#include <ISW/Command.h>

#define superclass (&boxClassRec)

/* Event mask for the toplevel shell dismiss handler */
#define DISMISS_MASK (XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE | \
                      XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_VISIBILITY_CHANGE)

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Destroy(Widget);
static void InsertChild(Widget);

/* Action procedures - registered globally so MenuButton children can use them */
static void MenuBarEnter(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void MenuBarLeave(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void MenuBarClick(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void MenuBarDismiss(Widget, xcb_generic_event_t *, String *, Cardinal *);

/* Private functions */
static void OpenMenu(MenuBarWidget, Widget);
static void CloseMenu(MenuBarWidget);
static void SwitchMenu(MenuBarWidget, Widget);
static Widget FindMenuForButton(Widget);
static void MenuPopdownCB(Widget, IswPointer, IswPointer);
static void OutsideClickHandler(Widget, IswPointer, xcb_generic_event_t *, Boolean *);
static Widget FindToplevelShell(Widget);

/* Override SimpleMenu translations for menubar-owned menus.
 * <Motion> is needed because menubar menus run under our own xcb_grab_pointer
 * (IswGrabNone at popup time); <BtnMotion> only fires while a button is held.
 * <Btn4Down>/<Btn5Down> make the scroll wheel dismiss the menu. */
static char menuBarMenuTranslations[] =
    "<EnterWindow>:     highlight()             \n\
     <LeaveWindow>:     unhighlight()           \n\
     <Motion>:          highlight()             \n\
     <BtnMotion>:       highlight()             \n\
     <Btn4Down>:        unhighlight() popdown() \n\
     <Btn5Down>:        unhighlight() popdown() \n\
     <BtnDown>:         notify() unhighlight() popdown()";

/* Translations for MenuButton children inside the menubar */
static char menuBarChildTranslations[] =
    "<EnterWindow>:     menubar-enter()         \n\
     <LeaveWindow>:     menubar-leave()         \n\
     Any<BtnDown>:      menubar-click()";

static IswActionsRec actionsList[] = {
    {"menubar-dismiss",  MenuBarDismiss},
};

/* Actions registered globally (for use by MenuButton children) */
static IswActionsRec globalActionsList[] = {
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
    IswInheritRealize,                   /* realize                */
    actionsList,                        /* actions                */
    IswNumber(actionsList),              /* num_actions            */
    NULL,                               /* resources              */
    0,                                  /* resource_count         */
    NULLQUARK,                          /* xrm_class              */
    TRUE,                               /* compress_motion        */
    TRUE,                               /* compress_exposure      */
    TRUE,                               /* compress_enterleave    */
    FALSE,                              /* visible_interest       */
    Destroy,                            /* destroy                */
    IswInheritResize,                    /* resize                 */
    Redisplay,                          /* expose                 */
    NULL,                               /* set_values             */
    NULL,                               /* set_values_hook        */
    IswInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    IswVersion,                          /* version                */
    NULL,                               /* callback_private       */
    defaultTranslations,                /* tm_table               */
    IswInheritQueryGeometry,             /* query_geometry         */
    IswInheritDisplayAccelerator,        /* display_accelerator    */
    NULL                                /* extension              */
  },
  { /* composite */
    IswInheritGeometryManager,           /* geometry_manager       */
    IswInheritChangeManaged,             /* change_managed         */
    InsertChild,                        /* insert_child           */
    IswInheritDeleteChild,               /* delete_child           */
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

    if (!IswIsRealized(w) || w->core.width == 0 || w->core.height == 0)
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
    mbw->box.orientation = IswOrientHorizontal;
    mbw->box.h_space = 0;
    mbw->box.v_space = 2;

    /* Register global actions once so MenuButton children can find them */
    if (!globalActionsRegistered) {
        IswAppAddActions(IswWidgetToApplicationContext(new),
                        globalActionsList, IswNumber(globalActionsList));
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
    if (IswIsSubclass(child, menuButtonWidgetClass)) {
        Arg args[6];
        Cardinal n = 0;
        static IswTranslations parsed = NULL;

        /* Flat appearance: no border, no 3D shadow, no highlight frame */
        IswSetArg(args[n], IswNborderWidth, 0); n++;
        IswSetArg(args[n], IswNcornerRadius, 0); n++;
        IswSetArg(args[n], IswNinternalWidth, 6); n++;
        IswSetArg(args[n], IswNinternalHeight, 2); n++;
        IswSetValues(child, args, n);

        if (parsed == NULL)
            parsed = IswParseTranslationTable(menuBarChildTranslations);
        IswOverrideTranslations(child, parsed);
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
MenuBarEnter(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw;

    if (!IswIsSubclass(w, menuButtonWidgetClass))
        return;
    mbw = (MenuBarWidget) IswParent(w);
    if (!IswIsSubclass((Widget)mbw, menuBarWidgetClass))
        return;

    /* Highlight the button on hover */
    IswCallActionProc(w, "highlight", event, NULL, 0);

    /* If a menu is open and we entered a different button, switch */
    if (mbw->menu_bar.menu_is_open && w != mbw->menu_bar.active_button)
        SwitchMenu(mbw, w);
}

/*
 * Called when pointer leaves a MenuButton child.
 * Don't unhighlight the active button while its menu is open.
 */
static void
MenuBarLeave(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw;

    if (!IswIsSubclass(w, menuButtonWidgetClass))
        return;
    mbw = (MenuBarWidget) IswParent(w);
    if (!IswIsSubclass((Widget)mbw, menuBarWidgetClass))
        return;

    /* Keep the active button highlighted while its menu is open */
    if (mbw->menu_bar.menu_is_open && w == mbw->menu_bar.active_button)
        return;

    IswCallActionProc(w, "reset", event, NULL, 0);
}

/*
 * Called when a MenuButton child is clicked.
 * Toggle: if this button's menu is open, close it; otherwise open it.
 */
static void
MenuBarClick(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw;

    if (!IswIsSubclass(w, menuButtonWidgetClass))
        return;
    mbw = (MenuBarWidget) IswParent(w);
    if (!IswIsSubclass((Widget)mbw, menuBarWidgetClass))
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
MenuBarDismiss(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    MenuBarWidget mbw = (MenuBarWidget) w;

    if (!IswIsSubclass(w, menuBarWidgetClass))
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
        menu = IswNameToWidget(temp, mbtn->menu_button.menu_name);
        if (menu == NULL)
            temp = IswParent(temp);
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
    static IswTranslations menu_translations = NULL;

    menu = FindMenuForButton(button);
    if (menu == NULL)
        return;

    /* Remove border from menu */
    {
        Arg flat[1];
        IswSetArg(flat[0], IswNborderWidth, 0);
        IswSetValues(menu, flat, 1);
    }

    if (!IswIsRealized(menu))
        IswRealizeWidget(menu);

    /* Position menu below the button */
    menu_width = menu->core.width + 2 * menu->core.border_width;
    button_height = button->core.height + 2 * button->core.border_width;
    menu_height = menu->core.height + 2 * menu->core.border_width;

    IswTranslateCoords(button, 0, 0, &button_x, &button_y);
    menu_x = button_x;
    menu_y = button_y + button_height;

    /* Clamp to screen edges */
    if (menu_x >= 0) {
        int scr_width = WidthOfScreen(IswScreen(menu));
        if (menu_x + menu_width > scr_width)
            menu_x = scr_width - menu_width;
    }
    if (menu_x < 0)
        menu_x = 0;

    if (menu_y >= 0) {
        int scr_height = HeightOfScreen(IswScreen(menu));
        if (menu_y + menu_height > scr_height)
            menu_y = scr_height - menu_height;
    }
    if (menu_y < 0)
        menu_y = 0;

    num_args = 0;
    IswSetArg(arglist[num_args], IswNx, menu_x); num_args++;
    IswSetArg(arglist[num_args], IswNy, menu_y); num_args++;
    IswSetValues(menu, arglist, num_args);

    /* Override SimpleMenu translations for click-to-select behavior */
    if (menu_translations == NULL)
        menu_translations = IswParseTranslationTable(menuBarMenuTranslations);
    IswOverrideTranslations(menu, menu_translations);

    /* Pop up without grab -- we handle dismissal ourselves */
    IswPopup(menu, IswGrabNone);

    /* X server pointer grab — delivers all button events (scroll, outside
     * clicks) to the menu window. Same technique as GTK/Motif popups. */
    if (IswIsRealized(menu)) {
        xcb_grab_pointer(IswDisplay(menu), True, IswWindow(menu),
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION |
            XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
            XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
        xcb_flush(IswDisplay(menu));
    }

    /* Visually activate the button (inverted/set state) */
    IswCallActionProc(button, "set", NULL, NULL, 0);
    IswCallActionProc(button, "highlight", NULL, (String[]){"Always"}, 1);

    /* Register popdown callback to clean up state */
    IswAddCallback(menu, IswNpopdownCallback, MenuPopdownCB, (IswPointer)mbw);

    /* Install click-outside handler on toplevel shell */
    {
        Widget toplevel = FindToplevelShell((Widget)mbw);
        if (toplevel)
            IswAddEventHandler(toplevel, DISMISS_MASK, False,
                              OutsideClickHandler, (IswPointer)mbw);
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

    xcb_ungrab_pointer(IswDisplay((Widget)mbw), XCB_CURRENT_TIME);
    xcb_flush(IswDisplay((Widget)mbw));

    /* Remove click-outside handler */
    toplevel = FindToplevelShell((Widget)mbw);
    if (toplevel)
        IswRemoveEventHandler(toplevel, DISMISS_MASK, False,
                             OutsideClickHandler, (IswPointer)mbw);

    if (menu) {
        IswRemoveCallback(menu, IswNpopdownCallback, MenuPopdownCB, (IswPointer)mbw);
        IswPopdown(menu);
    }

    /* Reset the button visual state */
    if (button) {
        IswCallActionProc(button, "unset", NULL, NULL, 0);
        IswCallActionProc(button, "unhighlight", NULL, NULL, 0);
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

    xcb_ungrab_pointer(IswDisplay((Widget)mbw), XCB_CURRENT_TIME);
    xcb_flush(IswDisplay((Widget)mbw));

    /* Remove popdown callback and click-outside handler from old menu */
    toplevel = FindToplevelShell((Widget)mbw);
    if (toplevel)
        IswRemoveEventHandler(toplevel, DISMISS_MASK, False,
                             OutsideClickHandler, (IswPointer)mbw);

    if (old_menu) {
        IswRemoveCallback(old_menu, IswNpopdownCallback, MenuPopdownCB, (IswPointer)mbw);
        IswPopdown(old_menu);
    }

    /* Reset old button visual state */
    if (old_button) {
        IswCallActionProc(old_button, "unset", NULL, NULL, 0);
        IswCallActionProc(old_button, "unhighlight", NULL, NULL, 0);
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
MenuPopdownCB(Widget menu, IswPointer client_data, IswPointer call_data)
{
    MenuBarWidget mbw = (MenuBarWidget) client_data;
    Widget button, toplevel;

    if (!mbw->menu_bar.menu_is_open)
        return;

    button = mbw->menu_bar.active_button;

    xcb_ungrab_pointer(IswDisplay((Widget)mbw), XCB_CURRENT_TIME);
    xcb_flush(IswDisplay((Widget)mbw));

    /* Remove click-outside handler */
    toplevel = FindToplevelShell((Widget)mbw);
    if (toplevel)
        IswRemoveEventHandler(toplevel, DISMISS_MASK, False,
                             OutsideClickHandler, (IswPointer)mbw);

    IswRemoveCallback(menu, IswNpopdownCallback, MenuPopdownCB, (IswPointer)mbw);

    /* Reset button visual state */
    if (button) {
        IswCallActionProc(button, "unset", NULL, NULL, 0);
        IswCallActionProc(button, "unhighlight", NULL, NULL, 0);
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
OutsideClickHandler(Widget w, IswPointer client_data, xcb_generic_event_t *event, Boolean *cont)
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
        target = IswWindowToWidget(IswDisplay((Widget)mbw), bev->event);
        if (target == NULL) {
            CloseMenu(mbw);
            return;
        }

        /* If click is on a MenuButton child of this menubar, let MenuBarClick handle it */
        if (IswParent(target) == (Widget)mbw && IswIsSubclass(target, menuButtonWidgetClass))
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
                check = IswParent(check);
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
    while (w != NULL && !IswIsShell(w))
        w = IswParent(w);
    return w;
}
