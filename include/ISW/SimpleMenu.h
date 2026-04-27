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
 * Author:  Chris D. Peterson, MIT X Consortium
 */

/*
 * SimpleMenu.h - Public Header file for SimpleMenu widget.
 *
 * This is the public header file for the Athena SimpleMenu widget.
 * It is intended to provide one pane pulldown and popup menus within
 * the framework of the X Toolkit.  As the name implies it is a first and
 * by no means complete implementation of menu code. It does not attempt to
 * fill the needs of all applications, but does allow a resource oriented
 * interface to menus.
 *
 * Date:    April 3, 1989
 *
 * By:      Chris D. Peterson
 *          MIT X Consortium
 *          kit@expo.lcs.mit.edu
 */

#ifndef _ISW_SimpleMenu_h
#define _ISW_SimpleMenu_h

#include <ISW/Shell.h>

/****************************************************************
 *
 * SimpleMenu widget
 *
 ****************************************************************/

/* SimpleMenu Resources:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		Pixel		IswDefaultBackground
 backgroundPixmap    BackgroundPixmap	Pixmap          None
 borderColor	     BorderColor	Pixel		IswDefaultForeground
 borderPixmap	     BorderPixmap	Pixmap		None
 borderWidth	     BorderWidth	Dimension	1
 bottomMargin        VerticalMargins    Dimension       VerticalSpace
 columnWidth         ColumnWidth        Dimension       Width of widest text
 cursor              Cursor             Cursor          None
 destroyCallback     Callback		Pointer		NULL
 height		     Height		Dimension	0
 label               Label              String          NULL (No label)
 labelClass          LabelClass         Pointer         smeBSBObjectClass
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 rowHeight           RowHeight          Dimension       Height of Font
 sensitive	     Sensitive		Boolean		True
 topMargin           VerticalMargins    Dimension       VerticalSpace
 width		     Width		Dimension	0
 x		     Position		Position	0n
 y		     Position		Position	0

*/

typedef struct _SimpleMenuClassRec*	SimpleMenuWidgetClass;
typedef struct _SimpleMenuRec*		SimpleMenuWidget;

extern WidgetClass simpleMenuWidgetClass;

#define IswNcursor "cursor"
#define IswNbottomMargin "bottomMargin"
#define IswNcolumnWidth "columnWidth"
#define IswNlabelClass "labelClass"
#define IswNmenuOnScreen "menuOnScreen"
#define IswNpopupOnEntry "popupOnEntry"
#define IswNrowHeight "rowHeight"
#define IswNtopMargin "topMargin"
#define IswNjumpScroll "jumpScroll"
#define IswNleftWhitespace "leftWhitespace"
#define IswNrightWhitespace "rightWhitespace"

#define IswCColumnWidth "ColumnWidth"
#define IswCLabelClass "LabelClass"
#define IswCMenuOnScreen "MenuOnScreen"
#define IswCPopupOnEntry "PopupOnEntry"
#define IswCRowHeight "RowHeight"
#define IswCVerticalMargins "VerticalMargins"
#define IswCJumpScroll "JumpScroll"
#define IswCLeftWhitespace "LeftWhitespace"
#define IswCRightWhitespace "RightWhitespace"
#define IswCHorizontalWhitespace "HorizontalWhitespace"

/************************************************************
 *
 * Public Functions.
 *
 ************************************************************/

_XFUNCPROTOBEGIN

/*	Function Name: IswSimpleMenuAddGlobalActions
 *	Description: adds the global actions to the simple menu widget.
 *	Arguments: app_con - the appcontext.
 *	Returns: none.
 */

extern void IswSimpleMenuAddGlobalActions(
    IswAppContext	/* app_con */
);

/*	Function Name: IswSimpleMenuGetActiveEntry
 *	Description: Gets the currently active (set) entry.
 *	Arguments: w - the smw widget.
 *	Returns: the currently set entry or NULL if none is set.
 */

extern Widget IswSimpleMenuGetActiveEntry(
    Widget		/* w */
);

/*	Function Name: IswSimpleMenuClearActiveEntry
 *	Description: Unsets the currently active (set) entry.
 *	Arguments: w - the smw widget.
 *	Returns: none.
 */

extern void IswSimpleMenuClearActiveEntry(
    Widget		/* w */
);

/*	Function Name: IswSimpleMenuInstallAccelerators
 *	Description: Builds an accelerator table from SmeBSB children that
 *		     have IswNaccelerator set, and installs it on the
 *		     destination widget so shortcuts work when the menu
 *		     is closed.
 *	Arguments: destination - widget that receives key events.
 *		   menu - the SimpleMenu widget.
 *	Returns: none.
 */

extern void IswSimpleMenuInstallAccelerators(
    Widget		/* destination */,
    Widget		/* menu */
);

_XFUNCPROTOEND

#endif /* _ISW_SimpleMenu_h */
