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
 * Portions Copyright (c) 2003 Brian V. Smith
 * Rights, permissions, and disclaimer per the above X Consortium license.
 */

/*
 * SmeBSB.c - Source code file for BSB Menu Entry object.
 *
 * Date:    September 26, 1989
 *
 * By:      Chris D. Peterson
 *          MIT X Consortium
 *          kit@expo.lcs.mit.edu
 */

#ifdef HAVE_CONFIG_H
#include "config.h"

/* Shadow resource name definitions (previously from ThreeD.h) */
#define XtNshadowWidth "shadowWidth"
#define XtCShadowWidth "ShadowWidth"
#define XtNtopShadowPixel "topShadowPixel"
#define XtCTopShadowPixel "TopShadowPixel"
#define XtNbottomShadowPixel "bottomShadowPixel"
#define XtCBottomShadowPixel "BottomShadowPixel"
#define XtNrelief "relief"
#define XtCRelief "Relief"
#define XtRRelief "Relief"

#endif
#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
#include <ISW/ISWInit.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeBSBP.h>
#include <ISW/Cardinals.h>
#include <ISW/ISWRender.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "ISWXcbDraw.h"

/* needed for abs() */
#ifndef X_NOT_STDC_ENV
#include <stdlib.h>
#else
extern int abs();
#endif

#define XtNshadowed "shadowed"
#define XtCShadowed "Shadowed"

#define offset(field) XtOffsetOf(SmeBSBRec, sme_bsb.field)

static XtResource resources[] = {
  {XtNlabel,  XtCLabel, XtRString, sizeof(String),
     offset(label), XtRString, NULL},
  {XtNvertSpace,  XtCVertSpace, XtRInt, sizeof(int),
     offset(vert_space), XtRImmediate, (XtPointer) 25},
  {XtNleftBitmap, XtCLeftBitmap, XtRBitmap, sizeof(Pixmap),
     offset(left_bitmap), XtRImmediate, (XtPointer)None},
  {XtNjustify, XtCJustify, XtRJustify, sizeof(XtJustify),
     offset(justify), XtRImmediate, (XtPointer) XtJustifyLeft},
  {XtNrightBitmap, XtCRightBitmap, XtRBitmap, sizeof(Pixmap),
     offset(right_bitmap), XtRImmediate, (XtPointer)None},
  {XtNleftMargin,  XtCHorizontalMargins, XtRDimension, sizeof(Dimension),
     offset(left_margin), XtRImmediate, (XtPointer) 4},
  {XtNrightMargin,  XtCHorizontalMargins, XtRDimension, sizeof(Dimension),
     offset(right_margin), XtRImmediate, (XtPointer) 4},
  {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
     offset(foreground), XtRString, XtDefaultForeground},
  {XtNfont,  XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     offset(font), XtRString, XtDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
  {XtNfontSet,  XtCFontSet, XtRFontSet, sizeof(ISWFontSet *),
     offset(fontset),XtRString, XtDefaultFontSet},
#endif
  {XtNmenuName, XtCMenuName, XtRString, sizeof(String),
     offset(menu_name), XtRImmediate, (XtPointer) NULL},
  {XtNunderline,  XtCIndex, XtRInt, sizeof(int),
     offset(underline), XtRImmediate, (XtPointer) -1},
  /* Shadow resources (formerly from SmeThreeD) */
  {XtNshadowWidth, XtCShadowWidth, XtRDimension, sizeof(Dimension),
     offset(shadow_width), XtRImmediate, (XtPointer) 2},
  {XtNtopShadowPixel, XtCTopShadowPixel, XtRPixel, sizeof(Pixel),
     offset(top_shadow_pixel), XtRString, XtDefaultForeground},
  {XtNbottomShadowPixel, XtCBottomShadowPixel, XtRPixel, sizeof(Pixel),
     offset(bot_shadow_pixel), XtRString, XtDefaultForeground},
  {XtNshadowed, XtCShadowed, XtRBoolean, sizeof(Boolean),
     offset(shadowed), XtRImmediate, (XtPointer) False},
};
#undef offset

/*
 * Semi Public function definitions.
 */

static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Destroy(Widget);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Highlight(Widget);
static void Unhighlight(Widget);
static void ClassInitialize(void);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static XtGeometryResult QueryGeometry(Widget, XtWidgetGeometry *, XtWidgetGeometry *);

/*
 * Private Function Definitions.
 */

static void GetDefaultSize(Widget, Dimension *, Dimension *);
static void DrawBitmaps(Widget, GC);
static void GetBitmapInfo(Widget, Boolean);
static void CreateGCs(Widget);
static void DestroyGCs(Widget);
static void FlipColors(Widget);
static void DrawShadows(Widget);

#define superclass (&smeClassRec)
SmeBSBClassRec smeBSBClassRec = {
  {
    /* superclass         */    (WidgetClass) superclass,
    /* class_name         */    "SmeBSB",
    /* size               */    sizeof(SmeBSBRec),
    /* class_initializer  */	ClassInitialize,
    /* class_part_initialize*/	NULL,
    /* Class init'ed      */	FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */	NULL,
    /* realize            */    NULL,
    /* actions            */    NULL,
    /* num_actions        */    ZERO,
    /* resources          */    resources,
    /* resource_count     */	XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    FALSE,
    /* compress_exposure  */    FALSE,
    /* compress_enterleave*/ 	FALSE,
    /* visible_interest   */    FALSE,
    /* destroy            */    Destroy,
    /* resize             */    NULL,
    /* expose             */    Redisplay,
    /* set_values         */    SetValues,
    /* set_values_hook    */	NULL,
    /* set_values_almost  */	XtInheritSetValuesAlmost,
    /* get_values_hook    */	NULL,
    /* accept_focus       */    NULL,
    /* intrinsics version */	XtVersion,
    /* callback offsets   */    NULL,
    /* tm_table		  */    NULL,
    /* query_geometry	  */    QueryGeometry,
    /* display_accelerator*/    NULL,
    /* extension	  */    NULL
  },{
    /* SimpleMenuClass Fields */
    /* highlight          */	Highlight,
    /* unhighlight        */	Unhighlight,
    /* notify             */	XtInheritNotify,
    /* extension	  */	NULL
  }, {
    /* BSBClass Fields */
    /* extension	  */    NULL
  }
};

WidgetClass smeBSBObjectClass = (WidgetClass) &smeBSBClassRec;

/************************************************************
 *
 * Semi-Public Functions.
 *
 ************************************************************/

/*	Function Name: ClassInitialize
 *	Description: Initializes the SmeBSBObject.
 *	Arguments: none.
 *	Returns: none.
 */

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    XtSetTypeConverter( XtRString, XtRJustify, ISWCvtStringToJustify,
		    (XtConvertArgList)NULL, 0, XtCacheNone, (XtDestructor)NULL );
}

