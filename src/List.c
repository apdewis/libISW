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
*/

/*
 * List.c - List widget
 *
 * This is a List widget.  It allows the user to select an item in a list and
 * notifies the application through a callback function.
 *
 *	Created: 	8/13/88
 *	By:		Chris D. Peterson
 *                      MIT X Consortium
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h>
#include <ISW/ISWInit.h>
#include <ISW/ListP.h>
#include <ISW/FocusMgrI.h>
#include <ISW/ISWRender.h>
#include <ISW/Viewport.h>
#include <ISW/ViewportP.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeBSB.h>
#include <ISW/Shell.h>
#include <ISW/IswArgMacros.h>
#include "ISWXcbDraw.h"
#include <math.h>

extern double _IswGetScaleFactor(xcb_connection_t *dpy);

/* These added so widget knows whether its height, width are user selected.
I also added the freedoms member of the list widget part. */

#define HeightLock  1
#define WidthLock   2
#define LongestLock 4

#define HeightFree( w )  !(((ListWidget)(w))->list.freedoms & HeightLock )
#define WidthFree( w )   !(((ListWidget)(w))->list.freedoms & WidthLock )
#define LongestFree( w ) !(((ListWidget)(w))->list.freedoms & LongestLock )

/*
 * Default Translation table.
 */

static char defaultTranslations[] =
  "<Btn1Down>:   Set()\n\
   <Btn1Up>:     Notify()\n\
   <Key>Up:        PrevItem()\n\
   <Key>Down:      NextItem()\n\
   <Key>Home:      FirstItem()\n\
   <Key>End:       LastItem()\n\
   <Key>Page_Up:   PageBackward()\n\
   <Key>Page_Down: PageForward()\n\
   <Key>Return:    Activate()\n\
   <Key>space:     Activate()";

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

#define offset(field) IswOffset(ListWidget, field)

