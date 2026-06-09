/***********************************************************

Copyright (c) 1987, 1988, 1994  X Consortium

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <stdio.h>
#include <ISW/IntrinsicP.h>
#include <ISW/EventI.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/SimpleP.h>
#include "ISWPlatformPrivate.h"

#define offset(field) IswOffsetOf(SimpleRec, simple.field)

static IswResource resources[] = {
  {IswNcursor, IswCCursor, IswRCursor, sizeof(IswCursor),
     offset(cursor), IswRImmediate, (IswPointer) None},
  /* Color cursor resources removed - not available in XCB
  {IswNpointerColor, IswCForeground, IswRPixel, sizeof(Pixel),
     offset(pointer_fg), IswRString, IswDefaultForeground},
  {IswNpointerColorBackground, IswCBackground, IswRPixel, sizeof(Pixel),
     offset(pointer_bg), IswRString, IswDefaultBackground},
  */
  {IswNcursorName, IswCCursor, IswRString, sizeof(String),
     offset(cursor_name), IswRString, NULL},
  {IswNinternational, IswCInternational, IswRBoolean, sizeof(Boolean),
     offset(international), IswRImmediate, (IswPointer) FALSE},
  {IswNtraversalOn, IswCTraversalOn, IswRBoolean, sizeof(Boolean),
     offset(traversal_on), IswRImmediate, (IswPointer) FALSE},
  {IswNtabIndex, IswCTabIndex, IswRInt, sizeof(int),
     offset(tab_index), IswRImmediate, (IswPointer) 0},
#undef offset
};

static void ClassPartInitialize(WidgetClass);
static void ClassInitialize(void);
static void Realize(IswDisplay, Widget, IswValueMask *, uint32_t *);
static void ConvertCursor(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean ChangeSensitive(Widget);

SimpleClassRec simpleClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &widgetClassRec,
    /* class_name		*/	"Simple",
    /* widget_size		*/	sizeof(SimpleRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	ClassPartInitialize,
    /* class_inited		*/	FALSE,
    /* initialize		*/	NULL,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* num_resources		*/	IswNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	NULL,
    /* resize			*/	NULL,
    /* expose			*/	NULL,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	IswInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	IswVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	NULL,
    /* query_geometry		*/	IswInheritQueryGeometry,
    /* display_accelerator	*/	IswInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* simple fields */
    /* change_sensitive		*/	ChangeSensitive,
    /* hit_child		*/	NULL,
    /* nth_windowless_child	*/	NULL
  }
};

WidgetClass simpleWidgetClass = (WidgetClass)&simpleClassRec;

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    /* Color cursor converter removed - not available in XCB
    IswSetTypeConverter( IswRString, IswRColorCursor, XmuCvtStringToColorCursor,
         convertArg, IswNumber(convertArg),
         IswCacheByDisplay, (IswDestructor)NULL);
    */
}

static void
ClassPartInitialize(WidgetClass class)
{
    SimpleWidgetClass c     = (SimpleWidgetClass) class;
    SimpleWidgetClass super = (SimpleWidgetClass)
      c->core_class.superclass;

    if (c->simple_class.change_sensitive == NULL) {
	char buf[BUFSIZ];

	(void) sprintf(buf,
		"%s Widget: The Simple Widget class method 'change_sensitive' is undefined.\nA function must be defined or inherited.",
		c->core_class.class_name);
	IswWarning(buf);
	c->simple_class.change_sensitive = ChangeSensitive;
    }

    if (c->simple_class.change_sensitive == IswInheritChangeSensitive)
	c->simple_class.change_sensitive = super->simple_class.change_sensitive;

    if (c->simple_class.hit_child == IswInheritHitChild)
	c->simple_class.hit_child = super->simple_class.hit_child;

    if (c->simple_class.nth_windowless_child == IswInheritNthWindowlessChild)
	c->simple_class.nth_windowless_child =
	    super->simple_class.nth_windowless_child;
}

static void
Realize(IswDisplay dpy, Widget w, IswValueMask *valueMask, uint32_t *attributes)
{
    ConvertCursor(w);

    /* Windowless widgets have no window to attach a cursor to; the cursor is
       applied to the windowed ancestor's window on pointer-enter (see
       _IswSimpleApplyCursor).  Keep the resolved cursor in simple.cursor. */
    if (!w->core.windowless &&
        ((SimpleWidget)w)->simple.cursor != None &&
        ((SimpleWidget)w)->simple.cursor != (IswCursor)0xffffffff) {
	*valueMask |= XCB_CW_CURSOR;
	attributes[__builtin_popcount(*valueMask & (XCB_CW_CURSOR - 1))] = _IswXcbCursor(((SimpleWidget)w)->simple.cursor);
    }

    IswCreateWindow(IswDisplayOf(w), w, (unsigned int)XCB_WINDOW_CLASS_INPUT_OUTPUT,
                   (IswVisual)CopyFromParent, *valueMask, attributes);
}

/*
 * _IswSetWindowCursor - set the pointer cursor on a windowed target's window.
 * The single owner of the XCB_CW_CURSOR window-attribute change; widgets call
 * this instead of issuing xcb_change_window_attributes directly.
 */
void
_IswSetWindowCursor(Widget anc, IswCursor cursor)
{
    /* Gate on the window itself, not IswIsRealized: a shell sets its cursor
       inside its own realize method, before the realized flag is set. */
    if (anc == NULL || !IswIsWidget(anc) || _IswXcbWindow(anc->core.window) == XCB_NONE ||
        anc->core.being_destroyed)
        return;

    _IswPlatformSetWindowCursor(
        IswDisplayOf(anc), anc->core.window, cursor);
}

