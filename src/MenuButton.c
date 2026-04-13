/*
Copyright (c) 1989, 1994  X Consortium

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
 *
 */


/***********************************************************************
 *
 * MenuButton Widget
 *
 ***********************************************************************/

/*
 * MenuButton.c - Source code for MenuButton widget.
 *
 * This is the source code for the Athena MenuButton widget.
 * It is intended to provide an easy method of activating pulldown menus.
 *
 * Date:    May 2, 1989
 *
 * By:      Chris D. Peterson
 *          MIT X Consortium
 *          kit@expo.lcs.mit.edu
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>

#include <ISW/ISWInit.h>
#include <ISW/MenuButtoP.h>
#include "ISWXcbDraw.h"

static void ClassInitialize(void);
static void PopupMenu(Widget, xcb_generic_event_t *, String *, Cardinal *);

#define superclass ((CommandWidgetClass)&commandClassRec)

static char defaultTranslations[] =
"<EnterWindow>: highlight()\n\
 <LeaveWindow>: reset()\n\
 Any<BtnDown>:  reset() PopupMenu()";

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

#define offset(field) IswOffsetOf(MenuButtonRec, field)
static IswResource resources[] = {
  {
    IswNmenuName, IswCMenuName, IswRString, sizeof(String),
    offset(menu_button.menu_name), IswRString, (IswPointer)"menu"},
};
#undef offset

static IswActionsRec actionsList[] =
{
  {"PopupMenu",	PopupMenu}
};

MenuButtonClassRec menuButtonClassRec = {
  {
    (WidgetClass) superclass,		/* superclass		  */
    "MenuButton",			/* class_name		  */
    sizeof(MenuButtonRec),       	/* size			  */
    ClassInitialize,			/* class_initialize	  */
    NULL,				/* class_part_initialize  */
    FALSE,				/* class_inited		  */
    NULL,				/* initialize		  */
    NULL,				/* initialize_hook	  */
    IswInheritRealize,			/* realize		  */
    actionsList,			/* actions		  */
    IswNumber(actionsList),		/* num_actions		  */
    resources,				/* resources		  */
    IswNumber(resources),		/* resource_count	  */
    NULLQUARK,				/* xrm_class		  */
    FALSE,				/* compress_motion	  */
    TRUE,				/* compress_exposure	  */
    TRUE,				/* compress_enterleave    */
    FALSE,				/* visible_interest	  */
    NULL,				/* destroy		  */
    IswInheritResize,			/* resize		  */
    IswInheritExpose,			/* expose		  */
    NULL,				/* set_values		  */
    NULL,				/* set_values_hook	  */
    IswInheritSetValuesAlmost,		/* set_values_almost	  */
    NULL,				/* get_values_hook	  */
    NULL,				/* accept_focus		  */
    IswVersion,				/* version		  */
    NULL,				/* callback_private	  */
    defaultTranslations,               	/* tm_table		  */
    IswInheritQueryGeometry,		/* query_geometry	  */
    IswInheritDisplayAccelerator,	/* display_accelerator	  */
    NULL				/* extension		  */
  },  /* CoreClass fields initialization */
  {
    IswInheritChangeSensitive		/* change_sensitive	  */
  },  /* SimpleClass fields initialization */
  {
    0,                                     /* field not used    */
  },  /* LabelClass fields initialization */
  {
    0,                                     /* field not used    */
  },  /* CommandClass fields initialization */
  {
    0,                                     /* field not used    */
  }  /* MenuButtonClass fields initialization */
};

  /* for public consumption */
WidgetClass menuButtonWidgetClass = (WidgetClass) &menuButtonClassRec;

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    IswRegisterGrabAction(PopupMenu, True,
			 (unsigned int)(XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE),
			 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
}

/* ARGSUSED */
static void
PopupMenu(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  MenuButtonWidget mbw = (MenuButtonWidget) w;
  Widget menu = NULL, temp;
  Arg arglist[2];
  Cardinal num_args;
  int menu_x, menu_y, menu_width, menu_height, button_height;
  Position button_x, button_y;

  temp = w;
  while(temp != NULL) {
    menu = IswNameToWidget(temp, mbw->menu_button.menu_name);
    if (menu == NULL)
      temp = IswParent(temp);
    else
      break;
  }

  if (menu == NULL) {
    char error_buf[BUFSIZ];
    (void) sprintf(error_buf, "MenuButton: %s %s.",
	    "Could not find menu widget named", mbw->menu_button.menu_name);
    IswAppWarning(IswWidgetToApplicationContext(w), error_buf);
    return;
  }
  if (!IswIsRealized(menu))
    IswRealizeWidget(menu);

  menu_width = menu->core.width + 2 * menu->core.border_width;
  button_height = w->core.height + 2 * w->core.border_width;
  menu_height = menu->core.height + 2 * menu->core.border_width;

  IswTranslateCoords(w, 0, 0, &button_x, &button_y);
  menu_x = button_x;
  menu_y = button_y + button_height;

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

  IswPopupSpringLoaded(menu);
}