/*      Function Name: Initialize
 *      Description: Initializes the simple menu widget
 *      Arguments: request - the widget requested by the argument list.
 *                 new     - the new widget with both resource and non
 *                           resource values.
 *      Returns: none.
 */

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SmeBSBObject entry = (SmeBSBObject) new;

    /* Initialize Cairo rendering context */
    entry->sme_bsb.render_ctx = NULL;

    /* XCB Fix: XtRFontStruct converter may fail in XCB mode, leaving font NULL.
     * If font is NULL but fontset is available, create a minimal XFontStruct
     * using the fontset's font_id (similar to Label.c approach). */
    if (entry->sme_bsb.font == NULL) {
#ifdef ISW_INTERNATIONALIZATION
	if (entry->sme_bsb.fontset != NULL) {
	    /* Allocate and initialize a minimal XFontStruct from fontset */
	    entry->sme_bsb.font = (XFontStruct *)XtMalloc(sizeof(XFontStruct));
	    memset(entry->sme_bsb.font, 0, sizeof(XFontStruct));
	    entry->sme_bsb.font->fid = entry->sme_bsb.fontset->font_id;
	    entry->sme_bsb.font->min_char_or_byte2 = 0;
	    entry->sme_bsb.font->max_char_or_byte2 = 255;
	} else
#endif
	{
	    XtAppWarning(XtWidgetToApplicationContext(new),
			 "SmeBSB widget: font and fontset are both NULL - text rendering will fail");
	}
    }

    if (entry->sme_bsb.label == NULL)
	entry->sme_bsb.label = XtName(new);
    else
	entry->sme_bsb.label = XtNewString( entry->sme_bsb.label );

    CreateGCs(new);

    GetBitmapInfo(new, TRUE);	/* Left Bitmap Info */
    GetBitmapInfo(new, FALSE);	/* Right Bitmap Info */

    entry->sme_bsb.left_stippled = entry->sme_bsb.right_stippled = None;

    GetDefaultSize(new, &(entry->rectangle.width), &(entry->rectangle.height));
}

/*      Function Name: Destroy
 *      Description: Called at destroy time, cleans up.
 *      Arguments: w - the simple menu widget.
 *      Returns: none.
 */

static void
Destroy(Widget w)
{
    SmeBSBObject entry = (SmeBSBObject) w;

    /* Destroy Cairo rendering context */
    if (entry->sme_bsb.render_ctx) {
        ISWRenderDestroy(entry->sme_bsb.render_ctx);
        entry->sme_bsb.render_ctx = NULL;
    }

    DestroyGCs(w);
#ifdef ISW_MULTIPLANE_PIXMAPS
    if (entry->sme_bsb.left_stippled != None)
	XFreePixmap(XtDisplayOfObject(w), entry->sme_bsb.left_stippled);
    if (entry->sme_bsb.right_stippled != None)
	XFreePixmap(XtDisplayOfObject(w), entry->sme_bsb.right_stippled);
#endif
    if (entry->sme_bsb.label != XtName(w))
	XtFree(entry->sme_bsb.label);
}

/*      Function Name: Redisplay
 *      Description: Redisplays the contents of the widget.
 *      Arguments: w - the simple menu widget.
 *                 event - the X event that caused this redisplay.
 *                 region - the region the needs to be repainted.
 *      Returns: none.
 */

/* ARGSUSED */
static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    GC gc;
    SmeBSBObject entry = (SmeBSBObject) w;
    Dimension s = entry->sme_bsb.shadow_width;
    int	font_ascent = 0, font_descent = 0, y_loc;
#ifdef ISW_INTERNATIONALIZATION
    int	fontset_ascent = 0, fontset_descent = 0;
#endif

    entry->sme_bsb.set_values_area_cleared = FALSE;
#ifdef ISW_INTERNATIONALIZATION
    if ( entry->sme.international == True ) {
        fontset_ascent = entry->sme_bsb.fontset->ascent;
        fontset_descent = entry->sme_bsb.fontset->descent;
    }
    else
