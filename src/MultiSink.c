/*
 * Copyright 1991 by OMRON Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of OMRON not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.  OMRON makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * OMRON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OMRON BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTUOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *      Author: Li Yuhong	 OMRON Corporation
 */

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
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <ISW/ISWInit.h>
#include <ISW/MultiSinkP.h>
#include <ISW/MultiSrcP.h>
#include <ISW/TextP.h>
#include "ISWI18n.h"
#include "ISWXcbDraw.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#ifdef GETLASTPOS
#undef GETLASTPOS		/* We will use our own GETLASTPOS. */
#endif

#define GETLASTPOS XawTextSourceScan(source, (ISWTextPosition) 0, XawstAll, XawsdRight, 1, TRUE)

#ifdef ISW_INTERNATIONALIZATION
/* XawWcToUtf8: Convert wide-char string to UTF-8
 * Returns malloced UTF-8 string or NULL on failure
 * Caller must free returned string with free()
 */
static char *
XawWcToUtf8(const wchar_t *wcs, int wc_len, int *utf8_len_out)
{
#ifdef ISW_HAS_XIM
    /* Would use Xlib XwcTextListToTextProperty here */
    return NULL;
#else
    /* XCB: UTF-8 conversion not fully supported without iconv */
    /* Fallback: return NULL to indicate conversion not available */
    if (utf8_len_out) *utf8_len_out = 0;
    return NULL;
#endif
}
#endif /* ISW_INTERNATIONALIZATION */

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static int MaxLines(Widget, Dimension);
static int MaxHeight(Widget, int);
static void SetTabs(Widget, int, short *);

static void DisplayText(Widget, Position, Position, ISWTextPosition,
                        ISWTextPosition, Boolean);
static void InsertCursor(Widget, Position, Position, XawTextInsertState);
static void FindPosition(Widget, ISWTextPosition, int, int, Boolean,
                         ISWTextPosition *, int *, int *);
static void FindDistance(Widget, ISWTextPosition, int, ISWTextPosition,
                         int *, ISWTextPosition *, int *);
static void Resolve(Widget, ISWTextPosition, int, int, ISWTextPosition *);
static void GetCursorBounds(Widget, xcb_rectangle_t *);

#define offset(field) XtOffsetOf(MultiSinkRec, multi_sink.field)

static XtResource resources[] = {
    {XtNfontSet, XtCFontSet, XtRFontSet, sizeof (ISWFontSet*),  /* Phase 3.5 */
	offset(fontset), XtRString, XtDefaultFontSet},
    {XtNecho, XtCOutput, XtRBoolean, sizeof(Boolean),
	offset(echo), XtRImmediate, (XtPointer) True},
    {XtNdisplayNonprinting, XtCOutput, XtRBoolean, sizeof(Boolean),
	offset(display_nonprinting), XtRImmediate, (XtPointer) True},
};
#undef offset

#define SuperClass		(&textSinkClassRec)
MultiSinkClassRec multiSinkClassRec = {
  { /* core_class fields */
    /* superclass	  	*/	(WidgetClass) SuperClass,
    /* class_name	  	*/	"MultiSink",
    /* widget_size	  	*/	sizeof(MultiSinkRec),
    /* class_initialize   	*/	XawInitializeWidgetSet,
    /* class_part_initialize	*/	NULL,
    /* class_inited       	*/	FALSE,
    /* initialize	  	*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* obj1		  	*/	NULL,
    /* obj2		  	*/	NULL,
    /* obj3		  	*/	0,
    /* resources	  	*/	resources,
    /* num_resources	  	*/	XtNumber(resources),
    /* xrm_class	  	*/	NULLQUARK,
    /* obj4		  	*/	FALSE,
    /* obj5		  	*/	FALSE,
    /* obj6			*/	FALSE,
    /* obj7		  	*/	FALSE,
    /* destroy		  	*/	Destroy,
    /* obj8		  	*/	NULL,
    /* obj9		  	*/	NULL,
    /* set_values	  	*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* obj10			*/	NULL,
    /* get_values_hook		*/	NULL,
    /* obj11		 	*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private   	*/	NULL,
    /* obj12		   	*/	NULL,
    /* obj13			*/	NULL,
    /* obj14			*/	NULL,
    /* extension		*/	NULL
  },
  { /* text_sink_class fields */
    /* DisplayText              */      DisplayText,
    /* InsertCursor             */      InsertCursor,
    /* ClearToBackground        */      XtInheritClearToBackground,
    /* FindPosition             */      FindPosition,
    /* FindDistance             */      FindDistance,
    /* Resolve                  */      Resolve,
    /* MaxLines                 */      MaxLines,
    /* MaxHeight                */      MaxHeight,
    /* SetTabs                  */      SetTabs,
    /* GetCursorBounds          */      GetCursorBounds
  },
  { /* multi_sink_class fields */
    /* unused			*/	0
  }
};