static IswResource resources[] = {
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
	offset(list.foreground), IswRString, IswDefaultForeground},
    {IswNcursor, IswCCursor, IswRCursor, sizeof(xcb_cursor_t),
       offset(simple.cursor), IswRString, (IswPointer)"left_ptr"},
    {IswNfont,  IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
	offset(list.font),IswRString, IswDefaultFont},
    {IswNlist, IswCList, IswRPointer, sizeof(char **),
       offset(list.list), IswRString, NULL},
    {IswNdefaultColumns, IswCColumns, IswRInt,  sizeof(int),
	offset(list.default_cols), IswRImmediate, (IswPointer)2},
    {IswNlongest, IswCLongest, IswRInt,  sizeof(int),
	offset(list.longest), IswRImmediate, (IswPointer)0},
    {IswNnumberStrings, IswCNumberStrings, IswRInt,  sizeof(int),
	offset(list.nitems), IswRImmediate, (IswPointer)0},
    {IswNpasteBuffer, IswCBoolean, IswRBoolean,  sizeof(Boolean),
	offset(list.paste), IswRImmediate, (IswPointer) False},
    {IswNforceColumns, IswCColumns, IswRBoolean,  sizeof(Boolean),
	offset(list.force_cols), IswRImmediate, (IswPointer) False},
    {IswNverticalList, IswCBoolean, IswRBoolean,  sizeof(Boolean),
	offset(list.vertical_cols), IswRImmediate, (IswPointer) False},
    {IswNinternalWidth, IswCWidth, IswRDimension,  sizeof(Dimension),
	offset(list.internal_width), IswRImmediate, (IswPointer)2},
    {IswNinternalHeight, IswCHeight, IswRDimension, sizeof(Dimension),
	offset(list.internal_height), IswRImmediate, (IswPointer)2},
    {IswNcolumnSpacing, IswCSpacing, IswRDimension,  sizeof(Dimension),
	offset(list.column_space), IswRImmediate, (IswPointer)6},
    {IswNrowSpacing, IswCSpacing, IswRDimension,  sizeof(Dimension),
	offset(list.row_space), IswRImmediate, (IswPointer)2},
    {IswNcallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        offset(list.callback), IswRCallback, NULL},
    {IswNdropdownMode, IswCDropdownMode, IswRBoolean, sizeof(Boolean),
	offset(list.dropdown), IswRImmediate, (IswPointer) False},
};

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void ChangeSize(Widget, Dimension, Dimension);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Destroy(Widget);
static Boolean Layout(Widget, Boolean, Boolean, Dimension *, Dimension *);
static IswGeometryResult PreferredGeom(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void Notify(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Set(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Unset(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void PrevItem(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void NextItem(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void FirstItem(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void LastItem(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void PageForward(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void PageBackward(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Activate(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void DropdownMenuSelect(Widget, IswPointer, IswPointer);
static void DropdownPopdownCB(Widget, IswPointer, IswPointer);
static void DropdownDismissHandler(Widget, IswPointer, xcb_generic_event_t *, Boolean *);

static IswActionsRec actions[] = {
      {"Notify",         Notify},
      {"Set",            Set},
      {"Unset",          Unset},
      {"PrevItem",       PrevItem},
      {"NextItem",       NextItem},
      {"FirstItem",      FirstItem},
      {"LastItem",       LastItem},
      {"PageForward",    PageForward},
      {"PageBackward",   PageBackward},
      {"Activate",       Activate},
};

ListClassRec listClassRec = {
  {
/* core_class fields */
    /* superclass	  	*/	(WidgetClass) &simpleClassRec,
    /* class_name	  	*/	"List",
    /* widget_size	  	*/	sizeof(ListRec),
    /* class_initialize   	*/	IswInitializeWidgetSet,
    /* class_part_initialize	*/	NULL,
    /* class_inited       	*/	FALSE,
    /* initialize	  	*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize		  	*/	IswInheritRealize,
    /* actions		  	*/	actions,
    /* num_actions	  	*/	IswNumber(actions),
    /* resources	  	*/	resources,
    /* num_resources	  	*/	IswNumber(resources),
    /* xrm_class	  	*/	NULLQUARK,
    /* compress_motion	  	*/	TRUE,
    /* compress_exposure  	*/	FALSE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest	  	*/	FALSE,
    /* destroy		  	*/	Destroy,
    /* resize		  	*/	Resize,
    /* expose		  	*/	Redisplay,
    /* set_values	  	*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	IswInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus	 	*/	NULL,
    /* version			*/	IswVersion,
    /* callback_private   	*/	NULL,
    /* tm_table		   	*/	defaultTranslations,
   /* query_geometry		*/      PreferredGeom,
  },
/* Simple class fields initialization */
  {
    /* change_sensitive		*/	IswInheritChangeSensitive
  },
/* List class fields initialization */
  {
    /* not used			*/	0
  },
};

WidgetClass listWidgetClass = (WidgetClass)&listClassRec;

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

/* CalculatedValues()
 *
 * does routine checks/computations that must be done after data changes
 * but won't hurt if accidently called
 *
 * These calculations were needed in SetValues.  They were in ResetList.
 * ResetList called ChangeSize, which made an IswGeometryRequest.  You
 * MAY NOT change your geometry from within a SetValues. (Xt man,
 * sect. 9.7.2)  So, I factored these changes out. */

static void
CalculatedValues(Widget w)
{
    int i, len;

    ListWidget lw = (ListWidget) w;

    /* If list is NULL then the list will just be the name of the widget. */

    if (lw->list.list == NULL) {
      lw->list.list = &(lw->core.name);
      lw->list.nitems = 1;
    }

    /* Get number of items. */

    if (lw->list.nitems == 0)
        for ( ; lw->list.list[lw->list.nitems] != NULL ; lw->list.nitems++);

    /* Get column width. */

    if ( LongestFree( lw ) )  {

        lw->list.longest = 0; /* so it will accumulate real longest below */

        for ( i = 0 ; i < lw->list.nitems; i++)  {
            len = ISWScaledTextWidth((Widget)lw, lw->list.font,
                                    lw->list.list[i],
                                    strlen(lw->list.list[i]));
            if (len > lw->list.longest)
                lw->list.longest = len;
        }
    }

    lw->list.col_width = lw->list.longest + lw->list.column_space;
}

/*	Function Name: ResetList
 *	Description: Resets the new list when important things change.
 *	Arguments: w - the widget.
 *                 changex, changey - allow the height or width to change?
 *
 *	Returns: TRUE if width or height have been changed
 */

static void
ResetList(Widget w, Boolean changex, Boolean changey)
{
    Dimension width = w->core.width;
    Dimension height = w->core.height;

    CalculatedValues( w );

    if( Layout( w, changex, changey, &width, &height ) )
      ChangeSize( w, width, height );
}

/*	Function Name: ChangeSize.
 *	Description: Laysout the widget.
 *	Arguments: w - the widget to try change the size of.
 *	Returns: none.
 */

static void
ChangeSize(Widget w, Dimension width, Dimension height)
{
    IswWidgetGeometry request, reply;

    request.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    request.width = width;
    request.height = height;

    switch ( IswMakeGeometryRequest(w, &request, &reply) ) {
    case IswGeometryYes:
        break;
    case IswGeometryNo:
        break;
    case IswGeometryAlmost:
	Layout(w, (request.height != reply.height),
	          (request.width != reply.width),
	       &(reply.width), &(reply.height));
	request = reply;
	switch (IswMakeGeometryRequest(w, &request, &reply) ) {
	case IswGeometryYes:
	case IswGeometryNo:
	    break;
	case IswGeometryAlmost:
	    request = reply;
	    Layout(w, FALSE, FALSE, &(request.width), &(request.height));
	    request.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
	    IswMakeGeometryRequest(w, &request, &reply);
	    break;
	default:
	  IswAppWarning(IswWidgetToApplicationContext(w),
		       "List Widget: Unknown geometry return.");
	  break;
	}
	break;
    default:
	IswAppWarning(IswWidgetToApplicationContext(w),
		     "List Widget: Unknown geometry return.");
	break;
    }
}

/*	Function Name: Initialize
 *	Description: Function that initilizes the widget instance.
 *	Arguments: junk - NOT USED.
 *                 new  - the new widget.
 *	Returns: none
 */

/* ARGSUSED */
static void
Initialize(Widget junk, Widget new, ArgList args, Cardinal *num_args)
{
    ListWidget lw = (ListWidget) new;

    /* Opt into Tab traversal. */
    ((SimpleWidget) new)->simple.traversal_on = True;

    /* HiDPI: scale dimension resources */
    lw->list.internal_width = (lw->list.internal_width);
    lw->list.internal_height = (lw->list.internal_height);
    lw->list.row_space = (lw->list.row_space);
    lw->list.column_space = (lw->list.column_space);

    lw->list.clip_contents = NULL;

/*
 * Initialize all private resources.
 */

    if (lw->list.font == NULL) {
	IswAppWarning(IswWidgetToApplicationContext(new),
		     "List widget: font is NULL - text rendering will fail");
    }

    /* record for posterity if we are free */
    lw->list.freedoms = (lw->core.width != 0) * WidthLock +
                        (lw->core.height != 0) * HeightLock +
                        (lw->list.longest != 0) * LongestLock;

    /* Set row height using Cairo-matched metrics for correct HiDPI sizing */
    lw->list.row_height = ISWScaledFontHeight(new, lw->list.font)
                          + lw->list.row_space;

    ResetList( new, WidthFree( lw ), HeightFree( lw ) );

    lw->list.highlight = lw->list.is_highlighted = NO_HIGHLIGHT;

    /* Initialize Cairo rendering context */
    lw->list.render_ctx = NULL;

    /* Dropdown mode initialization */
    lw->list.selected_item = 0;
    lw->list.collapsed_height = 0;
    lw->list.popup_shell = NULL;

    if (lw->list.dropdown && lw->list.nitems > 0) {
        /* Save collapsed height: one row + borders */
        lw->list.collapsed_height = lw->list.row_height
            + 2 * lw->list.internal_height;
        /* Collapse to single row */
        lw->core.height = lw->list.collapsed_height;
    }

} /* Initialize */

/*	Function Name: CvtToItem
 *	Description: Converts Xcoord to item number of item containing that
 *                   point.
 *	Arguments: w - the list widget.
 *                 xloc, yloc - x location, and y location.
 *	Returns: the item number.
 */

static int
CvtToItem(Widget w, int xloc, int yloc, int *item)
{
    int one, another;
    ListWidget lw = (ListWidget) w;
    int ret_val = OKAY;

    if (lw->list.vertical_cols) {
        one = lw->list.nrows * ((xloc - (int) lw->list.internal_width)
	    / lw->list.col_width);
        another = (yloc - (int) lw->list.internal_height)
	        / lw->list.row_height;
	 /* If out of range, return minimum possible value. */
	if (another >= lw->list.nrows) {
	    another = lw->list.nrows - 1;
	    ret_val = OUT_OF_RANGE;
	}
    }
    else {
        one = (lw->list.ncols * ((yloc - (int) lw->list.internal_height)
              / lw->list.row_height)) ;
	/* If in right margin handle things right. */
        another = (xloc - (int) lw->list.internal_width) / lw->list.col_width;
	if (another >= lw->list.ncols) {
	    another = lw->list.ncols - 1;
	    ret_val = OUT_OF_RANGE;
	}
    }
    if ((xloc < 0) || (yloc < 0))
        ret_val = OUT_OF_RANGE;
    if (one < 0) one = 0;
    if (another < 0) another = 0;
    *item = one + another;
    if (*item >= lw->list.nitems) return(OUT_OF_RANGE);
    return(ret_val);
}

/*	Function Name: ItemInRectangle
 *	Description: returns TRUE if the item passed is in the given rectangle.
 *	Arguments: w - the list widget.
 *                 ul, lr - corners of the rectangle in item space.
 *                 item - item to check.
 *	Returns: TRUE if the item passed is in the given rectangle.
 */

static Boolean
ItemInRectangle(Widget w, int ul, int lr, int item)
{
    ListWidget lw = (ListWidget) w;
    int mod_item;
    int things;

    if (item < ul || item > lr)
        return(FALSE);
    if (lw->list.vertical_cols)
        things = lw->list.nrows;
    else
        things = lw->list.ncols;

    mod_item = item % things;
    if ( (mod_item >= ul % things) && (mod_item <= lr % things ) )
        return(TRUE);
    return(FALSE);
}


/* HighlightBackground()
 *
 * Paints the color of the background for the given item.  It performs
 * clipping to the interior of internal_width/height by hand, as its a
 * simple calculation and probably much faster than using Xlib and a clip mask.
 *
 *  x, y - ul corner of the area item occupies.
 *  color - the pixel color to fill the background with */

static void
HighlightBackground(Widget w, int x, int y, Pixel color)
{
    ListWidget lw = (ListWidget) w;

    /* easy to clip the rectangle by hand and probably alot faster than Xlib */

    Dimension width               = (lw->list.ncols <= 1)
                                    ? (w->core.width - x)
                                    : lw->list.col_width;
    Dimension height              = lw->list.row_height;
    Dimension frame_limited_width = w->core.width - lw->list.internal_width - x;
    Dimension frame_limited_height= w->core.height- lw->list.internal_height- y;

    /* Clip the rectangle width and height to the edge of the drawable area */

    if  ( width > frame_limited_width )
        width = frame_limited_width;
    if  ( height> frame_limited_height)
        height = frame_limited_height;

    /* Clip the rectangle x and y to the edge of the drawable area */

    if ( x < lw->list.internal_width ) {
        width = width - ( lw->list.internal_width - x );
        x = lw->list.internal_width;
    }
    if ( y < lw->list.internal_height) {
        height = height - ( lw->list.internal_height - y );
        y = lw->list.internal_height;
    }

    ISWRenderSetColor(lw->list.render_ctx, color);
    ISWRenderFillRectangle(lw->list.render_ctx, x, y, width, height);
}


/*  PaintItemName()
 *
 *  paints the name of the item in the appropriate location.
 *  w - the list widget.
 *  item - the item to draw.
 *
 *  NOTE: no action taken on an unrealized widget. */

static void
PaintItemName(Widget w, int item)
{
    const char * str;
    Boolean is_highlighted;
    int x, y, str_y;
    ListWidget lw = (ListWidget) w;

    if (!IswIsRealized(w)) return; /* Just in case... */

    if (lw->list.vertical_cols) {
	x = lw->list.col_width * (item / lw->list.nrows)
	  + lw->list.internal_width;
        y = lw->list.row_height * (item % lw->list.nrows)
	  + lw->list.internal_height;
    }
    else {
        x = lw->list.col_width * (item % lw->list.ncols)
	  + lw->list.internal_width;
        y = lw->list.row_height * (item / lw->list.ncols)
	  + lw->list.internal_height;
    }

    str_y = y + ISWScaledFontAscent(w, lw->list.font);

    if (item == lw->list.is_highlighted) {
        if (item == lw->list.highlight) {
            is_highlighted = True;
            HighlightBackground(w, x, y, lw->list.foreground);
        }
        else {
            is_highlighted = False;
            HighlightBackground(w, x, y, lw->core.background_pixel);
            lw->list.is_highlighted = NO_HIGHLIGHT;
        }
    }
    else {
        if (item == lw->list.highlight) {
            is_highlighted = True;
            HighlightBackground(w, x, y, lw->list.foreground);
            lw->list.is_highlighted = item;
        }
        else {
            is_highlighted = False;
            HighlightBackground(w, x, y, lw->core.background_pixel);
        }
    }

    /* List's overall width contains the same number of inter-column
    column_space's as columns.  There should thus be a half
    column_width margin on each side of each column.
    The row case is symmetric. */

    x     += lw->list.column_space / 2;
    str_y += lw->list.row_space    / 2;

    str =  lw->list.list[item];	/* draw it */

    /* Use Cairo rendering for text if available */
    if (lw->list.render_ctx) {
        /* Set clip for Cairo */
        xcb_rectangle_t clip_rect;
        clip_rect.x = x;
        clip_rect.y = lw->list.internal_height;
        clip_rect.height = lw->core.height - lw->list.internal_height * 2;
        clip_rect.width = lw->core.width - lw->list.internal_width - x;
        if (clip_rect.width > lw->list.longest)
            clip_rect.width = lw->list.longest;

        ISWRenderSetClipRectangle(lw->list.render_ctx,
                                  clip_rect.x, clip_rect.y,
                                  clip_rect.width, clip_rect.height);

        /* Determine text color: highlighted items swap fg/bg */
        Pixel text_color;
        if (is_highlighted)
            text_color = lw->core.background_pixel;
        else
            text_color = lw->list.foreground;

        ISWRenderSetColor(lw->list.render_ctx, text_color);
        if (lw->list.font)
            ISWRenderSetFont(lw->list.render_ctx, lw->list.font);
        ISWRenderDrawString(lw->list.render_ctx, str, strlen(str), x, str_y);

        ISWRenderClearClip(lw->list.render_ctx);
    }
}


/* Redisplay()
 *
 * Repaints the widget window on expose events.
 * w - the list widget.
 * event - the expose event for this repaint.
 * junk - not used, unless three-d patch enabled. */

/* ARGSUSED */
static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    int item;			/* an item to work with. */
    int ul_item, lr_item;       /* corners of items we need to paint. */
    ListWidget lw = (ListWidget) w;
    (void)region;

    /* Create render context if needed (lazy initialization) */
    if (!lw->list.render_ctx && IswIsRealized(w)) {
        if (w->core.width > 0 && w->core.height > 0) {
            lw->list.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
        }
    }

    /* Begin Cairo rendering if available */
    if (lw->list.render_ctx) {
        ISWRenderBegin(lw->list.render_ctx);
    }

    /* Always repaint all items: the Cairo back buffer is cleared above on
     * every expose, so limiting paint to the expose rectangle would leave
     * items outside it blank. */
    ul_item = 0;
    lr_item = lw->list.nrows * lw->list.ncols - 1;
    (void)event;

    /* Always fill background: the Cairo back buffer persists across frames,
     * so partial expose-driven repaints would otherwise leave stale content
     * (e.g. a dismissed focus ring) visible. */
    if (lw->list.render_ctx) {
        ISWRenderSetColor(lw->list.render_ctx, w->core.background_pixel);
        ISWRenderFillRectangle(lw->list.render_ctx, 0, 0,
                               w->core.width, w->core.height);
    }

    /* Dropdown collapsed: only paint the selected item */
    if (lw->list.dropdown) {
        if (lw->list.selected_item >= 0 &&
            lw->list.selected_item < lw->list.nitems) {
            /* Draw selected item at row 0 position */
            int sel = lw->list.selected_item;
            int str_y;
            int x = lw->list.internal_width;
            int y = lw->list.internal_height;
            String str = lw->list.list[sel];

            str_y = y + ISWScaledFontAscent(w, lw->list.font);
            x += lw->list.column_space / 2;
            str_y += lw->list.row_space / 2;

            if (lw->list.render_ctx) {
                ISWRenderSetColor(lw->list.render_ctx, lw->list.foreground);
                if (lw->list.font)
                    ISWRenderSetFont(lw->list.render_ctx, lw->list.font);
                ISWRenderDrawString(lw->list.render_ctx, str, strlen(str),
                                    x, str_y);

                /* Draw dropdown arrow indicator on the right */
                int arrow_size = lw->list.row_height / 3;
                int ax = w->core.width - lw->list.internal_width - arrow_size * 2;
                int ay = (w->core.height - arrow_size) / 2;
                ISWRenderSetColor(lw->list.render_ctx, lw->list.foreground);
                ISWRenderDrawLine(lw->list.render_ctx,
                                  ax, ay, ax + arrow_size, ay + arrow_size);
                ISWRenderDrawLine(lw->list.render_ctx,
                                  ax + arrow_size, ay + arrow_size,
                                  ax + arrow_size * 2, ay);
            }
        }
    } else {
        for (item = ul_item; (item <= lr_item && item < lw->list.nitems) ; item++)
          if (ItemInRectangle(w, ul_item, lr_item, item))
            PaintItemName(w, item);
    }

    /* Focus ring overlay (drawn before End so it's part of the same frame). */
    if (lw->list.render_ctx)
        _IswFocusMgrDrawRing(w, lw->list.render_ctx, lw->list.foreground, 2.0);

    /* End Cairo rendering if available */
    if (lw->list.render_ctx) {
        ISWRenderEnd(lw->list.render_ctx);
    }
}


/* PreferredGeom()
 *
 * This tells the parent what size we would like to be
 * given certain constraints.
 * w - the widget.
 * intended - what the parent intends to do with us.
 * requested - what we want to happen. */

static IswGeometryResult
PreferredGeom(Widget w, IswWidgetGeometry *intended, IswWidgetGeometry *requested)
{
    Dimension new_width, new_height;
    Boolean change, width_req, height_req;

    width_req = intended->request_mode & XCB_CONFIG_WINDOW_WIDTH;
    height_req = intended->request_mode & XCB_CONFIG_WINDOW_HEIGHT;

    if (width_req)
      new_width = intended->width;
    else
      new_width = w->core.width;

    if (height_req)
      new_height = intended->height;
    else
      new_height = w->core.height;

    requested->request_mode = 0;

/*
 * We only care about our height and width.
 */

    if ( !width_req && !height_req)
      return(IswGeometryYes);

    change = Layout(w, !width_req, !height_req, &new_width, &new_height);

    requested->request_mode |= XCB_CONFIG_WINDOW_WIDTH;
    requested->width = new_width;
    requested->request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
    requested->height = new_height;

    if (change)
        return(IswGeometryAlmost);
    return(IswGeometryYes);
}


/* Resize()
 *
 * resizes the widget, by changing the number of rows and columns. */

static void
Resize(Widget w)
{
    Dimension width, height;

    width = w->core.width;
    height = w->core.height;

    if (Layout(w, FALSE, FALSE, &width, &height))
 IswAppWarning(IswWidgetToApplicationContext(w),
    "List Widget: Size changed when it shouldn't have when resising.");

}


/* Layout()
 *
 * lays out the item in the list.
 * w - the widget.
 * xfree, yfree - TRUE if we are free to resize the widget in
 *                this direction.
 * width, height- the is the current width and height that we are going
 *                we are going to layout the list widget to,
 *                depending on xfree and yfree of course.
 *
 * RETURNS: TRUE if width or height have been changed. */

static Boolean
Layout(Widget w, Boolean xfree, Boolean yfree, Dimension *width, Dimension *height)
{
    ListWidget lw = (ListWidget) w;
    Boolean change = FALSE;

/*
 * If force columns is set then always use number of columns specified
 * by default_cols.
 */

    if (lw->list.force_cols) {
        lw->list.ncols = lw->list.default_cols;
	if (lw->list.ncols <= 0) lw->list.ncols = 1;
	/* 12/3 = 4 and 10/3 = 4, but 9/3 = 3 */
	lw->list.nrows = ( ( lw->list.nitems - 1) / lw->list.ncols) + 1 ;
	if (xfree) {		/* If allowed resize width. */

            /* this counts the same number
            of inter-column column_space 's as columns.  There should thus be a
            half column_space margin on each side of each column...*/

	    *width = lw->list.ncols * lw->list.col_width
	           + 2 * lw->list.internal_width;
	    change = TRUE;
	}
	if (yfree) {		/* If allowed resize height. */
	    *height = (lw->list.nrows * lw->list.row_height)
                    + 2 * lw->list.internal_height;
	    change = TRUE;
	}
	return(change);
    }

/*
 * If both width and height are free to change the use default_cols
 * to determine the number columns and set new width and height to
 * just fit the window.
 */

    if (xfree && yfree) {
        lw->list.ncols = lw->list.default_cols;
	if (lw->list.ncols <= 0) lw->list.ncols = 1;
	lw->list.nrows = ( ( lw->list.nitems - 1) / lw->list.ncols) + 1 ;
        *width = lw->list.ncols * lw->list.col_width
	       + 2 * lw->list.internal_width;
	*height = (lw->list.nrows * lw->list.row_height)
                + 2 * lw->list.internal_height;
	change = TRUE;
    }
/*
 * If the width is fixed then use it to determine the number of columns.
 * If the height is free to move (width still fixed) then resize the height
 * of the widget to fit the current list exactly.
 */
    else if (!xfree) {
        lw->list.ncols = ( (int)(*width - 2 * lw->list.internal_width)
	                    / (int)lw->list.col_width);
	if (lw->list.ncols <= 0) lw->list.ncols = 1;
	lw->list.nrows = ( ( lw->list.nitems - 1) / lw->list.ncols) + 1 ;
	if ( yfree ) {
  	    *height = (lw->list.nrows * lw->list.row_height)
		    + 2 * lw->list.internal_height;
	    change = TRUE;
	}
    }
/*
 * The last case is xfree and !yfree we use the height to determine
 * the number of rows and then set the width to just fit the resulting
 * number of columns.
 */
    else if (!yfree) {		/* xfree must be TRUE. */
        lw->list.nrows = (int)(*height - 2 * lw->list.internal_height)
	                 / (int)lw->list.row_height;
	if (lw->list.nrows <= 0) lw->list.nrows = 1;
	lw->list.ncols = (( lw->list.nitems - 1 ) / lw->list.nrows) + 1;
	*width = lw->list.ncols * lw->list.col_width
	       + 2 * lw->list.internal_width;
	change = TRUE;
    }
    return(change);
}


static Boolean
ListConvertSelection(Widget w, xcb_atom_t *selection, xcb_atom_t *target,
		     xcb_atom_t *type, IswPointer *value,
		     unsigned long *length, int *format)
{
    ListWidget lw = (ListWidget) w;

    if (*target == XCB_ATOM_TARGETS(IswDisplay(w))) {
	xcb_atom_t *targets = (xcb_atom_t *) IswMalloc(2 * sizeof(xcb_atom_t));
	targets[0] = XCB_ATOM_TARGETS(IswDisplay(w));
	targets[1] = XCB_ATOM_STRING;
	*type = XCB_ATOM_ATOM;
	*value = (IswPointer) targets;
	*length = 2;
	*format = 32;
	return True;
    }

    if (*target == XCB_ATOM_STRING) {
	if (lw->list.clip_contents == NULL)
	    return False;
	*type = XCB_ATOM_STRING;
	*value = IswNewString(lw->list.clip_contents);
	*length = strlen(lw->list.clip_contents);
	*format = 8;
	return True;
    }

    return False;
}

static void
ListLoseSelection(Widget w, xcb_atom_t *selection)
{
    ListWidget lw = (ListWidget) w;
    if (lw->list.clip_contents) {
	IswFree(lw->list.clip_contents);
	lw->list.clip_contents = NULL;
    }
}

/* Notify() - ACTION
 *
 * Notifies the user that a button has been pressed, and
 * calls the callback; if the IswNpasteBuffer resource is true
 * then the name of the item is placed on the CLIPBOARD selection. */

/* ARGSUSED */
static void
Notify(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListWidget lw = ( ListWidget ) w;
    int item;
    IswListReturnStruct ret_value;
    /* XCB: Cast to xcb_button_press_event_t */
    xcb_button_press_event_t *be = (xcb_button_press_event_t *)event;

/*
 * Find item and if out of range then unhighlight and return.
 *
 * If the current item is unhighlighted then the user has aborted the
 * notify, so unhighlight and return.
 */

    if ( ((CvtToItem(w, be->event_x, be->event_y, &item))
	  == OUT_OF_RANGE) || (lw->list.highlight != item) ) {
        IswListUnhighlight(w);
        return;
    }

    if ( lw->list.paste ) {
	if (lw->list.clip_contents)
	    IswFree(lw->list.clip_contents);
	lw->list.clip_contents = IswNewString(lw->list.list[item]);
	IswOwnSelection(w, XCB_ATOM_CLIPBOARD(IswDisplay(w)),
			IswLastTimestampProcessed(IswDisplay(w)),
			ListConvertSelection, ListLoseSelection, NULL);
    }

/*
 * Call Callback function.
 */

    ret_value.string = lw->list.list[item];
    ret_value.list_index = item;

    IswCallCallbacks( w, IswNcallback, (IswPointer) &ret_value);
}


/* Unset() - ACTION
 *
 * unhighlights the current element. */

/* ARGSUSED */
static void
Unset(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  IswListUnhighlight(w);
}

static int
PageStep(ListWidget lw)
{
    int s = lw->list.vertical_cols ? lw->list.ncols : lw->list.nrows;
    return s > 0 ? s : 1;
}

/* Returns True if we handled the key by opening the dropdown instead of
 * moving the cursor. In dropdown mode the closed widget is just a
 * collapsed selector, so arrows should pop the menu. */
static Boolean
KeyOpensDropdown(Widget w, xcb_generic_event_t *event)
{
    ListWidget lw = (ListWidget) w;
    if (!lw->list.dropdown) return False;
    if (lw->list.popup_shell != NULL) return False;
    String params[1] = {NULL};
    Cardinal nparams = 0;
    Set(w, event, params, &nparams);
    return True;
}

static void
ScrollToItem(Widget w, int item)
{
    ListWidget lw = (ListWidget) w;
    Widget p, viewport;
    int item_y;

    if (lw->list.row_height <= 0) return;

    if (lw->list.vertical_cols)
        item_y = lw->list.row_height * (item % lw->list.nrows)
                 + lw->list.internal_height;
    else
        item_y = lw->list.row_height * (item / lw->list.ncols)
                 + lw->list.internal_height;

    for (p = IswParent(w); p != NULL; p = IswParent(p)) {
        if (IswIsSubclass(p, viewportWidgetClass))
            break;
    }
    if (p == NULL) return;
    viewport = p;

    ViewportWidget vw = (ViewportWidget) viewport;
    Widget clip = vw->viewport.clip;
    Widget child = vw->viewport.child;
    if (child == NULL || clip == NULL) return;

    int abs_y = item_y;
    for (Widget a = w; a != child && a != NULL; a = IswParent(a))
        abs_y += a->core.y;

    int scroll_y = -(child->core.y);
    int clip_h = (int)clip->core.height;
    int item_h = lw->list.row_height;

    if (abs_y >= scroll_y && abs_y + item_h <= scroll_y + clip_h)
        return;

    int new_y;
    if (abs_y < scroll_y)
        new_y = abs_y;
    else
        new_y = abs_y + item_h - clip_h;

    IswViewportSetCoordinates(viewport, -(child->core.x), (Position)new_y);
}

static void
MoveCursor(Widget w, int new_index)
{
    ListWidget lw = (ListWidget) w;
    if (lw->list.nitems <= 0) return;
    if (new_index < 0) new_index = 0;
    if (new_index >= lw->list.nitems) new_index = lw->list.nitems - 1;
    if (new_index == lw->list.highlight) return;
    IswListHighlight(w, new_index);
    ScrollToItem(w, new_index);
}

static void
PrevItem(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ListWidget lw = (ListWidget) w;
    int cur;
    (void)p; (void)np;
    if (KeyOpensDropdown(w, e)) return;
    cur = (lw->list.highlight == NO_HIGHLIGHT) ? 0 : lw->list.highlight - 1;
    MoveCursor(w, cur);
}

static void
NextItem(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ListWidget lw = (ListWidget) w;
    int cur;
    (void)p; (void)np;
    if (KeyOpensDropdown(w, e)) return;
    cur = (lw->list.highlight == NO_HIGHLIGHT) ? 0 : lw->list.highlight + 1;
    MoveCursor(w, cur);
}

static void
FirstItem(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    (void)p; (void)np;
    if (KeyOpensDropdown(w, e)) return;
    MoveCursor(w, 0);
}

static void
LastItem(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ListWidget lw = (ListWidget) w;
    (void)p; (void)np;
    if (KeyOpensDropdown(w, e)) return;
    MoveCursor(w, lw->list.nitems - 1);
}

static void
PageForward(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ListWidget lw = (ListWidget) w;
    int cur;
    (void)p; (void)np;
    if (KeyOpensDropdown(w, e)) return;
    cur = (lw->list.highlight == NO_HIGHLIGHT) ? 0 : lw->list.highlight;
    MoveCursor(w, cur + PageStep(lw));
}

static void
PageBackward(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ListWidget lw = (ListWidget) w;
    int cur;
    (void)p; (void)np;
    if (KeyOpensDropdown(w, e)) return;
    cur = (lw->list.highlight == NO_HIGHLIGHT) ? 0 : lw->list.highlight;
    MoveCursor(w, cur - PageStep(lw));
}

static void
Activate(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ListWidget lw = (ListWidget) w;
    IswListReturnStruct ret_value;
    (void)p; (void)np;
    if (KeyOpensDropdown(w, e)) return;
    if (lw->list.highlight == NO_HIGHLIGHT || lw->list.nitems <= 0) return;
    ret_value.string     = lw->list.list[lw->list.highlight];
    ret_value.list_index = lw->list.highlight;
    IswCallCallbacks(w, IswNcallback, (IswPointer)&ret_value);
}


/* Set() - ACTION
 *
 * Highlights the current element. */

/* ARGSUSED */
static void
Set(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  int item;
  ListWidget lw = (ListWidget) w;
  /* XCB: Cast to xcb_button_press_event_t */
  xcb_button_press_event_t *be = (xcb_button_press_event_t *)event;

  /* Dropdown mode: clicking the collapsed widget opens a popup menu */
  if (lw->list.dropdown) {
    Position abs_x, abs_y;
    IswArgBuilder ab = IswArgBuilderInit();
    int i;

    /* Destroy previous popup so it's rebuilt with current items */
    if (lw->list.popup_shell) {
        IswDestroyWidget(lw->list.popup_shell);
        lw->list.popup_shell = NULL;
    }

    /* Compute position below the collapsed widget */
    IswTranslateCoords(w, 0, 0, &abs_x, &abs_y);
    Position below_y = abs_y + (Position)w->core.height;

    /* Create SimpleMenu popup — same widget class as menubar submenus */
    IswArgBorderWidth(&ab, 0);
    IswArgWidth(&ab, w->core.width);
    lw->list.popup_shell = IswCreatePopupShell("dropdownPopup",
        simpleMenuWidgetClass, w, ab.args, ab.count);

    /* Populate with SmeBSB entries for each list item */
    for (i = 0; i < lw->list.nitems; i++) {
        Widget entry;
        IswArgBuilderReset(&ab);
        IswArgLabel(&ab, lw->list.list[i]);
        IswArgForeground(&ab, lw->list.foreground);
        if (lw->list.font) {
            IswArgFont(&ab, lw->list.font);
        }
        entry = IswCreateManagedWidget(lw->list.list[i],
            smeBSBObjectClass, lw->list.popup_shell, ab.args, ab.count);
        IswAddCallback(entry, IswNcallback,
                      DropdownMenuSelect, (IswPointer)(intptr_t)i);
    }

    /* Override translations: hover to highlight, click to select
     * (same as menubar submenus) */
    {
        static char dropdownTranslations[] =
            "<EnterWindow>:     highlight()             \n\
             <LeaveWindow>:     unhighlight()           \n\
             <Motion>:          highlight()             \n\
             <BtnMotion>:       highlight()             \n\
             <Btn4Down>:        scroll-up()              \n\
             <Btn5Down>:        scroll-down()            \n\
             <BtnUp>:           highlight()             \n\
             <BtnDown>:         notify() unhighlight() popdown()";
        IswOverrideTranslations(lw->list.popup_shell,
            IswParseTranslationTable(dropdownTranslations));
    }

    /* Compute available space and pick direction */
    {
        double sf = _IswGetScaleFactor(IswDisplay(w));
        int scr_height = (int)lrint(HeightOfScreen(IswScreen(w)) / sf);
        int space_below = scr_height - below_y;
        int space_above = abs_y;
        Position popup_y = below_y;

        /* Realize so SimpleMenu calculates its natural height */
        IswRealizeWidget(lw->list.popup_shell);

        int menu_h = (int)lw->list.popup_shell->core.height;

        /* If it doesn't fit below, try above */
        if (menu_h > space_below && space_above > space_below) {
            popup_y = abs_y - menu_h;
            if (popup_y < 0)
                popup_y = 0;
        }

        /* Constrain height to available space */
        int avail = (popup_y == below_y) ? space_below : (abs_y - popup_y);
        if (menu_h > avail && avail > 0) {
            SimpleMenuWidget smw = (SimpleMenuWidget)lw->list.popup_shell;
            smw->simple_menu.too_tall = TRUE;
            IswArgBuilderReset(&ab);
            IswArgHeight(&ab, (Dimension)avail);
            IswSetValues(lw->list.popup_shell, ab.args, ab.count);
        }

        IswArgBuilderReset(&ab);
        IswArgX(&ab, abs_x);
        IswArgY(&ab, popup_y);
        IswSetValues(lw->list.popup_shell, ab.args, ab.count);
    }

    IswPopup(lw->list.popup_shell, IswGrabNone);

    /* X server pointer grab — all button events (scroll, outside clicks)
     * delivered to popup window. Same technique as GTK/Motif popups. */
    xcb_grab_pointer(IswDisplay(w), False, IswWindow(lw->list.popup_shell),
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
        XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);

    /* Keyboard grab so arrow / Return / Escape route to the popup */
    xcb_grab_keyboard(IswDisplay(w), False, IswWindow(lw->list.popup_shell),
        XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    xcb_flush(IswDisplay(w));

    /* Dismiss on focus loss, minimize, or visibility change */
    {
        Widget shell = w;
        while (shell && !IswIsShell(shell))
            shell = IswParent(shell);
        if (shell)
            IswAddEventHandler(shell,
                              XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                              XCB_EVENT_MASK_VISIBILITY_CHANGE,
                              False, DropdownDismissHandler, (IswPointer)lw);
    }

    /* Dismiss if any ancestor moves (e.g. viewport scrolled).
     * Walk up to the shell, installing handlers on each ancestor. */
    {
        Widget ancestor = IswParent(w);
        while (ancestor && !IswIsShell(ancestor)) {
            IswAddEventHandler(ancestor, XCB_EVENT_MASK_STRUCTURE_NOTIFY, False,
                              DropdownDismissHandler, (IswPointer)lw);
            ancestor = IswParent(ancestor);
        }
    }

    IswAddCallback(lw->list.popup_shell, IswNpopdownCallback,
                  DropdownPopdownCB, (IswPointer)lw);
    return;
  }

  if ( (CvtToItem(w, be->event_x, be->event_y, &item))
      == OUT_OF_RANGE)
    IswListUnhighlight(w);		        /* Unhighlight current item. */
  else if ( lw->list.is_highlighted != item )   /* If this item is not */
    IswListHighlight(w, item);	                /* highlighted then do it. */
}

/*
 * Set specified arguments into widget
 */

static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ListWidget cl = (ListWidget) current;
    ListWidget rl = (ListWidget) request;
    ListWidget nl = (ListWidget) new;
    Boolean redraw = FALSE;

    /* If the request height/width is different, lock it.  Unless its 0. If */
    /* neither new nor 0, leave it as it was.  Not in R5. */
    if ( nl->core.width != cl->core.width )
        nl->list.freedoms |= WidthLock;
    if ( nl->core.width == 0 )
        nl->list.freedoms &= ~WidthLock;

    if ( nl->core.height != cl->core.height )
        nl->list.freedoms |= HeightLock;
    if ( nl->core.height == 0 )
        nl->list.freedoms &= ~HeightLock;

    if ( nl->list.longest != cl->list.longest )
        nl->list.freedoms |= LongestLock;
    if ( nl->list.longest == 0 )
        nl->list.freedoms &= ~LongestLock;

    /* _DONT_ check for fontset here - it doesn't affect color/layout.*/

    /* XCB Fix: Add NULL checks before comparing font->fid */
    Bool font_changed = False;
    if (cl->list.font != NULL && nl->list.font != NULL) {
 font_changed = (cl->list.font->fid != nl->list.font->fid);
    } else if (cl->list.font != nl->list.font) {
 /* One is NULL and the other isn't */
 font_changed = True;
    }

    if (  (cl->list.foreground       != nl->list.foreground)       ||
   (cl->core.background_pixel != nl->core.background_pixel) ||
   font_changed ) {
        redraw = TRUE;
    }

    if ( font_changed ) {
	nl->list.row_height = ISWScaledFontHeight(new, nl->list.font)
	                      + nl->list.row_space;
    }
    else if ( cl->list.row_space != nl->list.row_space ) {
	nl->list.row_height = ISWScaledFontHeight(new, nl->list.font)
	                      + nl->list.row_space;
    }

    if ((cl->core.width           != nl->core.width)           ||
	(cl->core.height          != nl->core.height)          ||
	(cl->list.internal_width  != nl->list.internal_width)  ||
	(cl->list.internal_height != nl->list.internal_height) ||
	(cl->list.column_space    != nl->list.column_space)    ||
	(cl->list.row_space       != nl->list.row_space)       ||
	(cl->list.default_cols    != nl->list.default_cols)    ||
	(  (cl->list.force_cols   != nl->list.force_cols) &&
	   (rl->list.force_cols   != nl->list.ncols) )         ||
	(cl->list.vertical_cols   != nl->list.vertical_cols)   ||
	(cl->list.longest         != nl->list.longest)         ||
	(cl->list.nitems          != nl->list.nitems)          ||
	(cl->list.font            != nl->list.font)            ||
	(cl->list.list            != nl->list.list)          )   {

        CalculatedValues( new );
        Layout( new, WidthFree( nl ), HeightFree( nl ),
			 &nl->core.width, &nl->core.height );
        redraw = TRUE;
    }

    if (cl->list.list != nl->list.list)
	nl->list.is_highlighted = nl->list.highlight = NO_HIGHLIGHT;

    if ((cl->core.sensitive != nl->core.sensitive) ||
	(cl->core.ancestor_sensitive != nl->core.ancestor_sensitive)) {
        nl->list.highlight = NO_HIGHLIGHT;
	redraw = TRUE;
    }

    if (!IswIsRealized(current))
      return(FALSE);

    return(redraw);
}

static void
Destroy(Widget w)
{
    ListWidget lw = (ListWidget) w;

    /* Cleanup Cairo render context */
    if (lw->list.render_ctx) {
        ISWRenderDestroy(lw->list.render_ctx);
        lw->list.render_ctx = NULL;
    }

    /* Clean up dropdown popup */
    if (lw->list.popup_shell) {
        IswDestroyWidget(lw->list.popup_shell);
        lw->list.popup_shell = NULL;
    }

    if (lw->list.clip_contents) {
	IswFree(lw->list.clip_contents);
	lw->list.clip_contents = NULL;
    }
}

/* Exported Functions */

/*	Function Name: IswListChange.
 *	Description: Changes the list being used and shown.
 *	Arguments: w - the list widget.
 *                 list - the new list.
 *                 nitems - the number of items in the list.
 *                 longest - the length (in Pixels) of the longest element
 *                           in the list.
 *                 resize - if TRUE the the list widget will
 *                          try to resize itself.
 *	Returns: none.
 *      NOTE:      If nitems of longest are <= 0 then they will be calculated.
 *                 If nitems is <= 0 then the list needs to be NULL terminated.
 */

void
IswListChange(Widget w, String* list, int nitems, int longest,
#if NeedWidePrototypes
	      int resize_it)
#else
	      Boolean resize_it)
#endif
{
    ListWidget lw = (ListWidget) w;
    Dimension new_width = w->core.width;
    Dimension new_height = w->core.height;
    

    lw->list.list = list;

    if ( nitems <= 0 ) nitems = 0;
    lw->list.nitems = nitems;
    if ( longest <= 0 ) longest = 0;

    /* If the user passes 0 meaning "calculate it", it must be free */
    if ( longest != 0 )
        lw->list.freedoms |= LongestLock;
    else /* the user's word is god. */
        lw->list.freedoms &= ~LongestLock;

    if ( resize_it )
        lw->list.freedoms &= ~WidthLock & ~HeightLock;

    lw->list.longest = longest;

    CalculatedValues( w );

    Layout( w, WidthFree( w ), HeightFree( w ), &new_width, &new_height );

    if ( new_width != w->core.width || new_height != w->core.height )
        ChangeSize( w, new_width, new_height );

    lw->list.is_highlighted = lw->list.highlight = NO_HIGHLIGHT;
    if ( IswIsRealized( w ) ) {
      /* Only repaint if the window is actually viewable.  When a
       * parent shell hasn't been mapped yet the blit goes to an
       * unmapped window whose contents the X server will clear at
       * map time.  The server's Expose after mapping handles the
       * repaint; calling Redisplay here would be wasted work that
       * races with the map and can leave the window blank. */
      xcb_get_window_attributes_cookie_t ac =
          xcb_get_window_attributes(IswDisplay(w), IswWindow(w));
      xcb_get_window_attributes_reply_t *ar =
          xcb_get_window_attributes_reply(IswDisplay(w), ac, NULL);
      int viewable = ar && ar->map_state == XCB_MAP_STATE_VIEWABLE;
      free(ar);
      if (viewable)
          Redisplay( w, NULL, 0 );
    }
}

/*	Function Name: IswListUnhighlight
 *	Description: unlights the current highlighted element.
 *	Arguments: w - the widget.
 *	Returns: none.
 */

void
IswListUnhighlight(Widget w)
{
    ListWidget lw = ( ListWidget ) w;

    lw->list.highlight = NO_HIGHLIGHT;
    if (lw->list.is_highlighted != NO_HIGHLIGHT) {
        if (lw->list.render_ctx)
            ISWRenderBegin(lw->list.render_ctx);
        PaintItemName(w, lw->list.is_highlighted);
        if (lw->list.render_ctx)
            ISWRenderEnd(lw->list.render_ctx);
    }
}

/*	Function Name: IswListHighlight
 *	Description: Highlights the given item.
 *	Arguments: w - the list widget.
 *                 item - the item to hightlight.
 *	Returns: none.
 */

void
IswListHighlight(Widget w, int item)
{
    ListWidget lw = ( ListWidget ) w;

    if (IswIsSensitive(w)) {
        lw->list.highlight = item;
        if (lw->list.render_ctx)
            ISWRenderBegin(lw->list.render_ctx);
        if (lw->list.is_highlighted != NO_HIGHLIGHT)
            PaintItemName(w, lw->list.is_highlighted);  /* Unhighlight. */
	PaintItemName(w, item); /* HIGHLIGHT this one. */
        if (lw->list.render_ctx)
            ISWRenderEnd(lw->list.render_ctx);
    }
}

/*	Function Name: IswListShowCurrent
 *	Description: returns the currently highlighted object.
 *	Arguments: w - the list widget.
 *	Returns: the info about the currently highlighted object.
 */

IswListReturnStruct *
IswListShowCurrent(Widget w)
{
    ListWidget lw = ( ListWidget ) w;
    IswListReturnStruct * ret_val;

    ret_val = (IswListReturnStruct *)
	          IswMalloc (sizeof (IswListReturnStruct));/* SPARE MALLOC OK */

    ret_val->list_index = lw->list.highlight;
    if (ret_val->list_index == XAW_LIST_NONE)
      ret_val->string = "";
    else
      ret_val->string = lw->list.list[ ret_val->list_index ];

    return(ret_val);
}


/*
 * Dropdown popup callbacks
 */

/*
 * DropdownMenuSelect - SmeBSB callback. client_data carries the item index.
 */
static void
DropdownMenuSelect(Widget w, IswPointer client_data, IswPointer call_data)
{
    (void)call_data;
    Widget menu = IswParent(w);
    Widget list_w = IswParent(menu);
    ListWidget lw = (ListWidget) list_w;
    int index = (int)(intptr_t)client_data;
    IswListReturnStruct parent_ret;

    /* Update selection */
    lw->list.selected_item = index;

    /* Redraw the collapsed widget to show new selection */
    Redisplay(list_w, NULL, 0);

    /* Fire the parent widget's callbacks */
    parent_ret.string = lw->list.list[index];
    parent_ret.list_index = index;
    IswCallCallbacks(list_w, IswNcallback, (IswPointer)&parent_ret);
}

static void
DropdownDismissHandler(Widget w, IswPointer client_data, xcb_generic_event_t *event,
                       Boolean *continue_to_dispatch)
{
    ListWidget lw = (ListWidget) client_data;
    uint8_t type;
    *continue_to_dispatch = True;

    if (!lw->list.popup_shell)
        return;

    type = event->response_type & 0x7f;

    /* Ignore focus changes caused by grabs: when we install our
     * keyboard/pointer grab on the popup, the parent shell sees a
     * synthetic FocusOut with mode != Normal — that's not a real
     * dismissal trigger. */
    if (type == XCB_FOCUS_OUT || type == XCB_FOCUS_IN) {
        xcb_focus_out_event_t *fe = (xcb_focus_out_event_t *)event;
        if (fe->mode != XCB_NOTIFY_MODE_NORMAL &&
            fe->mode != XCB_NOTIFY_MODE_WHILE_GRABBED)
            return;
        if (type == XCB_FOCUS_IN)
            return;  /* only FocusOut dismisses */
    }

    if (type == XCB_FOCUS_OUT || type == XCB_UNMAP_NOTIFY ||
        type == XCB_VISIBILITY_NOTIFY || type == XCB_CONFIGURE_NOTIFY) {
        IswPopdown(lw->list.popup_shell);
    }
}

static void
DropdownPopdownCB(Widget menu, IswPointer client_data, IswPointer call_data)
{
    (void)call_data;
    ListWidget lw = (ListWidget) client_data;
    Widget shell = (Widget)lw;

    xcb_ungrab_pointer(IswDisplay((Widget)lw), XCB_CURRENT_TIME);
    xcb_ungrab_keyboard(IswDisplay((Widget)lw), XCB_CURRENT_TIME);
    xcb_flush(IswDisplay((Widget)lw));

    while (shell && !IswIsShell(shell))
        shell = IswParent(shell);
    if (shell)
        IswRemoveEventHandler(shell,
                             XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                             XCB_EVENT_MASK_VISIBILITY_CHANGE,
                             False, DropdownDismissHandler, (IswPointer)lw);

    /* Remove ancestor-move handlers */
    {
        Widget ancestor = IswParent((Widget)lw);
        while (ancestor && !IswIsShell(ancestor)) {
            IswRemoveEventHandler(ancestor, XCB_EVENT_MASK_STRUCTURE_NOTIFY, False,
                                 DropdownDismissHandler, (IswPointer)lw);
            ancestor = IswParent(ancestor);
        }
    }

    IswRemoveCallback(menu, IswNpopdownCallback, DropdownPopdownCB, client_data);

    /* Destroy the popup widget and clear the pointer so subsequent key
     * presses know the dropdown is closed and re-open it instead of
     * operating on the (no-longer-visible) menu. */
    if (lw->list.popup_shell) {
        IswDestroyWidget(lw->list.popup_shell);
        lw->list.popup_shell = NULL;
    }
}

