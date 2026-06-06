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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <X11/Xos.h>
#include <ISW/ISWInit.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeBSBP.h>
#include <ISW/FocusMgrI.h>
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

#define SME_SUBMENU_ARROW_SIZE 6

#define offset(field) IswOffsetOf(SmeBSBRec, sme_bsb.field)

static IswResource resources[] = {
  {IswNlabel,  IswCLabel, IswRString, sizeof(String),
     offset(label), IswRString, NULL},
  {IswNvertSpace,  IswCVertSpace, IswRInt, sizeof(int),
     offset(vert_space), IswRImmediate, (IswPointer) 25},
  {IswNleftImage, IswCLeftImage, IswRString, sizeof(String),
     offset(left_image_source), IswRImmediate, (IswPointer)NULL},
  {IswNjustify, IswCJustify, IswRJustify, sizeof(IswJustify),
     offset(justify), IswRImmediate, (IswPointer) IswJustifyLeft},
  {IswNrightImage, IswCRightImage, IswRString, sizeof(String),
     offset(right_image_source), IswRImmediate, (IswPointer)NULL},
  {IswNleftMargin,  IswCHorizontalMargins, IswRDimension, sizeof(Dimension),
     offset(left_margin), IswRImmediate, (IswPointer) 4},
  {IswNrightMargin,  IswCHorizontalMargins, IswRDimension, sizeof(Dimension),
     offset(right_margin), IswRImmediate, (IswPointer) 4},
  {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
     offset(foreground), IswRString, IswDefaultForeground},
  {IswNfont,  IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
     offset(font), IswRString, IswDefaultFont},
  {IswNmenuName, IswCMenuName, IswRString, sizeof(String),
     offset(menu_name), IswRImmediate, (IswPointer) NULL},
  {IswNunderline,  IswCIndex, IswRInt, sizeof(int),
     offset(underline), IswRImmediate, (IswPointer) -1},
  {IswNmnemonicKey, IswCMnemonicKey, IswRInt, sizeof(xcb_keysym_t),
     offset(mnemonic_key), IswRImmediate, (IswPointer) 0},
  {IswNaccelerator, IswCAccelerator, IswRString, sizeof(String),
     offset(accelerator), IswRString, NULL},
  {IswNacceleratorText, IswCAcceleratorText, IswRString, sizeof(String),
     offset(accelerator_text), IswRString, NULL},
};
#undef offset

/*
 * Semi Public function definitions.
 */