WidgetClass multiSinkObjectClass = (WidgetClass)&multiSinkClassRec;

/* Utilities */

static int
CharWidth (
    Widget w,
    int x,
    wchar_t c)
{
    int    i, width;
    MultiSinkObject sink = (MultiSinkObject) w;
    Position *tab;

    if ( c == _Xaw_atowc(XawLF) ) return(0);

    if (c == _Xaw_atowc(XawTAB)) {
	/* Adjust for Left Margin. */
	x -= ((TextWidget) XtParent(w))->text.margin.left;

	if (x >= (int)XtParent(w)->core.width) return 0;
	for (i = 0, tab = sink->text_sink.tabs ;
	     i < sink->text_sink.tab_count ; i++, tab++) {
	    if (x < *tab) {
		if (*tab < (int)XtParent(w)->core.width)
		    return *tab - x;
		else
		    return 0;
	    }
	}
	return 0;
    }

    /* Phase 3.5: WC→UTF8 conversion for width measurement */
    int utf8_len;
    char *utf8_text = XawWcToUtf8(&c, 1, &utf8_len);
    if (utf8_text && XawTextWidth(sink->multi_sink.fontset, utf8_text, utf8_len) == 0) {
	if (sink->multi_sink.display_nonprinting)
	    c = _Xaw_atowc('@');
	else {
	    c = _Xaw_atowc(XawSP);
	}
    }
    if (utf8_text) free(utf8_text);

    /*
     * if more efficiency(suppose one column is one ASCII char)

    width = XwcGetColumn(fontset->font_charset, fontset->num_of_fonts, c) *
            fontset->font_struct_list[0]->min_bounds.width;
     *
     * WARNING: Very Slower!!!
     *
     * Li Yuhong.
     */

    /* Phase 3.5: WC→UTF8 conversion for width calculation */
    utf8_text = XawWcToUtf8(&c, 1, &utf8_len);
    width = 0;
    if (utf8_text) {
        width = XawTextWidth(sink->multi_sink.fontset, utf8_text, utf8_len);
        free(utf8_text);
    }

    return width;
}

/*	Function Name: PaintText
 *	Description: Actually paints the text into the windoe.
 *	Arguments: w - the text widget.
 *                 gc - gc to paint text with.
 *                 x, y - location to paint the text.
 *                 buf, len - buffer and length of text to paint.
 *	Returns: the width of the text painted, or 0.
 *
 * NOTE:  If this string attempts to paint past the end of the window
 *        then this function will return zero.
 */

static Dimension
PaintText(Widget w, GC gc, Position x, Position y, wchar_t* buf, int len)
{
    MultiSinkObject sink = (MultiSinkObject) w;
    TextWidget ctx = (TextWidget) XtParent(w);

    Position max_x;
    Dimension width;
    
    /* Phase 3.5: WC→UTF8 conversion for rendering */
    int utf8_len;
    char *utf8_text = XawWcToUtf8(buf, len, &utf8_len);
    if (!utf8_text) return 0;
    
    width = XawTextWidth(sink->multi_sink.fontset, utf8_text, utf8_len);
    max_x = (Position) ctx->core.width;

    if ( ((int) width) <= -x) {	           /* Don't draw if we can't see it. */
      free(utf8_text);
      return(width);
    }

    XawDrawString(XtDisplay(ctx), XtWindow(ctx), sink->multi_sink.fontset, gc,
                  (int) x, (int) y, utf8_text, utf8_len);
    free(utf8_text);
    
    if ( (((Position) width + x) > max_x) && (ctx->text.margin.right != 0) ) {
 x = ctx->core.width - ctx->text.margin.right;
 width = ctx->text.margin.right;
 xcb_connection_t *conn = XtDisplay((Widget) ctx);
 xcb_rectangle_t rect = {(int) x,
                       (int) y - sink->multi_sink.fontset->ascent,
                       (unsigned int) width,
                       (unsigned int) sink->multi_sink.fontset->height};
 xcb_poly_fill_rectangle(conn, XtWindow((Widget) ctx),
         sink->multi_sink.normgc, 1, &rect);
 xcb_flush(conn);
 return(0);
    }
    return(width);
}

