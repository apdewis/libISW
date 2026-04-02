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
#include <ISW/ISWImage.h>
#include <cairo.h>
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

#define offset(field) XtOffsetOf(SmeBSBRec, sme_bsb.field)

static XtResource resources[] = {
  {XtNlabel,  XtCLabel, XtRString, sizeof(String),
     offset(label), XtRString, NULL},
  {XtNvertSpace,  XtCVertSpace, XtRInt, sizeof(int),
     offset(vert_space), XtRImmediate, (XtPointer) 25},
  {XtNleftImage, XtCLeftImage, XtRString, sizeof(String),
     offset(left_image_source), XtRImmediate, (XtPointer)NULL},
  {XtNjustify, XtCJustify, XtRJustify, sizeof(XtJustify),
     offset(justify), XtRImmediate, (XtPointer) XtJustifyLeft},
  {XtNrightImage, XtCRightImage, XtRString, sizeof(String),
     offset(right_image_source), XtRImmediate, (XtPointer)NULL},
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
static void DrawBitmaps(Widget, xcb_gcontext_t);
static void GetImageInfo(Widget, Boolean);
static void CreateGCs(Widget);
static void DestroyGCs(Widget);
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

    /* HiDPI: scale dimension resources */
    entry->sme_bsb.left_margin = ISWScaleDim(new, entry->sme_bsb.left_margin);
    entry->sme_bsb.right_margin = ISWScaleDim(new, entry->sme_bsb.right_margin);

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

    /* Load images from string resources */
    if (entry->sme_bsb.left_image_source) {
        entry->sme_bsb.left_image_source = XtNewString(entry->sme_bsb.left_image_source);
        entry->sme_bsb.left_image = ISWImageLoad(entry->sme_bsb.left_image_source,
                                                  96.0, NULL);
    }
    if (entry->sme_bsb.right_image_source) {
        entry->sme_bsb.right_image_source = XtNewString(entry->sme_bsb.right_image_source);
        entry->sme_bsb.right_image = ISWImageLoad(entry->sme_bsb.right_image_source,
                                                   96.0, NULL);
    }
    if (entry->sme_bsb.left_image) {
        entry->sme_bsb.left_image_width  = (Dimension)ISWImageGetWidth(entry->sme_bsb.left_image);
        entry->sme_bsb.left_image_height = (Dimension)ISWImageGetHeight(entry->sme_bsb.left_image);
    }
    if (entry->sme_bsb.right_image) {
        entry->sme_bsb.right_image_width  = (Dimension)ISWImageGetWidth(entry->sme_bsb.right_image);
        entry->sme_bsb.right_image_height = (Dimension)ISWImageGetHeight(entry->sme_bsb.right_image);
    }

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

    /* render_ctx lives on the parent SimpleMenu — nothing to destroy here */

    if (entry->sme_bsb.left_image_source)
        XtFree(entry->sme_bsb.left_image_source);
    if (entry->sme_bsb.right_image_source)
        XtFree(entry->sme_bsb.right_image_source);
    if (entry->sme_bsb.left_image) {
        ISWImageDestroy(entry->sme_bsb.left_image);
        entry->sme_bsb.left_image = NULL;
    }
    if (entry->sme_bsb.right_image) {
        ISWImageDestroy(entry->sme_bsb.right_image);
        entry->sme_bsb.right_image = NULL;
    }

    DestroyGCs(w);
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
    xcb_gcontext_t gc;
    SmeBSBObject entry = (SmeBSBObject) w;
    Dimension s = 1;  /* inset from SimpleMenu's 1px drawn border */
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
     font_ascent = ISWScaledFontAscent(XtParent(w), entry->sme_bsb.font);
     font_descent = ISWScaledFontHeight(XtParent(w), entry->sme_bsb.font)
                    - font_ascent;
 } else {
     font_ascent = ISWScaleDim(w, 11);
     font_descent = ISWScaleDim(w, 3);
 }
    }
    y_loc = entry->rectangle.y;

    /* Use the parent SimpleMenu's shared render context */
    ISWRenderContext *ctx = ((SimpleMenuWidget)XtParent(w))->simple_menu.render_ctx;

    if (XtIsSensitive(w) && XtIsSensitive( XtParent(w) ) ) {
 if ( w == IswSimpleMenuGetActiveEntry(XtParent(w)) ) {
     if (ctx) {
         ISWRenderBegin(ctx);
         ISWRenderSetColor(ctx, entry->sme_bsb.foreground);
         ISWRenderFillRectangle(ctx,
                                s, y_loc + s,
                                entry->rectangle.width - 2 * s,
                                entry->rectangle.height - 2 * s);
         ISWRenderEnd(ctx);
     }
     gc = entry->sme_bsb.rev_gc;
 }
 else {
     if (ctx) {
         ISWRenderBegin(ctx);
         ISWRenderSetColor(ctx, XtParent(w)->core.background_pixel);
         ISWRenderFillRectangle(ctx,
                                s, y_loc + s,
                                entry->rectangle.width - 2 * s,
                                entry->rectangle.height - 2 * s);
         ISWRenderEnd(ctx);
     }
     gc = entry->sme_bsb.norm_gc;
 }
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
		    t_width = ISWScaledTextWidth(XtParent(w), entry->sme_bsb.font, label, len);

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
		    t_width = ISWScaledTextWidth(XtParent(w), entry->sme_bsb.font, label, len);

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

            if (ctx) {
                Pixel text_color;
                if (gc == entry->sme_bsb.rev_gc)
                    text_color = XtParent(w)->core.background_pixel;
                else
                    text_color = entry->sme_bsb.foreground;
                ISWRenderBegin(ctx);
                ISWRenderSetColor(ctx, text_color);
                if (entry->sme_bsb.font)
                    ISWRenderSetFont(ctx, entry->sme_bsb.font);
                ISWRenderDrawString(ctx, label, len,
                                    x_loc + s, y_loc);
                ISWRenderEnd(ctx);
            }
        }

	if (entry->sme_bsb.underline >= 0 && entry->sme_bsb.underline < len) {
	    int ul = entry->sme_bsb.underline;
	    int ul_x1_loc = x_loc + s;
	    int ul_wid;
	    Pixel underline_color;

	    if (ul != 0)
	 ul_x1_loc += ISWScaledTextWidth(XtParent(w), entry->sme_bsb.font, label, ul);
	    ul_wid = ISWScaledTextWidth(XtParent(w), entry->sme_bsb.font, &label[ul], 1) - 2;
	    
	    /* Determine underline color based on which xcb_gcontext_t is being used */
	    if (gc == entry->sme_bsb.rev_gc) {
	        underline_color = entry->sme_bsb.foreground;
	    } else {
	        underline_color = XtParent(w)->core.background_pixel;
	    }
	    
	    /* Draw underline using Cairo or XCB */
	    if (ctx) {
	        ISWRenderBegin(ctx);
	        ISWRenderSetColor(ctx, underline_color);
	        ISWRenderDrawLine(ctx,
	                          ul_x1_loc, y_loc + 1,
	                          ul_x1_loc + ul_wid, y_loc + 1);
	        ISWRenderEnd(ctx);
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

    if (entry->sme_bsb.left_image_source != old_entry->sme_bsb.left_image_source) {
        if (old_entry->sme_bsb.left_image_source)
            XtFree(old_entry->sme_bsb.left_image_source);
        if (old_entry->sme_bsb.left_image)
            ISWImageDestroy(old_entry->sme_bsb.left_image);
        entry->sme_bsb.left_image_source = entry->sme_bsb.left_image_source
            ? XtNewString(entry->sme_bsb.left_image_source) : NULL;
        entry->sme_bsb.left_image = entry->sme_bsb.left_image_source
            ? ISWImageLoad(entry->sme_bsb.left_image_source, 96.0, NULL) : NULL;
        GetImageInfo(new, TRUE);
        ret_val = TRUE;
    }

    if (entry->sme_bsb.left_margin != old_entry->sme_bsb.left_margin)
	ret_val = TRUE;

    if (entry->sme_bsb.right_image_source != old_entry->sme_bsb.right_image_source) {
        if (old_entry->sme_bsb.right_image_source)
            XtFree(old_entry->sme_bsb.right_image_source);
        if (old_entry->sme_bsb.right_image)
            ISWImageDestroy(old_entry->sme_bsb.right_image);
        entry->sme_bsb.right_image_source = entry->sme_bsb.right_image_source
            ? XtNewString(entry->sme_bsb.right_image_source) : NULL;
        entry->sme_bsb.right_image = entry->sme_bsb.right_image_source
            ? ISWImageLoad(entry->sme_bsb.right_image_source, 96.0, NULL) : NULL;
        GetImageInfo(new, FALSE);
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

    if ( ((mode & XCB_CONFIG_WINDOW_WIDTH) && (intended->width != width)) ||
	 !(mode & XCB_CONFIG_WINDOW_WIDTH) ) {
	return_val->request_mode |= XCB_CONFIG_WINDOW_WIDTH;
	return_val->width = width;
	ret_val = XtGeometryAlmost;
    }

    if ( ((mode & XCB_CONFIG_WINDOW_HEIGHT) && (intended->height != height)) ||
	 !(mode & XCB_CONFIG_WINDOW_HEIGHT) ) {
	return_val->request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
	return_val->height = height;
	ret_val = XtGeometryAlmost;
    }

    if (ret_val == XtGeometryAlmost) {
	mode = return_val->request_mode;

	if ( ((mode & XCB_CONFIG_WINDOW_WIDTH) && (width == entry->rectangle.width)) &&
	     ((mode & XCB_CONFIG_WINDOW_HEIGHT) && (height == entry->rectangle.height)) )
	    return(XtGeometryNo);
    }

    entry->rectangle.width = width;
    entry->rectangle.height = height;

    return(ret_val);
}

static void
Highlight(Widget w)
{
    Redisplay(w, NULL, (xcb_xfixes_region_t)0);
}

static void
Unhighlight(Widget w)
{
    Redisplay(w, NULL, (xcb_xfixes_region_t)0);
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
  *width = ISWScaledTextWidth(XtParent(w), entry->sme_bsb.font,
    entry->sme_bsb.label, strlen(entry->sme_bsb.label));

     *height = ISWScaledFontHeight(XtParent(w), entry->sme_bsb.font);
 } else {
     /* No font available - use defaults */
     if (entry->sme_bsb.label == NULL)
  *width = 0;
     else
  *width = ISWScaleDim(w, strlen(entry->sme_bsb.label) * 8);
     *height = ISWScaleDim(w, 14);
 }
    }

    *width += entry->sme_bsb.left_margin + entry->sme_bsb.right_margin;

    h = (entry->sme_bsb.left_image_height > entry->sme_bsb.right_image_height)
	    ? entry->sme_bsb.left_image_height : entry->sme_bsb.right_image_height;
    if (h > *height) *height = h;
    *height = ((int)*height * (100 + entry->sme_bsb.vert_space)) / 100;
}

