/***********************************************************

Copyright (c) 1987, 1988, 1989, 1994  X Consortium

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

******************************************************************/

/*
 * TextSink.c - UTF-8 text sink for the Text widget.
 *
 * Single concrete class. Text is rendered as UTF-8 via the ISWRender
 * pipeline.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>

#include <X11/Xatom.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/TextSinkP.h>
#include <ISW/ISWUtf8.h>
#include <ISW/TextSrcP.h>
#include <ISW/TextP.h>
#include <ISW/ISWRender.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#ifdef HAVE_CAIRO
#include <cairo.h>
#include <cairo-xcb.h>
#endif
#include "ISWXcbDraw.h"
#include "ISWPlatformPrivate.h"

/* HiDPI helpers: return Cairo-matched scaled font metrics */
static int ScaledAscent(TextSinkObject sink) {
    return ISWScaledFontAscent(IswParent((Widget)sink), sink->text_sink.font);
}
static int ScaledFontHeight(TextSinkObject sink) {
    return ISWScaledFontHeight(IswParent((Widget)sink), sink->text_sink.font);
}
static int ScaledDescent(TextSinkObject sink) {
    return ScaledFontHeight(sink) - ScaledAscent(sink);
}

#ifdef GETLASTPOS
#undef GETLASTPOS
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
static void ClearToBackground(Widget, Position, Position, Dimension, Dimension);
static void FindPosition(Widget, ISWTextPosition, int, int, Boolean,
                         ISWTextPosition *, int *, int *);
static void FindDistance(Widget, ISWTextPosition, int, ISWTextPosition, int *,
                         ISWTextPosition *, int *);
static void Resolve(Widget, ISWTextPosition, int, int, ISWTextPosition *);
static void GetCursorBounds(Widget, IswRectangle *);

#define offset(field) IswOffsetOf(TextSinkRec, text_sink.field)

static IswResource resources[] = {
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
        offset(foreground), IswRString, IswDefaultForeground},
    {IswNbackground, IswCBackground, IswRPixel, sizeof(Pixel),
        offset(background), IswRString, IswDefaultBackground},
    {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
        offset(font), IswRString, IswDefaultFont},
    {IswNecho, IswCOutput, IswRBoolean, sizeof(Boolean),
        offset(echo), IswRImmediate, (IswPointer) True},
    {IswNdisplayNonprinting, IswCOutput, IswRBoolean, sizeof(Boolean),
        offset(display_nonprinting), IswRImmediate, (IswPointer) True},
};
#undef offset

#define SuperClass (&objectClassRec)
TextSinkClassRec textSinkClassRec = {
  {
    /* superclass          */      (WidgetClass) SuperClass,
    /* class_name          */      "TextSink",
    /* widget_size         */      sizeof(TextSinkRec),
    /* class_initialize    */      IswInitializeWidgetSet,
    /* class_part_initialize */    NULL,
    /* class_inited        */      FALSE,
    /* initialize          */      Initialize,
    /* initialize_hook     */      NULL,
    /* obj1                */      NULL,
    /* obj2                */      NULL,
    /* obj3                */      0,
    /* resources           */      resources,
    /* num_resources       */      IswNumber(resources),
    /* xrm_class           */      NULLQUARK,
    /* obj4                */      FALSE,
    /* obj5                */      FALSE,
    /* obj6                */      FALSE,
    /* obj7                */      FALSE,
    /* destroy             */      Destroy,
    /* obj8                */      NULL,
    /* obj9                */      NULL,
    /* set_values          */      SetValues,
    /* set_values_hook     */      NULL,
    /* obj10               */      NULL,
    /* get_values_hook     */      NULL,
    /* obj11               */      NULL,
    /* version             */      IswVersion,
    /* callback_private    */      NULL,
    /* obj12               */      NULL,
    /* obj13               */      NULL,
    /* obj14               */      NULL,
    /* extension           */      NULL
  },
  {
    /* DisplayText         */      DisplayText,
    /* InsertCursor        */      InsertCursor,
    /* ClearToBackground   */      ClearToBackground,
    /* FindPosition        */      FindPosition,
    /* FindDistance        */      FindDistance,
    /* Resolve             */      Resolve,
    /* MaxLines            */      MaxLines,
    /* MaxHeight           */      MaxHeight,
    /* SetTabs             */      SetTabs,
    /* GetCursorBounds     */      GetCursorBounds
  }
};