static void Redisplay(Widget, IswEvent *, xcb_xfixes_region_t);
static void Destroy(Widget);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Highlight(Widget);
static void Unhighlight(Widget);
static void ClassInitialize(void);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static IswGeometryResult QueryGeometry(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

/*
 * Private Function Definitions.
 */

static void GetDefaultSize(Widget, Dimension *, Dimension *);
static void DrawBitmaps(Widget, Boolean);
static void GetImageInfo(Widget, Boolean);
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
    /* resource_count     */	IswNumber(resources),
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
    /* set_values_almost  */	IswInheritSetValuesAlmost,
    /* get_values_hook    */	NULL,
    /* accept_focus       */    NULL,
    /* intrinsics version */	IswVersion,
    /* callback offsets   */    NULL,
    /* tm_table		  */    NULL,
    /* query_geometry	  */    QueryGeometry,
    /* display_accelerator*/    NULL,
    /* extension	  */    NULL
  },{
    /* SimpleMenuClass Fields */
    /* highlight          */	Highlight,
    /* unhighlight        */	Unhighlight,
    /* notify             */	IswInheritNotify,
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
    IswSetTypeConverter( IswRString, IswRJustify, ISWCvtStringToJustify,
		    (IswConvertArgList)NULL, 0, IswCacheNone, (IswDestructor)NULL );
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
    entry->sme_bsb.left_margin = (entry->sme_bsb.left_margin);
    entry->sme_bsb.right_margin = (entry->sme_bsb.right_margin);

    if (entry->sme_bsb.font == NULL) {
	IswAppWarning(IswWidgetToApplicationContext(new),
		     "SmeBSB widget: font is NULL - text rendering will fail");
    }

    if (entry->sme_bsb.label == NULL)
	entry->sme_bsb.label = IswName(new);
    else
	entry->sme_bsb.label = IswNewString( entry->sme_bsb.label );

    /* Load images from string resources */
    if (entry->sme_bsb.left_image_source) {
        entry->sme_bsb.left_image_source = IswNewString(entry->sme_bsb.left_image_source);
        entry->sme_bsb.left_image = ISWImageLoad(entry->sme_bsb.left_image_source,
                                                  96.0, NULL);
    }
    if (entry->sme_bsb.right_image_source) {
        entry->sme_bsb.right_image_source = IswNewString(entry->sme_bsb.right_image_source);
        entry->sme_bsb.right_image = ISWImageLoad(entry->sme_bsb.right_image_source,
                                                   96.0, NULL);
    }
    if (entry->sme_bsb.left_image) {
        float tmp_w = ISWImageGetWidth(entry->sme_bsb.left_image);
        float tmp_h = ISWImageGetHeight(entry->sme_bsb.left_image);
        entry->sme_bsb.left_image_width  = (Dimension)tmp_w;
        entry->sme_bsb.left_image_height = (Dimension)tmp_h;
    }
    if (entry->sme_bsb.right_image) {
        float tmp_w = ISWImageGetWidth(entry->sme_bsb.right_image);
        float tmp_h = ISWImageGetHeight(entry->sme_bsb.right_image);
        entry->sme_bsb.right_image_width  = (Dimension)tmp_w;
        entry->sme_bsb.right_image_height = (Dimension)tmp_h;
    }

    if (entry->sme_bsb.accelerator)
	entry->sme_bsb.accelerator = IswNewString(entry->sme_bsb.accelerator);
    if (entry->sme_bsb.accelerator_text)
	entry->sme_bsb.accelerator_text = IswNewString(entry->sme_bsb.accelerator_text);

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
        IswFree(entry->sme_bsb.left_image_source);
    if (entry->sme_bsb.right_image_source)
        IswFree(entry->sme_bsb.right_image_source);
    if (entry->sme_bsb.left_image) {
        ISWImageDestroy(entry->sme_bsb.left_image);
        entry->sme_bsb.left_image = NULL;
    }
    if (entry->sme_bsb.right_image) {
        ISWImageDestroy(entry->sme_bsb.right_image);
        entry->sme_bsb.right_image = NULL;
    }

    if (entry->sme_bsb.label != IswName(w))
	IswFree(entry->sme_bsb.label);
    if (entry->sme_bsb.accelerator)
	IswFree(entry->sme_bsb.accelerator);
    if (entry->sme_bsb.accelerator_text)
	IswFree(entry->sme_bsb.accelerator_text);
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
Redisplay(Widget w, IswEvent *event, xcb_xfixes_region_t region)
{
    Boolean highlighted_active = False;
    SmeBSBObject entry = (SmeBSBObject) w;
    Dimension s = 1;  /* inset from SimpleMenu's 1px drawn border */
    int	font_ascent = 0, font_descent = 0, y_loc;

    entry->sme_bsb.set_values_area_cleared = FALSE;
    {
 /* XCB Fix: Add NULL check for font before accessing fields */
 if (entry->sme_bsb.font != NULL) {
     font_ascent = ISWScaledFontAscent(IswParent(w), entry->sme_bsb.font);
     font_descent = ISWScaledFontHeight(IswParent(w), entry->sme_bsb.font)
                    - font_ascent;
 } else {
     font_ascent = (11);
     font_descent = (3);
 }
    }
    y_loc = entry->rectangle.y;

    /* Use the parent SimpleMenu's shared render context */
    ISWRenderContext *ctx = ((SimpleMenuWidget)IswParent(w))->simple_menu.render_ctx;

    if (IswIsSensitive(w) && IswIsSensitive( IswParent(w) ) ) {
 if ( w == IswSimpleMenuGetActiveEntry(IswParent(w)) ) {
     if (ctx) {
         ISWRenderBegin(ctx);
         ISWRenderSetColor(ctx, entry->sme_bsb.foreground);
         ISWRenderFillRectangle(ctx,
                                s, y_loc + s,
                                entry->rectangle.width - 2 * s,
                                entry->rectangle.height - 2 * s);
         ISWRenderEnd(ctx);
     }
     highlighted_active = True;
 }
 else {
     if (ctx) {
         ISWRenderBegin(ctx);
         ISWRenderSetColor(ctx, IswParent(w)->core.background_pixel);
         ISWRenderFillRectangle(ctx,
                                s, y_loc + s,
                                entry->rectangle.width - 2 * s,
                                entry->rectangle.height - 2 * s);
         ISWRenderEnd(ctx);
     }
 }
    }

    if (entry->sme_bsb.label != NULL) {
	int x_loc = entry->sme_bsb.left_margin;
	int len = strlen(entry->sme_bsb.label);
	int width, t_width;
	const char * label = entry->sme_bsb.label;

	switch(entry->sme_bsb.justify) {
	    case IswJustifyCenter:
		t_width = ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font, label, len);

		width = entry->rectangle.width -
				(entry->sme_bsb.left_margin +
				entry->sme_bsb.right_margin);
		x_loc += (width - t_width)/2;
		break;
	    case IswJustifyRight:
		t_width = ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font, label, len);

		x_loc = entry->rectangle.width -
				(entry->sme_bsb.right_margin + t_width);
		break;
	    case IswJustifyLeft:
	    default:
		break;
	}

	/* this will center the text in the gadget top-to-bottom */

        {
            y_loc += ((int)entry->rectangle.height -
                      (font_ascent + font_descent)) / 2 + font_ascent;

            if (ctx) {
                Pixel text_color = highlighted_active
                    ? IswParent(w)->core.background_pixel
                    : entry->sme_bsb.foreground;
                ISWRenderBegin(ctx);
                ISWRenderSetColor(ctx, text_color);
                if (entry->sme_bsb.font)
                    ISWRenderSetFont(ctx, entry->sme_bsb.font);
                ISWRenderDrawString(ctx, label, len,
                                    x_loc + s, y_loc);
                ISWRenderEnd(ctx);
            }
        }

	{
	    /* Explicit IswNunderline wins; else use IswNmnemonicKey if Alt is
	     * held OR this menu was opened via a mnemonic (in which case the
	     * underlines stay until the menu is dismissed). */
	    int ul = entry->sme_bsb.underline;
	    Boolean from_mnemonic = False;
	    if (ul < 0 &&
	        _IswFocusMgrShowMnemonicsForMenu(IswParent(w)) &&
	        entry->sme_bsb.mnemonic_key != 0) {
	        ul = _IswFocusMgrFindMnemonicIndex(label, entry->sme_bsb.mnemonic_key);
	        from_mnemonic = (ul >= 0);
	    }
	    if (ul >= 0 && ul < len) {
	        int ul_x1_loc = x_loc + s;
	        int ul_wid;
	        Pixel underline_color;

	        if (ul != 0)
	            ul_x1_loc += ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font, label, ul);
	        ul_wid = ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font, &label[ul], 1) - 2;

	        /* Mnemonic underlines should always be visible (foreground),
	         * even when not highlighted. The legacy IswNunderline path
	         * retains its old highlight-driven toggle behavior. */
	        if (from_mnemonic)
	            underline_color = entry->sme_bsb.foreground;
	        else
	            underline_color = highlighted_active
	                ? entry->sme_bsb.foreground
	                : IswParent(w)->core.background_pixel;

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
    }

    if (entry->sme_bsb.accelerator_text != NULL && ctx) {
	int accel_len = strlen(entry->sme_bsb.accelerator_text);
	int accel_w = ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font,
					 entry->sme_bsb.accelerator_text, accel_len);
	int accel_x = entry->rectangle.width - entry->sme_bsb.right_margin - accel_w;
	Pixel accel_color = highlighted_active
	    ? IswParent(w)->core.background_pixel
	    : entry->sme_bsb.foreground;

	ISWRenderBegin(ctx);
	ISWRenderSetColor(ctx, accel_color);
	if (entry->sme_bsb.font)
	    ISWRenderSetFont(ctx, entry->sme_bsb.font);
	ISWRenderDrawString(ctx, entry->sme_bsb.accelerator_text, accel_len,
			    accel_x, y_loc);
	ISWRenderEnd(ctx);
    }

    DrawBitmaps(w, highlighted_active);

    if (entry->sme_bsb.menu_name != NULL && ctx) {
	int sz = SME_SUBMENU_ARROW_SIZE;
	int ax = entry->rectangle.width - entry->sme_bsb.right_margin / 2 - sz;
	int ay = entry->rectangle.y + entry->rectangle.height / 2;
	xcb_point_t tri[3];
	Pixel arrow_color = highlighted_active
	    ? IswParent(w)->core.background_pixel
	    : entry->sme_bsb.foreground;

	tri[0].x = ax;        tri[0].y = ay - sz;
	tri[1].x = ax;        tri[1].y = ay + sz;
	tri[2].x = ax + sz;   tri[2].y = ay;

	ISWRenderBegin(ctx);
	ISWRenderSetColor(ctx, arrow_color);
	ISWRenderFillPolygon(ctx, tri, 3);
	ISWRenderEnd(ctx);
    }
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
    Widget parent = IswParent(new);
    SmeBSBObject entry = (SmeBSBObject) new;
    SmeBSBObject old_entry = (SmeBSBObject) current;
    Boolean ret_val = FALSE;

    if (old_entry->sme_bsb.label != entry->sme_bsb.label) {
        if (old_entry->sme_bsb.label != IswName( new ) )
	    IswFree( (char *) old_entry->sme_bsb.label );

	if (entry->sme_bsb.label != IswName(new) )
	    entry->sme_bsb.label = IswNewString( entry->sme_bsb.label );

	ret_val = TRUE;
    }

    if (entry->sme_bsb.underline != old_entry->sme_bsb.underline)
	ret_val = TRUE;

    if (entry->rectangle.sensitive != old_entry->rectangle.sensitive)
	ret_val = TRUE;

    /* XCB Fix: Add NULL checks before comparing font->fid */
    Bool font_changed = False;
    if (old_entry->sme.international == False) {
	if (old_entry->sme_bsb.font != NULL && entry->sme_bsb.font != NULL) {
	    font_changed = (old_entry->sme_bsb.font->fid != entry->sme_bsb.font->fid);
	} else if (old_entry->sme_bsb.font != entry->sme_bsb.font) {
	    /* One is NULL and the other isn't */
	    font_changed = True;
	}
    }

    if ( font_changed ||
	(old_entry->sme_bsb.foreground != entry->sme_bsb.foreground) ) {
	ret_val = TRUE;
    }

    if (entry->sme_bsb.left_image_source != old_entry->sme_bsb.left_image_source) {
        if (old_entry->sme_bsb.left_image_source)
            IswFree(old_entry->sme_bsb.left_image_source);
        if (old_entry->sme_bsb.left_image)
            ISWImageDestroy(old_entry->sme_bsb.left_image);
        entry->sme_bsb.left_image_source = entry->sme_bsb.left_image_source
            ? IswNewString(entry->sme_bsb.left_image_source) : NULL;
        entry->sme_bsb.left_image = entry->sme_bsb.left_image_source
            ? ISWImageLoad(entry->sme_bsb.left_image_source, 96.0, NULL) : NULL;
        GetImageInfo(new, TRUE);
        ret_val = TRUE;
    }

    if (entry->sme_bsb.left_margin != old_entry->sme_bsb.left_margin)
	ret_val = TRUE;

    if (entry->sme_bsb.right_image_source != old_entry->sme_bsb.right_image_source) {
        if (old_entry->sme_bsb.right_image_source)
            IswFree(old_entry->sme_bsb.right_image_source);
        if (old_entry->sme_bsb.right_image)
            ISWImageDestroy(old_entry->sme_bsb.right_image);
        entry->sme_bsb.right_image_source = entry->sme_bsb.right_image_source
            ? IswNewString(entry->sme_bsb.right_image_source) : NULL;
        entry->sme_bsb.right_image = entry->sme_bsb.right_image_source
            ? ISWImageLoad(entry->sme_bsb.right_image_source, 96.0, NULL) : NULL;
        GetImageInfo(new, FALSE);
        ret_val = TRUE;
    }

    if (entry->sme_bsb.right_margin != old_entry->sme_bsb.right_margin)
	ret_val = TRUE;

    if (old_entry->sme_bsb.accelerator != entry->sme_bsb.accelerator) {
	if (old_entry->sme_bsb.accelerator)
	    IswFree(old_entry->sme_bsb.accelerator);
	entry->sme_bsb.accelerator = entry->sme_bsb.accelerator
	    ? IswNewString(entry->sme_bsb.accelerator) : NULL;
	ret_val = TRUE;
    }

    if (old_entry->sme_bsb.accelerator_text != entry->sme_bsb.accelerator_text) {
	if (old_entry->sme_bsb.accelerator_text)
	    IswFree(old_entry->sme_bsb.accelerator_text);
	entry->sme_bsb.accelerator_text = entry->sme_bsb.accelerator_text
	    ? IswNewString(entry->sme_bsb.accelerator_text) : NULL;
	ret_val = TRUE;
    }

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