#endif
    { /*else, compute size from font like R5*/
 /* XCB Fix: Add NULL check for font before accessing fields */
 if (entry->sme_bsb.font != NULL) {
     font_ascent = entry->sme_bsb.font->ascent;
     font_descent = entry->sme_bsb.font->descent;
 } else {
     font_ascent = 11;   /* Default ascent */
     font_descent = 3;   /* Default descent */
 }
    }
    y_loc = entry->rectangle.y;

    if (XtIsSensitive(w) && XtIsSensitive( XtParent(w) ) ) {
 if ( w == IswSimpleMenuGetActiveEntry(XtParent(w)) ) {
     /* Draw highlight background using Cairo or XCB */
     if (entry->sme_bsb.render_ctx == NULL) {
         entry->sme_bsb.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
     }
     
     if (entry->sme_bsb.render_ctx) {
         ISWRenderBegin(entry->sme_bsb.render_ctx);
         /* norm_gc has parent background as foreground for highlight */
         ISWRenderSetColor(entry->sme_bsb.render_ctx, XtParent(w)->core.background_pixel);
         ISWRenderFillRectangle(entry->sme_bsb.render_ctx,
                                s, y_loc + s,
                                entry->rectangle.width - 2 * s,
                                entry->rectangle.height - 2 * s);
         ISWRenderEnd(entry->sme_bsb.render_ctx);
     } else {
         /* XCB fallback */
         xcb_connection_t *conn = XtDisplayOfObject(w);
         xcb_rectangle_t rect = {s, y_loc + s,
          (unsigned int) entry->rectangle.width - 2 * s,
          (unsigned int) entry->rectangle.height - 2 * s};
         xcb_poly_fill_rectangle(conn, XtWindowOfObject(w),
          entry->sme_bsb.norm_gc, 1, &rect);
         xcb_flush(conn);
     }
     gc = entry->sme_bsb.rev_gc;
 }
 else
     gc = entry->sme_bsb.norm_gc;
    }
    else
 gc = entry->sme_bsb.norm_gray_gc;

    if (entry->sme_bsb.label != NULL) {
	int x_loc = entry->sme_bsb.left_margin;
	int len = strlen(entry->sme_bsb.label);
	int width, t_width;
	char * label = entry->sme_bsb.label;

	switch(entry->sme_bsb.justify) {
	    case XtJustifyCenter:
#ifdef ISW_INTERNATIONALIZATION
		if ( entry->sme.international == True )
		    t_width = IswTextWidth(entry->sme_bsb.fontset,label,len);
		else
#endif
		    t_width = XTextWidth(entry->sme_bsb.font, label, len);

		width = entry->rectangle.width -
				(entry->sme_bsb.left_margin +
				entry->sme_bsb.right_margin);
		x_loc += (width - t_width)/2;
		break;
	    case XtJustifyRight:
#ifdef ISW_INTERNATIONALIZATION
		if ( entry->sme.international == True )
		    t_width = IswTextWidth(entry->sme_bsb.fontset,label,len);
		else
#endif
		    t_width = XTextWidth(entry->sme_bsb.font, label, len);

		x_loc = entry->rectangle.width -
				(entry->sme_bsb.right_margin + t_width);
		break;
	    case XtJustifyLeft:
	    default:
		break;
	}

	/* this will center the text in the gadget top-to-bottom */

#ifdef ISW_INTERNATIONALIZATION
        if ( entry->sme.international==True ) {
            y_loc += ((int)entry->rectangle.height -
		  (fontset_ascent + fontset_descent)) / 2 + fontset_ascent;

            IswDrawString(XtDisplayOfObject(w), XtWindowOfObject(w),
                entry->sme_bsb.fontset, gc, x_loc + s, y_loc, label, len);
        }
        else
#endif
        {
            y_loc += ((int)entry->rectangle.height -
    (font_ascent + font_descent)) / 2 + font_ascent;

            if (entry->sme_bsb.render_ctx) {
                Pixel text_color;
                if (gc == entry->sme_bsb.rev_gc)
                    text_color = entry->sme_bsb.foreground;
                else
                    text_color = XtParent(w)->core.background_pixel;
                ISWRenderBegin(entry->sme_bsb.render_ctx);
                ISWRenderSetColor(entry->sme_bsb.render_ctx, text_color);
                if (entry->sme_bsb.font)
                    ISWRenderSetFont(entry->sme_bsb.render_ctx, entry->sme_bsb.font);
                ISWRenderDrawString(entry->sme_bsb.render_ctx, label, len,
                                    x_loc + s, y_loc);
                ISWRenderEnd(entry->sme_bsb.render_ctx);
            } else {
                ISWXcbDrawText(XtDisplayOfObject(w), XtWindowOfObject(w), gc,
                    x_loc + s, y_loc, label, len);
            }
        }

	if (entry->sme_bsb.underline >= 0 && entry->sme_bsb.underline < len) {
	    int ul = entry->sme_bsb.underline;
	    int ul_x1_loc = x_loc + s;
	    int ul_wid;
	    Pixel underline_color;

	    if (ul != 0)
	 ul_x1_loc += XTextWidth(entry->sme_bsb.font, label, ul);
	    ul_wid = XTextWidth(entry->sme_bsb.font, &label[ul], 1) - 2;
	    
	    /* Determine underline color based on which GC is being used */
	    if (gc == entry->sme_bsb.rev_gc) {
	        underline_color = entry->sme_bsb.foreground;
	    } else {
	        underline_color = XtParent(w)->core.background_pixel;
	    }
	    
	    /* Draw underline using Cairo or XCB */
	    if (entry->sme_bsb.render_ctx) {
	        ISWRenderBegin(entry->sme_bsb.render_ctx);
	        ISWRenderSetColor(entry->sme_bsb.render_ctx, underline_color);
	        ISWRenderDrawLine(entry->sme_bsb.render_ctx,
	                          ul_x1_loc, y_loc + 1,
	                          ul_x1_loc + ul_wid, y_loc + 1);
	        ISWRenderEnd(entry->sme_bsb.render_ctx);
	    } else {
	        /* XCB fallback */
	        xcb_connection_t *conn = XtDisplayOfObject(w);
	        xcb_point_t points[2] = {
	     {ul_x1_loc, y_loc + 1},
	     {ul_x1_loc + ul_wid, y_loc + 1}
	        };
	        xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, XtWindowOfObject(w), gc, 2, points);
	        xcb_flush(conn);
	    }
	}
    }

    DrawBitmaps(w, gc);
}


