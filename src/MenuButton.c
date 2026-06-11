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
#include <ISW/LabelP.h>
#include <ISW/ISWRender.h>
#include <ISW/FocusMgrI.h>
#include <ISW/IswArgMacros.h>
#include "ISWPlatformPrivate.h"
#include <math.h>

extern double _IswGetScaleFactor(IswDisplay dpy);

static void ClassInitialize(void);
static void PopupMenu(Widget, IswEvent *, String *, Cardinal *);

#define superclass ((CommandWidgetClass)&commandClassRec)

static char defaultTranslations[] =
"<EnterWindow>: highlight()\n\
 <LeaveWindow>: reset()\n\
 Any<BtnDown>:  reset() PopupMenu()\n\
 <Key>space:    PopupMenu()\n\
 <Key>Return:   PopupMenu()";

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
  {
    IswNmnemonicKey, IswCMnemonicKey, IswRInt, sizeof(IswKeySym),
    offset(menu_button.mnemonic_key), IswRImmediate, (IswPointer) 0},
};
#undef offset

static IswActionsRec actionsList[] =
{
  {"PopupMenu",	PopupMenu}
};

static void Redisplay(Widget, IswEvent *, IswRegion);

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
    Redisplay,                          /* expose		  */
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
			 (unsigned int)(IswButtonPressMask | IswButtonReleaseMask),
			 1, 1);
}

/* Chain to Command's expose, then draw a mnemonic underline if Alt is
 * held and a mnemonic_key is configured. */
static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    MenuButtonWidget mbw = (MenuButtonWidget) w;
    (*superclass->core_class.expose)(w, event, region);

    if (!_IswFocusMgrAltHeld() || mbw->menu_button.mnemonic_key == 0)
        return;
    if (!mbw->label.label) return;

    int idx = _IswFocusMgrFindMnemonicIndex(mbw->label.label,
                                            mbw->menu_button.mnemonic_key);
    if (idx < 0) return;

    ISWRenderContext *ctx = mbw->label.render_ctx;
    if (!ctx) return;

    int x = mbw->label.label_x;
    int baseline = mbw->label.label_y
                 + ISWScaledFontAscent(w, mbw->label.font);

    if (idx > 0)
        x += ISWScaledTextWidth(w, mbw->label.font, mbw->label.label, idx);
    int ul_w = ISWScaledTextWidth(w, mbw->label.font,
                                  &mbw->label.label[idx], 1) - 2;
    if (ul_w < 1) ul_w = 1;

    ISWRenderBegin(ctx);
    ISWRenderSetColor(ctx, mbw->label.foreground);
    ISWRenderDrawLine(ctx, x, baseline + 1, x + ul_w, baseline + 1);
    ISWRenderEnd(ctx);
}

/* Positions this MenuButton's menu under the button and pops it up with
 * a non-exclusive grab. Returns the menu widget or NULL if not found. */
Widget
_IswMenuButtonPopup(Widget w)
{
  MenuButtonWidget mbw = (MenuButtonWidget) w;
  Widget menu = NULL, temp;
  IswArgBuilder ab = IswArgBuilderInit();
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
    return NULL;
  }
  if (!IswIsRealized(menu))
    IswRealizeWidget(menu);

  menu_width = menu->core.width + 2 * menu->core.border_width;
  button_height = w->core.height + 2 * w->core.border_width;
  menu_height = menu->core.height + 2 * menu->core.border_width;

  IswTranslateCoords(w, 0, 0, &button_x, &button_y);
  menu_x = button_x;
  menu_y = button_y + button_height;

  {
    double sf = _IswGetScaleFactor(IswDisplayOf(w));

    if (menu_x >= 0) {
      int scr_width = (int)lrint(_IswPlatformScreenWidth(IswDisplayOf(menu), IswScreenOf(menu)) / sf);
      if (menu_x + menu_width > scr_width)
        menu_x = scr_width - menu_width;
    }
    if (menu_x < 0)
      menu_x = 0;

    if (menu_y >= 0) {
      int scr_height = (int)lrint(_IswPlatformScreenHeight(IswDisplayOf(menu), IswScreenOf(menu)) / sf);
      if (menu_y + menu_height > scr_height)
        menu_y = scr_height - menu_height;
    }
    if (menu_y < 0)
      menu_y = 0;
  }

  IswArgX(&ab, menu_x);
  IswArgY(&ab, menu_y);
  IswSetValues(menu, ab.args, ab.count);

  IswPopup(menu, IswGrabNonexclusive);
  return menu;
}

/* ARGSUSED */
static void
PopupMenu(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    (void)iswev; (void)params; (void)num_params;
    _IswMenuButtonPopup(w);
}

