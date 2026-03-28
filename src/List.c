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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h>
#include <ISW/ISWInit.h>
#include <ISW/ListP.h>
#include <ISW/ISWRender.h>
#include <ISW/Viewport.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeBSB.h>
#include <X11/Shell.h>
#include "ISWXcbDraw.h"

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
   <Btn1Up>:     Notify()";

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

#define offset(field) XtOffset(ListWidget, field)

static XtResource resources[] = {
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(list.foreground), XtRString, XtDefaultForeground},
    {XtNcursor, XtCCursor, XtRCursor, sizeof(Cursor),
       offset(simple.cursor), XtRString, "left_ptr"},
    {XtNfont,  XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	offset(list.font),XtRString, XtDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
    {XtNfontSet,  XtCFontSet, XtRFontSet, sizeof(ISWFontSet *),
	offset(list.fontset),XtRString, XtDefaultFontSet},
#endif
    {XtNlist, XtCList, XtRPointer, sizeof(char **),
       offset(list.list), XtRString, NULL},
    {XtNdefaultColumns, XtCColumns, XtRInt,  sizeof(int),
	offset(list.default_cols), XtRImmediate, (XtPointer)2},
    {XtNlongest, XtCLongest, XtRInt,  sizeof(int),
	offset(list.longest), XtRImmediate, (XtPointer)0},
    {XtNnumberStrings, XtCNumberStrings, XtRInt,  sizeof(int),
	offset(list.nitems), XtRImmediate, (XtPointer)0},
    {XtNpasteBuffer, XtCBoolean, XtRBoolean,  sizeof(Boolean),
	offset(list.paste), XtRImmediate, (XtPointer) False},
    {XtNforceColumns, XtCColumns, XtRBoolean,  sizeof(Boolean),
	offset(list.force_cols), XtRImmediate, (XtPointer) False},
    {XtNverticalList, XtCBoolean, XtRBoolean,  sizeof(Boolean),
	offset(list.vertical_cols), XtRImmediate, (XtPointer) False},
    {XtNinternalWidth, XtCWidth, XtRDimension,  sizeof(Dimension),
	offset(list.internal_width), XtRImmediate, (XtPointer)2},
    {XtNinternalHeight, XtCHeight, XtRDimension, sizeof(Dimension),
	offset(list.internal_height), XtRImmediate, (XtPointer)2},
    {XtNcolumnSpacing, XtCSpacing, XtRDimension,  sizeof(Dimension),
	offset(list.column_space), XtRImmediate, (XtPointer)6},
    {XtNrowSpacing, XtCSpacing, XtRDimension,  sizeof(Dimension),
	offset(list.row_space), XtRImmediate, (XtPointer)2},
    {XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer),
        offset(list.callback), XtRCallback, NULL},
    {XtNdropdownMode, XtCDropdownMode, XtRBoolean, sizeof(Boolean),
	offset(list.dropdown), XtRImmediate, (XtPointer) False},
};

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void ChangeSize(Widget, Dimension, Dimension);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Destroy(Widget);
static Boolean Layout(Widget, Boolean, Boolean, Dimension *, Dimension *);
static XtGeometryResult PreferredGeom(Widget, XtWidgetGeometry *, XtWidgetGeometry *);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void Notify(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Set(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Unset(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void DropdownMenuSelect(Widget, XtPointer, XtPointer);
static void DropdownPopdownCB(Widget, XtPointer, XtPointer);
static void DropdownDismissHandler(Widget, XtPointer, xcb_generic_event_t *, Boolean *);

static XtActionsRec actions[] = {
      {"Notify",         Notify},
      {"Set",            Set},
      {"Unset",          Unset},
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
    /* realize		  	*/	XtInheritRealize,
    /* actions		  	*/	actions,
    /* num_actions	  	*/	XtNumber(actions),
    /* resources	  	*/	resources,
    /* num_resources	  	*/	XtNumber(resources),
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
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus	 	*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private   	*/	NULL,
    /* tm_table		   	*/	defaultTranslations,
   /* query_geometry		*/      PreferredGeom,
  },
/* Simple class fields initialization */
  {
    /* change_sensitive		*/	XtInheritChangeSensitive
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

static void
GetGCs(Widget w)
{
    xcb_create_gc_value_list_t	values;
    ListWidget lw = (ListWidget) w;

    memset(&values, 0, sizeof(values));
    values.foreground	= lw->list.foreground;
    values.background	= lw->core.background_pixel;  /* CRITICAL: Set background for text rendering */

    /* XCB Fix: Add NULL check for font before accessing fid */
    XtGCMask gc_mask = (unsigned) (GCForeground | GCBackground);  /* Include background */
    if (lw->list.font != NULL) {
        values.font = lw->list.font->fid;
        gc_mask |= GCFont;
    }

#ifdef ISW_INTERNATIONALIZATION
    if ( lw->simple.international == True )
        lw->list.normgc = XtAllocateGC( w, 0, gc_mask & ~GCFont,
				 &values, GCFont, 0 );
    else
#endif
        lw->list.normgc = XtGetGC( w, gc_mask, &values);

    values.foreground	= lw->core.background_pixel;
    values.background	= lw->list.foreground;  /* Swap colors for reverse xcb_gcontext_t */

#ifdef ISW_INTERNATIONALIZATION
    if ( lw->simple.international == True )
        lw->list.revgc = XtAllocateGC( w, 0, gc_mask & ~GCFont,
				 &values, GCFont, 0 );
    else
#endif
        lw->list.revgc = XtGetGC( w, gc_mask, &values);

    values.tile       = IswCreateStippledPixmap(XtDisplay(w), XtWindow(w),
    		lw->list.foreground,
    		lw->core.background_pixel,
    		lw->core.depth);
    values.fill_style = XCB_FILL_STYLE_TILED;

    /* XCB Fix: Add NULL check for font before accessing fid */
    XtGCMask gray_gc_mask = (unsigned) (GCTile | GCFillStyle);
    if (lw->list.font != NULL) {
        gray_gc_mask |= GCFont;
    }

#ifdef ISW_INTERNATIONALIZATION
    if ( lw->simple.international == True )
        lw->list.graygc = XtAllocateGC( w, 0, gray_gc_mask & ~GCFont,
			      &values, GCFont, 0 );
    else
#endif
        lw->list.graygc = XtGetGC( w, gray_gc_mask, &values);
}


/* CalculatedValues()
 *
 * does routine checks/computations that must be done after data changes
 * but won't hurt if accidently called
 *
 * These calculations were needed in SetValues.  They were in ResetList.
 * ResetList called ChangeSize, which made an XtGeometryRequest.  You
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
    XtWidgetGeometry request, reply;

    request.request_mode = CWWidth | CWHeight;
    request.width = width;
    request.height = height;

    switch ( XtMakeGeometryRequest(w, &request, &reply) ) {
    case XtGeometryYes:
        break;
    case XtGeometryNo:
        break;
    case XtGeometryAlmost:
	Layout(w, (request.height != reply.height),
	          (request.width != reply.width),
	       &(reply.width), &(reply.height));
	request = reply;
	switch (XtMakeGeometryRequest(w, &request, &reply) ) {
	case XtGeometryYes:
	case XtGeometryNo:
	    break;
	case XtGeometryAlmost:
	    request = reply;
	    Layout(w, FALSE, FALSE, &(request.width), &(request.height));
	    request.request_mode = CWWidth | CWHeight;
	    XtMakeGeometryRequest(w, &request, &reply);
	    break;
	default:
	  XtAppWarning(XtWidgetToApplicationContext(w),
		       "List Widget: Unknown geometry return.");
	  break;
	}
	break;
    default:
	XtAppWarning(XtWidgetToApplicationContext(w),
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

    /* HiDPI: scale dimension resources */
    lw->list.internal_width = ISWScaleDim(new, lw->list.internal_width);
    lw->list.internal_height = ISWScaleDim(new, lw->list.internal_height);
    lw->list.row_space = ISWScaleDim(new, lw->list.row_space);
    lw->list.column_space = ISWScaleDim(new, lw->list.column_space);

    lw->list.clip_contents = NULL;

/*
 * Initialize all private resources.
 */

    /* XCB Fix: XtRFontStruct converter may fail in XCB mode, leaving font NULL.
     * If font is NULL but fontset is available, create a minimal XFontStruct
     * using the fontset's font_id (similar to Label.c approach). */
    if (lw->list.font == NULL) {
#ifdef ISW_INTERNATIONALIZATION
	if (lw->list.fontset != NULL) {
	    /* Allocate and initialize a minimal XFontStruct from fontset */
	    lw->list.font = (XFontStruct *)XtMalloc(sizeof(XFontStruct));
	    memset(lw->list.font, 0, sizeof(XFontStruct));
	    lw->list.font->fid = lw->list.fontset->font_id;
	    lw->list.font->min_char_or_byte2 = 0;
	    lw->list.font->max_char_or_byte2 = 255;
	} else
#endif
	{
	    XtAppWarning(XtWidgetToApplicationContext(new),
			 "List widget: font and fontset are both NULL - text rendering will fail");
	}
    }

    /* record for posterity if we are free */
    lw->list.freedoms = (lw->core.width != 0) * WidthLock +
                        (lw->core.height != 0) * HeightLock +
                        (lw->list.longest != 0) * LongestLock;

    GetGCs(new);

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

/*	Function Name: FindCornerItems.
 *	Description: Find the corners of the rectangle in item space.
 *	Arguments: w - the list widget.
 *                 event - the event structure that has the rectangle it it.
 *                 ul_ret, lr_ret - the corners ** RETURNED **.
 *	Returns: none.
 */

static void
FindCornerItems(Widget w, xcb_generic_event_t *event, int *ul_ret, int *lr_ret)
{
    int xloc, yloc;
    /* XCB: Cast to xcb_expose_event_t */
    xcb_expose_event_t *expose = (xcb_expose_event_t *)event;

    xloc = expose->x;
    yloc = expose->y;
    CvtToItem(w, xloc, yloc, ul_ret);
    xloc += expose->width;
    yloc += expose->height;
    CvtToItem(w, xloc, yloc, lr_ret);
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
 *  gc - the gc to use to paint this rectangle */

static void
HighlightBackground(Widget w, int x, int y, xcb_gcontext_t gc)
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
        height = height - ( lw->list.internal_height - x );
        y = lw->list.internal_height;
    }

    /* Determine color based on which xcb_gcontext_t is being used */
    Pixel color;
    if (gc == lw->list.normgc) {
        color = lw->list.foreground;
    } else if (gc == lw->list.revgc) {
        color = lw->core.background_pixel;
    } else {
        /* graygc - use foreground for now */
        color = lw->list.foreground;
    }

    ISWRenderSetColor(lw->list.render_ctx, color);
    ISWRenderFillRectangle(lw->list.render_ctx, x, y, width, height);
}


/* ClipToShadowInteriorAndLongest()
 *
 * Converts the passed gc so that any drawing done with that xcb_gcontext_t will not
 * write in the empty margin (specified by internal_width/height) (which also
 * prevents erasing the shadow.  It also clips against the value longest.
 * If the user doesn't set longest, this has no effect (as longest is the
 * maximum of all item lengths).  If the user does specify, say, 80 pixel
 * columns, though, this prevents items from overwriting other items. */

static void
ClipToShadowInteriorAndLongest(ListWidget lw, xcb_gcontext_t *gc_p, Dimension x)
{
    xcb_rectangle_t rect;

    rect.x = x;
    rect.y = lw->list.internal_height;
    rect.height = lw->core.height - lw->list.internal_height * 2;
    rect.width = lw->core.width - lw->list.internal_width - x;
    if ( rect.width > lw->list.longest )
        rect.width = lw->list.longest;

    xcb_connection_t *conn = XtDisplay((Widget)lw);
    xcb_set_clip_rectangles(conn, XCB_CLIP_ORDERING_YX_BANDED, *gc_p, 0, 0, 1, &rect);
    xcb_flush(conn);
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
    char * str;
    xcb_gcontext_t gc;
    int x, y, str_y;
    ListWidget lw = (ListWidget) w;

    if (!XtIsRealized(w)) return; /* Just in case... */

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
            gc = lw->list.revgc;
     HighlightBackground(w, x, y, lw->list.normgc);
 }
        else {
     if (XtIsSensitive(w))
         gc = lw->list.normgc;
     else
         gc = lw->list.graygc;
     HighlightBackground(w, x, y, lw->list.revgc);
     lw->list.is_highlighted = NO_HIGHLIGHT;
        }
    }
    else {
        if (item == lw->list.highlight) {
            gc = lw->list.revgc;
     HighlightBackground(w, x, y, lw->list.normgc);
     lw->list.is_highlighted = item;
 }
 else {
     if (XtIsSensitive(w))
         gc = lw->list.normgc;
     else
         gc = lw->list.graygc;
     HighlightBackground(w, x, y, lw->list.revgc);
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

        /* Determine text color */
        Pixel text_color;
        if (gc == lw->list.revgc)
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
    if (!lw->list.render_ctx && XtIsRealized(w)) {
        if (w->core.width > 0 && w->core.height > 0) {
            lw->list.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
        }
    }

    /* Begin Cairo rendering if available */
    if (lw->list.render_ctx) {
        ISWRenderBegin(lw->list.render_ctx);
    }

    if (event == NULL) {	/* repaint all. */
        ul_item = 0;
        lr_item = lw->list.nrows * lw->list.ncols - 1;
        if (lw->list.render_ctx) {
            ISWRenderSetColor(lw->list.render_ctx, w->core.background_pixel);
            ISWRenderFillRectangle(lw->list.render_ctx, 0, 0,
                                   w->core.width, w->core.height);
        }
    }
    else
        FindCornerItems(w, (xcb_generic_event_t*)event, &ul_item, &lr_item);

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

static XtGeometryResult
PreferredGeom(Widget w, XtWidgetGeometry *intended, XtWidgetGeometry *requested)
{
    Dimension new_width, new_height;
    Boolean change, width_req, height_req;

    width_req = intended->request_mode & CWWidth;
    height_req = intended->request_mode & CWHeight;

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
      return(XtGeometryYes);

    change = Layout(w, !width_req, !height_req, &new_width, &new_height);

    requested->request_mode |= CWWidth;
    requested->width = new_width;
    requested->request_mode |= CWHeight;
    requested->height = new_height;

    if (change)
        return(XtGeometryAlmost);
    return(XtGeometryYes);
}


/* Resize()
 *
 * resizes the widget, by changing the number of rows and columns. */

static void
Resize(Widget w)
{
    Dimension width, height;
    ListWidget lw = (ListWidget) w;

    width = w->core.width;
    height = w->core.height;

    if (Layout(w, FALSE, FALSE, &width, &height))
 XtAppWarning(XtWidgetToApplicationContext(w),
    "List Widget: Size changed when it shouldn't have when resising.");

    /* On resize, destroy and recreate the render context on next redisplay */
    if (lw->list.render_ctx) {
        ISWRenderDestroy(lw->list.render_ctx);
        lw->list.render_ctx = NULL;
    }
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
		     xcb_atom_t *type, XtPointer *value,
		     unsigned long *length, int *format)
{
    ListWidget lw = (ListWidget) w;

    if (*target == XCB_ATOM_TARGETS(XtDisplay(w))) {
	xcb_atom_t *targets = (xcb_atom_t *) XtMalloc(2 * sizeof(xcb_atom_t));
	targets[0] = XCB_ATOM_TARGETS(XtDisplay(w));
	targets[1] = XCB_ATOM_STRING;
	*type = XCB_ATOM_ATOM;
	*value = (XtPointer) targets;
	*length = 2;
	*format = 32;
	return True;
    }

    if (*target == XCB_ATOM_STRING) {
	if (lw->list.clip_contents == NULL)
	    return False;
	*type = XCB_ATOM_STRING;
	*value = XtNewString(lw->list.clip_contents);
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
	XtFree(lw->list.clip_contents);
	lw->list.clip_contents = NULL;
    }
}

/* Notify() - ACTION
 *
 * Notifies the user that a button has been pressed, and
 * calls the callback; if the XtNpasteBuffer resource is true
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
	    XtFree(lw->list.clip_contents);
	lw->list.clip_contents = XtNewString(lw->list.list[item]);
	XtOwnSelection(w, XCB_ATOM_CLIPBOARD(XtDisplay(w)),
			XtLastTimestampProcessed(XtDisplay(w)),
			ListConvertSelection, ListLoseSelection, NULL);
    }

/*
 * Call Callback function.
 */

    ret_value.string = lw->list.list[item];
    ret_value.list_index = item;

    XtCallCallbacks( w, XtNcallback, (XtPointer) &ret_value);
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
    Arg args[10];
    Cardinal n;
    int i;

    /* Destroy previous popup so it's rebuilt with current items */
    if (lw->list.popup_shell) {
        XtDestroyWidget(lw->list.popup_shell);
        lw->list.popup_shell = NULL;
    }

    /* Compute position below the collapsed widget */
    XtTranslateCoords(w, 0, 0, &abs_x, &abs_y);
    Position below_y = abs_y + (Position)w->core.height;

    /* Create SimpleMenu popup — same widget class as menubar submenus */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNwidth, w->core.width); n++;
    lw->list.popup_shell = XtCreatePopupShell("dropdownPopup",
        simpleMenuWidgetClass, w, args, n);

    /* Populate with SmeBSB entries for each list item */
    for (i = 0; i < lw->list.nitems; i++) {
        Widget entry;
        n = 0;
        XtSetArg(args[n], XtNlabel, lw->list.list[i]); n++;
        if (lw->list.font) {
            XtSetArg(args[n], XtNfont, lw->list.font); n++;
        }
        entry = XtCreateManagedWidget(lw->list.list[i],
            smeBSBObjectClass, lw->list.popup_shell, args, n);
        XtAddCallback(entry, XtNcallback,
                      DropdownMenuSelect, (XtPointer)(intptr_t)i);
    }

    /* Override translations: hover to highlight, click to select
     * (same as menubar submenus) */
    {
        static char dropdownTranslations[] =
            "<EnterWindow>:     highlight()             \n\
             <LeaveWindow>:     unhighlight()           \n\
             <Motion>:          highlight()             \n\
             <BtnMotion>:       highlight()             \n\
             <Btn4Down>:        unhighlight() popdown() \n\
             <Btn5Down>:        unhighlight() popdown() \n\
             <BtnUp>:           highlight()             \n\
             <BtnDown>:         notify() unhighlight() popdown()";
        XtOverrideTranslations(lw->list.popup_shell,
            XtParseTranslationTable(dropdownTranslations));
    }

    /* Compute available space and pick direction */
    {
        int scr_height = HeightOfScreen(XtScreen(w));
        int space_below = scr_height - below_y;
        int space_above = abs_y;
        Position popup_y = below_y;

        /* Realize so SimpleMenu calculates its natural height */
        XtRealizeWidget(lw->list.popup_shell);

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
            n = 0;
            XtSetArg(args[n], XtNheight, (Dimension)avail); n++;
            XtSetValues(lw->list.popup_shell, args, n);
        }

        n = 0;
        XtSetArg(args[n], XtNx, abs_x); n++;
        XtSetArg(args[n], XtNy, popup_y); n++;
        XtSetValues(lw->list.popup_shell, args, n);
    }

    XtPopup(lw->list.popup_shell, XtGrabNone);

    /* X server pointer grab — all button events (scroll, outside clicks)
     * delivered to popup window. Same technique as GTK/Motif popups. */
    xcb_grab_pointer(XtDisplay(w), False, XtWindow(lw->list.popup_shell),
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
        XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
    xcb_flush(XtDisplay(w));

    /* Dismiss on focus loss, minimize, or visibility change */
    {
        Widget shell = w;
        while (shell && !XtIsShell(shell))
            shell = XtParent(shell);
        if (shell)
            XtAddEventHandler(shell,
                              FocusChangeMask | StructureNotifyMask |
                              VisibilityChangeMask,
                              False, DropdownDismissHandler, (XtPointer)lw);
    }

    /* Dismiss if any ancestor moves (e.g. viewport scrolled).
     * Walk up to the shell, installing handlers on each ancestor. */
    {
        Widget ancestor = XtParent(w);
        while (ancestor && !XtIsShell(ancestor)) {
            XtAddEventHandler(ancestor, StructureNotifyMask, False,
                              DropdownDismissHandler, (XtPointer)lw);
            ancestor = XtParent(ancestor);
        }
    }

    XtAddCallback(lw->list.popup_shell, XtNpopdownCallback,
                  DropdownPopdownCB, (XtPointer)lw);
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

    /* _DONT_ check for fontset here - it's not in xcb_gcontext_t.*/

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
 /* XCB: Skip XGetGCValues - tile tracking would need separate mechanism */
 XtReleaseGC(current, cl->list.graygc);
 XtReleaseGC(current, cl->list.revgc);
 XtReleaseGC(current, cl->list.normgc);
        GetGCs(new);
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
   /* Equiv. fontsets might have different values, but the same fonts, so the
   next comparison is sloppy but not dangerous.  */
#ifdef ISW_INTERNATIONALIZATION
	(cl->list.fontset         != nl->list.fontset)         ||
#endif
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

    if (!XtIsRealized(current))
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
        XtDestroyWidget(lw->list.popup_shell);
        lw->list.popup_shell = NULL;
    }

    if (lw->list.clip_contents) {
	XtFree(lw->list.clip_contents);
	lw->list.clip_contents = NULL;
    }

    /* XCB: Skip XGetGCValues - tile tracking would need separate mechanism */
    XtReleaseGC(w, lw->list.graygc);
    XtReleaseGC(w, lw->list.revgc);
    XtReleaseGC(w, lw->list.normgc);
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
    /* else - still resize if its not locked */

    lw->list.longest = longest;

    CalculatedValues( w );

    if( Layout( w, WidthFree( w ), HeightFree( w ),
		&new_width, &new_height ) )
        ChangeSize( w, new_width, new_height );

    lw->list.is_highlighted = lw->list.highlight = NO_HIGHLIGHT;
    if ( XtIsRealized( w ) )
      Redisplay( w, NULL, 0 );
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

    if (XtIsSensitive(w)) {
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
	          XtMalloc (sizeof (IswListReturnStruct));/* SPARE MALLOC OK */

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
DropdownMenuSelect(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)call_data;
    Widget menu = XtParent(w);
    Widget list_w = XtParent(menu);
    ListWidget lw = (ListWidget) list_w;
    int index = (int)(intptr_t)client_data;
    IswListReturnStruct parent_ret;

    /* Update selection */
    lw->list.selected_item = index;

    /* Redraw the collapsed widget to show new selection */
    if (lw->list.render_ctx) {
        ISWRenderDestroy(lw->list.render_ctx);
        lw->list.render_ctx = NULL;
    }
    Redisplay(list_w, NULL, 0);

    /* Fire the parent widget's callbacks */
    parent_ret.string = lw->list.list[index];
    parent_ret.list_index = index;
    XtCallCallbacks(list_w, XtNcallback, (XtPointer)&parent_ret);
}

static void
DropdownDismissHandler(Widget w, XtPointer client_data, xcb_generic_event_t *event,
                       Boolean *continue_to_dispatch)
{
    ListWidget lw = (ListWidget) client_data;
    uint8_t type;
    *continue_to_dispatch = True;

    if (!lw->list.popup_shell)
        return;

    type = event->response_type & 0x7f;

    if (type == XCB_FOCUS_OUT || type == XCB_UNMAP_NOTIFY ||
        type == XCB_VISIBILITY_NOTIFY || type == XCB_CONFIGURE_NOTIFY) {
        XtPopdown(lw->list.popup_shell);
    }
}

static void
DropdownPopdownCB(Widget menu, XtPointer client_data, XtPointer call_data)
{
    (void)call_data;
    ListWidget lw = (ListWidget) client_data;
    Widget shell = (Widget)lw;

    xcb_ungrab_pointer(XtDisplay((Widget)lw), XCB_CURRENT_TIME);
    xcb_flush(XtDisplay((Widget)lw));

    while (shell && !XtIsShell(shell))
        shell = XtParent(shell);
    if (shell)
        XtRemoveEventHandler(shell,
                             FocusChangeMask | StructureNotifyMask |
                             VisibilityChangeMask,
                             False, DropdownDismissHandler, (XtPointer)lw);

    /* Remove ancestor-move handlers */
    {
        Widget ancestor = XtParent((Widget)lw);
        while (ancestor && !XtIsShell(ancestor)) {
            XtRemoveEventHandler(ancestor, StructureNotifyMask, False,
                                 DropdownDismissHandler, (XtPointer)lw);
            ancestor = XtParent(ancestor);
        }
    }

    XtRemoveCallback(menu, XtNpopdownCallback, DropdownPopdownCB, client_data);
}