/*      Function Name: SetValues
 *      Description: Relayout the menu when one of the resources is changed.
 *      Arguments: current - current state of the widget.
 *                 request - what was requested.
 *                 new - what the widget will become.
 *      Returns: none
 */

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    Widget parent = XtParent(new);
    SmeBSBObject entry = (SmeBSBObject) new;
    SmeBSBObject old_entry = (SmeBSBObject) current;
    Boolean ret_val = FALSE;

    if (old_entry->sme_bsb.label != entry->sme_bsb.label) {
        if (old_entry->sme_bsb.label != XtName( new ) )
	    XtFree( (char *) old_entry->sme_bsb.label );

	if (entry->sme_bsb.label != XtName(new) )
	    entry->sme_bsb.label = XtNewString( entry->sme_bsb.label );

	ret_val = TRUE;
    }

    if (entry->sme_bsb.underline != old_entry->sme_bsb.underline)
	ret_val = TRUE;

    if (entry->rectangle.sensitive != old_entry->rectangle.sensitive)
	ret_val = TRUE;

    /* XCB Fix: Add NULL checks before comparing font->fid */
    Bool font_changed = False;
#ifdef ISW_INTERNATIONALIZATION
    if (old_entry->sme.international == False) {
#endif
	if (old_entry->sme_bsb.font != NULL && entry->sme_bsb.font != NULL) {
	    font_changed = (old_entry->sme_bsb.font->fid != entry->sme_bsb.font->fid);
	} else if (old_entry->sme_bsb.font != entry->sme_bsb.font) {
	    /* One is NULL and the other isn't */
	    font_changed = True;
	}
#ifdef ISW_INTERNATIONALIZATION
    }
#endif

    if ( font_changed ||
	(old_entry->sme_bsb.foreground != entry->sme_bsb.foreground) ) {
	DestroyGCs(current);
	CreateGCs(new);

	ret_val = TRUE;
    }

    if (entry->sme_bsb.left_bitmap != old_entry->sme_bsb.left_bitmap)
    {
	GetBitmapInfo(new, TRUE);

#ifdef ISW_MULTIPLANE_PIXMAPS
	entry->sme_bsb.left_stippled = None;
	if (old_entry->sme_bsb.left_stippled != None)
	    XFreePixmap(XtDisplayOfObject(current),
			old_entry->sme_bsb.left_stippled);
#endif

	ret_val = TRUE;
    }

    if (entry->sme_bsb.left_margin != old_entry->sme_bsb.left_margin)
	ret_val = TRUE;

    if (entry->sme_bsb.right_bitmap != old_entry->sme_bsb.right_bitmap)
    {
	GetBitmapInfo(new, FALSE);

#ifdef ISW_MULTIPLANE_PIXMAPS
	entry->sme_bsb.right_stippled = None;
	if (old_entry->sme_bsb.right_stippled != None)
	    XFreePixmap(XtDisplayOfObject(current),
			old_entry->sme_bsb.right_stippled);
#endif

	ret_val = TRUE;
    }

    if (entry->sme_bsb.right_margin != old_entry->sme_bsb.right_margin)
	ret_val = TRUE;

#ifdef ISW_INTERNATIONALIZATION
    if ( ( old_entry->sme_bsb.fontset != entry->sme_bsb.fontset) &&
				(old_entry->sme.international == True ) )
        /* don't change the GCs - the fontset is not in them */
        ret_val = TRUE;
#endif

    if (ret_val) {
	GetDefaultSize(new,
		       &(entry->rectangle.width), &(entry->rectangle.height));
	entry->sme_bsb.set_values_area_cleared = TRUE;

	(parent->core.widget_class->core_class.resize)(new);
    }

    return(ret_val);
}

/*	Function Name: QueryGeometry.
 *	Description: Returns the preferred geometry for this widget.
 *	Arguments: w - the menu entry object.
 *                 itended, return_val - the intended and return geometry info.
 *	Returns: A Geometry Result.
 *
 * See the Intrinsics manual for details on what this function is for.
 *
 * I just return the height and width of the label plus the margins.
 */

static XtGeometryResult
QueryGeometry(Widget w, XtWidgetGeometry *intended, XtWidgetGeometry *return_val)
{
    SmeBSBObject entry = (SmeBSBObject) w;
    Dimension width, height;
    XtGeometryResult ret_val = XtGeometryYes;
    XtGeometryMask mode = intended->request_mode;

    GetDefaultSize(w, &width, &height );

    if ( ((mode & CWWidth) && (intended->width != width)) ||
	 !(mode & CWWidth) ) {
	return_val->request_mode |= CWWidth;
	return_val->width = width;
	ret_val = XtGeometryAlmost;
    }

    if ( ((mode & CWHeight) && (intended->height != height)) ||
	 !(mode & CWHeight) ) {
	return_val->request_mode |= CWHeight;
	return_val->height = height;
	ret_val = XtGeometryAlmost;
    }

    if (ret_val == XtGeometryAlmost) {
	mode = return_val->request_mode;

	if ( ((mode & CWWidth) && (width == entry->rectangle.width)) &&
	     ((mode & CWHeight) && (height == entry->rectangle.height)) )
	    return(XtGeometryNo);
    }

    entry->rectangle.width = width;
    entry->rectangle.height = height;

    return(ret_val);
}