WidgetClass textSinkObjectClass = (WidgetClass)&textSinkClassRec;

/* Utilities */

static int
CharWidth(Widget w, int x, unsigned char c)
{
    int i, nonPrinting;
    TextSinkObject sink = (TextSinkObject) w;
    IswFontStruct *font = sink->text_sink.font;
    Position *tab;

    if (c == IswLF) return 0;

    if (c == IswTAB) {
        x -= ((TextWidget) IswParent(w))->text.margin.left;
        if (x >= (int)IswParent(w)->core.width) return 0;
        for (i = 0, tab = sink->text_sink.tabs;
             i < sink->text_sink.tab_count; i++, tab++) {
            if (x < *tab) {
                if (*tab < (int)IswParent(w)->core.width)
                    return *tab - x;
                else
                    return 0;
            }
        }
        return 0;
    }

    if ((nonPrinting = (c < (unsigned char) IswSP))) {
        if (sink->text_sink.display_nonprinting)
            c += '@';
        else {
            c = IswSP;
            nonPrinting = False;
        }
    }

    if (sink->text_sink.render_ctx) {
        char ch_buf[2];
        if (nonPrinting) {
            ch_buf[0] = '^';
            ch_buf[1] = (char)c;
            return ISWRenderTextWidth(sink->text_sink.render_ctx, ch_buf, 2);
        }
        ch_buf[0] = (char)c;
        return ISWRenderTextWidth(sink->text_sink.render_ctx, ch_buf, 1);
    }

    {
        char ch_buf[2];
        if (nonPrinting) {
            ch_buf[0] = '^';
            ch_buf[1] = (char)c;
            return ISWScaledTextWidth(IswParent(w), font, ch_buf, 2);
        }
        ch_buf[0] = (char)c;
        return ISWScaledTextWidth(IswParent(w), font, ch_buf, 1);
    }
}

static Dimension
PaintText(Widget w, Boolean highlight, Position x, Position y,
          unsigned char *buf, int len)
{
    TextSinkObject sink = (TextSinkObject) w;
    TextWidget ctx = (TextWidget) IswParent(w);
    Position max_x;
    Dimension width;

    if (!sink->text_sink.render_ctx && IswIsRealized((Widget)ctx) &&
        ctx->core.width > 0 && ctx->core.height > 0) {
        sink->text_sink.render_ctx = ISWRenderCreate((Widget)ctx, ISW_RENDER_BACKEND_AUTO);
        if (sink->text_sink.render_ctx && sink->text_sink.font) {
            ISWRenderSetFont(sink->text_sink.render_ctx, sink->text_sink.font);
        }
    }

    width = ISWRenderTextWidth(sink->text_sink.render_ctx, (char *)buf, len);
    max_x = (Position) ctx->core.width;

    if (((int) width) <= -x)
        return width;

    {
        int asc = ScaledAscent(sink);
        int desc = ScaledDescent(sink);
        Pixel bg = highlight ?
            sink->text_sink.foreground : sink->text_sink.background;
        ISWRenderSave(sink->text_sink.render_ctx);
        ISWRenderSetColor(sink->text_sink.render_ctx, bg);
        ISWRenderFillRectangle(sink->text_sink.render_ctx,
                               (int)x, (int)y - asc,
                               (int)(max_x - x), (int)(asc + desc + 1));
        ISWRenderRestore(sink->text_sink.render_ctx);
        ISWRenderDrawString(sink->text_sink.render_ctx, (char *)buf, len,
                            (int)x, (int)y);
    }

    if ((((Position) width + x) > max_x) && (ctx->text.margin.right != 0)) {
        x = ctx->core.width - ctx->text.margin.right;
        width = ctx->text.margin.right;
        int ascent = ScaledAscent(sink);
        int descent = ScaledDescent(sink);
        ISWRenderFillRectangle(sink->text_sink.render_ctx,
                               (int)x, (int)y - ascent,
                               (int)width, (int)(ascent + descent));
        return 0;
    }
    return width;
}