/*
 * _IswFreeCursor - release a server cursor allocated for a widget.  The single
 * owner of cursor release for widget code.
 */
void
_IswFreeCursor(Widget w, IswCursor cursor)
{
    if (cursor == None || (xcb_cursor_t) cursor == XCB_CURSOR_NONE)
        return;

    _IswPlatformFreeCursor(IswDisplayOf(w), cursor);
}

/*
 * _IswChangeActivePointerGrabCursor - change the cursor of the active pointer
 * grab.  The single owner of xcb_change_active_pointer_grab for widget code
 * (a narrow grab-cursor refresh that stays on the seam).
 */
void
_IswChangeActivePointerGrabCursor(Widget w, IswCursor cursor,
                                  IswTime time, uint16_t event_mask)
{
    xcb_change_active_pointer_grab(_IswXcbConn(IswDisplayOf(w)),
                                   _IswXcbCursor(cursor),
                                   (xcb_timestamp_t) time, event_mask);
}

/*
 * _IswSimpleApplyCursor - apply the cursor for the windowless widget currently
 * under the pointer onto its windowed ancestor's window.  Called from the
 * event dispatcher when the pointer widget changes.  A NULL widget, or one
 * with no cursor, clears the ancestor back to its own (windowed) cursor.
 */
void
_IswSimpleApplyCursor(Widget w)
{
    Widget anc;
    IswCursor cursor = None;

    if (w == NULL || !IswIsWidget(w))
        return;

    anc = _IswWindowedAncestor(w);
    if (anc == NULL || !IswIsRealized(anc) || anc->core.being_destroyed)
        return;

    /* Cursor of the windowless pointer widget, if it is a Simple subclass. */
    if (IswIsSubclass(w, simpleWidgetClass)) {
        IswCursor c = ((SimpleWidget) w)->simple.cursor;
        if (c != None && c != (IswCursor) 0xffffffff)
            cursor = c;
    }

    /* Fall back to the windowed ancestor's own cursor when the pointer widget
       specifies none, so leaving a widget restores the container cursor. */
    if (cursor == None && IswIsSubclass(anc, simpleWidgetClass)) {
        IswCursor c = ((SimpleWidget) anc)->simple.cursor;
        if (c != None && c != (IswCursor) 0xffffffff)
            cursor = c;
    }

    _IswSetWindowCursor(anc, cursor);
}

/*	Function Name: ConvertCursor
 *	Description: Converts a name to a new cursor.
 *	Arguments: w - the simple widget.
 *	Returns: none.
 */

static void
ConvertCursor(Widget w)
{
    SimpleWidget simple = (SimpleWidget) w;
    XrmValue from, to;
    IswCursor cursor;

    if (simple->simple.cursor_name == NULL)
	return;

    from.addr = (IswPointer) simple->simple.cursor_name;
    from.size = strlen((char *) from.addr) + 1;

    to.size = sizeof(IswCursor);
    to.addr = (IswPointer) &cursor;

    /* Changed IswRColorCursor to IswRCursor for XCB compatibility */
    if (IswConvertAndStore(w, IswRString, &from, IswRCursor, &to)) {
 if ( cursor !=  None)
     simple->simple.cursor = cursor;
    }
    else {
 IswAppErrorMsg(IswWidgetToApplicationContext(w),
        "convertFailed","ConvertCursor","IswError",
        "Simple: ConvertCursor failed.",
        (String *)NULL, (Cardinal *)NULL);
    }
}


/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SimpleWidget s_old = (SimpleWidget) current;
    SimpleWidget s_new = (SimpleWidget) new;
    Boolean new_cursor = FALSE;

    /* this disables user changes after creation*/
    s_new->simple.international = s_old->simple.international;

    if ( IswIsSensitive(current) != IswIsSensitive(new) )
	(*((SimpleWidgetClass)IswClass(new))->
	     simple_class.change_sensitive) ( new );

    if (s_old->simple.cursor != s_new->simple.cursor) {
	new_cursor = TRUE;
    }

/*
 * We are not handling the string cursor_name correctly here.
 */

    if ( (s_old->simple.pointer_fg != s_new->simple.pointer_fg) ||
	(s_old->simple.pointer_bg != s_new->simple.pointer_bg) ||
	(s_old->simple.cursor_name != s_new->simple.cursor_name) ) {
	ConvertCursor(new);
	new_cursor = TRUE;
    }

    if (new_cursor && IswIsRealized(new) && !new->core.windowless)
        _IswSetWindowCursor(new, s_new->simple.cursor);
    /* Windowless: the new cursor is applied to the windowed ancestor on the
       next pointer-enter (_IswSimpleApplyCursor); changing it while the
       pointer is elsewhere must not alter the visible cursor. */

    return False;
}


static Boolean
ChangeSensitive(Widget w)
{
    if (IswIsRealized(w)) {
	if (w->core.windowless)
	    /* Windowless: repaint our own surface and composite the ancestor.
	       xcb_clear_area(IswWindowOf(w)) would blank the shared ancestor. */
	    _IswRepaintWindowless(w);
	else
	    xcb_clear_area(_IswXcbConn(IswDisplayOf(w)), 1, _IswXcbWindow(IswWindowOf(w)), 0, 0, 0, 0);
    }
    return False;
}
