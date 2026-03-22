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
#include <stdio.h>

#include <X11/Xatom.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/AsciiSinkP.h>
#include <ISW/AsciiSrcP.h>	/* For source function defs. */
#include <ISW/TextP.h>	/* I also reach into the text widget. */
#include <ISW/ISWRender.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#ifdef HAVE_CAIRO
#include <cairo.h>
#include <cairo-xcb.h>
#endif
#include "ISWXcbDraw.h"

/* XCB-based XFontStruct lacks per_char metrics */
#define XFONTSTRUCT_HAS_NO_PER_CHAR 1

#ifdef GETLASTPOS
#undef GETLASTPOS		/* We will use our own GETLASTPOS. */
#endif

#define GETLASTPOS IswTextSourceScan(source, (ISWTextPosition) 0, IswstAll, IswsdRight, 1, TRUE)

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static int MaxLines(Widget, Dimension);
static int MaxHeight(Widget, int);
static void SetTabs(Widget, int, short *);

static void DisplayText(Widget, Position, Position, ISWTextPosition,
                        ISWTextPosition, Boolean);
static void InsertCursor(Widget, Position, Position, IswTextInsertState);
static void AsciiSinkClearToBackground(Widget, Position, Position,
                                       Dimension, Dimension);
static void FindPosition(Widget, ISWTextPosition, int, int, Boolean,
            ISWTextPosition *, int *, int *);
static void FindDistance(Widget, ISWTextPosition, int, ISWTextPosition, int *,
                         ISWTextPosition *, int *);
static void Resolve(Widget, ISWTextPosition, int, int, ISWTextPosition *);
static void GetCursorBounds(Widget, xcb_rectangle_t *);

#define offset(field) XtOffsetOf(AsciiSinkRec, ascii_sink.field)

static XtResource resources[] = {
    {XtNfont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
	offset(font), XtRString, XtDefaultFont},
    {XtNecho, XtCOutput, XtRBoolean, sizeof(Boolean),
	offset(echo), XtRImmediate, (XtPointer) True},
    {XtNdisplayNonprinting, XtCOutput, XtRBoolean, sizeof(Boolean),
	offset(display_nonprinting), XtRImmediate, (XtPointer) True},
};
#undef offset