static void
DisplayText(Widget w, Position x, Position y, ISWTextPosition pos1,
            ISWTextPosition pos2, Boolean highlight)
{
    TextSinkObject sink = (TextSinkObject) w;
    Widget source = IswTextGetSource(IswParent(w));
    TextWidget ctx = (TextWidget) IswParent(w);
    unsigned char buf[BUFSIZ];
    int j, k;
    ISWTextBlock blk;
    Pixel fg_color = highlight ? sink->text_sink.background : sink->text_sink.foreground;
    Pixel bg_color = highlight ? sink->text_sink.foreground : sink->text_sink.background;

    if (!sink->text_sink.echo) return;

    if (!sink->text_sink.render_ctx && IswIsRealized((Widget)ctx) &&
        ctx->core.width > 0 && ctx->core.height > 0) {
        sink->text_sink.render_ctx = ISWRenderCreate((Widget)ctx, ISW_RENDER_BACKEND_AUTO);
        if (sink->text_sink.render_ctx && sink->text_sink.font) {
            ISWRenderSetFont(sink->text_sink.render_ctx, sink->text_sink.font);
        }
    }

    if (sink->text_sink.render_ctx) {
        /* Clip to the text content area, excluding the scrollbar bands.  The
           scrollbars are windowless children sharing this window, so text and
           background drawing must never reach into them.  vbar occupies the
           left band, hbar the bottom. */
        int clip_l = 0, clip_t = 0;
        int clip_r = (int) ctx->core.width;
        int clip_b = (int) ctx->core.height;
        if (ctx->text.vbar != NULL)
            clip_l = (int) (ctx->text.vbar->core.width +
                            2 * ctx->text.vbar->core.border_width);
        if (ctx->text.hbar != NULL)
            clip_b -= (int) (ctx->text.hbar->core.height +
                             2 * ctx->text.hbar->core.border_width);

        ISWRenderBegin(sink->text_sink.render_ctx);
        if (clip_r > clip_l && clip_b > clip_t)
            ISWRenderSetClipRectangle(sink->text_sink.render_ctx,
                                      clip_l, clip_t,
                                      clip_r - clip_l, clip_b - clip_t);
        ISWRenderSetColor(sink->text_sink.render_ctx, fg_color);
    }

    y += ScaledAscent(sink);

    for (j = 0; pos1 < pos2;) {
        pos1 = IswTextSourceRead(source, pos1, &blk, (int) pos2 - pos1);
        for (k = 0; k < blk.length; k++) {
            if (j >= BUFSIZ) {
                x += PaintText(w, highlight, x, y, buf, j);
                j = 0;
            }
            buf[j] = blk.ptr[k];
            if (buf[j] == IswLF)
                continue;
            else if (buf[j] == '\t') {
                Position temp = 0;
                Dimension width;
                if ((j != 0) && ((temp = PaintText(w, highlight, x, y, buf, j)) == 0))
                    return;
                x += temp;
                width = CharWidth(w, x, (unsigned char) '\t');
                int ascent = ScaledAscent(sink);
                int descent = ScaledDescent(sink);
                ISWRenderSave(sink->text_sink.render_ctx);
                ISWRenderSetColor(sink->text_sink.render_ctx, bg_color);
                ISWRenderFillRectangle(sink->text_sink.render_ctx,
                                       (int)x, (int)y - ascent,
                                       (int)width, (int)(ascent + descent));
                ISWRenderRestore(sink->text_sink.render_ctx);
                x += width;
                j = -1;
            }
            else if (buf[j] < (unsigned char) ' ') {
                if (sink->text_sink.display_nonprinting) {
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
        (void) PaintText(w, highlight, x, y, buf, j);

    if (sink->text_sink.render_ctx) {
        ISWRenderEnd(sink->text_sink.render_ctx);
    }
}

static void
ClearToBackground(Widget w, Position x, Position y,
                  Dimension width, Dimension height)
{
    if (height == 0 || width == 0) return;

    TextSinkObject sink = (TextSinkObject) w;
    TextWidget ctx = (TextWidget) IswParent(w);

    if (!sink->text_sink.render_ctx && IswIsRealized((Widget)ctx) &&
        ctx->core.width > 0 && ctx->core.height > 0) {
        sink->text_sink.render_ctx = ISWRenderCreate((Widget)ctx, ISW_RENDER_BACKEND_AUTO);
        if (sink->text_sink.render_ctx && sink->text_sink.font)
            ISWRenderSetFont(sink->text_sink.render_ctx, sink->text_sink.font);
    }

    if (sink->text_sink.render_ctx) {
        /* Keep the background fill out of the scrollbar bands (windowless
           children sharing this window). */
        int clip_l = 0, clip_t = 0;
        int clip_r = (int) ctx->core.width;
        int clip_b = (int) ctx->core.height;
        if (ctx->text.vbar != NULL)
            clip_l = (int) (ctx->text.vbar->core.width +
                            2 * ctx->text.vbar->core.border_width);
        if (ctx->text.hbar != NULL)
            clip_b -= (int) (ctx->text.hbar->core.height +
                             2 * ctx->text.hbar->core.border_width);

        ISWRenderBegin(sink->text_sink.render_ctx);
        if (clip_r > clip_l && clip_b > clip_t)
            ISWRenderSetClipRectangle(sink->text_sink.render_ctx,
                                      clip_l, clip_t,
                                      clip_r - clip_l, clip_b - clip_t);
        ISWRenderSetColor(sink->text_sink.render_ctx,
                          sink->text_sink.background);
        ISWRenderFillRectangle(sink->text_sink.render_ctx,
                               (int)x, (int)y,
                               (int)width, (int)height);
        ISWRenderEnd(sink->text_sink.render_ctx);
    }
}

#define insertCursor_width 6
#define insertCursor_height 3
static char insertCursor_bits[] = {0x0c, 0x1e, 0x33};

static xcb_pixmap_t
CreateInsertCursor(Widget w)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOfObject(w));
    xcb_screen_t *s = _IswXcbScreen(IswScreenOfObject(w));
    xcb_drawable_t root = RootWindowOfScreen(s);
    return IswCreateBitmapFromData(conn, root,
            insertCursor_bits, insertCursor_width, insertCursor_height);
}

static void
GetCursorBounds(Widget w, IswRectangle *rect)
{
    TextSinkObject sink = (TextSinkObject) w;

    rect->width = (uint16_t) insertCursor_width;
    rect->height = (uint16_t) insertCursor_height;
    rect->x = sink->text_sink.cursor_x - (int16_t) (rect->width / 2);
    rect->y = sink->text_sink.cursor_y - (int16_t) rect->height;
}

static void
InsertCursor(Widget w, Position x, Position y, IswTextInsertState state)
{
    TextSinkObject sink = (TextSinkObject) w;
    Widget text_widget = IswParent(w);
    IswRectangle rect;

    sink->text_sink.cursor_x = x;
    sink->text_sink.cursor_y = y;

    GetCursorBounds(w, &rect);
    if (state != sink->text_sink.laststate && IswIsRealized(text_widget)) {
        if (state == IswisOn) {
            int h = ScaledFontHeight(sink);
            ISWRenderBegin(sink->text_sink.render_ctx);
            ISWRenderSetColor(sink->text_sink.render_ctx,
                              sink->text_sink.foreground);
            ISWRenderFillRectangle(sink->text_sink.render_ctx,
                                   (int)x - 1, (int)y - h, 1, h);
            ISWRenderEnd(sink->text_sink.render_ctx);
        } else if (state == IswisOff) {
            int h = ScaledFontHeight(sink);
            ISWRenderBegin(sink->text_sink.render_ctx);
            ISWRenderSetColor(sink->text_sink.render_ctx,
                              sink->text_sink.background);
            ISWRenderFillRectangle(sink->text_sink.render_ctx,
                                   (int)x - 1, (int)y - h, 1, h);
            ISWRenderEnd(sink->text_sink.render_ctx);
        }
    }
    sink->text_sink.laststate = state;
}

static void
FindDistance(Widget w, ISWTextPosition fromPos, int fromx,
             ISWTextPosition toPos, int *resWidth,
             ISWTextPosition *resPos, int *resHeight)
{
    TextSinkObject sink = (TextSinkObject) w;
    Widget source = IswTextGetSource(IswParent(w));
    ISWTextPosition index, lastPos;
    unsigned char c;
    ISWTextBlock blk;

    lastPos = GETLASTPOS;
    IswTextSourceRead(source, fromPos, &blk, (int) toPos - fromPos);
    *resWidth = 0;
    {
        unsigned char buf[BUFSIZ];
        int buflen = 0;

        for (index = fromPos; index != toPos && index < lastPos; index++) {
            if (index - blk.firstPos >= blk.length)
                IswTextSourceRead(source, index, &blk, (int) toPos - fromPos);
            c = blk.ptr[index - blk.firstPos];
            if (c == IswLF) {
                index++;
                break;
            }
            if (c == IswTAB || c < (unsigned char)IswSP) {
                if (buflen > 0 && sink->text_sink.render_ctx) {
                    *resWidth += ISWRenderTextWidth(sink->text_sink.render_ctx,
                                                    (char *)buf, buflen);
                } else if (buflen > 0) {
                    *resWidth += ISWScaledTextWidth(IswParent(w),
                                                    sink->text_sink.font,
                                                    (char *)buf, buflen);
                }
                buflen = 0;
                *resWidth += CharWidth(w, fromx + *resWidth, c);
            } else {
                if (c < (unsigned char)IswSP && sink->text_sink.display_nonprinting) {
                    buf[buflen++] = '^';
                    buf[buflen++] = c + '@';
                } else {
                    buf[buflen++] = c;
                }
                if (buflen >= BUFSIZ - 2) {
                    if (sink->text_sink.render_ctx)
                        *resWidth += ISWRenderTextWidth(sink->text_sink.render_ctx,
                                                        (char *)buf, buflen);
                    else
                        *resWidth += ISWScaledTextWidth(IswParent(w),
                                                        sink->text_sink.font,
                                                        (char *)buf, buflen);
                    buflen = 0;
                }
            }
        }
        if (buflen > 0) {
            if (sink->text_sink.render_ctx)
                *resWidth += ISWRenderTextWidth(sink->text_sink.render_ctx,
                                                (char *)buf, buflen);
            else
                *resWidth += ISWScaledTextWidth(IswParent(w),
                                                sink->text_sink.font,
                                                (char *)buf, buflen);
        }
    }
    *resPos = index;
    *resHeight = ScaledFontHeight(sink);
}

static void
FindPosition(Widget w,
             ISWTextPosition fromPos,
             int fromx,
             int width,
             Boolean stopAtWordBreak,
             ISWTextPosition *resPos,
             int *resWidth,
             int *resHeight)
{
    TextSinkObject sink = (TextSinkObject) w;
    Widget source = IswTextGetSource(IswParent(w));
    ISWTextPosition lastPos, index, whiteSpacePosition = 0;
    int lastWidth = 0, whiteSpaceWidth = 0;
    Boolean whiteSpaceSeen;
    unsigned char c;
    ISWTextBlock blk;

    lastPos = GETLASTPOS;
    IswTextSourceRead(source, fromPos, &blk, BUFSIZ);
    *resWidth = 0;
    whiteSpaceSeen = FALSE;
    c = 0;
    index = fromPos;
    while (*resWidth <= width && index < lastPos) {
        lastWidth = *resWidth;
        if (index - blk.firstPos >= blk.length)
            IswTextSourceRead(source, index, &blk, BUFSIZ);
        c = blk.ptr[index - blk.firstPos];

        int step;
        if (c == IswLF || c == IswTAB || c < (unsigned char)IswSP) {
            *resWidth += CharWidth(w, fromx + *resWidth, c);
            step = 1;
        } else {
            int avail = (int)(blk.length - (index - blk.firstPos));
            step = _IswUtf8CharLen((const char *)(blk.ptr + (index - blk.firstPos)), avail);
            if (step <= 0) step = 1;
            if (step > avail) step = avail;
            if (sink->text_sink.render_ctx) {
                *resWidth += ISWRenderTextWidth(sink->text_sink.render_ctx,
                                                (const char *)(blk.ptr + (index - blk.firstPos)),
                                                step);
            } else {
                *resWidth += ISWScaledTextWidth(IswParent(w), sink->text_sink.font,
                                                (const char *)(blk.ptr + (index - blk.firstPos)),
                                                step);
            }
        }

        if ((c == IswSP || c == IswTAB) && *resWidth <= width) {
            whiteSpaceSeen = TRUE;
            whiteSpacePosition = index;
            whiteSpaceWidth = *resWidth;
        }
        if (c == IswLF) {
            index += step;
            break;
        }
        index += step;
    }
    if (*resWidth > width && index > fromPos) {
        *resWidth = lastWidth;
        ISWTextPosition prev = fromPos;
        ISWTextPosition probe = fromPos;
        while (probe < index) {
            if (probe - blk.firstPos >= blk.length)
                IswTextSourceRead(source, probe, &blk, BUFSIZ);
            unsigned char pc = blk.ptr[probe - blk.firstPos];
            int st;
            if (pc == IswLF || pc == IswTAB || pc < (unsigned char)IswSP) {
                st = 1;
            } else {
                int av = (int)(blk.length - (probe - blk.firstPos));
                st = _IswUtf8CharLen((const char *)(blk.ptr + (probe - blk.firstPos)), av);
                if (st <= 0) st = 1;
            }
            prev = probe;
            probe += st;
        }
        index = prev;
        if (stopAtWordBreak && whiteSpaceSeen) {
            index = whiteSpacePosition + 1;
            *resWidth = whiteSpaceWidth;
        }
    }
    if (index == lastPos && c != IswLF) index = lastPos + 1;
    *resPos = index;
    *resHeight = ScaledFontHeight(sink);
}

static void
Resolve(Widget w, ISWTextPosition pos, int fromx, int width, ISWTextPosition *resPos)
{
    int resWidth, resHeight;
    Widget source = IswTextGetSource(IswParent(w));

    FindPosition(w, pos, fromx, width, FALSE, resPos, &resWidth, &resHeight);
    if (*resPos > GETLASTPOS)
        *resPos = GETLASTPOS;
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    TextSinkObject sink = (TextSinkObject) new;

    sink->text_sink.tab_count = 0;
    sink->text_sink.tabs = NULL;
    sink->text_sink.char_tabs = NULL;

    if (sink->text_sink.font == NULL) {
        IswAppWarning(IswWidgetToApplicationContext(new),
                      "TextSink widget: font is NULL - text rendering will fail");
    }

    sink->text_sink.insertCursorOn = CreateInsertCursor(new);
    sink->text_sink.laststate = IswisOff;
    sink->text_sink.cursor_x = sink->text_sink.cursor_y = 0;
    sink->text_sink.render_ctx = NULL;
}

static void
Destroy(Widget w)
{
    TextSinkObject sink = (TextSinkObject) w;

    IswFree((char *) sink->text_sink.tabs);
    IswFree((char *) sink->text_sink.char_tabs);

    if (sink->text_sink.render_ctx) {
        ISWRenderDestroy(sink->text_sink.render_ctx);
        sink->text_sink.render_ctx = NULL;
    }

    ISWFreePixmap(_IswXcbConn(IswDisplayOfObject(w)), sink->text_sink.insertCursorOn);
}

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    TextSinkObject w = (TextSinkObject) new;
    TextSinkObject old_w = (TextSinkObject) current;

    Bool font_changed = False;
    if (w->text_sink.font != NULL && old_w->text_sink.font != NULL) {
        font_changed = (w->text_sink.font->fid != old_w->text_sink.font->fid);
    } else if (w->text_sink.font != old_w->text_sink.font) {
        font_changed = True;
    }

    if (font_changed ||
        w->text_sink.background != old_w->text_sink.background ||
        w->text_sink.foreground != old_w->text_sink.foreground) {
        ((TextWidget)IswParent(new))->text.redisplay_needed = True;
    } else {
        if ((w->text_sink.echo != old_w->text_sink.echo) ||
            (w->text_sink.display_nonprinting !=
                 old_w->text_sink.display_nonprinting))
            ((TextWidget)IswParent(new))->text.redisplay_needed = True;
    }

    return False;
}

/* ARGSUSED */
static int
MaxLines(Widget w, Dimension height)
{
    TextSinkObject sink = (TextSinkObject) w;
    int font_height = ScaledFontHeight(sink);
    return ((int) height) / font_height;
}

/* ARGSUSED */
static int
MaxHeight(Widget w, int lines)
{
    TextSinkObject sink = (TextSinkObject) w;
    int line_height = ScaledFontHeight(sink);
    return lines * line_height;
}

static void
SetTabs(Widget w, int tab_count, short *tabs)
{
    TextSinkObject sink = (TextSinkObject) w;
    int i;
    unsigned long figure_width = 0;
    IswFontStruct *font = sink->text_sink.font;

    figure_width = ISWScaledTextWidth(IswParent(w), font, "$", 1);
    if (figure_width == 0)
        figure_width = 8;

    if (tab_count > sink->text_sink.tab_count) {
        sink->text_sink.tabs = (Position *)
            IswRealloc((char *) sink->text_sink.tabs,
                       (Cardinal) (tab_count * sizeof(Position)));
        sink->text_sink.char_tabs = (short *)
            IswRealloc((char *) sink->text_sink.char_tabs,
                       (Cardinal) (tab_count * sizeof(short)));
    }

    for (i = 0; i < tab_count; i++) {
        sink->text_sink.tabs[i] = tabs[i] * figure_width;
        sink->text_sink.char_tabs[i] = tabs[i];
    }

    sink->text_sink.tab_count = tab_count;

#ifndef NO_TAB_FIX
    {
        TextWidget ctx = (TextWidget)IswParent(w);
        ctx->text.redisplay_needed = True;
        _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
    }
#endif
}

/************************************************************
 *
 * Public dispatch API — unchanged from the old abstract TextSink.
 *
 ************************************************************/

void
IswTextSinkDisplayText(Widget w,
#if NeedWidePrototypes
                       int x, int y,
#else
                       Position x, Position y,
#endif
                       ISWTextPosition pos1, ISWTextPosition pos2,
#if NeedWidePrototypes
                       int highlight)
#else
                       Boolean highlight)