/*      Function Name: DrawBitmaps
 *      Description: Draws left and right bitmaps.
 *      Arguments: w - the simple menu widget.
 *                 gc - graphics context to use for drawing.
 *      Returns: none
 */

static void
DrawBitmaps(Widget w, xcb_gcontext_t gc)
{
    int x_loc, y_loc;
    SmeBSBObject entry = (SmeBSBObject) w;
    unsigned int rw, rh;
    const unsigned char *pixels;
    ISWRenderContext *ctx = ((SimpleMenuWidget)XtParent(w))->simple_menu.render_ctx;

    (void)gc;  /* gc no longer used — images are RGBA */

    if (!entry->sme_bsb.left_image && !entry->sme_bsb.right_image)
        return;

    /* Draw left image */
    if (entry->sme_bsb.left_image) {
        x_loc = (int)(entry->sme_bsb.left_margin - entry->sme_bsb.left_image_width) / 2;
        y_loc = entry->rectangle.y +
                (int)(entry->rectangle.height - entry->sme_bsb.left_image_height) / 2;

        pixels = ISWImageRasterize(entry->sme_bsb.left_image,
                                   entry->sme_bsb.left_image_width,
                                   entry->sme_bsb.left_image_height,
                                   &rw, &rh);
        if (pixels && ctx) {
            ISWRenderBegin(ctx);
            ISWRenderDrawImageRGBA(ctx, pixels, rw, rh,
                                   x_loc, y_loc,
                                   entry->sme_bsb.left_image_width,
                                   entry->sme_bsb.left_image_height);
            ISWRenderEnd(ctx);
        }
    }

    /* Draw right image */
    if (entry->sme_bsb.right_image) {
        x_loc = entry->rectangle.width -
                (int)(entry->sme_bsb.right_margin + entry->sme_bsb.right_image_width) / 2;
        y_loc = entry->rectangle.y +
                (int)(entry->rectangle.height - entry->sme_bsb.right_image_height) / 2;

        pixels = ISWImageRasterize(entry->sme_bsb.right_image,
                                   entry->sme_bsb.right_image_width,
                                   entry->sme_bsb.right_image_height,
                                   &rw, &rh);
        if (pixels && ctx) {
            ISWRenderBegin(ctx);
            ISWRenderDrawImageRGBA(ctx, pixels, rw, rh,
                                   x_loc, y_loc,
                                   entry->sme_bsb.right_image_width,
                                   entry->sme_bsb.right_image_height);
            ISWRenderEnd(ctx);
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
GetImageInfo(Widget w, Boolean is_left)
{
    SmeBSBObject entry = (SmeBSBObject) w;

    if (is_left) {
        if (entry->sme_bsb.left_image) {
            entry->sme_bsb.left_image_width  = (Dimension)ISWImageGetWidth(entry->sme_bsb.left_image);
            entry->sme_bsb.left_image_height = (Dimension)ISWImageGetHeight(entry->sme_bsb.left_image);
        } else {
            entry->sme_bsb.left_image_width  = 0;
            entry->sme_bsb.left_image_height = 0;
        }
    } else {
        if (entry->sme_bsb.right_image) {
            entry->sme_bsb.right_image_width  = (Dimension)ISWImageGetWidth(entry->sme_bsb.right_image);
            entry->sme_bsb.right_image_height = (Dimension)ISWImageGetHeight(entry->sme_bsb.right_image);
        } else {
            entry->sme_bsb.right_image_width  = 0;
            entry->sme_bsb.right_image_height = 0;
        }
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
}