#define SuperClass		(&textSinkClassRec)
AsciiSinkClassRec asciiSinkClassRec = {
  {
/* core_class fields */
    /* superclass	  	*/	(WidgetClass) SuperClass,
    /* class_name	  	*/	"AsciiSink",
    /* widget_size	  	*/	sizeof(AsciiSinkRec),
    /* class_initialize   	*/	IswInitializeWidgetSet,
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
/* text_sink_class fields */
  {
    /* DisplayText              */      DisplayText,
    /* InsertCursor             */      InsertCursor,
    /* ClearToBackground        */      AsciiSinkClearToBackground,
    /* FindPosition             */      FindPosition,
    /* FindDistance             */      FindDistance,
    /* Resolve                  */      Resolve,
    /* MaxLines                 */      MaxLines,
    /* MaxHeight                */      MaxHeight,
    /* SetTabs                  */      SetTabs,
    /* GetCursorBounds          */      GetCursorBounds
  },
/* ascii_sink_class fields */
  {
    /* unused			*/	0
  }
};

WidgetClass asciiSinkObjectClass = (WidgetClass)&asciiSinkClassRec;

/* Utilities */

static int
CharWidth (Widget w, int x, unsigned char c)
{
    int    i, width, nonPrinting;
    AsciiSinkObject sink = (AsciiSinkObject) w;
    XFontStruct *font = sink->ascii_sink.font;
    Position *tab;

    if ( c == IswLF ) return(0);

    if (c == IswTAB) {
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

    if ( (nonPrinting = (c < (unsigned char) IswSP)) ) {
	if (sink->ascii_sink.display_nonprinting)
	    c += '@';
	else {
	    c = IswSP;
	    nonPrinting = False;
	}
    }

    /* Use Cairo metrics when available so that positioning matches rendering.
     * XCB font metrics can differ from Cairo's, causing cumulative drift. */
    if (sink->ascii_sink.render_ctx) {
	char ch_buf[2];
	if (nonPrinting) {
	    ch_buf[0] = '^';
	    ch_buf[1] = (char)c;
	    return ISWRenderTextWidth(sink->ascii_sink.render_ctx, ch_buf, 2);
	}
	ch_buf[0] = (char)c;
	return ISWRenderTextWidth(sink->ascii_sink.render_ctx, ch_buf, 1);
    }

    /* XCB Fix: Add NULL check for font before accessing fid */
    if (font == NULL) {
	width = 8;  /* Default width if no font */
    } else {
#ifdef XFONTSTRUCT_HAS_NO_PER_CHAR
	/* XCB-based fonts don't have per_char metrics - query server */
	{
	    xcb_connection_t *conn = XtDisplayOfObject(w);
	    width = ISWFontCharWidth(conn, font->fid, c);
	    if (width == 0)
		width = 8;  /* Fallback if query fails */
	}
#else
	if (font->per_char &&
		(c >= font->min_char_or_byte2 && c <= font->max_char_or_byte2))
	    width = font->per_char[c - font->min_char_or_byte2].width;
	else
	    width = font->min_bounds.width;
#endif
    }

    if (nonPrinting)
	width += CharWidth(w, x, (unsigned char) '^');

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
PaintText(Widget w, GC gc, Position x, Position y, unsigned char * buf, int len)
{
    AsciiSinkObject sink = (AsciiSinkObject) w;
    TextWidget ctx = (TextWidget) XtParent(w);
    xcb_connection_t *conn = XtDisplay((Widget) ctx);

    Position max_x;
    Dimension width;
    
    /* Lazy render context creation with dimension validation */
    if (!sink->ascii_sink.render_ctx && XtIsRealized((Widget)ctx) &&
        ctx->core.width > 0 && ctx->core.height > 0) {
        sink->ascii_sink.render_ctx = ISWRenderCreate((Widget)ctx, ISW_RENDER_BACKEND_AUTO);
        if (sink->ascii_sink.render_ctx && sink->ascii_sink.font) {
            ISWRenderSetFont(sink->ascii_sink.render_ctx, sink->ascii_sink.font);
        }
    }
    
    /* Calculate text width */
    if (sink->ascii_sink.render_ctx) {
        width = ISWRenderTextWidth(sink->ascii_sink.render_ctx, (char *)buf, len);
    } else {
        /* XCB fallback: Add NULL check for font before accessing fid */
        width = (sink->ascii_sink.font != NULL) ?
            ISWFontTextWidth(conn, sink->ascii_sink.font->fid, (char *) buf, len) :
            len * 8;  /* Default width if no font */
    }
    max_x = (Position) ctx->core.width;

    if ( ((int) width) <= -x)	           /* Don't draw if we can't see it. */
      return(width);

    /* Draw text using Cairo or XCB fallback */
    if (sink->ascii_sink.render_ctx) {
        /* Cairo only draws foreground glyphs (unlike XDrawImageString which
         * fills the background too). Fill the text background first.
         * Use ascent + descent + 1 to match line table height and cover
         * descenders fully. */
        int asc = sink->ascii_sink.font ? sink->ascii_sink.font->ascent : 11;
        int desc = sink->ascii_sink.font ? sink->ascii_sink.font->descent : 3;
        Pixel bg = (gc == sink->ascii_sink.invgc) ?
            sink->text_sink.foreground : sink->text_sink.background;
        ISWRenderSave(sink->ascii_sink.render_ctx);
        ISWRenderSetColor(sink->ascii_sink.render_ctx, bg);
        ISWRenderFillRectangle(sink->ascii_sink.render_ctx,
                              (int)x, (int)y - asc,
                              (int)width, (int)(asc + desc + 1));
        ISWRenderRestore(sink->ascii_sink.render_ctx);
        ISWRenderDrawString(sink->ascii_sink.render_ctx, (char *)buf, len,
                          (int)x, (int)y);
    } else {
        /* XCB fallback */
        ISWXcbDrawImageString(conn, XtWindow(ctx), gc,
           (int) x, (int) y, (char *) buf, len);
    }
    
    if ( (((Position) width + x) > max_x) && (ctx->text.margin.right != 0) ) {
        x = ctx->core.width - ctx->text.margin.right;
        width = ctx->text.margin.right;
        /* XCB Fix: Add NULL check for font before accessing fields */
        int ascent = sink->ascii_sink.font ? sink->ascii_sink.font->ascent : 11;
        int descent = sink->ascii_sink.font ? sink->ascii_sink.font->descent : 3;
        
        /* Draw margin background using Cairo or XCB fallback */
        if (sink->ascii_sink.render_ctx) {
            ISWRenderFillRectangle(sink->ascii_sink.render_ctx,
                                 (int)x, (int)y - ascent,
                                 (int)width, (int)(ascent + descent));
        } else {
            /* XCB fallback */
            xcb_rectangle_t rect = {(int) x,
                (int) y - ascent,
                (unsigned int) width,
                (unsigned int) (ascent + descent)};
            xcb_poly_fill_rectangle(conn, XtWindow((Widget) ctx),
                sink->ascii_sink.normgc, 1, &rect);
            xcb_flush(conn);
        }
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
    AsciiSinkObject sink = (AsciiSinkObject) w;
    Widget source = IswTextGetSource(XtParent(w));
    TextWidget ctx = (TextWidget) XtParent(w);
    unsigned char buf[BUFSIZ];

    int j, k;
    ISWTextBlock blk;
    GC gc = highlight ? sink->ascii_sink.invgc : sink->ascii_sink.normgc;
    GC invgc = highlight ? sink->ascii_sink.normgc : sink->ascii_sink.invgc;
    Pixel fg_color = highlight ? sink->text_sink.background : sink->text_sink.foreground;
    Pixel bg_color = highlight ? sink->text_sink.foreground : sink->text_sink.background;

    if (!sink->ascii_sink.echo) return;

    /* Lazy render context creation — must happen BEFORE ISWRenderBegin so that
     * the Begin/End calls are always balanced on the same render_ctx. */
    if (!sink->ascii_sink.render_ctx && XtIsRealized((Widget)ctx) &&
        ctx->core.width > 0 && ctx->core.height > 0) {
        sink->ascii_sink.render_ctx = ISWRenderCreate((Widget)ctx, ISW_RENDER_BACKEND_AUTO);
        if (sink->ascii_sink.render_ctx && sink->ascii_sink.font) {
            ISWRenderSetFont(sink->ascii_sink.render_ctx, sink->ascii_sink.font);
        }
    }

    /* Begin Cairo rendering if context exists */
    if (sink->ascii_sink.render_ctx) {
        ISWRenderBegin(sink->ascii_sink.render_ctx);

        ISWRenderSetClipRectangle(sink->ascii_sink.render_ctx,
                                  0, 0,
                                  (int)ctx->core.width,
                                  (int)ctx->core.height);

        ISWRenderSetColor(sink->ascii_sink.render_ctx, fg_color);
    }

    /* XCB Fix: Add NULL check for font before accessing ascent */
    y += sink->ascii_sink.font ? sink->ascii_sink.font->ascent : 11;
    
    for ( j = 0 ; pos1 < pos2 ; ) {
	pos1 = IswTextSourceRead(source, pos1, &blk, (int) pos2 - pos1);
	for (k = 0; k < blk.length; k++) {
	    if (j >= BUFSIZ) {	/* buffer full, dump the text. */
	        x += PaintText(w, gc, x, y, buf, j);
		j = 0;
	    }
	    buf[j] = blk.ptr[k];
	    if (buf[j] == IswLF)	/* line feeds ('\n') are not printed. */
	        continue;

	    else if (buf[j] == '\t') {
	        Position temp = 0;
		Dimension width;

	        if ((j != 0) && ((temp = PaintText(w, gc, x, y, buf, j)) == 0))
		  return;

	 x += temp;
	 width = CharWidth(w, x, (unsigned char) '\t');
	 /* XCB Fix: Add NULL check for font before accessing fields */
	 int ascent = sink->ascii_sink.font ? sink->ascii_sink.font->ascent : 11;
	 int descent = sink->ascii_sink.font ? sink->ascii_sink.font->descent : 3;
	 
	 /* Draw tab background using Cairo or XCB fallback */
	 if (sink->ascii_sink.render_ctx) {
	     ISWRenderSave(sink->ascii_sink.render_ctx);
	     ISWRenderSetColor(sink->ascii_sink.render_ctx, bg_color);
	     ISWRenderFillRectangle(sink->ascii_sink.render_ctx,
	                          (int)x, (int)y - ascent,
	                          (int)width, (int)(ascent + descent));
	     ISWRenderRestore(sink->ascii_sink.render_ctx);
	 } else {
	     /* XCB fallback */
	     xcb_connection_t *conn = XtDisplayOfObject(w);
	     xcb_rectangle_t rect = {(int) x,
	      (int) y - ascent,
	      (unsigned int) width,
	      (unsigned int) (ascent + descent)};
	     xcb_poly_fill_rectangle(conn, XtWindowOfObject(w),
	      invgc, 1, &rect);
	     xcb_flush(conn);
	 }
	 x += width;
		j = -1;
	    }
	    else if ( buf[j] < (unsigned char) ' ' ) {
	        if (sink->ascii_sink.display_nonprinting) {
		    buf[j + 1] = buf[j] + '@';
		    buf[j] = '^';
		    j++;
		}
		else
		    buf[j] = ' ';
	    }
	    j++;
	}
    }
    if (j > 0)
        (void) PaintText(w, gc, x, y, buf, j);
    
    /* End Cairo rendering */
    if (sink->ascii_sink.render_ctx) {
        ISWRenderEnd(sink->ascii_sink.render_ctx);
    }
}

/*
 * ClearToBackground override: use Cairo when render_ctx exists.
 * Must not mix xcb_clear_area with Cairo — they conflict on the same surface.
 */
static void
AsciiSinkClearToBackground(Widget w, Position x, Position y,
                           Dimension width, Dimension height)
{
    if (height == 0 || width == 0) return;

    AsciiSinkObject sink = (AsciiSinkObject) w;

    if (sink->ascii_sink.render_ctx) {
        ISWRenderBegin(sink->ascii_sink.render_ctx);
        ISWRenderSetColor(sink->ascii_sink.render_ctx,
                          sink->text_sink.background);
        ISWRenderFillRectangle(sink->ascii_sink.render_ctx,
                               (int)x, (int)y,
                               (int)width, (int)height);
        ISWRenderEnd(sink->ascii_sink.render_ctx);
        return;
    }

    xcb_connection_t *conn = XtDisplayOfObject(w);
    xcb_clear_area(conn, 0, XtWindowOfObject(w), x, y, width, height);
    xcb_flush(conn);
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
    return IswCreateBitmapFromData(conn, root,
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
    AsciiSinkObject sink = (AsciiSinkObject) w;

    rect->width = (uint16_t) insertCursor_width;
    rect->height = (uint16_t) insertCursor_height;
    rect->x = sink->ascii_sink.cursor_x - (int16_t) (rect->width / 2);
    rect->y = sink->ascii_sink.cursor_y - (int16_t) rect->height;
}

/*
 * The following procedure manages the "insert" cursor.
 */

static void
InsertCursor (Widget w, Position x, Position y, IswTextInsertState state)
{
    AsciiSinkObject sink = (AsciiSinkObject) w;
    Widget text_widget = XtParent(w);
    xcb_rectangle_t rect;

    sink->ascii_sink.cursor_x = x;
    sink->ascii_sink.cursor_y = y;

    GetCursorBounds(w, &rect);
    if (state != sink->ascii_sink.laststate && XtIsRealized(text_widget)) {
        if (sink->ascii_sink.render_ctx && state == IswisOn) {
            /* Draw cursor as a filled bar using Cairo.
             * When turning off, the full redraw will erase it. */
            int asc = sink->ascii_sink.font ? sink->ascii_sink.font->ascent : 11;
            int desc = sink->ascii_sink.font ? sink->ascii_sink.font->descent : 3;
            ISWRenderBegin(sink->ascii_sink.render_ctx);
            ISWRenderSetColor(sink->ascii_sink.render_ctx,
                              sink->text_sink.foreground);
            ISWRenderFillRectangle(sink->ascii_sink.render_ctx,
                                   (int)x - 1, (int)y - asc,
                                   2, asc + desc);
            ISWRenderEnd(sink->ascii_sink.render_ctx);
        } else if (sink->ascii_sink.render_ctx && state == IswisOff) {
            /* Erase cursor by redrawing background, text will be redrawn */
            int asc = sink->ascii_sink.font ? sink->ascii_sink.font->ascent : 11;
            int desc = sink->ascii_sink.font ? sink->ascii_sink.font->descent : 3;
            ISWRenderBegin(sink->ascii_sink.render_ctx);
            ISWRenderSetColor(sink->ascii_sink.render_ctx,
                              sink->text_sink.background);
            ISWRenderFillRectangle(sink->ascii_sink.render_ctx,
                                   (int)x - 1, (int)y - asc,
                                   2, asc + desc);
            ISWRenderEnd(sink->ascii_sink.render_ctx);
        } else {
            /* XCB fallback: use original XOR method */
            xcb_connection_t *conn = XtDisplay(text_widget);
            xcb_copy_plane(conn,
                sink->ascii_sink.insertCursorOn,
                XtWindow(text_widget), sink->ascii_sink.xorgc,
                0, 0, (int) rect.x, (int) rect.y,
                (unsigned int) rect.width, (unsigned int) rect.height, 1);
            xcb_flush(conn);
        }
    }
    sink->ascii_sink.laststate = state;
}

/*
 * Given two positions, find the distance between them.
 */

static void
FindDistance (Widget w,
              ISWTextPosition fromPos,	/* First position. */
              int fromx,		/* Horizontal location of first position. */
              ISWTextPosition toPos,	/* Second position. */
              int *resWidth,		/* Distance between fromPos and resPos. */
              ISWTextPosition *resPos,	/* Actual second position used. */
              int *resHeight		/* Height required. */)
{
    AsciiSinkObject sink = (AsciiSinkObject) w;
    Widget source = IswTextGetSource(XtParent(w));

    ISWTextPosition index, lastPos;
    unsigned char c;
    ISWTextBlock blk;

    /* we may not need this */
    lastPos = GETLASTPOS;
    IswTextSourceRead(source, fromPos, &blk, (int) toPos - fromPos);
    *resWidth = 0;
    for (index = fromPos; index != toPos && index < lastPos; index++) {
	if (index - blk.firstPos >= blk.length)
	    IswTextSourceRead(source, index, &blk, (int) toPos - fromPos);
	c = blk.ptr[index - blk.firstPos];
	*resWidth += CharWidth(w, fromx + *resWidth, c);
	if (c == IswLF) {
	    index++;
	    break;
	}
    }
    *resPos = index;
    /* XCB Fix: Add NULL check for font before accessing fields */
    *resHeight = sink->ascii_sink.font ?
 (sink->ascii_sink.font->ascent + sink->ascii_sink.font->descent) : 14;
}


static void
FindPosition(Widget w,
             ISWTextPosition fromPos, 	/* Starting position. */
             int fromx,			/* Horizontal location of starting position.*/
             int width,			/* Desired width. */
             Boolean stopAtWordBreak,	/* Whether the resulting position should
					   be at a word break. */
             ISWTextPosition *resPos,	/* Resulting position. */
             int *resWidth,		/* Actual width used. */
             int *resHeight		/* Height required. */)
{
    AsciiSinkObject sink = (AsciiSinkObject) w;
    Widget source = IswTextGetSource(XtParent(w));

    ISWTextPosition lastPos, index, whiteSpacePosition = 0;
    int     lastWidth = 0, whiteSpaceWidth = 0;
    Boolean whiteSpaceSeen;
    unsigned char c;
    ISWTextBlock blk;

    lastPos = GETLASTPOS;

    IswTextSourceRead(source, fromPos, &blk, BUFSIZ);
    *resWidth = 0;
    whiteSpaceSeen = FALSE;
    c = 0;
    for (index = fromPos; *resWidth <= width && index < lastPos; index++) {
	lastWidth = *resWidth;
	if (index - blk.firstPos >= blk.length)
	    IswTextSourceRead(source, index, &blk, BUFSIZ);
	c = blk.ptr[index - blk.firstPos];
	*resWidth += CharWidth(w, fromx + *resWidth, c);

	if ((c == IswSP || c == IswTAB) && *resWidth <= width) {
	    whiteSpaceSeen = TRUE;
	    whiteSpacePosition = index;
	    whiteSpaceWidth = *resWidth;
	}
	if (c == IswLF) {
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
    if (index == lastPos && c != IswLF) index = lastPos + 1;
    *resPos = index;
    *resHeight = sink->ascii_sink.font->ascent +sink->ascii_sink.font->descent;
}

static void
Resolve (Widget w, ISWTextPosition pos, int fromx, int width, ISWTextPosition *resPos)
{
    int resWidth, resHeight;
    Widget source = IswTextGetSource(XtParent(w));

    FindPosition(w, pos, fromx, width, FALSE, resPos, &resWidth, &resHeight);
    if (*resPos > GETLASTPOS)
      *resPos = GETLASTPOS;
}

static void
GetGC(AsciiSinkObject sink)
{
    xcb_create_gc_value_list_t values;

    ISWInitGCValues(&values);
    values.graphics_exposures = 0;

    /* XCB Fix: Add NULL check for font before accessing fid */
    XtGCMask valuemask = XCB_GC_GRAPHICS_EXPOSURES | XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
    if (sink->ascii_sink.font != NULL) {
	values.font = sink->ascii_sink.font->fid;
	valuemask |= XCB_GC_FONT;
    }

    values.foreground = sink->text_sink.foreground;
    values.background = sink->text_sink.background;
    sink->ascii_sink.normgc = XtGetGC((Widget)sink, valuemask, &values);

    values.foreground = sink->text_sink.background;
    values.background = sink->text_sink.foreground;
    sink->ascii_sink.invgc = XtGetGC((Widget)sink, valuemask, &values);

    values.function = GXxor;
    values.background = (unsigned long) 0L;	/* (pix ^ 0) = pix */
    values.foreground = (sink->text_sink.background ^
			 sink->text_sink.foreground);
    valuemask = XCB_GC_GRAPHICS_EXPOSURES | XCB_GC_FUNCTION | XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;

    sink->ascii_sink.xorgc = XtGetGC((Widget)sink, valuemask, &values);
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
    AsciiSinkObject sink = (AsciiSinkObject) new;

    /* XCB Fix: XtRFontStruct converter may fail in XCB mode, leaving font NULL.
     * AsciiSink doesn't have fontset support, so just issue a warning. */
    if (sink->ascii_sink.font == NULL) {
	XtAppWarning(XtWidgetToApplicationContext(new),
		     "AsciiSink widget: font is NULL - text rendering will fail");
    }

    GetGC(sink);

    sink->ascii_sink.insertCursorOn= CreateInsertCursor(new);
    sink->ascii_sink.laststate = IswisOff;
    sink->ascii_sink.cursor_x = sink->ascii_sink.cursor_y = 0;
    sink->ascii_sink.render_ctx = NULL;
}

/*	Function Name: Destroy
 *	Description: This function cleans up when the object is
 *                   destroyed.
 *	Arguments: w - the AsciiSink Object.
 *	Returns: none.
 */

static void
Destroy(Widget w)
{
   AsciiSinkObject sink = (AsciiSinkObject) w;

   if (sink->ascii_sink.render_ctx) {
       ISWRenderDestroy(sink->ascii_sink.render_ctx);
       sink->ascii_sink.render_ctx = NULL;
   }

   XtReleaseGC(w, sink->ascii_sink.normgc);
   XtReleaseGC(w, sink->ascii_sink.invgc);
   XtReleaseGC(w, sink->ascii_sink.xorgc);
   ISWFreePixmap(XtDisplayOfObject(w), sink->ascii_sink.insertCursorOn);
}

/*	Function Name: SetValues
 *	Description: Sets the values for the AsciiSink
 *	Arguments: current - current state of the object.
 *                 request - what was requested.
 *                 new - what the object will become.
 *	Returns: True if redisplay is needed.
 */

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    AsciiSinkObject w = (AsciiSinkObject) new;
    AsciiSinkObject old_w = (AsciiSinkObject) current;

    /* XCB Fix: Add NULL checks before comparing font->fid */
    Bool font_changed = False;
    if (w->ascii_sink.font != NULL && old_w->ascii_sink.font != NULL) {
	font_changed = (w->ascii_sink.font->fid != old_w->ascii_sink.font->fid);
    } else if (w->ascii_sink.font != old_w->ascii_sink.font) {
	/* One is NULL and the other isn't */
	font_changed = True;
    }

    if (font_changed ||
	w->text_sink.background != old_w->text_sink.background ||
	w->text_sink.foreground != old_w->text_sink.foreground) {
	XtReleaseGC((Widget)w, w->ascii_sink.normgc);
	XtReleaseGC((Widget)w, w->ascii_sink.invgc);
	XtReleaseGC((Widget)w, w->ascii_sink.xorgc);
	GetGC(w);
	((TextWidget)XtParent(new))->text.redisplay_needed = True;
    } else {
	if ( (w->ascii_sink.echo != old_w->ascii_sink.echo) ||
	     (w->ascii_sink.display_nonprinting !=
                                     old_w->ascii_sink.display_nonprinting) )
	    ((TextWidget)XtParent(new))->text.redisplay_needed = True;
    }

    return False;
}

/*	Function Name: MaxLines
 *	Description: Finds the Maximum number of lines that will fit in
 *                   a given height.
 *	Arguments: w - the AsciiSink Object.
 *                 height - height to fit lines into.
 *	Returns: the number of lines that will fit.
 */

/* ARGSUSED */
static int
MaxLines(Widget w, Dimension height)
{
  AsciiSinkObject sink = (AsciiSinkObject) w;
  int font_height;

  /* XCB Fix: Add NULL check for font before accessing fields */
  font_height = sink->ascii_sink.font ?
      (sink->ascii_sink.font->ascent + sink->ascii_sink.font->descent) : 14;
  return( ((int) height) / font_height );
}

/*	Function Name: MaxHeight
 *	Description: Finds the Minium height that will contain a given number
 *                   lines.
 *	Arguments: w - the AsciiSink Object.
 *                 lines - the number of lines.
 *	Returns: the height.
 */

/* ARGSUSED */
static int
MaxHeight(Widget w, int lines)
{
  AsciiSinkObject sink = (AsciiSinkObject) w;

  /* XCB Fix: Add NULL check for font before accessing fields */
  int line_height = sink->ascii_sink.font ?
      (sink->ascii_sink.font->ascent + sink->ascii_sink.font->descent) : 14;
  return(lines * line_height);
}

/*	Function Name: SetTabs
 *	Description: Sets the Tab stops.
 *	Arguments: w - the AsciiSink Object.
 *                 tab_count - the number of tabs in the list.
 *                 tabs - the text positions of the tabs.
 *	Returns: none
 */

static void
SetTabs(Widget w, int tab_count, short *tabs)
{
  AsciiSinkObject sink = (AsciiSinkObject) w;
  int i;
  xcb_atom_t XCB_ATOM_FIGURE_WIDTH;
  unsigned long figure_width = 0;
  XFontStruct *font = sink->ascii_sink.font;
  xcb_connection_t *conn = XtDisplayOfObject(w);

/*
 * Find the figure width of the current font.
 */

  XCB_ATOM_FIGURE_WIDTH = IswXcbInternAtom(conn, "FIGURE_WIDTH", FALSE);
  if ( (XCB_ATOM_FIGURE_WIDTH != None) &&
       ( (!IswGetFontProperty(conn, font, XCB_ATOM_FIGURE_WIDTH, &figure_width)) ||
	 (figure_width == 0)) ) {
#ifdef XFONTSTRUCT_HAS_NO_PER_CHAR
    /* XCB-based fonts don't have per_char metrics - query for '$' width */
    figure_width = ISWFontCharWidth(conn, font->fid, '$');
    if (figure_width == 0)
        figure_width = 8;  /* Default figure width */
#else
    if (font->per_char && font->min_char_or_byte2 <= '$' &&
	font->max_char_or_byte2 >= '$')
      figure_width = font->per_char['$' - font->min_char_or_byte2].width;
    else
      figure_width = font->max_bounds.width;
#endif
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
  {  TextWidget ctx = (TextWidget)XtParent(w);
      ctx->text.redisplay_needed = True;
      _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
  }
#endif
}