static IswGeometryResult
QueryGeometry(Widget w, IswWidgetGeometry *intended, IswWidgetGeometry *return_val)
{
    SmeBSBObject entry = (SmeBSBObject) w;
    Dimension width, height;
    IswGeometryResult ret_val = IswGeometryYes;
    IswGeometryMask mode = intended->request_mode;

    GetDefaultSize(w, &width, &height );

    if ( ((mode & XCB_CONFIG_WINDOW_WIDTH) && (intended->width != width)) ||
	 !(mode & XCB_CONFIG_WINDOW_WIDTH) ) {
	return_val->request_mode |= XCB_CONFIG_WINDOW_WIDTH;
	return_val->width = width;
	ret_val = IswGeometryAlmost;
    }

    if ( ((mode & XCB_CONFIG_WINDOW_HEIGHT) && (intended->height != height)) ||
	 !(mode & XCB_CONFIG_WINDOW_HEIGHT) ) {
	return_val->request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
	return_val->height = height;
	ret_val = IswGeometryAlmost;
    }

    if (ret_val == IswGeometryAlmost) {
	mode = return_val->request_mode;

	if ( ((mode & XCB_CONFIG_WINDOW_WIDTH) && (width == entry->rectangle.width)) &&
	     ((mode & XCB_CONFIG_WINDOW_HEIGHT) && (height == entry->rectangle.height)) )
	    return(IswGeometryNo);
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

    {
 /* XCB Fix: Add NULL check for font before accessing fields */
 if (entry->sme_bsb.font != NULL) {
     if (entry->sme_bsb.label == NULL)
  *width = 0;
     else
  *width = ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font,
    entry->sme_bsb.label, strlen(entry->sme_bsb.label));

     *height = ISWScaledFontHeight(IswParent(w), entry->sme_bsb.font);
 } else {
     /* No font available - use defaults */
     if (entry->sme_bsb.label == NULL)
  *width = 0;
     else
  *width = (strlen(entry->sme_bsb.label) * 8);
     *height = (14);
 }
    }

    if (entry->sme_bsb.accelerator_text != NULL && entry->sme_bsb.font != NULL) {
	int accel_w = ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font,
					 entry->sme_bsb.accelerator_text,
					 strlen(entry->sme_bsb.accelerator_text));
	*width += accel_w + ISWScaledTextWidth(IswParent(w), entry->sme_bsb.font, "  ", 2);
    }

    *width += entry->sme_bsb.left_margin + entry->sme_bsb.right_margin;

    if (entry->sme_bsb.menu_name != NULL)
	*width += SME_SUBMENU_ARROW_SIZE * 2 + 4;

    h = (entry->sme_bsb.left_image_height > entry->sme_bsb.right_image_height)
	    ? entry->sme_bsb.left_image_height : entry->sme_bsb.right_image_height;
    if (h > *height) *height = h;
    *height = ((int)*height * (100 + entry->sme_bsb.vert_space)) / 100;
}