/* Sink Object Functions */

/*
 * This function does not know about drawing more than one line of text.
 */

static void
DisplayText(Widget w, Position x, Position y, ISWTextPosition pos1,
            ISWTextPosition pos2, Boolean highlight)
{
    MultiSinkObject sink = (MultiSinkObject) w;
    Widget source = XawTextGetSource(XtParent(w));
    wchar_t buf[BUFSIZ];
    /* Phase 3.5: Use ISWFontSet fields directly */

    int j, k;
    ISWTextBlock blk;
    GC gc = highlight ? sink->multi_sink.invgc : sink->multi_sink.normgc;
    GC invgc = highlight ? sink->multi_sink.normgc : sink->multi_sink.invgc;

    if (!sink->multi_sink.echo) return;

    y += sink->multi_sink.fontset->ascent;
    for ( j = 0 ; pos1 < pos2 ; ) {
	pos1 = XawTextSourceRead(source, pos1, &blk, (int) pos2 - pos1);
	for (k = 0; k < blk.length; k++) {
	    if (j >= BUFSIZ) {	/* buffer full, dump the text. */
	        x += PaintText(w, gc, x, y, buf, j);
		j = 0;
	    }
	    buf[j] = ((wchar_t *)blk.ptr)[k];
	    if (buf[j] == _Xaw_atowc(XawLF))
	        continue;

	    else if (buf[j] == _Xaw_atowc(XawTAB)) {
	        Position temp = 0;
		Dimension width;

	        if ((j != 0) && ((temp = PaintText(w, gc, x, y, buf, j)) == 0))
		  return;

	        x += temp;
	               width = CharWidth(w, x, _Xaw_atowc(XawTAB));
	 xcb_connection_t *conn = XtDisplayOfObject(w);
	 xcb_rectangle_t rect = {(int) x,
	                              (int) y - sink->multi_sink.fontset->ascent,
	                              (unsigned int)width,
	                              (unsigned int)sink->multi_sink.fontset->height};
	 xcb_poly_fill_rectangle(conn, XtWindowOfObject(w),
	         invgc, 1, &rect);
	 xcb_flush(conn);
	               x += width;
                j = -1;
            }
            else {
                /* Phase 3.5: WC→UTF8 check for zero-width chars */
                int utf8_len;
                char *utf8_text = XawWcToUtf8(&buf[j], 1, &utf8_len);
                if (utf8_text && XawTextWidth(sink->multi_sink.fontset, utf8_text, utf8_len) == 0) {
                    if (sink->multi_sink.display_nonprinting)
                        buf[j] = _Xaw_atowc('@');
                    else
                        buf[j] = _Xaw_atowc(' ');
                }
                if (utf8_text) free(utf8_text);
            }
	    j++;
	}
    }
    if (j > 0)
        (void) PaintText(w, gc, x, y, buf, j);
}

#define insertCursor_width 6
#define insertCursor_height 3
static char insertCursor_bits[] = {0x0c, 0x1e, 0x33};

static Pixmap
CreateInsertCursor(Widget w)
{
    xcb_connection_t *conn = XtDisplayOfObject(w);
    xcb_screen_t *s = XtScreenOfObject(w);
    xcb_drawable_t root = RootWindowOfScreen(s);
    return XawCreateBitmapFromData(conn, root,
		  insertCursor_bits, insertCursor_width, insertCursor_height);
}

/*	Function Name: GetCursorBounds
 *	Description: Returns the size and location of the cursor.
 *	Arguments: w - the text object.
 * RETURNED        rect - an X rectangle to return the cursor bounds in.
 *	Returns: none.
 */

static void
GetCursorBounds(Widget w, xcb_rectangle_t * rect)
{
    MultiSinkObject sink = (MultiSinkObject) w;

    rect->width = (uint16_t) insertCursor_width;
    rect->height = (uint16_t) insertCursor_height;
    rect->x = sink->multi_sink.cursor_x - (int16_t) (rect->width / 2);
    rect->y = sink->multi_sink.cursor_y - (int16_t) rect->height;
}