/*
 * FlipColors() used to be called directly, but it's blind
 * state toggling caused re-unhighlighting problems.
 */

static void
Highlight(Widget w)
{
    SmeBSBObject entry = (SmeBSBObject) w;

    entry->sme_bsb.shadowed = True;
    FlipColors(w);
}

static void
Unhighlight(Widget w)
{
    SmeBSBObject entry = (SmeBSBObject) w;

    entry->sme_bsb.shadowed = False;
    FlipColors(w);
}

/************************************************************
 *
 * Private Functions.
 *
 ************************************************************/

/*	Function Name: GetDefaultSize
 *	Description: Calculates the Default (preferred) size of
 *                   this menu entry.
 *	Arguments: w - the menu entry widget.
 *                 width, height - default sizes (RETURNED).
 *	Returns: none.
 */

static void
GetDefaultSize(Widget w, Dimension * width, Dimension * height)
{
    SmeBSBObject entry = (SmeBSBObject) w;
    Dimension h;

#ifdef ISW_INTERNATIONALIZATION
    if ( entry->sme.international == True ) {
        if (entry->sme_bsb.label == NULL)
	    *width = 0;
        else
	    *width = IswTextWidth(entry->sme_bsb.fontset, entry->sme_bsb.label,
			    strlen(entry->sme_bsb.label));

        *height = entry->sme_bsb.fontset->height;
    }
    else
#endif
    {
 /* XCB Fix: Add NULL check for font before accessing fields */
 if (entry->sme_bsb.font != NULL) {
     if (entry->sme_bsb.label == NULL)
  *width = 0;
     else
  *width = XTextWidth(entry->sme_bsb.font, entry->sme_bsb.label,
    strlen(entry->sme_bsb.label));

     *height = (entry->sme_bsb.font->ascent +
     entry->sme_bsb.font->descent);
 } else {
     /* No font available - use defaults */
     if (entry->sme_bsb.label == NULL)
  *width = 0;
     else
  *width = strlen(entry->sme_bsb.label) * 8;  /* Default width */
     *height = 14;  /* Default height */
 }
    }

    *width += entry->sme_bsb.left_margin + entry->sme_bsb.right_margin;
    *width += (2 * entry->sme_bsb.shadow_width);

    h = (entry->sme_bsb.left_bitmap_height > entry->sme_bsb.right_bitmap_height)
	    ? entry->sme_bsb.left_bitmap_height : entry->sme_bsb.right_bitmap_height;
    if (h > *height) *height = h;
    *height = ((int)*height * (100 + entry->sme_bsb.vert_space)) / 100;
    *height += (2 * entry->sme_bsb.shadow_width);
}

/*      Function Name: DrawBitmaps
 *      Description: Draws left and right bitmaps.
 *      Arguments: w - the simple menu widget.
 *                 gc - graphics context to use for drawing.
 *      Returns: none
 */