/*      Function Name: DrawBitmaps
 *      Description: Draws left and right bitmaps.
 *      Arguments: w - the simple menu widget.
 *                 highlighted - TRUE if the entry is highlighted and active.
 *      Returns: none
 */

static void
DrawBitmaps(Widget w, Boolean highlighted)
{
    int x_loc, y_loc;
    SmeBSBObject entry = (SmeBSBObject) w;
    unsigned int rw, rh;
    const unsigned char *pixels;
    ISWRenderContext *ctx = ((SimpleMenuWidget)IswParent(w))->simple_menu.render_ctx;

    (void)highlighted;  /* reserved for future tinting */

    if (!entry->sme_bsb.left_image && !entry->sme_bsb.right_image)
        return;

    /* Draw left image */
    if (entry->sme_bsb.left_image) {
        x_loc = (int)(entry->sme_bsb.left_margin - entry->sme_bsb.left_image_width) / 2;
        y_loc = entry->rectangle.y +
                (int)(entry->rectangle.height - entry->sme_bsb.left_image_height) / 2;

        {
        float msf = (float)ISWScaleFactor(IswParent(w));
        pixels = ISWImageRasterize(entry->sme_bsb.left_image,
                                   (unsigned int)(entry->sme_bsb.left_image_width * msf + 0.5f),
                                   (unsigned int)(entry->sme_bsb.left_image_height * msf + 0.5f),
                                   &rw, &rh);
        }
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

        {
        float msf2 = (float)ISWScaleFactor(IswParent(w));
        pixels = ISWImageRasterize(entry->sme_bsb.right_image,
                                   (unsigned int)(entry->sme_bsb.right_image_width * msf2 + 0.5f),
                                   (unsigned int)(entry->sme_bsb.right_image_height * msf2 + 0.5f),
                                   &rw, &rh);
        }
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
            float tmp_w = ISWImageGetWidth(entry->sme_bsb.left_image);
            float tmp_h = ISWImageGetHeight(entry->sme_bsb.left_image);
            entry->sme_bsb.left_image_width  = (Dimension)tmp_w;
            entry->sme_bsb.left_image_height = (Dimension)tmp_h;
        } else {
            entry->sme_bsb.left_image_width  = 0;
            entry->sme_bsb.left_image_height = 0;
        }
    } else {
        if (entry->sme_bsb.right_image) {
            float tmp_w = ISWImageGetWidth(entry->sme_bsb.right_image);
            float tmp_h = ISWImageGetHeight(entry->sme_bsb.right_image);
            entry->sme_bsb.right_image_width  = (Dimension)tmp_w;
            entry->sme_bsb.right_image_height = (Dimension)tmp_h;
        } else {
            entry->sme_bsb.right_image_width  = 0;
            entry->sme_bsb.right_image_height = 0;
        }
    }
}