/*
 * The following procedure manages the "insert" cursor.
 */

static void
InsertCursor (Widget w, Position x, Position y, XawTextInsertState state)
{
    MultiSinkObject sink = (MultiSinkObject) w;
    Widget text_widget = XtParent(w);
    xcb_rectangle_t rect;

    sink->multi_sink.cursor_x = x;
    sink->multi_sink.cursor_y = y;

    GetCursorBounds(w, &rect);
    if (state != sink->multi_sink.laststate && XtIsRealized(text_widget)) {
        xcb_connection_t *conn = XtDisplay(text_widget);
        xcb_copy_plane(conn,
     sink->multi_sink.insertCursorOn,
     XtWindow(text_widget), sink->multi_sink.xorgc,
     0, 0, (int) rect.x, (int) rect.y,
     (unsigned int) rect.width, (unsigned int) rect.height, 1);
        xcb_flush(conn);
    }
    sink->multi_sink.laststate = state;
}

/*
 * Given two positions, find the distance between them.
 */

static void
FindDistance (Widget w, ISWTextPosition fromPos, int fromx, ISWTextPosition toPos,
              int *resWidth, ISWTextPosition *resPos, int *resHeight)
{
    MultiSinkObject sink = (MultiSinkObject) w;
    Widget source = XawTextGetSource(XtParent(w));

    ISWTextPosition index, lastPos;
    wchar_t c;
    ISWTextBlock blk;

    /* we may not need this */
    lastPos = GETLASTPOS;
    XawTextSourceRead(source, fromPos, &blk, (int) toPos - fromPos);
    *resWidth = 0;
    for (index = fromPos; index != toPos && index < lastPos; index++) {
	if (index - blk.firstPos >= blk.length)
	    XawTextSourceRead(source, index, &blk, (int) toPos - fromPos);
        c = ((wchar_t *)blk.ptr)[index - blk.firstPos];
	*resWidth += CharWidth(w, fromx + *resWidth, c);
	if (c == _Xaw_atowc(XawLF)) {
	    index++;
	    break;
	}
    }
    *resPos = index;
    *resHeight = sink->multi_sink.fontset->height;  /* Phase 3.5 */
}


static void
FindPosition(Widget w, ISWTextPosition fromPos, int fromx, int width,
             Boolean stopAtWordBreak, ISWTextPosition *resPos, int *resWidth,
             int *resHeight)
{
    MultiSinkObject sink = (MultiSinkObject) w;
    Widget source = XawTextGetSource(XtParent(w));

    ISWTextPosition lastPos, index, whiteSpacePosition = 0;
    int     lastWidth = 0, whiteSpaceWidth = 0;
    Boolean whiteSpaceSeen;
    wchar_t c;
    ISWTextBlock blk;

    lastPos = GETLASTPOS;

    XawTextSourceRead(source, fromPos, &blk, BUFSIZ);
    *resWidth = 0;
    whiteSpaceSeen = FALSE;
    c = 0;
    for (index = fromPos; *resWidth <= width && index < lastPos; index++) {
	lastWidth = *resWidth;
	if (index - blk.firstPos >= blk.length)
	    XawTextSourceRead(source, index, &blk, BUFSIZ);
        c = ((wchar_t *)blk.ptr)[index - blk.firstPos];
        *resWidth += CharWidth(w, fromx + *resWidth, c);

        if ((c == _Xaw_atowc(XawSP) || c == _Xaw_atowc(XawTAB)) &&
	    *resWidth <= width) {
	    whiteSpaceSeen = TRUE;
	    whiteSpacePosition = index;
	    whiteSpaceWidth = *resWidth;
	}
	if (c == _Xaw_atowc(XawLF)) {
	    index++;
	    break;
	}
    }
    if (*resWidth > width && index > fromPos) {
	*resWidth = lastWidth;
	index--;
	if (stopAtWordBreak && whiteSpaceSeen) {
	    index = whiteSpacePosition + 1;
	    *resWidth = whiteSpaceWidth;
	}
    }
    if (index == lastPos && c != _Xaw_atowc(XawLF)) index = lastPos + 1;
    *resPos = index;
    *resHeight = sink->multi_sink.fontset->height;  /* Phase 3.5 */
}