#endif
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    (*class->text_sink_class.DisplayText)(w, x, y, pos1, pos2, highlight);
}

void
IswTextSinkInsertCursor(Widget w,
#if NeedWidePrototypes
                        int x, int y, int state)
#else
                        Position x, Position y, IswTextInsertState state)
#endif
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    (*class->text_sink_class.InsertCursor)(w, x, y, state);
}

void
IswTextSinkClearToBackground(Widget w,
#if NeedWidePrototypes
                             int x, int y, int width, int height)
#else
                             Position x, Position y,
                             Dimension width, Dimension height)
#endif
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    (*class->text_sink_class.ClearToBackground)(w, x, y, width, height);
}

void
IswTextSinkFindPosition(Widget w, ISWTextPosition fromPos, int fromx,
                        int width,
#if NeedWidePrototypes
                        int stopAtWordBreak,
#else
                        Boolean stopAtWordBreak,
#endif
                        ISWTextPosition *resPos, int *resWidth, int *resHeight)
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    (*class->text_sink_class.FindPosition)(w, fromPos, fromx, width,
                                            stopAtWordBreak,
                                            resPos, resWidth, resHeight);
}

void
IswTextSinkFindDistance(Widget w, ISWTextPosition fromPos, int fromx,
                        ISWTextPosition toPos, int *resWidth,
                        ISWTextPosition *resPos, int *resHeight)
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    (*class->text_sink_class.FindDistance)(w, fromPos, fromx, toPos,
                                           resWidth, resPos, resHeight);
}