static void
DrawBitmaps(Widget w, GC gc)
{
#ifdef ISW_MULTIPLANE_PIXMAPS
    Widget parent = XtParent(w);
#endif
    int x_loc, y_loc;
    SmeBSBObject entry = (SmeBSBObject) w;
    Pixmap pm;

    if ( (entry->sme_bsb.left_bitmap == None) &&
	 (entry->sme_bsb.right_bitmap == None) ) return;

/*
 * Draw Left Bitmap.
 */

  if (entry->sme_bsb.left_bitmap != None) {
    x_loc = entry->sme_bsb.shadow_width +
  (int)(entry->sme_bsb.left_margin -
		entry->sme_bsb.left_bitmap_width) / 2;

    y_loc = entry->rectangle.y + (int)(entry->rectangle.height -
		      entry->sme_bsb.left_bitmap_height) / 2;

    pm = entry->sme_bsb.left_bitmap;
#ifdef ISW_MULTIPLANE_PIXMAPS
    if (!XtIsSensitive(w)) {
	if (entry->sme_bsb.left_stippled == None)
	    entry->sme_bsb.left_stippled = stipplePixmap(w,
			entry->sme_bsb.left_bitmap,
			parent->core.colormap,
			parent->core.background_pixel,
			entry->sme_bsb.left_depth);
	if (entry->sme_bsb.left_stippled != None)
	    pm = entry->sme_bsb.left_stippled;
    }
#endif

    if (entry->sme_bsb.render_ctx) {
	Pixel fg = (gc == entry->sme_bsb.rev_gc) ?
	    XtParent(w)->core.background_pixel : entry->sme_bsb.foreground;
	ISWRenderBegin(entry->sme_bsb.render_ctx);
	ISWRenderSetColor(entry->sme_bsb.render_ctx, fg);
	ISWRenderDrawPixmap(entry->sme_bsb.render_ctx, pm, 0, 0,
			    x_loc, y_loc,
			    entry->sme_bsb.left_bitmap_width,
			    entry->sme_bsb.left_bitmap_height,
			    entry->sme_bsb.left_depth);
	ISWRenderEnd(entry->sme_bsb.render_ctx);
    } else {
	xcb_connection_t *conn = XtDisplayOfObject(w);
	if (entry->sme_bsb.left_depth == 1)
	    xcb_copy_plane(conn, pm, XtWindowOfObject(w), gc, 0, 0,
			   x_loc, y_loc,
			   entry->sme_bsb.left_bitmap_width,
			   entry->sme_bsb.left_bitmap_height, 1);
	else
	    xcb_copy_area(conn, pm, XtWindowOfObject(w), gc, 0, 0,
			  x_loc, y_loc,
			  entry->sme_bsb.left_bitmap_width,
			  entry->sme_bsb.left_bitmap_height);
	xcb_flush(conn);
    }
  }

/*
 * Draw Right Bitmap.
 */

  if (entry->sme_bsb.right_bitmap != None) {
    x_loc = entry->rectangle.width - entry->sme_bsb.shadow_width -
  (int)(entry->sme_bsb.right_margin +
		entry->sme_bsb.right_bitmap_width) / 2;

    y_loc = entry->rectangle.y + (int)(entry->rectangle.height -
		      entry->sme_bsb.right_bitmap_height) / 2;

     pm = entry->sme_bsb.right_bitmap;
#ifdef ISW_MULTIPLANE_PIXMAPS
    if (!XtIsSensitive(w)) {
	if (entry->sme_bsb.right_stippled == None)
	    entry->sme_bsb.right_stippled = stipplePixmap(w,
			entry->sme_bsb.right_bitmap,
			parent->core.colormap,
			parent->core.background_pixel,
			entry->sme_bsb.right_depth);
	if (entry->sme_bsb.right_stippled != None)
	    pm = entry->sme_bsb.right_stippled;
    }
#endif

    if (entry->sme_bsb.render_ctx) {
	Pixel fg = (gc == entry->sme_bsb.rev_gc) ?
	    XtParent(w)->core.background_pixel : entry->sme_bsb.foreground;
	ISWRenderBegin(entry->sme_bsb.render_ctx);
	ISWRenderSetColor(entry->sme_bsb.render_ctx, fg);
	ISWRenderDrawPixmap(entry->sme_bsb.render_ctx, pm, 0, 0,
			    x_loc, y_loc,
			    entry->sme_bsb.right_bitmap_width,
			    entry->sme_bsb.right_bitmap_height,
			    entry->sme_bsb.right_depth);
	ISWRenderEnd(entry->sme_bsb.render_ctx);
    } else {
	xcb_connection_t *conn = XtDisplayOfObject(w);
	if (entry->sme_bsb.right_depth == 1)
	    xcb_copy_plane(conn, pm, XtWindowOfObject(w), gc, 0, 0,
			   x_loc, y_loc,
			   entry->sme_bsb.right_bitmap_width,
			   entry->sme_bsb.right_bitmap_height, 1);
	else
	    xcb_copy_area(conn, pm, XtWindowOfObject(w), gc, 0, 0,
			  x_loc, y_loc,
			  entry->sme_bsb.right_bitmap_width,
			  entry->sme_bsb.right_bitmap_height);
	xcb_flush(conn);
    }
  }
}

/*      Function Name: GetBitmapInfo
 *      Description: Gets the bitmap information from either of the bitmaps.
 *      Arguments: w - the bsb menu entry widget.
 *                 is_left - TRUE if we are testing left bitmap,
 *                           FALSE if we are testing the right bitmap.
 *      Returns: none
 */

static void
GetBitmapInfo(Widget w, Boolean is_left)
{
    SmeBSBObject entry = (SmeBSBObject) w;
    xcb_window_t root;
    int x, y;
    unsigned int width, height, bw;
    char buf[BUFSIZ];

    if (is_left) {
	width = height = 0;

	if (entry->sme_bsb.left_bitmap != None) {
	    xcb_connection_t *conn = XtDisplayOfObject(w);
	    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, entry->sme_bsb.left_bitmap);
	    xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(conn, cookie, NULL);
	    if (geom == NULL) {
	 (void) sprintf(buf, "Isw SmeBSB Object: %s %s \"%s\".",
	  "Could not get Left Bitmap",
	  "geometry information for menu entry",
	  XtName(w));
	 XtAppError(XtWidgetToApplicationContext(w), buf);
	    } else {
	 width = geom->width;
	 height = geom->height;
	 entry->sme_bsb.left_depth = geom->depth;
	 free(geom);
	    }
#ifdef NEVER
	    if (entry->sme_bsb.left_depth != 1) {
	 (void) sprintf(buf, "Isw SmeBSB Object: %s \"%s\" %s.",
	  "Left Bitmap of entry",  XtName(w),
	  "is not one bit deep");
	 XtAppError(XtWidgetToApplicationContext(w), buf);
	    }
#endif
	}

	entry->sme_bsb.left_bitmap_width = (Dimension) width;
	entry->sme_bsb.left_bitmap_height = (Dimension) height;
	   } else {
	width = height = 0;

	if (entry->sme_bsb.right_bitmap != None) {
	    xcb_connection_t *conn = XtDisplayOfObject(w);
	    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, entry->sme_bsb.right_bitmap);
	    xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(conn, cookie, NULL);
	    if (geom == NULL) {
	 (void) sprintf(buf, "Isw SmeBSB Object: %s %s \"%s\".",
	  "Could not get Right Bitmap",
	  "geometry information for menu entry",
	  XtName(w));
	 XtAppError(XtWidgetToApplicationContext(w), buf);
	    } else {
	 width = geom->width;
	 height = geom->height;
	 entry->sme_bsb.right_depth = geom->depth;
	 free(geom);
	    }
#ifdef NEVER
	    if (entry->sme_bsb.right_depth != 1) {
	 (void) sprintf(buf, "Isw SmeBSB Object: %s \"%s\" %s.",
	  "Right Bitmap of entry", XtName(w),
	  "is not one bit deep");
	 XtAppError(XtWidgetToApplicationContext(w), buf);
	    }
#endif
	}

	entry->sme_bsb.right_bitmap_width = (Dimension) width;
	entry->sme_bsb.right_bitmap_height = (Dimension) height;
    }
}