static void
Resolve (Widget w, ISWTextPosition pos, int fromx, int width,
         ISWTextPosition *resPos)
{
    int resWidth, resHeight;
    Widget source = XawTextGetSource(XtParent(w));

    FindPosition(w, pos, fromx, width, FALSE, resPos, &resWidth, &resHeight);
    if (*resPos > GETLASTPOS)
      *resPos = GETLASTPOS;
}

static void
GetGC(MultiSinkObject sink)
{
    XtGCMask valuemask = (XCB_GC_GRAPHICS_EXPOSURES | XCB_GC_FOREGROUND | XCB_GC_BACKGROUND );
    xcb_create_gc_value_list_t values;

    ISWInitGCValues(&values);
    values.graphics_exposures = 0;

    values.foreground = sink->text_sink.foreground;
    values.background = sink->text_sink.background;

    sink->multi_sink.normgc = XtGetGC( (Widget)sink, valuemask, &values);

    values.foreground = sink->text_sink.background;
    values.background = sink->text_sink.foreground;
    sink->multi_sink.invgc = XtGetGC( (Widget)sink, valuemask, &values);

    values.function = GXxor;
    values.background = (unsigned long) 0L;	/* (pix ^ 0) = pix */
    values.foreground = (sink->text_sink.background ^
			 sink->text_sink.foreground);
    valuemask = XCB_GC_GRAPHICS_EXPOSURES | XCB_GC_FUNCTION | XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;

    sink->multi_sink.xorgc = XtGetGC( (Widget)sink, valuemask, &values);
}


/***** Public routines *****/

/*	Function Name: Initialize
 *	Description: Initializes the TextSink Object.
 *	Arguments: request, new - the requested and new values for the object
 *                                instance.
 *	Returns: none.
 *
 */

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    MultiSinkObject sink = (MultiSinkObject) new;

    GetGC(sink);

    sink->multi_sink.insertCursorOn= CreateInsertCursor(new);
    sink->multi_sink.laststate = XawisOff;
    sink->multi_sink.cursor_x = sink->multi_sink.cursor_y = 0;
}

/*	Function Name: Destroy
 *	Description: This function cleans up when the object is
 *                   destroyed.
 *	Arguments: w - the MultiSink Object.
 *	Returns: none.
 */

static void
Destroy(Widget w)
{
   MultiSinkObject sink = (MultiSinkObject) w;

   XtReleaseGC(w, sink->multi_sink.normgc);
   XtReleaseGC(w, sink->multi_sink.invgc);
   XtReleaseGC(w, sink->multi_sink.xorgc);

  ISWFreePixmap(XtDisplayOfObject(w), sink->multi_sink.insertCursorOn);
}

/*	Function Name: SetValues
 *	Description: Sets the values for the MultiSink
 *	Arguments: current - current state of the object.
 *                 request - what was requested.
 *                 new - what the object will become.
 *	Returns: True if redisplay is needed.
 */

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    MultiSinkObject w = (MultiSinkObject) new;
    MultiSinkObject old_w = (MultiSinkObject) current;

    /* Font set is not in the GC! Do not make a new GC when font set changes! */

    if ( w->multi_sink.fontset != old_w->multi_sink.fontset ) {
 ((TextWidget)XtParent(new))->text.redisplay_needed = True;
#ifndef NO_TAB_FIX
 SetTabs( (Widget)w, w->text_sink.tab_count, w->text_sink.char_tabs );
#endif
    }

    if (   w->text_sink.background != old_w->text_sink.background ||
	   w->text_sink.foreground != old_w->text_sink.foreground     ) {

	XtReleaseGC((Widget)w, w->multi_sink.normgc);
	XtReleaseGC((Widget)w, w->multi_sink.invgc);
	XtReleaseGC((Widget)w, w->multi_sink.xorgc);
	GetGC(w);
	((TextWidget)XtParent(new))->text.redisplay_needed = True;
    } else {
	if ( (w->multi_sink.echo != old_w->multi_sink.echo) ||
	     (w->multi_sink.display_nonprinting !=
                                     old_w->multi_sink.display_nonprinting) )
	    ((TextWidget)XtParent(new))->text.redisplay_needed = True;
    }

    return False;
}