void
IswTextSinkResolve(Widget w, ISWTextPosition pos, int fromx, int width,
                   ISWTextPosition *resPos)
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    (*class->text_sink_class.Resolve)(w, pos, fromx, width, resPos);
}

int
IswTextSinkMaxLines(Widget w,
#if NeedWidePrototypes
                    int height)
#else
                    Dimension height)
#endif
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    return (*class->text_sink_class.MaxLines)(w, height);
}

int
IswTextSinkMaxHeight(Widget w, int lines)
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    return (*class->text_sink_class.MaxHeight)(w, lines);
}

void
IswTextSinkSetTabs(Widget w, int tab_count, int *tabs)
{
    if (tab_count > 0) {
        TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
        short *char_tabs = (short*)IswMalloc((unsigned)tab_count * sizeof(short));
        short *tab;
        int i;

        for (i = tab_count, tab = char_tabs; i; i--) *tab++ = (short)*tabs++;

        (*class->text_sink_class.SetTabs)(w, tab_count, char_tabs);
        IswFree((char *)char_tabs);
    }
}

void
IswTextSinkGetCursorBounds(Widget w, IswRectangle *rect)
{
    TextSinkObjectClass class = (TextSinkObjectClass) w->core.widget_class;
    (*class->text_sink_class.GetCursorBounds)(w, rect);
}