/*      Function Name: CreateGCs
 *      Description: Creates all gc's for the simple menu widget.
 *      Arguments: w - the simple menu widget.
 *      Returns: none.
 */

static void
CreateGCs(Widget w)
{
    SmeBSBObject entry = (SmeBSBObject) w;
    xcb_create_gc_value_list_t values;
    XtGCMask mask;
#ifdef ISW_INTERNATIONALIZATION
    XtGCMask mask_i18n;
#endif

    memset(&values, 0, sizeof(values));
    values.foreground = XtParent(w)->core.background_pixel;
    values.background = entry->sme_bsb.foreground;
    values.graphics_exposures = 0;

    /* XCB Fix: Add NULL check for font before accessing fid */
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    if (entry->sme_bsb.font != NULL) {
        values.font = entry->sme_bsb.font->fid;
        mask |= XCB_GC_FONT;
    }

#ifdef ISW_INTERNATIONALIZATION
    mask_i18n = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    if ( entry->sme.international == True )
        entry->sme_bsb.rev_gc = XtAllocateGC(w, 0, mask_i18n, (xcb_create_gc_value_list_t*)&values, XCB_GC_FONT, 0 );
    else
#endif
        entry->sme_bsb.rev_gc = XtGetGC(w, mask, (xcb_create_gc_value_list_t*)&values);

    values.foreground = entry->sme_bsb.foreground;
    values.background = XtParent(w)->core.background_pixel;
#ifdef ISW_INTERNATIONALIZATION
    if ( entry->sme.international == True )
        entry->sme_bsb.norm_gc = XtAllocateGC(w, 0, mask_i18n, (xcb_create_gc_value_list_t*)&values, XCB_GC_FONT, 0 );
    else
#endif
        entry->sme_bsb.norm_gc = XtGetGC(w, mask, (xcb_create_gc_value_list_t*)&values);

    values.fill_style = XCB_FILL_STYLE_TILED;
    values.tile = IswCreateStippledPixmap(XtDisplayOfObject(w), XtWindowOfObject(XtParent(w)),
    	    entry->sme_bsb.foreground,
    	    XtParent(w)->core.background_pixel,
    	    XtParent(w)->core.depth);
    values.graphics_exposures = 0;

    /* XCB Fix: Add NULL check for font before accessing fid */
    XtGCMask gray_mask = XCB_GC_TILE | XCB_GC_FILL_STYLE | XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    if (entry->sme_bsb.font != NULL) {
        gray_mask |= XCB_GC_FONT;
    }

#ifdef ISW_INTERNATIONALIZATION
    if ( entry->sme.international == True )
        entry->sme_bsb.norm_gray_gc = XtAllocateGC(w, 0, mask_i18n, (xcb_create_gc_value_list_t*)&values, XCB_GC_FONT, 0 );
    else
#endif
        entry->sme_bsb.norm_gray_gc = XtGetGC(w, gray_mask, (xcb_create_gc_value_list_t*)&values);

    values.foreground ^= values.background;
    values.background = 0;
    values.function = XCB_GX_XOR;
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES | XCB_GC_FUNCTION;
    entry->sme_bsb.invert_gc = XtGetGC(w, mask, (xcb_create_gc_value_list_t*)&values);

    /* Create shadow GCs */
    values.foreground = entry->sme_bsb.top_shadow_pixel;
    values.background = XtParent(w)->core.background_pixel;
    values.graphics_exposures = 0;
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    entry->sme_bsb.top_shadow_GC = XtGetGC(w, mask, (xcb_create_gc_value_list_t*)&values);

    values.foreground = entry->sme_bsb.bot_shadow_pixel;
    entry->sme_bsb.bot_shadow_GC = XtGetGC(w, mask, (xcb_create_gc_value_list_t*)&values);

    values.foreground = XtParent(w)->core.background_pixel;
    entry->sme_bsb.erase_GC = XtGetGC(w, mask, (xcb_create_gc_value_list_t*)&values);
}

/*      Function Name: DestroyGCs
 *      Description: Removes all gc's for the simple menu widget.
 *      Arguments: w - the simple menu widget.
 *      Returns: none.
 */

static void
DestroyGCs(Widget w)
{
    SmeBSBObject entry = (SmeBSBObject) w;

    XtReleaseGC(w, entry->sme_bsb.norm_gc);
    XtReleaseGC(w, entry->sme_bsb.norm_gray_gc);
    XtReleaseGC(w, entry->sme_bsb.rev_gc);
    XtReleaseGC(w, entry->sme_bsb.invert_gc);
    XtReleaseGC(w, entry->sme_bsb.top_shadow_GC);
    XtReleaseGC(w, entry->sme_bsb.bot_shadow_GC);
    XtReleaseGC(w, entry->sme_bsb.erase_GC);
}

/*      Function Name: FlipColors
 *      Description: Invert the colors of the current entry.
 *      Arguments: w - the bsb menu entry widget.
 *      Returns: none.
 */