/*	Function Name: MaxLines
 *	Description: Finds the Maximum number of lines that will fit in
 *                   a given height.
 *	Arguments: w - the MultiSink Object.
 *                 height - height to fit lines into.
 *	Returns: the number of lines that will fit.
 */

/* ARGSUSED */
static int
MaxLines(Widget w, Dimension height)
{
  MultiSinkObject sink = (MultiSinkObject) w;
  int font_height;

  /* Phase 3.5: Use ISWFontSet fields directly */
  font_height = sink->multi_sink.fontset->height;
  return( ((int) height) / font_height );
}

/*	Function Name: MaxHeight
 *	Description: Finds the Minium height that will contain a given number
 *                   lines.
 *	Arguments: w - the MultiSink Object.
 *                 lines - the number of lines.
 *	Returns: the height.
 */

/* ARGSUSED */
static int
MaxHeight(
    Widget w,
    int lines )
{
  MultiSinkObject sink = (MultiSinkObject) w;

  /* Phase 3.5: Use ISWFontSet fields directly */
  return(lines * sink->multi_sink.fontset->height);
}

/*	Function Name: SetTabs
 *	Description: Sets the Tab stops.
 *	Arguments: w - the MultiSink Object.
 *                 tab_count - the number of tabs in the list.
 *                 tabs - the text positions of the tabs.
 *	Returns: none
 */

static void
SetTabs(
    Widget w,
    int tab_count,
    short* tabs )
{
  MultiSinkObject sink = (MultiSinkObject) w;
  int i;
  xcb_atom_t XCB_ATOM_FIGURE_WIDTH;
  unsigned long figure_width = 0;
  
  /* XCB Note: XFontSet is not available in XCB. Using ISWFontSet's font_id instead.
   * Create a minimal XFontStruct to work with existing code. */
  XFontStruct font_struct;
  XFontStruct *font = &font_struct;
  font->fid = sink->multi_sink.fontset->font_id;
  font->min_char_or_byte2 = 0;
  font->max_char_or_byte2 = 255;

/*
 * Find the figure width of the current font.
 */

  xcb_connection_t *conn = XtDisplayOfObject(w);
  XCB_ATOM_FIGURE_WIDTH = XawXcbInternAtom(conn, "FIGURE_WIDTH", FALSE);
  if ( (XCB_ATOM_FIGURE_WIDTH != None) &&
       ( (!XawGetFontProperty(conn, font, XCB_ATOM_FIGURE_WIDTH, &figure_width)) ||
  (figure_width == 0)) ) {
    /* In XCB, we don't have direct access to per_char metrics
     * Use ISWFontCharWidth to get character width */
    if (font->min_char_or_byte2 <= '$' && font->max_char_or_byte2 >= '$')
      figure_width = ISWFontCharWidth(conn, font->fid, '$');
    else {
      /* Query font metrics for max width */
      ISWFontMetrics metrics;
      ISWXcbQueryFontMetrics(conn, font->fid, &metrics);
      figure_width = metrics.max_char_width;
    }
  }

  if (tab_count > sink->text_sink.tab_count) {
    sink->text_sink.tabs = (Position *)
	XtRealloc((char *) sink->text_sink.tabs,
		  (Cardinal) (tab_count * sizeof(Position)));
    sink->text_sink.char_tabs = (short *)
	XtRealloc((char *) sink->text_sink.char_tabs,
		  (Cardinal) (tab_count * sizeof(short)));
  }

  for ( i = 0 ; i < tab_count ; i++ ) {
    sink->text_sink.tabs[i] = tabs[i] * figure_width;
    sink->text_sink.char_tabs[i] = tabs[i];
  }

  sink->text_sink.tab_count = tab_count;

#ifndef NO_TAB_FIX
  ((TextWidget)XtParent(w))->text.redisplay_needed = True;
#endif
}

void
_ISWMultiSinkPosToXY(
    Widget w,
    ISWTextPosition pos,
    Position *x,
    Position *y )
{
    MultiSinkObject sink = (MultiSinkObject) ((TextWidget)w)->text.sink;

    /* Phase 3.5: Use ISWFontSet fields directly */
    _XawTextPosToXY( w, pos, x, y );
    *y += sink->multi_sink.fontset->ascent;
}