static void
FlipColors(Widget w)
{
    SmeBSBObject entry = (SmeBSBObject) w;
    SmeBSBObjectClass oclass = (SmeBSBObjectClass) XtClass (w);
    SimpleMenuWidget smw = (SimpleMenuWidget) XtParent (w);
    Dimension s = smw->simple_menu.shadow_width;

    if (entry->sme_bsb.set_values_area_cleared) {
 entry->sme_bsb.shadowed = False;
 return;
    }

    if (entry->sme_bsb.shadow_width > 0) {
 DrawShadows(w);
    } else {
 /*
  * invert_gc uses XOR function which Cairo doesn't support directly.
  * Keep XCB fallback for this rare case (shadow_width == 0).
  */
 xcb_connection_t *conn = XtDisplayOfObject(w);
 xcb_rectangle_t rect = {s, (int) entry->rectangle.y,
     (unsigned int) entry->rectangle.width - 2 * s,
     (unsigned int) entry->rectangle.height};
 xcb_poly_fill_rectangle(conn, XtWindowOfObject(w),
     entry->sme_bsb.invert_gc, 1, &rect);
 xcb_flush(conn);
    }
}

/*	Function Name: DrawShadows
 *	Description: Draws the 3D shadows for the menu entry.
 *	Arguments: w - the menu entry widget.
 *	Returns: none.
 */
static void
DrawShadows(Widget w)
{
    SmeBSBObject entry = (SmeBSBObject) w;
    SimpleMenuWidget smw = (SimpleMenuWidget) XtParent(w);
    Dimension s = entry->sme_bsb.shadow_width;
    Dimension ps = smw->simple_menu.shadow_width;
    xcb_point_t pt[6];

    /*
     * Draw the shadows using the core part width and height,
     * and the shadow_width.
     *
     * No point to do anything if the shadow_width is 0 or the
     * widget has not been realized.
     */
    if (s > 0 && XtIsRealized(w))
    {
 Dimension h = entry->rectangle.height;
 Dimension w_dim = entry->rectangle.width - ps;
 Dimension x = entry->rectangle.x + ps;
 Dimension y = entry->rectangle.y;
 xcb_connection_t *dpy = XtDisplayOfObject(w);
 xcb_window_t win = XtWindowOfObject(w);
 xcb_gcontext_t top, bot;

 /* Try to create Cairo rendering context if not yet created */
 if (!entry->sme_bsb.render_ctx && w_dim > 0 && h > 0 &&
     w_dim < 32767 && h < 32767) {
     entry->sme_bsb.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
 }

 if (entry->sme_bsb.shadowed)
 {
     top = entry->sme_bsb.top_shadow_GC;
     bot = entry->sme_bsb.bot_shadow_GC;
 }
 else
     top = bot = entry->sme_bsb.erase_GC;

 /* Use Cairo rendering if available */
 if (entry->sme_bsb.render_ctx) {
     Pixel top_pixel, bot_pixel;
     Widget parent = XtParent(w);
     
     /* Determine which pixels to use based on shadowed flag */
     if (entry->sme_bsb.shadowed) {
  top_pixel = entry->sme_bsb.top_shadow_pixel;
  bot_pixel = entry->sme_bsb.bot_shadow_pixel;
     } else {
  /* Use parent background for both when not shadowed */
  top_pixel = bot_pixel = parent->core.background_pixel;
     }
     
     ISWRenderBegin(entry->sme_bsb.render_ctx);
     
     /* top-left shadow */
     pt[0].x = x;		pt[0].y = y + h;
     pt[1].x = x;		pt[1].y = y;
     pt[2].x = w_dim;		pt[2].y = y;
     pt[3].x = w_dim - s;	pt[3].y = y + s;
     pt[4].x = ps + s;		pt[4].y = y + s;
     pt[5].x = ps + s;		pt[5].y = y + h - s;
     ISWRenderSetColor(entry->sme_bsb.render_ctx, top_pixel);
     ISWRenderFillPolygon(entry->sme_bsb.render_ctx, pt, 6);

     /* bottom-right shadow */
     pt[1].x = w_dim;	pt[1].y = y + h;
     pt[4].x = w_dim - s;	pt[4].y = y + h - s;
     ISWRenderSetColor(entry->sme_bsb.render_ctx, bot_pixel);
     ISWRenderFillPolygon(entry->sme_bsb.render_ctx, pt, 6);

     ISWRenderEnd(entry->sme_bsb.render_ctx);
     return;  /* Done with Cairo rendering */
 }

 /* XCB fallback rendering path */
 /* top-left shadow */
 pt[0].x = x;		pt[0].y = y + h;
 pt[1].x = x;		pt[1].y = y;
 pt[2].x = w_dim;	pt[2].y = y;
 pt[3].x = w_dim - s;	pt[3].y = y + s;
 pt[4].x = ps + s;	pt[4].y = y + s;
 pt[5].x = ps + s;	pt[5].y = y + h - s;
 xcb_fill_poly(dpy, win, top, XCB_POLY_SHAPE_COMPLEX,
        XCB_COORD_MODE_ORIGIN, 6, pt);

 /* bottom-right shadow */
 pt[1].x = w_dim;	pt[1].y = y + h;
 pt[4].x = w_dim - s;	pt[4].y = y + h - s;
 xcb_fill_poly(dpy, win, bot, XCB_POLY_SHAPE_COMPLEX,
        XCB_COORD_MODE_ORIGIN, 6, pt);
 
 xcb_flush(dpy);
    }
}
