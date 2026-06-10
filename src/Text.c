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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/Shell.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <ISW/ISWInit.h>
#include <ISW/Cardinals.h>
#include <ISW/Scrollbar.h>
#include <ISW/TextP.h>
#include <ISW/IswArgMacros.h>
#include <ISW/ISWImP.h>
#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"
#include "ISWXcbDraw.h"
#include <ctype.h>		/* for isprint() */

/* XCB: Missing Xlib constants for text property conversions */
#ifndef Success
#define Success 0
#endif

#ifndef XCompoundTextStyle
#define XCompoundTextStyle 1
#endif

#ifndef XStringStyle
#define XStringStyle 0
#endif

#ifndef XTextStyle
#define XTextStyle XStringStyle
#endif

#ifndef MAX_LEN_CT
#define MAX_LEN_CT 6		/* for sequence: ESC $ ( A \xx \xx */
#endif

unsigned long FMT8BIT = 0L;
unsigned long IswFmt8Bit = 0L;

#define SinkClearToBG          IswTextSinkClearToBackground

#define SrcScan                IswTextSourceScan
#define SrcRead                IswTextSourceRead
#define SrcReplace             IswTextSourceReplace
#define SrcSearch              IswTextSourceSearch
#define SrcCvtSel              IswTextSourceConvertSelection
#define SrcSetSelection        IswTextSourceSetSelection

#define BIGNUM ((Dimension)32023)
#define MULTI_CLICK_TIME 500L


#define IsValidLine(ctx, num) ( ((num) == 0) || \
			        ((ctx)->text.lt.info[(num)].position != 0) )

/*
 * Defined in TextAction.c
 */
extern void _IswTextZapSelection(TextWidget, IswEvent *, Boolean);


/*
 * Defined in Text.c
 */
static void UnrealizeScrollbars(Widget, IswPointer, IswPointer);
static void VScroll(Widget, IswPointer, IswPointer);
static void VJump(Widget, IswPointer, IswPointer);
static void HScroll(Widget, IswPointer, IswPointer);
static void HJump(Widget, IswPointer, IswPointer);
static void ClearWindow(Widget);
static void DisplayTextWindow(Widget);
static void PaintScrollbars(TextWidget);
static Widget TextHitChild(Widget, int, int, int *, int *);
static Widget TextNthWindowlessChild(Widget, int);
static void ModifySelection(TextWidget, ISWTextPosition, ISWTextPosition);
static void PushCopyQueue(TextWidget, int, int);
static void UpdateTextInLine(TextWidget, int, Position, Position);
static void UpdateTextInRectangle(TextWidget, IswRectangle *);
static void PopCopyQueue(TextWidget);
static void FlushUpdate(TextWidget);
static Boolean LineAndXYForPosition(TextWidget, ISWTextPosition, int *,
                                    Position *, Position *);
static Boolean TranslateExposeRegion(TextWidget, IswRectangle *);
static ISWTextPosition FindGoodPosition(TextWidget, ISWTextPosition);
static ISWTextPosition _BuildLineTable(TextWidget, ISWTextPosition,
                                       ISWTextPosition, int);

void _IswTextAlterSelection(TextWidget, IswTextSelectionMode,
                            IswTextSelectionAction, String *, Cardinal *);
void _IswTextCheckResize(TextWidget);
void _IswTextClearAndCenterDisplay(TextWidget);
void _IswTextExecuteUpdate(TextWidget);
char *_IswTextGetText(TextWidget, ISWTextPosition, ISWTextPosition);
void _IswTextNeedsUpdating(TextWidget, ISWTextPosition, ISWTextPosition);
IswSelectionId * _IswTextSelectionList(TextWidget, String *, Cardinal);
void _IswTextSetScrollBars(TextWidget);
void _IswTextSetSelection(TextWidget, ISWTextPosition, ISWTextPosition,
                          String *, Cardinal);
void _IswTextShowPosition(TextWidget);
void _IswTextPrepareToUpdate(TextWidget);
int _IswTextReplace(TextWidget, ISWTextPosition, ISWTextPosition, ISWTextBlock *);
void _IswTextVScroll(TextWidget, int);

/* Shadow drawing removed - ThreeD eliminated */
static void
_TextDrawShadows(TextWidget ctx, Position x0, Position y0, Position x1, Position y1, Boolean out)
{
    /* No-op: shadow drawing functionality removed */
}

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

static IswTextSelectType defaultSelectTypes[] = {
  IswselectPosition, IswselectWord, IswselectLine, IswselectParagraph,
  IswselectAll,      IswselectNull,
};

static IswPointer defaultSelectTypesPtr = (IswPointer)defaultSelectTypes;
extern const char *_IswDefaultTextTranslations1, *_IswDefaultTextTranslations2,
  *_IswDefaultTextTranslations3, *_IswDefaultTextTranslations4;
static Dimension defWidth = 100;
static Dimension defHeight = DEFAULT_TEXT_HEIGHT;

#define offset(field) IswOffsetOf(TextRec, field)
static IswResource resources[] = {
  {IswNwidth, IswCWidth, IswRDimension, sizeof(Dimension),
     offset(core.width), IswRDimension, (IswPointer)&defWidth},
  {IswNcursor, IswCCursor, IswRCursor, sizeof(xcb_cursor_t),
     offset(simple.cursor), IswRString, (IswPointer)"xterm"},
  {IswNheight, IswCHeight, IswRDimension, sizeof(Dimension),
     offset(core.height), IswRDimension, (IswPointer)&defHeight},
  {IswNdisplayPosition, IswCTextPosition, IswRInt, sizeof(ISWTextPosition),
     offset(text.lt.top), IswRImmediate, (IswPointer)0},
  {IswNinsertPosition, IswCTextPosition, IswRInt, sizeof(ISWTextPosition),
     offset(text.insertPos), IswRImmediate,(IswPointer)0},
  {IswNleftMargin, IswCMargin, IswRPosition, sizeof (Position),
     offset(text.r_margin.left), IswRImmediate, (IswPointer)2},
  {IswNrightMargin, IswCMargin, IswRPosition, sizeof (Position),
     offset(text.r_margin.right), IswRImmediate, (IswPointer)4},
  {IswNtopMargin, IswCMargin, IswRPosition, sizeof (Position),
     offset(text.r_margin.top), IswRImmediate, (IswPointer)2},
  {IswNbottomMargin, IswCMargin, IswRPosition, sizeof (Position),
     offset(text.r_margin.bottom), IswRImmediate, (IswPointer)2},
  {IswNselectTypes, IswCSelectTypes, IswRPointer,
     sizeof(IswTextSelectType*), offset(text.sarray),
     IswRPointer, (IswPointer)&defaultSelectTypesPtr},
  {IswNtextSource, IswCTextSource, IswRWidget, sizeof (Widget),
     offset(text.source), IswRImmediate, NULL},
  {IswNtextSink, IswCTextSink, IswRWidget, sizeof (Widget),
     offset(text.sink), IswRImmediate, NULL},
  {IswNdisplayCaret, IswCOutput, IswRBoolean, sizeof(Boolean),
     offset(text.display_caret), IswRImmediate, (IswPointer)True},
  {IswNscrollVertical, IswCScroll, IswRScrollMode, sizeof(IswTextScrollMode),
     offset(text.scroll_vert), IswRImmediate, (IswPointer) IswtextScrollNever},
  {IswNscrollHorizontal, IswCScroll, IswRScrollMode, sizeof(IswTextScrollMode),
     offset(text.scroll_horiz), IswRImmediate, (IswPointer) IswtextScrollNever},
  {IswNwrap, IswCWrap, IswRWrapMode, sizeof(IswTextWrapMode),
     offset(text.wrap), IswRImmediate, (IswPointer) IswtextWrapNever},
  {IswNresize, IswCResize, IswRResizeMode, sizeof(IswTextResizeMode),
     offset(text.resize), IswRImmediate, (IswPointer) IswtextResizeNever},
  {IswNautoFill, IswCAutoFill, IswRBoolean, sizeof(Boolean),
     offset(text.auto_fill), IswRImmediate, (IswPointer) FALSE},
  {IswNconsumeTab, IswCConsumeTab, IswRBoolean, sizeof(Boolean),
     offset(text.consume_tab), IswRImmediate, (IswPointer) TRUE},
  {IswNunrealizeCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
     offset(text.unrealize_callbacks), IswRCallback, (IswPointer) NULL},
};
#undef offset

/* ARGSUSED */
static void
CvtStringToScrollMode(XrmValuePtr args, Cardinal *num_args, XrmValuePtr fromVal,
                      XrmValuePtr toVal)
{
  static IswTextScrollMode scrollMode;
  static  XrmQuark  QScrollNever, QScrollAlways, QScrollWhenNeeded;
  XrmQuark    q;
  char        lowerName[40];
  static Boolean inited = FALSE;

  if ( !inited ) {
    QScrollNever      = XrmPermStringToQuark(IswEtextScrollNever);
    QScrollWhenNeeded = XrmPermStringToQuark(IswEtextScrollWhenNeeded);
    QScrollAlways     = XrmPermStringToQuark(IswEtextScrollAlways);
    inited = TRUE;
  }

  if (strlen ((char*) fromVal->addr) < sizeof lowerName) {
    ISWCopyISOLatin1Lowered (lowerName, (char *)fromVal->addr);
    q = XrmStringToQuark(lowerName);

    if      (q == QScrollNever)          scrollMode = IswtextScrollNever;
    else if (q == QScrollWhenNeeded)     scrollMode = IswtextScrollWhenNeeded;
    else if (q == QScrollAlways)         scrollMode = IswtextScrollAlways;
    else {
      toVal->size = 0;
      toVal->addr = NULL;
      return;
    }
    toVal->size = sizeof scrollMode;
    toVal->addr = (IswPointer) &scrollMode;
    return;
  }
  toVal->size = 0;
  toVal->addr = NULL;
}

/* ARGSUSED */
static void
CvtStringToWrapMode(XrmValuePtr args, Cardinal *num_args, XrmValuePtr fromVal,
                    XrmValuePtr toVal)
{
  static IswTextWrapMode wrapMode;
  static  XrmQuark QWrapNever, QWrapLine, QWrapWord;
  XrmQuark    q;
  char        lowerName[BUFSIZ];
  static Boolean inited = FALSE;

  if ( !inited ) {
    QWrapNever = XrmPermStringToQuark(IswEtextWrapNever);
    QWrapLine  = XrmPermStringToQuark(IswEtextWrapLine);
    QWrapWord  = XrmPermStringToQuark(IswEtextWrapWord);
    inited = TRUE;
  }

  if (strlen ((char*) fromVal->addr) < sizeof lowerName) {
    ISWCopyISOLatin1Lowered (lowerName, (char *)fromVal->addr);
    q = XrmStringToQuark(lowerName);

    if      (q == QWrapNever)     wrapMode = IswtextWrapNever;
    else if (q == QWrapLine)      wrapMode = IswtextWrapLine;
    else if (q == QWrapWord)      wrapMode = IswtextWrapWord;
    else {
      toVal->size = 0;
      toVal->addr = NULL;
      return;
    }
    toVal->size = sizeof wrapMode;
    toVal->addr = (IswPointer) &wrapMode;
    return;
  }
  toVal->size = 0;
  toVal->addr = NULL;
}

/* ARGSUSED */
static void
CvtStringToResizeMode(XrmValuePtr args, Cardinal *num_args, XrmValuePtr fromVal,
                      XrmValuePtr toVal)
{
  static IswTextResizeMode resizeMode;
  static  XrmQuark  QResizeNever, QResizeWidth, QResizeHeight, QResizeBoth;
  XrmQuark    q;
  char        lowerName[40];
  static Boolean inited = FALSE;

  if ( !inited ) {
    QResizeNever      = XrmPermStringToQuark(IswEtextResizeNever);
    QResizeWidth      = XrmPermStringToQuark(IswEtextResizeWidth);
    QResizeHeight     = XrmPermStringToQuark(IswEtextResizeHeight);
    QResizeBoth       = XrmPermStringToQuark(IswEtextResizeBoth);
    inited = TRUE;
  }

  if (strlen ((char*) fromVal->addr) < sizeof lowerName) {
    ISWCopyISOLatin1Lowered (lowerName, (char *)fromVal->addr);
    q = XrmStringToQuark(lowerName);

    if      (q == QResizeNever)          resizeMode = IswtextResizeNever;
    else if (q == QResizeWidth)          resizeMode = IswtextResizeWidth;
    else if (q == QResizeHeight)         resizeMode = IswtextResizeHeight;
    else if (q == QResizeBoth)           resizeMode = IswtextResizeBoth;
    else {
      toVal->size = 0;
      toVal->addr = NULL;
      return;
    }
    toVal->size = sizeof resizeMode;
    toVal->addr = (IswPointer) &resizeMode;
    return;
  }
  toVal->size = 0;
  toVal->addr = NULL;
}

static void
ClassInitialize(void)
{
  int len1 = strlen (_IswDefaultTextTranslations1);
  int len2 = strlen (_IswDefaultTextTranslations2);
  int len3 = strlen (_IswDefaultTextTranslations3);
  int len4 = strlen (_IswDefaultTextTranslations4);
  char *buf = IswMalloc ((unsigned)(len1 + len2 + len3 + len4 + 1));
  char *cp = buf;

  if (!IswFmt8Bit)
    FMT8BIT = IswFmt8Bit = XrmPermStringToQuark("FMT8BIT");

  IswInitializeWidgetSet();

/*
 * Set the number of actions.
 */

  textClassRec.core_class.num_actions = _IswTextActionsTableCount;

  (void) strcpy( cp, _IswDefaultTextTranslations1); cp += len1;
  (void) strcpy( cp, _IswDefaultTextTranslations2); cp += len2;
  (void) strcpy( cp, _IswDefaultTextTranslations3); cp += len3;
  (void) strcpy( cp, _IswDefaultTextTranslations4);
  textWidgetClass->core_class.tm_table = buf;

  IswAddConverter(IswRString, IswRScrollMode, CvtStringToScrollMode,
			(IswConvertArgList)NULL, (Cardinal)0 );
  IswAddConverter(IswRString, IswRWrapMode,   CvtStringToWrapMode,
			(IswConvertArgList)NULL, (Cardinal)0 );
  IswAddConverter(IswRString, IswRResizeMode, CvtStringToResizeMode,
			(IswConvertArgList)NULL, (Cardinal)0 );
}

/*	Function Name: PositionHScrollBar.
 *	Description: Positions the Horizontal scrollbar.
 *	Arguments: ctx - the text widget.
 *	Returns: none
 */

static void
PositionHScrollBar(TextWidget ctx)
{
  Widget vbar = ctx->text.vbar, hbar = ctx->text.hbar;
  Position top, left = 0;
  int s = 0;

  if (ctx->text.hbar == NULL) return;

  if (vbar != NULL)
    left += (Position) (vbar->core.width + vbar->core.border_width);

  IswResizeWidget( hbar, ctx->core.width - left - s, hbar->core.height,
		 hbar->core.border_width );

  left = s / 2 - (Position) hbar->core.border_width;
  if (left < 0) left = 0;
  if (vbar != NULL)
    left += (Position) (vbar->core.width + vbar->core.border_width);

  top = ctx->core.height - (hbar->core.height + hbar->core.border_width + s / 2);

  IswMoveWidget( hbar, left, top);
}

/*	Function Name: PositionVScrollBar.
 *	Description: Positions the Vertical scrollbar.
 *	Arguments: ctx - the text widget.
 *	Returns: none.
 */

static void
PositionVScrollBar(TextWidget ctx)
{
  Widget vbar = ctx->text.vbar;
  Position pos;
  Dimension bw;
  int s = 0;

  if (vbar == NULL)
    return;
  
  bw = vbar->core.border_width;

  IswResizeWidget( vbar, vbar->core.width, ctx->core.height - s, bw);
  
  pos = s / 2 - (Position)bw;
  if (pos < 0) pos = 0;

  IswMoveWidget( vbar, pos, pos);
}

static void
CreateVScrollBar(TextWidget ctx)
{
  Widget vbar;

  if (ctx->text.vbar != NULL)
    return;

  ctx->text.vbar = vbar =
    IswCreateWidget("vScrollbar", scrollbarWidgetClass, (Widget)ctx,
  (ArgList) NULL, ZERO);
  
  IswAddCallback( vbar, IswNscrollProc, VScroll, (IswPointer)ctx );
  IswAddCallback( vbar, IswNjumpProc, VJump, (IswPointer)ctx );
  if (ctx->text.hbar == NULL)
      IswAddCallback((Widget) ctx, IswNunrealizeCallback, UnrealizeScrollbars,
      (IswPointer) NULL);

  ctx->text.r_margin.left += vbar->core.width + vbar->core.border_width;
  ctx->text.margin.left = ctx->text.r_margin.left;

  PositionVScrollBar(ctx);
  PositionHScrollBar(ctx);	/* May modify location of Horiz. Bar. */

  if (IswIsRealized((Widget)ctx)) {
    IswRealizeWidget(vbar);
    IswMapWidget(vbar);
    _IswPlatformFlush(IswDisplayOf((Widget)ctx));
  }
}

/*	Function Name: DestroyVScrollBar
 *	Description: Removes a vertical ScrollBar.
 *	Arguments: ctx - the parent text widget.
 *	Returns: none.
 */

static void
DestroyVScrollBar(TextWidget ctx)
{
  Widget vbar = ctx->text.vbar;

  if (vbar == NULL) return;

  ctx->text.r_margin.left -= vbar->core.width + vbar->core.border_width;
  ctx->text.margin.left = ctx->text.r_margin.left;
  if (ctx->text.hbar == NULL)
      IswRemoveCallback((Widget) ctx, IswNunrealizeCallback, UnrealizeScrollbars,
		       (IswPointer) NULL);
  IswDestroyWidget(vbar);
  ctx->text.vbar = NULL;
  PositionHScrollBar(ctx);
}

static void
CreateHScrollBar(TextWidget ctx)
{
  IswArgBuilder ab = IswArgBuilderInit();
  Widget hbar;

  if (ctx->text.hbar != NULL) return;

  IswArgOrientation(&ab, IswOrientHorizontal);
  ctx->text.hbar = hbar =
    IswCreateWidget("hScrollbar", scrollbarWidgetClass, (Widget)ctx, ab.args, ab.count);
  IswAddCallback( hbar, IswNscrollProc, HScroll, (IswPointer)ctx );
  IswAddCallback( hbar, IswNjumpProc, HJump, (IswPointer)ctx );
  if (ctx->text.vbar == NULL)
      IswAddCallback((Widget) ctx, IswNunrealizeCallback, UnrealizeScrollbars,
		    (IswPointer) NULL);

/**/
  ctx->text.r_margin.bottom += hbar->core.height + hbar->core.border_width;
  ctx->text.margin.bottom = ctx->text.r_margin.bottom;
/**/
  PositionHScrollBar(ctx);
  if (IswIsRealized((Widget)ctx)) {
    IswRealizeWidget(hbar);
    IswMapWidget(hbar);
  }
}

/*	Function Name: DestroyHScrollBar
 *	Description: Removes a horizontal ScrollBar.
 *	Arguments: ctx - the parent text widget.
 *	Returns: none.
 */

static void
DestroyHScrollBar(TextWidget ctx)
{
  Widget hbar = ctx->text.hbar;

  if (hbar == NULL) return;

/**/
  ctx->text.r_margin.bottom -= hbar->core.height + hbar->core.border_width;
  ctx->text.margin.bottom = ctx->text.r_margin.bottom;
/**/
  if (ctx->text.vbar == NULL)
      IswRemoveCallback((Widget) ctx, IswNunrealizeCallback, UnrealizeScrollbars,
		       (IswPointer) NULL);
  IswDestroyWidget(hbar);
  ctx->text.hbar = NULL;
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
  TextWidget ctx = (TextWidget) new;
  char error_buf[BUFSIZ];
  int s;

  ((SimpleWidget) new)->simple.traversal_on = True;

  /* HiDPI: scale dimension resources */  ctx->text.r_margin.left = (Position)(ctx->text.r_margin.left);
  ctx->text.r_margin.right = (Position)(ctx->text.r_margin.right);
  ctx->text.r_margin.top = (Position)(ctx->text.r_margin.top);
  ctx->text.r_margin.bottom = (Position)(ctx->text.r_margin.bottom);

  s = 0;

  ctx->text.r_margin.left += s;
  ctx->text.r_margin.right += s;
  ctx->text.r_margin.top += s;
  ctx->text.r_margin.bottom += s - 1;

  ctx->text.lt.lines = 0;
  ctx->text.lt.info = NULL;
  memset(&(ctx->text.origSel), 0, sizeof(IswTextSelection));
  memset(&(ctx->text.s), 0, sizeof(IswTextSelection));
  ctx->text.s.type = IswselectPosition;
  ctx->text.salt = NULL;
  ctx->text.hbar = ctx->text.vbar = (Widget) NULL;
  ctx->text.lasttime = 0; /* ||| correct? */
  ctx->text.time = 0; /* ||| correct? */
  ctx->text.showposition = TRUE;

  /* Auto-create a default source and sink unless the caller supplied them. */
  if (ctx->text.source == NULL)
    ctx->text.source = IswCreateWidget("textSource", textSrcObjectClass,
                                        new, args, *num_args);
  if (ctx->text.sink == NULL)
    ctx->text.sink = IswCreateWidget("textSink", textSinkObjectClass,
                                      new, args, *num_args);

  ctx->text.lastPos = GETLASTPOS;
  ctx->text.file_insert = NULL;
  ctx->text.search = NULL;
  ctx->text.updateFrom = (ISWTextPosition *) IswMalloc((unsigned) ONE);
  ctx->text.updateTo = (ISWTextPosition *) IswMalloc((unsigned) ONE);
  ctx->text.numranges = ctx->text.maxranges = 0;

  ctx->text.hasfocus = FALSE;
  ctx->text.margin = ctx->text.r_margin; /* Strucure copy. */
  ctx->text.update_disabled = FALSE;
  ctx->text.old_insert = -1;
  ctx->text.mult = 1;
  ctx->text.single_char = FALSE;
  ctx->text.copy_area_offsets = NULL;
  ctx->text.salt2 = NULL;

  if (ctx->core.height == DEFAULT_TEXT_HEIGHT)
    ctx->core.height = VMargins(ctx) + IswTextSinkMaxHeight(ctx->text.sink, 1);

  /* Default tab stops every 8 chars. */
  {
    int tabs[32], tab, i;
    for (i = 0, tab = 0; i < 32; i++)
      tabs[i] = (tab += 8);
    IswTextSinkSetTabs(ctx->text.sink, 32, tabs);
  }

  IswTextDisableRedisplay(new);
  IswTextEnableRedisplay(new);

  if (ctx->text.scroll_vert != IswtextScrollNever) {
    if ( (ctx->text.resize == IswtextResizeHeight) ||
     	 (ctx->text.resize == IswtextResizeBoth) ) {
      (void) sprintf(error_buf, "Isw Text Widget %s:\n %s %s.", ctx->core.name,
	      "Vertical scrolling not allowed with height resize.\n",
	      "Vertical scrolling has been DEACTIVATED.");
      IswAppWarning(IswWidgetToApplicationContext(new), error_buf);
      ctx->text.scroll_vert = IswtextScrollNever;
    }
    else if (ctx->text.scroll_vert == IswtextScrollAlways)
      CreateVScrollBar(ctx);
  }

  if (ctx->text.scroll_horiz != IswtextScrollNever) {
    if (ctx->text.wrap != IswtextWrapNever) {
      (void) sprintf(error_buf, "Isw Text Widget %s:\n %s %s.", ctx->core.name,
	      "Horizontal scrolling not allowed with wrapping active.\n",
	      "Horizontal scrolling has been DEACTIVATED.");
      IswAppWarning(IswWidgetToApplicationContext(new), error_buf);
      ctx->text.scroll_horiz = IswtextScrollNever;
    }
    else if ( (ctx->text.resize == IswtextResizeWidth) ||
	      (ctx->text.resize == IswtextResizeBoth) ) {
      (void) sprintf(error_buf, "Isw Text Widget %s:\n %s %s.", ctx->core.name,
	      "Horizontal scrolling not allowed with width resize.\n",
	      "Horizontal scrolling has been DEACTIVATED.");
      IswAppWarning(IswWidgetToApplicationContext(new), error_buf);
      ctx->text.scroll_horiz = IswtextScrollNever;
    }
    else if (ctx->text.scroll_horiz == IswtextScrollAlways)
      CreateHScrollBar(ctx);
  }

  /* Windowless: draw into the parent's window, no own X window.  Scrolling
     is done by full repaint (not xcb_copy_area) so it works without a
     dedicated window. */
  new->core.windowless = True;
}

static void
Realize(IswDisplay conn, Widget w, IswValueMask *valueMask, uint32_t *attributes)
{
  TextWidget ctx = (TextWidget)w;

  /* XCB-based libXt realize function requires 4 arguments */
  (*textClassRec.core_class.superclass->core_class.realize)
    (conn, w, valueMask, attributes);

  if (ctx->text.hbar != NULL) {	        /* Put up Hbar -- Must be first. */
    IswRealizeWidget(ctx->text.hbar);
    IswMapWidget(ctx->text.hbar);
  }

  if (ctx->text.vbar != NULL) {	        /* Put up Vbar. */
    IswRealizeWidget(ctx->text.vbar);
    IswMapWidget(ctx->text.vbar);
    _IswPlatformFlush(conn);
  }

  _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
  _IswTextSetScrollBars(ctx);
  _IswTextCheckResize(ctx);
}

/*ARGSUSED*/
static void
UnrealizeScrollbars(Widget widget, IswPointer client, IswPointer call)
{
    TextWidget ctx = (TextWidget) widget;

    if (ctx->text.hbar)
	IswUnrealizeWidget(ctx->text.hbar);
    if (ctx->text.vbar)
	IswUnrealizeWidget(ctx->text.vbar);
}

/* Utility routines for support of Text */


/*
 * Procedure to manage insert cursor visibility for editable text.  It uses
 * the value of ctx->insertPos and an implicit argument. In the event that
 * position is immediately preceded by an eol graphic, then the insert cursor
 * is displayed at the beginning of the next line.
*/
static void
InsertCursor (Widget w, IswTextInsertState state)
{
  TextWidget ctx = (TextWidget)w;
  Position x, y;
  int line;

  if (ctx->text.lt.lines < 1) return;

  if ( LineAndXYForPosition(ctx, ctx->text.insertPos, &line, &x, &y) ) {
    if (line < ctx->text.lt.lines)
      y += (ctx->text.lt.info[line + 1].y - ctx->text.lt.info[line].y) + 1;
    else
      y += (ctx->text.lt.info[line].y - ctx->text.lt.info[line - 1].y) + 1;

    if (ctx->text.display_caret)
      IswTextSinkInsertCursor(ctx->text.sink, x, y, state);
  }
  ctx->text.ev_x = x;
  ctx->text.ev_y = y;

  /* Keep Input Method up to speed  */

  if ( ctx->simple.international ) {
    IswArgBuilder ab = IswArgBuilderInit();

    IswArgInsertPosition(&ab, ctx->text.insertPos);
    _IswImSetValues (w, ab.args, ab.count);
  }
}

/*
 * Procedure to register a span of text that is no longer valid on the display
 * It is used to avoid a number of small, and potentially overlapping, screen
 * updates.
*/

void
_IswTextNeedsUpdating(TextWidget ctx, ISWTextPosition left, ISWTextPosition right)
{
  int i;
  if (left < right) {
    for (i = 0; i < ctx->text.numranges; i++) {
      if (left <= ctx->text.updateTo[i] && right >= ctx->text.updateFrom[i]) {
	ctx->text.updateFrom[i] = IswMin(left, ctx->text.updateFrom[i]);
	ctx->text.updateTo[i] = IswMax(right, ctx->text.updateTo[i]);
	return;
      }
    }
    ctx->text.numranges++;
    if (ctx->text.numranges > ctx->text.maxranges) {
      ctx->text.maxranges = ctx->text.numranges;
      i = ctx->text.maxranges * sizeof(ISWTextPosition);
      ctx->text.updateFrom = (ISWTextPosition *)
	IswRealloc((char *)ctx->text.updateFrom, (unsigned) i);
      ctx->text.updateTo = (ISWTextPosition *)
	IswRealloc((char *)ctx->text.updateTo, (unsigned) i);
    }
    ctx->text.updateFrom[ctx->text.numranges - 1] = left;
    ctx->text.updateTo[ctx->text.numranges - 1] = right;
  }
}

/*
 * Procedure to read a span of text as a flat byte buffer. This is purely a hack and
 * we probably need to add a function to sources to provide this functionality.
 * [note: this is really a private procedure but is used in multiple modules].
 */

char *
_IswTextGetText(TextWidget ctx, ISWTextPosition left, ISWTextPosition right)
{
  char *result, *tempResult;
  ISWTextBlock text;
  int bytes;

  bytes = sizeof(unsigned char);

  /* leave space for ZERO */
  tempResult=result=IswMalloc( (unsigned)(((Cardinal)(right-left))+ONE )* bytes);
  while (left < right) {
    left = SrcRead(ctx->text.source, left, &text, (int)(right - left));
    if (!text.length)
	break;
    memmove(tempResult, text.ptr, text.length * bytes);
    tempResult += text.length * bytes;
  }

  if (bytes == sizeof(wchar_t))
      *((wchar_t*)tempResult) = (wchar_t)0;
  else
      *tempResult = '\0';
  return(result);
}

/* Like _IswTextGetText, but enforces ICCCM STRING type encoding.  This
routine is currently used to put just the ASCII chars in the selection into a
cut buffer. */

char *
_IswTextGetSTRING(TextWidget ctx, ISWTextPosition left, ISWTextPosition right)
{
  unsigned char *s;
  unsigned char c;
  long i, j, n;

  {
     s = (unsigned char *)_IswTextGetText(ctx, left, right);
     /* only HT and NL control chars are allowed, strip out others */
     n = strlen((char *)s);
     i = 0;
     for (j = 0; j < n; j++) {
	c = s[j];
	if (((c >= 0x20) && c <= 0x7f) ||
	   (c >= 0xa0) || (c == IswTAB) || (c == IswLF) || (c == IswESC)) {
	   s[i] = c;
	   i++;
	}
     }
     s[i] = 0;
     return (char *)s;
  }
#undef ESC

}

/*
 * This routine maps an x and y position in a window that is displaying text
 * into the corresponding position in the source.
 *
 * NOTE: it is illegal to call this routine unless there is a valid line table!
 */

/*** figure out what line it is on ***/

static ISWTextPosition
PositionForXY (TextWidget ctx, Position x, Position y)
{
  int fromx, line, width, height;
  ISWTextPosition position;

  if (ctx->text.lt.lines == 0) return 0;

  for (line = 0; line < ctx->text.lt.lines - 1; line++) {
    if (y <= ctx->text.lt.info[line + 1].y)
      break;
  }
  position = ctx->text.lt.info[line].position;
  if (position >= ctx->text.lastPos)
    return(ctx->text.lastPos);
  fromx = (int) ctx->text.margin.left;
  IswTextSinkFindPosition( ctx->text.sink, position, fromx, x - fromx,
			  FALSE, &position, &width, &height);
  if (position > ctx->text.lastPos) return(ctx->text.lastPos);
  if (position >= ctx->text.lt.info[line + 1].position)
    position = SrcScan(ctx->text.source, ctx->text.lt.info[line + 1].position,
		       IswstPositions, IswsdLeft, 1, TRUE);
  return(position);
}

/*
 * This routine maps a source position in to the corresponding line number
 * of the text that is displayed in the window.
 *
 * NOTE: It is illegal to call this routine unless there is a valid line table!
 */

static int
LineForPosition (TextWidget ctx, ISWTextPosition position)
{
  int line;

  for (line = 0; line < ctx->text.lt.lines; line++)
    if (position < ctx->text.lt.info[line + 1].position)
      break;
  return(line);
}

/*
 * This routine maps a source position into the corresponding line number
 * and the x, y coordinates of the text that is displayed in the window.
 *
 * NOTE: It is illegal to call this routine unless there is a valid line table!
 */

static Boolean
LineAndXYForPosition(TextWidget ctx, ISWTextPosition pos, int *line,
                     Position *x, Position *y)
{
  ISWTextPosition linePos, endPos;
  Boolean visible;
  int realW, realH;

  *line = 0;
  *x = ctx->text.margin.left;
  *y = ctx->text.margin.top;
  if ((visible = IsPositionVisible(ctx, pos))) {
    *line = LineForPosition(ctx, pos);
    *y = ctx->text.lt.info[*line].y;
    *x = ctx->text.margin.left;
    linePos = ctx->text.lt.info[*line].position;
    IswTextSinkFindDistance( ctx->text.sink, linePos,
			    *x, pos, &realW, &endPos, &realH);
    *x += realW;
  }
  return(visible);
}

/*
 * This routine builds a line table. It does this by starting at the
 * specified position and measuring text to determine the staring position
 * of each line to be displayed. It also determines and saves in the
 * linetable all the required metrics for displaying a given line (e.g.
 * x offset, y offset, line length, etc.).
 */

void
_IswTextBuildLineTable (
    TextWidget ctx,
    ISWTextPosition position,
    _IswBoolean force_rebuild)
{
  Dimension height = 0;
  int lines = 0;
  Cardinal size;

  if ((int)ctx->core.height > VMargins(ctx)) {
    height = ctx->core.height - VMargins(ctx);
    lines = IswTextSinkMaxLines(ctx->text.sink, height);
  }
  size = sizeof(IswTextLineTableEntry) * (lines + 1);

  if ( (lines != ctx->text.lt.lines) || (ctx->text.lt.info == NULL) ) {
    ctx->text.lt.info = (IswTextLineTableEntry *) IswRealloc((char *) ctx->text.
							    lt.info, size);
    ctx->text.lt.lines = lines;
    force_rebuild = TRUE;
  }

  if ( force_rebuild || (position != ctx->text.lt.top) ) {
    memset(ctx->text.lt.info, 0, size);
    (void) _BuildLineTable(ctx, ctx->text.lt.top = position, zeroPosition, 0);
  }
}

/*
 * This assumes that the line table does not change size.
 */

static ISWTextPosition
_BuildLineTable(TextWidget ctx, ISWTextPosition position,
                ISWTextPosition min_pos, int line)
{
  IswTextLineTableEntry * lt = ctx->text.lt.info + line;
  ISWTextPosition endPos;
  Position y;
  int count, width, realW, realH;
  Widget src = ctx->text.source;

  if ( ((ctx->text.resize == IswtextResizeWidth) ||
	(ctx->text.resize == IswtextResizeBoth)    ) ||
       (ctx->text.wrap == IswtextWrapNever) )
    width = BIGNUM;
  else
    width = IswMax(0, ((int)ctx->core.width - (int)HMargins(ctx)));

  y = ( (line == 0) ? ctx->text.margin.top : lt->y );

  /* CONSTCOND */
  while ( TRUE ) {
    lt->y = y;
    lt->position = position;

    IswTextSinkFindPosition( ctx->text.sink, position, ctx->text.margin.left,
			    width, ctx->text.wrap == IswtextWrapWord,
			    &endPos, &realW, &realH);
    lt->textWidth = realW;
    y += realH;

    if (ctx->text.wrap == IswtextWrapNever)
      endPos = SrcScan(src, position, IswstEOL, IswsdRight, 1, TRUE);

    if ( endPos == ctx->text.lastPos) { /* We have reached the end. */
      if(SrcScan(src, position, IswstEOL, IswsdRight, 1, FALSE) == endPos)
	break;
    }

    ++lt;
    ++line;
    if ( (line > ctx->text.lt.lines) ||
	 ((lt->position == (position = endPos)) && (position > min_pos)) )
      return(position);
  }

/*
 * If we are at the end of the buffer put two special lines in the table.
 *
 * a) Both have position > text.lastPos and lt->textWidth = 0.
 * b) The first has a real height, and the second has a height that
 *    is the rest of the screen.
 *
 * I could fill in the rest of the table with valid heights and a large
 * lastPos, but this method keeps the number of fill regions down to a
 * minimum.
 *
 * One valid entry is needed at the end of the table so that the cursor
 * does not jump off the bottom of the window.
 */

  for ( count = 0; count < 2 ; count++)
    if (line++ < ctx->text.lt.lines) { /* make sure not to run of the end. */
      (++lt)->y = (count == 0) ? y : ctx->core.height;
      lt->textWidth = 0;
      lt->position = ctx->text.lastPos + 100;
    }

  if (line < ctx->text.lt.lines) /* Clear out rest of table. */
    memset((lt + 1), 0,
	  (ctx->text.lt.lines - line) * sizeof(IswTextLineTableEntry) );

  ctx->text.lt.info[ctx->text.lt.lines].position = lt->position;

  return(endPos);
}

/*	Function Name: GetWidestLine
 *	Description: Returns the width (in pixels) of the widest line that
 *                   is currently visable.
 *	Arguments: ctx - the text widget.
 *	Returns: the width of the widest line.
 *
 * NOTE: This function requires a valid line table.
 */

static Dimension
GetWidestLine(TextWidget ctx)
{
  int i;
  Dimension widest;
  IswTextLineTablePtr lt = &(ctx->text.lt);

  for (i = 0, widest = 1 ; i < lt->lines ; i++)
    if (widest < lt->info[i].textWidth)
      widest = lt->info[i].textWidth;

  return(widest);
}

static void
CheckVBarScrolling(TextWidget ctx)
{
  float first, last;
  Boolean temp = (ctx->text.vbar == NULL);

  if (ctx->text.scroll_vert == IswtextScrollNever) return;

  if ( (ctx->text.lastPos > 0) && (ctx->text.lt.lines > 0)) {
    ISWTextPosition visible_end, raw_pos;

    first = ctx->text.lt.top;
    first /= (float) ctx->text.lastPos;

    /* Get the visible end position, but clamp to lastPos
     * (line table uses sentinel values > lastPos) */
    raw_pos = ctx->text.lt.info[ctx->text.lt.lines].position;
    visible_end = raw_pos;
    if (visible_end > ctx->text.lastPos)
      visible_end = ctx->text.lastPos;

    last = visible_end;
    last /= (float) ctx->text.lastPos;

    if (ctx->text.scroll_vert == IswtextScrollWhenNeeded) {
      int line;
      ISWTextPosition last_pos;
      Position y = ctx->core.height - ctx->text.margin.bottom;

      if (ctx->text.hbar != NULL)
	y -= (ctx->text.hbar->core.height +
	      2 * ctx->text.hbar->core.border_width);

      last_pos = PositionForXY(ctx, (Position) ctx->core.width, y);
      line = LineForPosition(ctx, last_pos);

      if ( (y < ctx->text.lt.info[line + 1].y) || ((last - first) < 1.0) )
	CreateVScrollBar(ctx);
      else
	DestroyVScrollBar(ctx);
    }

    if (ctx->text.vbar != NULL)
      ISWScrollbarSetThumb(ctx->text.vbar, first, last - first);

    if ( (ctx->text.vbar == NULL) != temp) {
      _IswTextNeedsUpdating(ctx, zeroPosition, ctx->text.lastPos);
      if (ctx->text.vbar == NULL)
	_IswTextBuildLineTable (ctx, zeroPosition, FALSE);
    }
  }
  else if (ctx->text.vbar != NULL) {
    if (ctx->text.scroll_vert == IswtextScrollWhenNeeded)
      DestroyVScrollBar(ctx);
    else if (ctx->text.scroll_vert == IswtextScrollAlways)
      ISWScrollbarSetThumb(ctx->text.vbar, 0.0, 1.0);
  }
}

/*
 * This routine is used by Text to notify an associated scrollbar of the
 * correct metrics (position and shown fraction) for the text being currently
 * displayed in the window.
 */

void
_IswTextSetScrollBars(TextWidget ctx)
{
  float first, last, widest;
  Boolean temp = (ctx->text.hbar == NULL);
  Boolean vtemp = (ctx->text.vbar == NULL);
  int s = 0;

  CheckVBarScrolling(ctx);

  if (ctx->text.scroll_horiz == IswtextScrollNever) return;

  if (ctx->text.vbar != NULL)
    widest = (int)(ctx->core.width - ctx->text.vbar->core.width -
		   2 * s - ctx->text.vbar->core.border_width);
  else
    widest = ctx->core.width - 2 * s;
  widest /= (last = GetWidestLine(ctx));
  if (ctx->text.scroll_horiz == IswtextScrollWhenNeeded) {
    if (widest < 1.0)
      CreateHScrollBar(ctx);
    else
      DestroyHScrollBar(ctx);
  }

  if ( (ctx->text.hbar == NULL) != temp ) {
    _IswTextBuildLineTable (ctx, ctx->text.lt.top, TRUE);
    CheckVBarScrolling(ctx);	/* Recheck need for vbar, now that we added
				   or removed the hbar.*/
  }

  if (ctx->text.hbar != NULL) {
    first = ctx->text.r_margin.left - ctx->text.margin.left;
    first /= last;
    ISWScrollbarSetThumb(ctx->text.hbar, first, widest);
  }

  if (((ctx->text.hbar == NULL) && (ctx->text.margin.left !=
				   ctx->text.r_margin.left)) ||
      (ctx->text.vbar == NULL) != vtemp)
  {
    ctx->text.margin.left = ctx->text.r_margin.left;
    _IswTextNeedsUpdating(ctx, zeroPosition, ctx->text.lastPos);
    FlushUpdate(ctx);
  }
}

/*
 * The routine will scroll the displayed text by lines.  If the arg  is
 * positive, move up; otherwise, move down. [note: this is really a private
 * procedure but is used in multiple modules].
 */

void
_IswTextVScroll(TextWidget ctx, int n)
{
  ISWTextPosition top, target;
  int y;
  IswTextLineTable * lt = &(ctx->text.lt);
  int s = 0;

  if (abs(n) > ctx->text.lt.lines)
    n = (n > 0) ? ctx->text.lt.lines : -ctx->text.lt.lines;

  if (n == 0) return;

  /* Don't scroll past top or bottom */
  if (n > 0 && lt->info[lt->lines].position >= ctx->text.lastPos)
    return;
  if (n < 0 && lt->top == 0)
    return;

  if (n > 0) {
    ISWTextPosition scroll_from_pos;
    if ( IsValidLine(ctx, n) )
      top = IswMin(lt->info[n].position, ctx->text.lastPos);
    else
      top = ctx->text.lastPos;

    y = IsValidLine(ctx, n) ? lt->info[n].y : ctx->core.height - 2 * s;
    (void) y;
    (void) scroll_from_pos;
    _IswTextBuildLineTable(ctx, top, FALSE);
    DisplayTextWindow( (Widget) ctx);
    _IswTextSetScrollBars(ctx);
  }
  else {

    n = -n;
    target = lt->top;
    top = SrcScan(ctx->text.source, target, IswstEOL,
    IswsdLeft, n+1, FALSE);

    _IswTextBuildLineTable(ctx, top, FALSE);
    y = IsValidLine(ctx, n) ? lt->info[n].y : ctx->core.height - 2 * s;

    DisplayTextWindow((Widget)ctx);
    _IswTextSetScrollBars(ctx);
  }
  {
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgInsertPosition(&ab, ctx->text.lt.top+ctx->text.lt.lines);
    _IswImSetValues ((Widget) ctx, ab.args, ab.count);
  }

    _TextDrawShadows(ctx, 0, 0, ctx->core.width, ctx->core.height, False);
}

/*ARGSUSED*/
static void
HScroll(Widget w, IswPointer closure, IswPointer callData)
{
  TextWidget ctx = (TextWidget) closure;
  Position old_left, pixels = (Position)(intptr_t) callData;

  _IswTextPrepareToUpdate(ctx);

  old_left = ctx->text.margin.left;
  ctx->text.margin.left -= pixels;
  if (ctx->text.margin.left > ctx->text.r_margin.left) {
    ctx->text.margin.left = ctx->text.r_margin.left;
    pixels = old_left - ctx->text.margin.left;
  }

  if ( pixels != 0 )
    DisplayTextWindow( (Widget) ctx );

  _IswTextExecuteUpdate(ctx);
  _IswTextSetScrollBars(ctx);
}

/*ARGSUSED*/
static void
HJump(Widget w, IswPointer closure, IswPointer callData)
{
  TextWidget ctx = (TextWidget) closure;
  float * percent = (float *) callData;
  Position new_left, old_left = ctx->text.margin.left;

  long move; /*difference of Positions can be bigger than Position; lint err */

  new_left = ctx->text.r_margin.left;
  new_left -= (Position) (*percent * GetWidestLine(ctx));
  move = old_left - new_left;

  if (abs(move) < (int)ctx->core.width) {
    HScroll(w, (IswPointer) ctx, (IswPointer) move);
    return;
  }
  _IswTextPrepareToUpdate(ctx);
  ctx->text.margin.left = new_left;
  if (IswIsRealized((Widget) ctx)) DisplayTextWindow((Widget) ctx);
  _IswTextExecuteUpdate(ctx);
}

/*	Function Name: UpdateTextInLine
 *	Description: Updates some text in a given line.
 *	Arguments: ctx - the text widget.
 *                 line - the line number (in the line table) of this line.
 *                 left, right - left and right pixel offsets of the
 *                               area to update.
 *	Returns: none.
 */

static void
UpdateTextInLine(TextWidget ctx, int line, Position left, Position right)
{
  ISWTextPosition pos1, pos2;
  int width, height, local_left, local_width;
  IswTextLineTableEntry * lt = ctx->text.lt.info + line;

  if ( ((int)(lt->textWidth + ctx->text.margin.left) < left) ||
       ( ctx->text.margin.left > right ) )
    return;			/* no need to update. */

  local_width = left - ctx->text.margin.left;
  IswTextSinkFindPosition(ctx->text.sink, lt->position,
			  (int) ctx->text.margin.left,
			  local_width, FALSE, &pos1, &width, &height);

  if (right >= (Position) lt->textWidth - ctx->text.margin.left)
    if ( (IsValidLine(ctx, line + 1)) &&
	 (ctx->text.lt.info[line + 1].position <= ctx->text.lastPos) )
      pos2 = SrcScan( ctx->text.source, (lt + 1)->position, IswstPositions,
			   IswsdLeft, 1, TRUE);
    else
      pos2 = GETLASTPOS;
  else {
    ISWTextPosition t_pos;

    local_left = ctx->text.margin.left + width;
    local_width = right  - local_left;
    IswTextSinkFindPosition(ctx->text.sink, pos1, local_left,
			    local_width, FALSE, &pos2, &width, &height);

    t_pos = SrcScan( ctx->text.source, pos2,
		     IswstPositions, IswsdRight, 1, TRUE);
    if (t_pos < (lt + 1)->position)
      pos2 = t_pos;
  }

  _IswTextNeedsUpdating(ctx, pos1, pos2);
}

/*
 * The routine will scroll the displayed text by pixels.  If the calldata is
 * positive, move up; otherwise, move down.
 */

/*ARGSUSED*/
static void
VScroll(Widget w, IswPointer closure, IswPointer callData)
{
  TextWidget ctx = (TextWidget)closure;
  int height, nlines, lines = (intptr_t) callData;

  height = ctx->core.height - VMargins(ctx);
  if (height < 1)
    height = 1;
  nlines = (int) (lines * (int) ctx->text.lt.lines) / height;
  if (nlines == 0 && lines != 0)
    nlines = lines > 0 ? 1 : -1;
  _IswTextPrepareToUpdate(ctx);
  _IswTextVScroll(ctx, nlines);
  _IswTextExecuteUpdate(ctx);
}

/*
 * The routine "thumbs" the displayed text. Thumbing means reposition the
 * displayed view of the source to a new position determined by a fraction
 * of the way from beginning to end. Ideally, this should be determined by
 * the number of displayable lines in the source. This routine does it as a
 * fraction of the first position and last position and then normalizes to
 * the start of the line containing the position.
 *
 * BUG/deficiency: The normalize to line portion of this routine will
 * cause thumbing to always position to the start of the source.
 */

/*ARGSUSED*/
static void
VJump(Widget w, IswPointer closure, IswPointer callData)
{
  float * percent = (float *) callData;
  TextWidget ctx = (TextWidget)closure;
  ISWTextPosition position, old_top, old_bot;
  IswTextLineTable * lt = &(ctx->text.lt);

  _IswTextPrepareToUpdate(ctx);
  old_top = lt->top;
  if ( (lt->lines > 0) && (IsValidLine(ctx, lt->lines - 1)) )
    old_bot = lt->info[lt->lines - 1].position;
  else
    old_bot = ctx->text.lastPos;

  position = (long) (*percent * (float) ctx->text.lastPos);
  position= SrcScan(ctx->text.source, position, IswstEOL, IswsdLeft, 1, FALSE);
  if ( (position >= old_top) && (position <= old_bot) ) {
    int line = 0;
    for (;(line < lt->lines) && (position > lt->info[line].position) ; line++);
    _IswTextVScroll(ctx, line);
  }
  else {
    ISWTextPosition new_bot;
    _IswTextBuildLineTable(ctx, position, FALSE);
    new_bot = IsValidLine(ctx, lt->lines-1) ? lt->info[lt->lines-1].position
                                            : ctx->text.lastPos;

    if ((old_top >= lt->top) && (old_top <= new_bot)) {
      int line = 0;
      for (;(line < lt->lines) && (old_top > lt->info[line].position); line++);
      _IswTextBuildLineTable(ctx, old_top, FALSE);
      _IswTextVScroll(ctx, -line);
    }
    else
      DisplayTextWindow( (Widget) ctx);
  }
  _IswTextExecuteUpdate(ctx);
}

static Boolean
IswConvertStandardSelection(Widget w, IswTime time, IswSelectionId *selection,
                           IswSelectionId *target, IswSelectionId *type, IswPointer *value,
                           unsigned long *length, int *format)
{
    /* Minimal stub - returns empty targets list */
    *value = NULL;
    *length = 0;
    *format = 32;
    if (type)
        *type = _IswPlatformSelectionStdType(IswDisplayOf(w), ISW_SEL_STDTYPE_ID_LIST);
    return False;  /* Indicate no conversion performed */
}

static Boolean
MatchSelection(IswSelectionId selection, IswTextSelection *s)
{
    IswSelectionId    *match;
    int	    count;

    for (count = 0, match = s->selections; count < s->id_count; match++, count++)
	if (*match == selection)
	    return True;
    return False;
}

static Boolean
ConvertSelection(Widget w, IswSelectionId *selection, IswSelectionId *target, IswSelectionId *type,
                 IswPointer *value, unsigned long *length, int *format)
{
  IswDisplay d = IswDisplayOf(w);
  TextWidget ctx = (TextWidget)w;
  Widget src = ctx->text.source;
  IswTextEditType edit_mode;

  IswTextSelectionSalt	*salt = NULL;
  IswTextSelection	*s;

  /* Intern the selection-target ids once through the platform selection op
     (cached, no per-comparison server round-trip). */
  IswSelectionId a_targets  = _IswPlatformSelectionInternName(d, "TARGETS", False);
  IswSelectionId a_text     = _IswPlatformSelectionInternName(d, "TEXT", False);
  IswSelectionId a_ctext    = _IswPlatformSelectionInternName(d, "COMPOUND_TEXT", False);
  IswSelectionId a_length   = _IswPlatformSelectionInternName(d, "LENGTH", False);
  IswSelectionId a_listlen  = _IswPlatformSelectionInternName(d, "LIST_LENGTH", False);
  IswSelectionId a_charpos  = _IswPlatformSelectionInternName(d, "CHARACTER_POSITION", False);
  IswSelectionId a_delete   = _IswPlatformSelectionInternName(d, "DELETE", False);
  IswSelectionId str_type   = _IswPlatformSelectionStdType(d, ISW_SEL_STDTYPE_STRING);

  if (*target == a_targets) {
    IswSelectionId  idlist_type = _IswPlatformSelectionStdType(d, ISW_SEL_STDTYPE_ID_LIST);
    IswSelectionId* targetP, * std_targets;
    unsigned long std_length;

    if ( SrcCvtSel(src, selection, target, type, value, length, format) )
	return True;

    IswConvertStandardSelection(w, ctx->text.time, selection,
				target, type, (IswPointer*)&std_targets,
				&std_length, format);

    *value = IswMalloc((unsigned) sizeof(IswSelectionId)*(std_length + 7));
    targetP = *(IswSelectionId**)value;
    *length = std_length + 6;
    *targetP++ = str_type;
    *targetP++ = a_text;
    *targetP++ = a_ctext;
    *targetP++ = a_length;
    *targetP++ = a_listlen;
    *targetP++ = a_charpos;

    {
      IswArgBuilder ab = IswArgBuilderInit();
      IswArgEditType(&ab, (IswArgVal)&edit_mode);
      IswGetValues(src, ab.args, ab.count);
    }

    if (edit_mode == IswtextEdit) {
      *targetP++ = a_delete;
      (*length)++;
    }
    (void) memmove((char*)targetP, (char*)std_targets, sizeof(IswSelectionId)*std_length);
    IswFree((char*)std_targets);
    *type = idlist_type;
    *format = 32;
    return True;
  }

  if ( SrcCvtSel(src, selection, target, type, value, length, format) )
    return True;

  if (MatchSelection (*selection, &ctx->text.s))
    s = &ctx->text.s;
  else
  {
    for (salt = ctx->text.salt; salt; salt = salt->next)
	if (MatchSelection (*selection, &salt->s))
	    break;
    if (!salt)
	return False;
    s = &salt->s;
  }
  if (*target == str_type ||
      *target == a_text ||
      *target == a_ctext) {
	if (*target == a_text) {
		*type = str_type;
	} else {
	    *type = *target;
	}
	/*
	 * If salt is True, the salt->contents stores CT string,
	 * its length is measured in bytes.
	 * Refer to _IswTextSaltAwaySelection().
	 *
	 * by Li Yuhong, Mar. 20, 1991.
	 */
	if (!salt) {
	    *value = _IswTextGetSTRING(ctx, s->left, s->right);
	    *length = strlen(*value);
	} else {
	    *value = IswMalloc((salt->length + 1) * sizeof(unsigned char));
	    strcpy (*value, salt->contents);
	    *length = salt->length;
	}
	*format = 8;
	return True;
  }

  if ( (*target == a_listlen) || (*target == a_length) ) {
    long * temp;

    temp = (long *) IswMalloc( (unsigned) sizeof(long) );
    if (*target == a_listlen)
      *temp = 1L;
    else			/* *target == a_length */
      *temp = (long) (s->right - s->left);

    *value = (IswPointer) temp;
    *type = _IswPlatformSelectionInternName(d, "INTEGER", False);
    *length = 1L;
    *format = 32;
    return True;
  }

  if (*target == a_charpos) {
    long * temp;

    temp = (long *) IswMalloc( (unsigned)( 2 * sizeof(long) ) );
    temp[0] = (long) (s->left + 1);
    temp[1] = s->right;
    *value = (IswPointer) temp;
    *type = _IswPlatformSelectionInternName(d, "SPAN", False);
    *length = 2L;
    *format = 32;
    return True;
  }

  if (*target == a_delete) {
    if (!salt)
	_IswTextZapSelection( ctx, (IswEvent *) NULL, TRUE);
    *value = NULL;
    *type = _IswPlatformSelectionInternName(d, "NULL", False);
    *length = 0;
    *format = 32;
    return True;
  }

  if (IswConvertStandardSelection(w, ctx->text.time, selection, target, type,
				  (IswPointer *)value, length, format))
    return True;

  /* else */
  return False;
}


static void
LoseSelection(Widget w, IswSelectionId *selection)
{
  TextWidget ctx = (TextWidget) w;
  IswSelectionId* idP;
  int i;
  IswTextSelectionSalt	*salt, *prevSalt, *nextSalt;

  _IswTextPrepareToUpdate(ctx);

  idP = ctx->text.s.selections;
  for (i = 0 ; i < ctx->text.s.id_count; i++, idP++)
    if (*selection == *idP)
      *idP = ISW_SELECTION_NONE;

  while (ctx->text.s.id_count &&
	 ctx->text.s.selections[ctx->text.s.id_count-1] == 0)
    ctx->text.s.id_count--;

/*
 * Must walk the selection list in opposite order from UnsetSelection.
 */

  idP = ctx->text.s.selections;
  for (i = 0 ; i < ctx->text.s.id_count; i++, idP++)
    if (*idP == ISW_SELECTION_NONE) {
      *idP = ctx->text.s.selections[--ctx->text.s.id_count];
      while (ctx->text.s.id_count &&
	     ctx->text.s.selections[ctx->text.s.id_count-1] == 0)
	ctx->text.s.id_count--;
    }

  if (ctx->text.s.id_count == 0)
    ModifySelection(ctx, ctx->text.insertPos, ctx->text.insertPos);

  if (ctx->text.old_insert >= 0) /* Update in progress. */
    _IswTextExecuteUpdate(ctx);

  prevSalt = 0;
  for (salt = ctx->text.salt; salt; salt = nextSalt)
    {
    	idP = salt->s.selections;
	nextSalt = salt->next;
    	for (i = 0 ; i < salt->s.id_count; i++, idP++)
	    if (*selection == *idP)
		*idP = ISW_SELECTION_NONE;

    	while (salt->s.id_count &&
	       salt->s.selections[salt->s.id_count-1] == 0)
	{
	    salt->s.id_count--;
	}

    	/*
    	 * Must walk the selection list in opposite order from UnsetSelection.
    	 */

    	idP = salt->s.selections;
    	for (i = 0 ; i < salt->s.id_count; i++, idP++)
    	    if (*idP == ISW_SELECTION_NONE)
 	    {
      	      *idP = salt->s.selections[--salt->s.id_count];
      	      while (salt->s.id_count &&
	     	     salt->s.selections[salt->s.id_count-1] == 0)
    	    	salt->s.id_count--;
    	    }
	if (salt->s.id_count == 0)
	{
	    IswFree ((char *) salt->s.selections);
	    IswFree (salt->contents);
	    if (prevSalt)
		prevSalt->next = nextSalt;
	    else
		ctx->text.salt = nextSalt;
	    IswFree ((char *) salt);
	}
	else
	    prevSalt = salt;
    }
}

void
_IswTextSaltAwaySelection(TextWidget ctx, IswSelectionId *selections, int num_ids)
{
    IswTextSelectionSalt    *salt;
    int			    i;

    for (i = 0; i < num_ids; i++)
	LoseSelection ((Widget) ctx, selections + i);
    if (num_ids == 0)
	return;
    salt = (IswTextSelectionSalt *)
		IswMalloc( (unsigned) sizeof(IswTextSelectionSalt) );
    if (!salt)
	return;
    salt->s.selections = (IswSelectionId *)
	 IswMalloc( (unsigned) ( num_ids * sizeof (IswSelectionId) ) );
    if (!salt->s.selections)
    {
	IswFree ((char *) salt);
	return;
    }
    salt->s.left = ctx->text.s.left;
    salt->s.right = ctx->text.s.right;
    salt->s.type = ctx->text.s.type;
    salt->contents = _IswTextGetSTRING(ctx, ctx->text.s.left, ctx->text.s.right);
    salt->length = strlen (salt->contents);
    salt->next = ctx->text.salt;
    ctx->text.salt = salt;
    for (i = 0; i < num_ids; i++)
    {
	salt->s.selections[i] = selections[i];
	IswOwnSelection ((Widget) ctx, selections[i], ctx->text.time,
		ConvertSelection, LoseSelection, (IswSelectionDoneProc)NULL);
    }
    salt->s.id_count = num_ids;
}

static void
_SetSelection(TextWidget ctx, ISWTextPosition left, ISWTextPosition right,
              IswSelectionId *selections, Cardinal count)
{
  ISWTextPosition pos;

  if (left < ctx->text.s.left) {
    pos = IswMin(right, ctx->text.s.left);
    _IswTextNeedsUpdating(ctx, left, pos);
  }
  if (left > ctx->text.s.left) {
    pos = IswMin(left, ctx->text.s.right);
    _IswTextNeedsUpdating(ctx, ctx->text.s.left, pos);
  }
  if (right < ctx->text.s.right) {
    pos = IswMax(right, ctx->text.s.left);
    _IswTextNeedsUpdating(ctx, pos, ctx->text.s.right);
  }
  if (right > ctx->text.s.right) {
    pos = IswMax(left, ctx->text.s.right);
    _IswTextNeedsUpdating(ctx, pos, right);
  }

  ctx->text.s.left = left;
  ctx->text.s.right = right;

  SrcSetSelection(ctx->text.source, left, right,
		  (count == 0) ? None : selections[0]);

  if (left < right) {
    Widget w = (Widget) ctx;

    while (count) {
      IswSelectionId selection = selections[--count];
      IswOwnSelection(w, selection, ctx->text.time, ConvertSelection,
		     LoseSelection, (IswSelectionDoneProc)NULL);
    }
  }
  else
    IswTextUnsetSelection((Widget)ctx);
}

/*
 * This internal routine deletes the text from pos1 to pos2 in a source and
 * then inserts, at pos1, the text that was passed. As a side effect it
 * "invalidates" that portion of the displayed text (if any).
 *
 * NOTE: It is illegal to call this routine unless there is a valid line table!
 */

int
_IswTextReplace (TextWidget ctx, ISWTextPosition pos1, ISWTextPosition pos2,
                 ISWTextBlock *text)
{
  int i, line1, delta, error;
  ISWTextPosition updateFrom, updateTo;
  Widget src = ctx->text.source;
  IswTextEditType edit_mode;
  Boolean tmp = ctx->text.update_disabled;

  ctx->text.update_disabled = True; /* No redisplay during replacement. */

/*
 * The insertPos may not always be set to the right spot in IswtextAppend
 */

  {
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgEditType(&ab, (IswArgVal)&edit_mode);
    IswGetValues(src, ab.args, ab.count);
  }

  if ((pos1 == ctx->text.insertPos) && (edit_mode == IswtextAppend)) {
    ctx->text.insertPos = ctx->text.lastPos;
    pos2 = SrcScan(src, ctx->text.insertPos, IswstPositions, IswsdRight,
		   (int)(ctx->text.insertPos - pos1), (Boolean)TRUE);
    pos1 = ctx->text.insertPos;
    if ( (pos1 == pos2) && (text->length == 0) ) {
      ctx->text.update_disabled = FALSE; /* rearm redisplay. */
      return( IswEditError );
    }
  }

  updateFrom = SrcScan(src, pos1, IswstWhiteSpace, IswsdLeft, 1, FALSE);
  updateFrom = IswMax(updateFrom, ctx->text.lt.top);

  line1 = LineForPosition(ctx, updateFrom);
  if ( (error = SrcReplace(src, pos1, pos2, text)) != 0) {
    ctx->text.update_disabled = tmp; /* restore redisplay */
    return(error);
  }

  IswTextUnsetSelection((Widget)ctx);

  ctx->text.lastPos = GETLASTPOS;
  if (ctx->text.lt.top >= ctx->text.lastPos) {
    _IswTextBuildLineTable(ctx, ctx->text.lastPos, FALSE);
    ClearWindow( (Widget) ctx);
    ctx->text.update_disabled = tmp; /* restore redisplay */
    return(0);			/* Things are fine. */
  }

  ctx->text.single_char = (text->length <= 1 && pos2 - pos1 <= 1);

  delta = text->length - (pos2 - pos1);

  if (delta < ctx->text.lastPos) {
    for (pos2 += delta, i = 0; i < ctx->text.numranges; i++) {
      if (ctx->text.updateFrom[i] > pos1)
	ctx->text.updateFrom[i] += delta;
      if (ctx->text.updateTo[i] >= pos1)
	ctx->text.updateTo[i] += delta;
    }
  }

  /*
   * fixup all current line table entries to reflect edit.
   * %%% it is not legal to do arithmetic on positions.
   * using Scan would be more proper.
   */
  if (delta != 0) {
    IswTextLineTableEntry *lineP;
    i = LineForPosition(ctx, pos1) + 1;
    for (lineP = ctx->text.lt.info + i; i <= ctx->text.lt.lines; i++, lineP++)
      lineP->position += delta;
  }

  /*
   * Now process the line table and fixup in case edits caused
   * changes in line breaks. If we are breaking on word boundaries,
   * this code checks for moving words to and from lines.
   */

  if (IsPositionVisible(ctx, updateFrom)) {
    updateTo = _BuildLineTable(ctx,
			       ctx->text.lt.info[line1].position, pos1, line1);
    _IswTextNeedsUpdating(ctx, updateFrom, updateTo);
  }

  ctx->text.update_disabled = tmp; /* restore redisplay */
  return(0);			/* Things are fine. */
}

/*
 * This routine will display text between two arbitrary source positions.
 * In the event that this span contains highlighted text for the selection,
 * only that portion will be displayed highlighted.
 *
 * NOTE: it is illegal to call this routine unless there
 *       is a valid line table!
 */

static void
DisplayText(Widget w, ISWTextPosition pos1, ISWTextPosition pos2)
{
  TextWidget ctx = (TextWidget)w;
  Position x, y;
  int height, line, i, lastPos = ctx->text.lastPos;
  ISWTextPosition startPos, endPos;
  Boolean clear_eol, done_painting;
  Dimension s = 0;

  pos1 = (pos1 < ctx->text.lt.top) ? ctx->text.lt.top : pos1;
  pos2 = FindGoodPosition(ctx, pos2);
  if ( (pos1 >= pos2) || !LineAndXYForPosition(ctx, pos1, &line, &x, &y) )
    return;			/* line not visible, or pos1 >= pos2. */

  for ( startPos = pos1, i = line; IsValidLine(ctx, i) &&
                                   (i < ctx->text.lt.lines) ; i++) {


    if ( (endPos = ctx->text.lt.info[i + 1].position) > pos2 ) {
      clear_eol = ((endPos = pos2) >= lastPos);
      done_painting = (!clear_eol || ctx->text.single_char);
    }
    else {
      clear_eol = TRUE;
      done_painting = FALSE;
    }

    height = ctx->text.lt.info[i + 1].y - ctx->text.lt.info[i].y - s + 1;

    if ( (endPos > startPos) ) {

      if ( (x == (Position) ctx->text.margin.left) && (x > 0) )
      {
	 SinkClearToBG (ctx->text.sink,
			(Position) s, y,
			(Dimension) ctx->text.margin.left, (Dimension)height);
	   _TextDrawShadows(ctx, 0, 0, ctx->core.width, ctx->core.height, False);
      }

      if ( (startPos >= ctx->text.s.right) || (endPos <= ctx->text.s.left) )
	IswTextSinkDisplayText(ctx->text.sink, x, y, startPos, endPos, FALSE);
      else if ((startPos >= ctx->text.s.left) && (endPos <= ctx->text.s.right))
	IswTextSinkDisplayText(ctx->text.sink, x, y, startPos, endPos, TRUE);
      else {
	DisplayText(w, startPos, ctx->text.s.left);
	DisplayText(w, IswMax(startPos, ctx->text.s.left),
		    IswMin(endPos, ctx->text.s.right));
	DisplayText(w, ctx->text.s.right, endPos);
      }
    }
    startPos = endPos;
    if (clear_eol) {
	Position myx = ctx->text.lt.info[i].textWidth + ctx->text.margin.left;

	SinkClearToBG(ctx->text.sink,
		      (Position) myx,
		      (Position) y, w->core.width - myx/* - 2 * s*/,
		      (Dimension) height);
	  _TextDrawShadows(ctx, 0, 0, ctx->core.width, ctx->core.height, False);

	/*
	 * We only get here if single character is true, and we need
	 * to clear to the end of the screen.  We know that since there
	 * was only one character deleted that this is the same
	 * as clearing an extra line, so we do this, and are done.
	 *
	 * This a performance hack, and a pretty gross one, but it works.
	 *
	 * Chris Peterson 11/13/89.
	 */

	if (done_painting) {
	    y += height;
	    SinkClearToBG(ctx->text.sink,
			  (Position) ctx->text.margin.left, (Position) y,
			  w->core.width - ctx->text.margin.left/* - 2 * s*/,
			  (Dimension) IswMin(height, ctx->core.height - 2 * s - y));
	      _TextDrawShadows(ctx, 0, 0, ctx->core.width, ctx->core.height, False);

	    break;		/* set single_char to FALSE and return. */
	}
    }

    x = (Position) ctx->text.margin.left;
    y = ctx->text.lt.info[i + 1].y;
    if ( done_painting
	 || (y >= (int)(ctx->core.height - ctx->text.margin.bottom)) )
      break;
  }
  ctx->text.single_char = FALSE;
}

/*
 * This routine implements multi-click selection in a hardwired manner.
 * It supports multi-click entity cycling (char, word, line, file) and mouse
 * motion adjustment of the selected entitie (i.e. select a word then, with
 * button still down, adjust wich word you really meant by moving the mouse).
 * [NOTE: This routine is to be replaced by a set of procedures that
 * will allows clients to implements a wide class of draw through and
 * multi-click selection user interfaces.]
 */

static void
DoSelection (TextWidget ctx, ISWTextPosition pos, IswTime time, Boolean motion)
{
  ISWTextPosition newLeft, newRight;
  IswTextSelectType newType, *sarray;
  Widget src = ctx->text.source;

  if (motion)
    newType = ctx->text.s.type;
  else {
    if ( (abs((long) time - (long) ctx->text.lasttime) < MULTI_CLICK_TIME) &&
	 ((pos >= ctx->text.s.left) && (pos <= ctx->text.s.right))) {
      sarray = ctx->text.sarray;
      for (;*sarray != IswselectNull && *sarray != ctx->text.s.type; sarray++);

      if (*sarray == IswselectNull)
	newType = *(ctx->text.sarray);
      else {
	newType = *(sarray + 1);
	if (newType == IswselectNull)
	  newType = *(ctx->text.sarray);
      }
    }
    else 			                      /* single-click event */
      newType = *(ctx->text.sarray);

    ctx->text.lasttime = time;
  }
  switch (newType) {
  case IswselectPosition:
    newLeft = newRight = pos;
    break;
  case IswselectChar:
    newLeft = pos;
    newRight = SrcScan(src, pos, IswstPositions, IswsdRight, 1, FALSE);
    break;
  case IswselectWord:
  case IswselectParagraph:
    {
      IswTextScanType stype;

      if (newType == IswselectWord)
        stype = IswstWhiteSpace;
      else
	stype = IswstParagraph;

      /*
       * Somewhat complicated, but basically I treat the space between
       * two objects as another object.  The object that I am currently
       * in then becomes the end of the selection.
       *
       * Chris Peterson - 4/19/90.
       */

      newRight = SrcScan(ctx->text.source, pos, stype, IswsdRight, 1, FALSE);
      newRight =SrcScan(ctx->text.source, newRight,stype,IswsdLeft,1, FALSE);

      if (pos != newRight)
	newLeft = SrcScan(ctx->text.source, pos, stype, IswsdLeft, 1, FALSE);
      else
	newLeft = pos;

      newLeft =SrcScan(ctx->text.source, newLeft, stype, IswsdRight,1,FALSE);

      if (newLeft > newRight) {
	  ISWTextPosition temp = newLeft;
	  newLeft = newRight;
	  newRight = temp;
      }
    }
    break;
  case IswselectLine:
    newLeft = SrcScan(src, pos, IswstEOL, IswsdLeft, 1, FALSE);
    newRight = SrcScan(src, pos, IswstEOL, IswsdRight, 1, FALSE);
    break;
  case IswselectAll:
    newLeft = SrcScan(src, pos, IswstAll, IswsdLeft, 1, FALSE);
    newRight = SrcScan(src, pos, IswstAll, IswsdRight, 1, FALSE);
    break;
  default:
    IswAppWarning(IswWidgetToApplicationContext((Widget) ctx),
	       "Text Widget: empty selection array.");
    return;
  }

  if ( (newLeft != ctx->text.s.left) || (newRight != ctx->text.s.right)
      || (newType != ctx->text.s.type)) {
    ModifySelection(ctx, newLeft, newRight);
    if (pos - ctx->text.s.left < ctx->text.s.right - pos)
      ctx->text.insertPos = newLeft;
    else
      ctx->text.insertPos = newRight;
    ctx->text.s.type = newType;
  }
  if (!motion) { /* setup so we can freely mix select extend calls*/
    ctx->text.origSel.type = ctx->text.s.type;
    ctx->text.origSel.left = ctx->text.s.left;
    ctx->text.origSel.right = ctx->text.s.right;

    if (pos >= ctx->text.s.left + ((ctx->text.s.right - ctx->text.s.left) / 2))
      ctx->text.extendDir = IswsdRight;
    else
      ctx->text.extendDir = IswsdLeft;
  }
}

/*
 * This routine implements extension of the currently selected text in
 * the "current" mode (i.e. char word, line, etc.). It worries about
 * extending from either end of the selection and handles the case when you
 * cross through the "center" of the current selection (e.g. switch which
 * end you are extending!).
 */

static void
ExtendSelection (TextWidget ctx, ISWTextPosition pos, Boolean motion)
{
  IswTextScanDirection dir;

  if (!motion) {		/* setup for extending selection */
    if (ctx->text.s.left == ctx->text.s.right) /* no current selection. */
      ctx->text.s.left = ctx->text.s.right = ctx->text.insertPos;
    else {
      ctx->text.origSel.left = ctx->text.s.left;
      ctx->text.origSel.right = ctx->text.s.right;
    }

    ctx->text.origSel.type = ctx->text.s.type;

    if (pos >= ctx->text.s.left + ((ctx->text.s.right - ctx->text.s.left) / 2))
	ctx->text.extendDir = IswsdRight;
    else
	ctx->text.extendDir = IswsdLeft;
  }
  else /* check for change in extend direction */
    if ((ctx->text.extendDir == IswsdRight && pos <= ctx->text.origSel.left) ||
	(ctx->text.extendDir == IswsdLeft && pos >= ctx->text.origSel.right)) {
      ctx->text.extendDir = (ctx->text.extendDir == IswsdRight) ?
	                                            IswsdLeft : IswsdRight;
      ModifySelection(ctx, ctx->text.origSel.left, ctx->text.origSel.right);
    }

  dir = ctx->text.extendDir;
  switch (ctx->text.s.type) {
  case IswselectWord:
  case IswselectParagraph:
    {
      ISWTextPosition left_pos, right_pos;
      IswTextScanType stype;

      if (ctx->text.s.type == IswselectWord)
        stype = IswstWhiteSpace;
      else
	stype = IswstParagraph;

      /*
       * Somewhat complicated, but basically I treat the space between
       * two objects as another object.  The object that I am currently
       * in then becomes the end of the selection.
       *
       * Chris Peterson - 4/19/90.
       */

      right_pos = SrcScan(ctx->text.source, pos, stype, IswsdRight, 1, FALSE);
      right_pos =SrcScan(ctx->text.source, right_pos,stype,IswsdLeft,1, FALSE);

      if (pos != right_pos)
	left_pos = SrcScan(ctx->text.source, pos, stype, IswsdLeft, 1, FALSE);
      else
	left_pos = pos;

      left_pos =SrcScan(ctx->text.source, left_pos, stype, IswsdRight,1,FALSE);

      if (dir == IswsdLeft)
	pos = IswMin(left_pos, right_pos);
      else /* dir == IswsdRight */
	pos = IswMax(left_pos, right_pos);
    }
    break;
  case IswselectLine:
    pos = SrcScan(ctx->text.source, pos, IswstEOL, dir, 1, dir == IswsdRight);
    break;
  case IswselectAll:
    pos = ctx->text.insertPos;
  case IswselectPosition:	/* fall through. */
  default:
    break;
  }

  if (dir == IswsdRight)
    ModifySelection(ctx, ctx->text.s.left, pos);
  else
    ModifySelection(ctx, pos, ctx->text.s.right);

  ctx->text.insertPos = pos;
}

/*
 * Clear the window to background color.
 */

static void
ClearWindow (Widget w)
{
  TextWidget ctx = (TextWidget) w;
  int s = 0;

  if (IswIsRealized(w))
  {
    SinkClearToBG(ctx->text.sink,
    (Position) s, (Position) s,
    w->core.width - 2 * s, w->core.height - 2 * s);
  }
}

/*	Function Name: _IswTextClearAndCenterDisplay
 *	Description: Redraws the display with the cursor in insert point
 *                   centered vertically.
 *	Arguments: ctx - the text widget.
 *	Returns: none.
 */

void
_IswTextClearAndCenterDisplay(TextWidget ctx)
{
  int insert_line = LineForPosition(ctx, ctx->text.insertPos);
  int scroll_by = insert_line - ctx->text.lt.lines/2;

  _IswTextVScroll(ctx, scroll_by);
  DisplayTextWindow( (Widget) ctx);
}

/*
 * Internal redisplay entire window.
 * Legal to call only if widget is realized.
 */

static void
DisplayTextWindow (Widget w)
{
  TextWidget ctx = (TextWidget) w;
  ClearWindow(w);
  _IswTextBuildLineTable(ctx, ctx->text.lt.top, FALSE);
  _IswTextNeedsUpdating(ctx, zeroPosition, ctx->text.lastPos);
  _IswTextSetScrollBars(ctx);
  PaintScrollbars(ctx);
}

/* Repaint the scrollbars.  They are windowless children but Text is not a
   Composite, so they are not in composite.children and the generic expose
   delegation never reaches them — Text paints them itself, on top of its
   own content. */
static void
PaintScrollbars(TextWidget ctx)
{
  Widget bar;

  if ((bar = ctx->text.vbar) != NULL && IswIsRealized(bar) &&
      bar->core.widget_class->core_class.expose != NULL)
    (*bar->core.widget_class->core_class.expose)(bar, NULL, 0);

  if ((bar = ctx->text.hbar) != NULL && IswIsRealized(bar) &&
      bar->core.widget_class->core_class.expose != NULL)
    (*bar->core.widget_class->core_class.expose)(bar, NULL, 0);
}

/*
 * Hit-test hook (Simple class) so the windowless event dispatcher can route
 * pointer events to Text's scrollbars, which are not held in a composite
 * child list.  (x,y) are relative to Text's content origin.  Returns the bar
 * under the point with its content-origin offset, or NULL.
 */
static Widget
TextHitChild(Widget w, int x, int y, int *dx, int *dy)
{
  TextWidget ctx = (TextWidget) w;
  Widget bars[2];
  int i;

  bars[0] = ctx->text.vbar;
  bars[1] = ctx->text.hbar;

  for (i = 0; i < 2; i++) {
    Widget bar = bars[i];
    int bx, by, bw, bh;

    if (bar == NULL || !bar->core.windowless)
      continue;
    if (!bar->core.windowless_mapped)   /* only mapped (shown) bars */
      continue;

    bx = bar->core.x;
    by = bar->core.y;
    bw = (int) bar->core.width + 2 * (int) bar->core.border_width;
    bh = (int) bar->core.height + 2 * (int) bar->core.border_width;

    if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
      *dx = bx + (int) bar->core.border_width;
      *dy = by + (int) bar->core.border_width;
      return bar;
    }
  }
  return NULL;
}

/*
 * Enumerate Text's windowless sub-widgets (its scrollbars) for the paint and
 * composite passes — they are not held in composite.children (Text is not a
 * composite).  Stacking order: vbar then hbar.  Returns NULL past the end.
 */
static Widget
TextNthWindowlessChild(Widget w, int i)
{
  TextWidget ctx = (TextWidget) w;
  Widget bars[2];
  int n = 0;

  if (ctx->text.vbar != NULL) bars[n++] = ctx->text.vbar;
  if (ctx->text.hbar != NULL) bars[n++] = ctx->text.hbar;

  if (i < 0 || i >= n)
    return NULL;
  return bars[i];
}

/*
 * This routine checks to see if the window should be resized (grown or
 * shrunk) when text to be painted overflows to the right or
 * the bottom of the window. It is used by the keyboard input routine.
 */

void
_IswTextCheckResize(TextWidget ctx)
{
  Widget w = (Widget) ctx;
  int line = 0, old_height;
  IswWidgetGeometry rbox, return_geom;

  if ( ((ctx->text.resize == IswtextResizeWidth) ||
	(ctx->text.resize == IswtextResizeBoth)) &&
       ctx->text.wrap != IswtextWrapNever ) {
    IswTextLineTableEntry *lt;
    rbox.width = 0;
    for (lt = ctx->text.lt.info;
	 IsValidLine(ctx, line) && (line < ctx->text.lt.lines);
	 line++, lt++) {
      if ((int)(lt->textWidth + ctx->text.margin.left) > (int)rbox.width)
	  rbox.width = lt->textWidth + ctx->text.margin.left;
    }

    rbox.width += ctx->text.margin.right;
    if (rbox.width > ctx->core.width) { /* Only get wider. */
      rbox.request_mode = XCB_CONFIG_WINDOW_WIDTH;
      if (IswMakeGeometryRequest(w, &rbox, &return_geom) == IswGeometryAlmost)
	(void) IswMakeGeometryRequest(w, &return_geom, (IswWidgetGeometry*) NULL);
    }
  }

  if ( !((ctx->text.resize == IswtextResizeHeight) ||
	 (ctx->text.resize == IswtextResizeBoth)) )
      return;

  if (IsPositionVisible(ctx, ctx->text.lastPos))
    line = LineForPosition(ctx, ctx->text.lastPos);
  else
    line = ctx->text.lt.lines;

  if ( (line + 1) == ctx->text.lt.lines ) return;

  old_height = ctx->core.height;
  rbox.request_mode = XCB_CONFIG_WINDOW_HEIGHT;
  rbox.height = IswTextSinkMaxHeight(ctx->text.sink, line + 1) + VMargins(ctx);

  if ((int)rbox.height < old_height) return; /* It will only get taller. */

  if (IswMakeGeometryRequest(w, &rbox, &return_geom) == IswGeometryAlmost)
    if (IswMakeGeometryRequest(w, &return_geom, (IswWidgetGeometry*)NULL) != IswGeometryYes)
      return;

  _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
}

/*
 * Converts (params, num_params) to a list of selection ids & caches the
 * list in the TextWidget instance.
 */

IswSelectionId*
_IswTextSelectionList(TextWidget ctx, String *list, Cardinal nelems)
{
  IswSelectionId * sel = ctx->text.s.selections;
  IswDisplay dpy = IswDisplayOf((Widget) ctx);
  int n;

  if (nelems > ctx->text.s.array_size) {
    sel = (IswSelectionId *) IswRealloc((char *) sel, sizeof(IswSelectionId) * nelems);
    ctx->text.s.array_size = nelems;
    ctx->text.s.selections = sel;
  }
  for (n=nelems; --n >= 0; sel++, list++)
    *sel = _IswPlatformSelectionInternName(dpy, *list, False);

  ctx->text.s.id_count = nelems;
  return ctx->text.s.selections;
}

/*	Function Name: SetSelection
 *	Description: Sets the current selection.
 *	Arguments: ctx - the text widget.
 *                 defaultSel - the default selection.
 *                 l, r - the left and right ends of the selection.
 *                 list, nelems - the selection list (as strings).
 *	Returns: none.
 *
 *  NOTE: if (ctx->text.s.left >= ctx->text.s.right) then the selection
 *        is unset.
 */

void
_IswTextSetSelection(TextWidget ctx, ISWTextPosition l, ISWTextPosition r,
                     String *list, Cardinal nelems)
{
  if (nelems == 1 && !strcmp (list[0], "none"))
    return;
  String defaultSel = "PRIMARY";
  if (nelems == 0) {
    list = &defaultSel;
    nelems = 1;
  }
  _SetSelection(ctx, l, r, _IswTextSelectionList(ctx, list, nelems), nelems);
}


/*	Function Name: ModifySelection
 *	Description: Modifies the current selection.
 *	Arguments: ctx - the text widget.
 *                 left, right - the left and right ends of the selection.
 *	Returns: none.
 *
 *  NOTE: if (ctx->text.s.left >= ctx->text.s.right) then the selection
 *        is unset.
 */

static void
ModifySelection(TextWidget ctx, ISWTextPosition left, ISWTextPosition right)
{
  if (left == right)
    ctx->text.insertPos = left;
  _SetSelection( ctx, left, right, (IswSelectionId*) NULL, ZERO );
}

/*
 * This routine is used to perform various selection functions. The goal is
 * to be able to specify all the more popular forms of draw-through and
 * multi-click selection user interfaces from the outside.
 */

void
_IswTextAlterSelection (TextWidget ctx, IswTextSelectionMode mode,
                        IswTextSelectionAction action, String *params, Cardinal *num_params)
{
  ISWTextPosition position;
  Boolean flag;

/*
 * This flag is used by TextPop.c:DoReplace() to determine if the selection
 * is okay to use, or if it has been modified.
 */

  if (ctx->text.search != NULL)
    ctx->text.search->selection_changed = TRUE;

  position = PositionForXY (ctx, (int) ctx->text.ev_x, (int) ctx->text.ev_y);

  flag = (action != IswactionStart);
  if (mode == IswsmTextSelect)
    DoSelection (ctx, position, ctx->text.time, flag);
  else			/* mode == IswsmTextExtend */
   ExtendSelection (ctx, position, flag);

  if (action == IswactionEnd)
    _IswTextSetSelection(ctx, ctx->text.s.left, ctx->text.s.right,
			 params, *num_params);
}

/*	Function Name: RectanglesOverlap
 *	Description: Returns TRUE if two rectangles overlap.
 *	Arguments: rect1, rect2 - the two rectangles to check.
 *	Returns: TRUE iff these rectangles overlap.
 */

static Boolean
RectanglesOverlap(IswRectangle *rect1, IswRectangle *rect2)
{
  return ( (rect1->x < rect2->x + (short) rect2->width) &&
	   (rect2->x < rect1->x + (short) rect1->width) &&
	   (rect1->y < rect2->y + (short) rect2->height) &&
	   (rect2->y < rect1->y + (short) rect1->height) );
}

/*	Function Name: UpdateTextInRectangle.
 *	Description: Updates the text in a rectangle.
 *	Arguments: ctx - the text widget.
 *                 rect - the rectangle to update.
 *	Returns: none.
 */

static void
UpdateTextInRectangle(TextWidget ctx, IswRectangle * rect)
{
  IswTextLineTableEntry *info = ctx->text.lt.info;
  int line, x = rect->x, y = rect->y;
  int right = rect->width + x, bottom = rect->height + y;

  for (line = 0;( (line < ctx->text.lt.lines) &&
		 IsValidLine(ctx, line) && (info->y < bottom)); line++, info++)
    if ( (info + 1)->y >= y )
      UpdateTextInLine(ctx, line, x, right);
}

/*
 * This routine processes all "expose region" XEvents. In general, its job
 * is to the best job at minimal re-paint of the text, displayed in the
 * window, that it can.
 */

/* ARGSUSED */
static void
ProcessExposeRegion(Widget w, IswEvent *iswev, Region region)
{
    TextWidget ctx = (TextWidget) w;
    IswRectangle expose, cursor;
    Boolean need_to_draw;

    xcb_generic_event_t *native =
        (xcb_generic_event_t *) (iswev ? IswEventNative(iswev) : NULL);
    uint8_t type = native ? (native->response_type & ~0x80) : XCB_EXPOSE;

    if (iswev == NULL) {
        /* redraw the whole widget */
        expose.x = 0;
        expose.y = 0;
        expose.width = ctx->core.width;
        expose.height = ctx->core.height;
    }
    else if (iswev->kind == IswRedraw && type != XCB_GRAPHICS_EXPOSURE) {
	expose.x = iswev->redraw.x;
	expose.y = iswev->redraw.y;
	expose.width = iswev->redraw.width;
	expose.height = iswev->redraw.height;
    }
    else if (type == XCB_GRAPHICS_EXPOSURE) {
	xcb_graphics_exposure_event_t *gev = (xcb_graphics_exposure_event_t *)native;
	expose.x = gev->x;
	expose.y = gev->y;
	expose.width = gev->width;
	expose.height = gev->height;
    }
    else { /* No Expose */
	PopCopyQueue(ctx);
	return;			/* no more processing necessary. */
    }

    need_to_draw = TranslateExposeRegion(ctx, &expose);
    if (type == XCB_GRAPHICS_EXPOSURE) {
	xcb_graphics_exposure_event_t *gev = (xcb_graphics_exposure_event_t *)native;
	if (gev->count == 0)
	    PopCopyQueue(ctx);
    }

    if (!need_to_draw)
	return;			/* don't draw if we don't need to. */

    _IswTextPrepareToUpdate(ctx);
    UpdateTextInRectangle(ctx, &expose);
    IswTextSinkGetCursorBounds(ctx->text.sink, &cursor);
    if (RectanglesOverlap(&cursor, &expose)) {
	SinkClearToBG(ctx->text.sink, (Position) cursor.x, (Position) cursor.y,
		      (Dimension) cursor.width, (Dimension) cursor.height);
	UpdateTextInRectangle(ctx, &cursor);
    }
    _IswTextExecuteUpdate(ctx);

      _TextDrawShadows(ctx, 0, 0, ctx->core.width, ctx->core.height, False);

  PaintScrollbars(ctx);
}

/*
 * This routine does all setup required to syncronize batched screen updates
 */

void
_IswTextPrepareToUpdate(TextWidget ctx)
{
  if (ctx->text.old_insert < 0) {
    InsertCursor((Widget)ctx, IswisOff);
    ctx->text.numranges = 0;
    ctx->text.showposition = FALSE;
    ctx->text.old_insert = ctx->text.insertPos;
  }
}

/*
 * This is a private utility routine used by _IswTextExecuteUpdate. It
 * processes all the outstanding update requests and merges update
 * ranges where possible.
 */

static
void FlushUpdate(TextWidget ctx)
{
  if (!IswIsRealized((Widget)ctx)) {
    ctx->text.numranges = 0;
    return;
  }
  if (ctx->text.numranges > 0) {
    ctx->text.numranges = 0;
    /* Full redraw: clear entire text area and repaint all visible lines.
     * Cairo text rendering requires explicit background clearing (unlike
     * XDrawImageString which fills behind glyphs), so incremental updates
     * cause artifacts. A full redraw is simple and reliable. */
    ClearWindow((Widget)ctx);
    _IswTextBuildLineTable(ctx, ctx->text.lt.top, FALSE);
    DisplayText((Widget)ctx, zeroPosition, ctx->text.lastPos);
  }
}

/*
 * This is a private utility routine used by _IswTextExecuteUpdate. This
 * routine worries about edits causing new data or the insertion point becoming
 * invisible (off the screen, or under the horiz. scrollbar). Currently
 * it always makes it visible by scrolling. It probably needs
 * generalization to allow more options.
 */

void
_IswTextShowPosition(TextWidget ctx)
{
  int x, y, lines, number;
  Boolean no_scroll;
  ISWTextPosition max_pos, top, first;

  if ( (!IswIsRealized((Widget)ctx)) || (ctx->text.lt.lines <= 0) )
    return;

/*
 * Find out the bottom the visable window, and make sure that the
 * cursor does not go past the end of this space.
 *
 * This makes sure that the cursor does not go past the end of the
 * visable window.
 */

  x = ctx->core.width;
  y = ctx->core.height - ctx->text.margin.bottom;
  if (ctx->text.hbar != NULL)
    y -= ctx->text.hbar->core.height + 2 * ctx->text.hbar->core.border_width;

  max_pos = PositionForXY (ctx, x, y);
  lines = LineForPosition(ctx, max_pos) + 1; /* number of visable lines. */

  if ( (ctx->text.insertPos < ctx->text.lt.top) ||
       (ctx->text.insertPos >= max_pos)) {

    first = ctx->text.lt.top;
    no_scroll = FALSE;

    if (ctx->text.insertPos < first) { /* We need to scroll down. */
	top = SrcScan(ctx->text.source, ctx->text.insertPos,
		      IswstEOL, IswsdLeft, 1, FALSE);

	number = 0;
	while (first > top) {
	    first = SrcScan(ctx->text.source, first,
			    IswstEOL, IswsdLeft, 1, TRUE);

	    if ( - number > lines )
		break;

	    number--;
	}

	if (first <= top) {
	    first = SrcScan(ctx->text.source, first,
			    IswstPositions, IswsdRight, 1, TRUE);

	    if (first <= top)
		number++;

	    lines = number;
	}
	else
	    no_scroll = TRUE;
    }
    else {			/* We need to Scroll up */
	top = SrcScan(ctx->text.source, ctx->text.insertPos,
		      IswstEOL, IswsdLeft, lines, FALSE);

	if (top < max_pos)
	    lines = LineForPosition(ctx, top);
	else
	    no_scroll = TRUE;
    }

    if (no_scroll) {
	_IswTextBuildLineTable(ctx, top, FALSE);
	DisplayTextWindow((Widget)ctx);
    }
    else
	_IswTextVScroll(ctx, lines);
  }

  {
    int cur_line;
    Position cur_x, cur_y;
    Position visible_left, visible_right;

    LineAndXYForPosition(ctx, ctx->text.insertPos, &cur_line, &cur_x, &cur_y);

    visible_left = ctx->text.r_margin.left;
    visible_right = (Position)ctx->core.width - ctx->text.margin.right;

    if (cur_x < visible_left || cur_x >= visible_right) {
      Position target_left = ctx->text.margin.left + (visible_left - cur_x)
	+ (visible_right - visible_left) / 4;
      if (target_left > ctx->text.r_margin.left)
	target_left = ctx->text.r_margin.left;
      ctx->text.margin.left = target_left;
      _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
      _IswTextNeedsUpdating(ctx, zeroPosition, ctx->text.lastPos);
    }
  }

  _IswTextSetScrollBars(ctx);
}

/*
 * This routine causes all batched screen updates to be performed
 */

void
_IswTextExecuteUpdate(TextWidget ctx)
{
  if ( ctx->text.update_disabled || (ctx->text.old_insert < 0) )
    return;

  if((ctx->text.old_insert != ctx->text.insertPos) || (ctx->text.showposition))
    _IswTextShowPosition(ctx);
  FlushUpdate(ctx);
  InsertCursor((Widget)ctx, ctx->text.hasfocus ? IswisOn : IswisOff);
  ctx->text.old_insert = -1;
}


static void
TextDestroy(Widget w)
{
  TextWidget ctx = (TextWidget)w;

  DestroyHScrollBar(ctx);
  DestroyVScrollBar(ctx);

  if (ctx->text.source && w == IswParent(ctx->text.source))
    IswDestroyWidget(ctx->text.source);
  if (ctx->text.sink && w == IswParent(ctx->text.sink))
    IswDestroyWidget(ctx->text.sink);

  IswFree((char *)ctx->text.s.selections);
  IswFree((char *)ctx->text.lt.info);
  IswFree((char *)ctx->text.search);
  IswFree((char *)ctx->text.updateFrom);
  IswFree((char *)ctx->text.updateTo);
}

/*
 * by the time we are managed (and get this far) we had better
 * have both a source and a sink
 */

static void
Resize(Widget w)
{
  TextWidget ctx = (TextWidget) w;

  PositionVScrollBar(ctx);
  PositionHScrollBar(ctx);

  _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
  _IswTextSetScrollBars(ctx);
}

/*
 * This routine allow the application program to Set attributes.
 */

/*ARGSUSED*/
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
  TextWidget oldtw = (TextWidget) current;
  TextWidget newtw = (TextWidget) new;
  Boolean    redisplay = FALSE;
  Boolean    display_caret = newtw->text.display_caret;


  newtw->text.display_caret = oldtw->text.display_caret;
  _IswTextPrepareToUpdate(newtw);
  newtw->text.display_caret = display_caret;

  if (oldtw->text.r_margin.left != newtw->text.r_margin.left) {
    newtw->text.margin.left = newtw->text.r_margin.left;
    if (newtw->text.vbar != NULL)
      newtw->text.margin.left += newtw->text.vbar->core.width +
	                         newtw->text.vbar->core.border_width;
    redisplay = TRUE;
  }

  if (oldtw->text.scroll_vert != newtw->text.scroll_vert) {
    if (newtw->text.scroll_vert == IswtextScrollNever)
      DestroyVScrollBar(newtw);
    else if (newtw->text.scroll_vert == IswtextScrollAlways)
      CreateVScrollBar(newtw);
    redisplay = TRUE;
  }

  if (oldtw->text.r_margin.bottom != newtw->text.r_margin.bottom) {
    newtw->text.margin.bottom = newtw->text.r_margin.bottom;
    if (newtw->text.hbar != NULL)
      newtw->text.margin.bottom += newtw->text.hbar->core.height +
	                           newtw->text.hbar->core.border_width;
    redisplay = TRUE;
  }

  if (oldtw->text.scroll_horiz != newtw->text.scroll_horiz) {
    if (newtw->text.scroll_horiz == IswtextScrollNever)
      DestroyHScrollBar(newtw);
    else if (newtw->text.scroll_horiz == IswtextScrollAlways)
      CreateHScrollBar(newtw);
    redisplay = TRUE;
  }

  if ( oldtw->text.source != newtw->text.source )
    IswTextSetSource( (Widget) newtw, newtw->text.source, newtw->text.lt.top);

  newtw->text.redisplay_needed = False;
  IswSetValues( (Widget)newtw->text.source, args, *num_args );
  IswSetValues( (Widget)newtw->text.sink, args, *num_args );

  if ( oldtw->text.wrap != newtw->text.wrap ||
       oldtw->text.lt.top != newtw->text.lt.top ||
       oldtw->text.r_margin.right != newtw->text.r_margin.right ||
       oldtw->text.r_margin.top != newtw->text.r_margin.top ||
       oldtw->text.sink != newtw->text.sink ||
       newtw->text.redisplay_needed )
  {
    _IswTextBuildLineTable(newtw, newtw->text.lt.top, TRUE);
    redisplay = TRUE;
  }

  if (oldtw->text.insertPos != newtw->text.insertPos) {
    newtw->text.showposition = TRUE;
    redisplay = TRUE;
  }

  _IswTextExecuteUpdate(newtw);
  if (redisplay)
    _IswTextSetScrollBars(newtw);

  return redisplay;
}

/* invoked by the Simple widget's SetValues */
static Boolean
ChangeSensitive(Widget w)
{
    IswArgBuilder ab = IswArgBuilderInit();
    TextWidget tw = (TextWidget) w;

    (*(&simpleClassRec)->simple_class.change_sensitive)(w);

    IswArgAncestorSensitive(&ab,
	       (tw->core.ancestor_sensitive && tw->core.sensitive));
    if (tw->text.vbar)
	IswSetValues(tw->text.vbar, ab.args, ab.count);
    if (tw->text.hbar)
	IswSetValues(tw->text.hbar, ab.args, ab.count);
    return False;
}

/*	Function Name: GetValuesHook
 *	Description: This is a get values hook routine that gets the
 *                   values in the text source and sink.
 *	Arguments: w - the Text Widget.
 *                 args - the argument list.
 *                 num_args - the number of args.
 *	Returns: none.
 */

static void
GetValuesHook(Widget w, ArgList args, Cardinal * num_args)
{
  IswGetValues( ((TextWidget) w)->text.source, args, *num_args );
  IswGetValues( ((TextWidget) w)->text.sink, args, *num_args );
}

/*	Function Name: FindGoodPosition
 *	Description: Returns a valid position given any postition
 *	Arguments: pos - any position.
 *	Returns: a position between (0 and lastPos);
 */

static ISWTextPosition
FindGoodPosition(TextWidget ctx, ISWTextPosition pos)
{
  if (pos < 0) return(0);
  return ( ((pos > ctx->text.lastPos) ? ctx->text.lastPos : pos) );
}

/************************************************************
 *
 * Routines for handling the copy area expose queue.
 *
 ************************************************************/

/*	Function Name: PushCopyQueue
 *	Description: Pushes a value onto the copy queue.
 *	Arguments: ctx - the text widget.
 *                 h, v - amount of offset in the horiz and vert directions.
 *	Returns: none
 */

/* Retained for the copy-area scroll path, which the windowless conversion
   replaced with full repaints; no current caller. */
static void _X_UNUSED
PushCopyQueue(TextWidget ctx, int h, int v)
{
    struct text_move * offsets = IswNew(struct text_move);

    offsets->h = h;
    offsets->v = v;
    offsets->next = NULL;

    if (ctx->text.copy_area_offsets == NULL)
	ctx->text.copy_area_offsets = offsets;
    else {
	struct text_move * end = ctx->text.copy_area_offsets;
	for ( ; end->next != NULL; end = end->next) {}
	end->next = offsets;
    }
}

/*	Function Name: PopCopyQueue
 *	Description: Pops the top value off of the copy queue.
 *	Arguments: ctx - the text widget.
 *	Returns: none.
 */

static void
PopCopyQueue(TextWidget ctx)
{
    struct text_move * offsets = ctx->text.copy_area_offsets;

    if (offsets == NULL)
	(void) printf( "Isw Text widget %s: empty copy queue\n",
		       IswName( (Widget) ctx ) );
    else {
	ctx->text.copy_area_offsets = offsets->next;
	IswFree((char *) offsets);	/* free what you allocate. */
    }
}

/*	Function Name:  TranslateExposeRegion
 *	Description: Translates the expose that came into
 *                   the cordinates that now exist in the Text widget.
 *	Arguments: ctx - the text widget.
 *                 expose - a Rectangle, who's region currently
 *                          contains the expose event location.
 *                          this region will be returned containing
 *                          the new rectangle.
 *	Returns: True if there is drawing that needs to be done.
 */

static Boolean
TranslateExposeRegion(TextWidget ctx, IswRectangle *expose)
{
    struct text_move * offsets = ctx->text.copy_area_offsets;
    int value;
    int x, y, width, height;

    /*
     * Skip over the first one, this has already been taken into account.
     */

    if (!offsets || !(offsets = offsets->next))
	return(TRUE);

    x = expose->x;
    y = expose->y;
    width = expose->width;
    height = expose->height;

    while (offsets) {
	x += offsets->h;
	y += offsets->v;
	offsets = offsets->next;
    }

    /*
     * remove that area of the region that is now outside the window.
     */

    if (y < 0) {
	height += y;
	y = 0;
    }

    value = y + height - ctx->core.height;
    if (value > 0)
	height -= value;

    if (height <= 0)
	return(FALSE);		/* no need to draw outside the window. */

    /*
     * and now in the horiz direction...
     */

    if (x < 0) {
	width += x;
	x = 0;
    }

    value = x + width - ctx->core.width;
    if (value > 0)
	width -= value;

    if (width <= 0)
	return(FALSE);		/* no need to draw outside the window. */

    expose->x = x;
    expose->y = y;
    expose->width = width;
    expose->height = height;
    return(TRUE);
}

/* Li wrote this so the IM can find a given text position's screen position. */

void
_IswTextPosToXY(
    Widget w,
    ISWTextPosition pos,
    Position* x,
    Position* y )
{
    int line;
    LineAndXYForPosition( (TextWidget)w, pos, &line, x, y );
}

/*******************************************************************
The following routines provide procedural interfaces to Text window state
setting and getting. They need to be redone so than the args code can use
them. I suggest we create a complete set that takes the context as an
argument and then have the public version lookup the context and call the
internal one. The major value of this set is that they have actual application
clients and therefore the functionality provided is required for any future
version of Text.
********************************************************************/

void
IswTextDisplay (Widget w)
{
  if (!IswIsRealized(w)) return;

  _IswTextPrepareToUpdate( (TextWidget) w);
  DisplayTextWindow(w);
  _IswTextExecuteUpdate( (TextWidget) w);
}

void
IswTextSetSelectionArray(Widget w, IswTextSelectType *sarray)
{
  ((TextWidget)w)->text.sarray = sarray;
}

void
IswTextGetSelectionPos(Widget w, ISWTextPosition *left, ISWTextPosition *right)
{
  *left = ((TextWidget) w)->text.s.left;
  *right = ((TextWidget) w)->text.s.right;
}


void
IswTextSetSource(Widget w, Widget source, ISWTextPosition startPos)
{
  TextWidget ctx = (TextWidget) w;

  ctx->text.source = source;
  ctx->text.lt.top = startPos;
  ctx->text.s.left = ctx->text.s.right = 0;
  ctx->text.insertPos = startPos;
  ctx->text.lastPos = GETLASTPOS;

  _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
  IswTextDisplay(w);
}

/*
 * This public routine deletes the text from startPos to endPos in a source and
 * then inserts, at startPos, the text that was passed. As a side effect it
 * "invalidates" that portion of the displayed text (if any), so that things
 * will be repainted properly.
 */

int
IswTextReplace(Widget w, ISWTextPosition startPos, ISWTextPosition endPos,
               ISWTextBlock *text)
{
  TextWidget ctx = (TextWidget) w;
  int result;

  _IswTextPrepareToUpdate(ctx);
  endPos = FindGoodPosition(ctx, endPos);
  startPos = FindGoodPosition(ctx, startPos);
  if ((result = _IswTextReplace(ctx, startPos, endPos, text)) == IswEditDone) {
    int delta = text->length - (endPos - startPos);
    if (ctx->text.insertPos >= (endPos + delta)) {
      IswTextScanDirection sd = (delta < 0) ? IswsdLeft : IswsdRight;
      ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
				    IswstPositions, sd, abs(delta), TRUE);
    }
  }

  _IswTextCheckResize(ctx);
  _IswTextExecuteUpdate(ctx);
  _IswTextSetScrollBars(ctx);

  return result;
}

ISWTextPosition
IswTextTopPosition(Widget w)
{
  return( ((TextWidget) w)->text.lt.top );
}

void
IswTextSetInsertionPoint(Widget w, ISWTextPosition position)
{
  TextWidget ctx = (TextWidget) w;

  _IswTextPrepareToUpdate(ctx);
  ctx->text.insertPos = FindGoodPosition(ctx, position);
  ctx->text.showposition = TRUE;

  _IswTextExecuteUpdate(ctx);
}

ISWTextPosition
IswTextGetInsertionPoint(Widget w)
{
  return( ((TextWidget) w)->text.insertPos);
}

/*
 * NOTE: Must walk the selection list in opposite order from LoseSelection.
 */

void
IswTextUnsetSelection(Widget w)
{
  TextWidget ctx = (TextWidget)w;
  IswSelectionId clipboard = _IswPlatformSelectionInternName(IswDisplayOf(w), "CLIPBOARD", False);

  while (ctx->text.s.id_count != 0) {
    IswSelectionId sel = ctx->text.s.selections[ctx->text.s.id_count - 1];
    if ( sel != ISW_SELECTION_NONE ) {
/*
 * As selections are lost the id_count will decrement.
 * Keep CLIPBOARD ownership — it persists beyond the visible highlight.
 */
      if (sel == clipboard) {
	      ctx->text.s.selections[ctx->text.s.id_count - 1] = ISW_SELECTION_NONE;
	      ctx->text.s.id_count--;
	      continue;
      }
      IswDisownSelection(w, sel, ctx->text.time);
      LoseSelection(w, &sel); /* In case IswDisownSelection failed to call us. */
    }
  }
}

void
IswTextSetSelection (Widget w, ISWTextPosition left, ISWTextPosition right)
{
  TextWidget ctx = (TextWidget) w;

  _IswTextPrepareToUpdate(ctx);
  _IswTextSetSelection(ctx, FindGoodPosition(ctx, left),
		       FindGoodPosition(ctx, right), (String*)NULL, ZERO);
  _IswTextExecuteUpdate(ctx);
}

void
IswTextInvalidate(Widget w, ISWTextPosition from, ISWTextPosition to)
{
  TextWidget ctx = (TextWidget) w;

  from = FindGoodPosition(ctx, from);
  to = FindGoodPosition(ctx, to);
  ctx->text.lastPos = GETLASTPOS;
  _IswTextPrepareToUpdate(ctx);
  _IswTextNeedsUpdating(ctx, from, to);
  _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
  _IswTextExecuteUpdate(ctx);
}

/*ARGSUSED*/
void
IswTextDisableRedisplay(Widget w)
{
  ((TextWidget) w)->text.update_disabled = True;
  _IswTextPrepareToUpdate( (TextWidget) w);
}

void
IswTextEnableRedisplay(Widget w)
{
  TextWidget ctx = (TextWidget)w;
  ISWTextPosition lastPos;

  if (!ctx->text.update_disabled) return;

  ctx->text.update_disabled = False;
  lastPos = ctx->text.lastPos = GETLASTPOS;
  ctx->text.lt.top = FindGoodPosition(ctx, ctx->text.lt.top);
  ctx->text.insertPos = FindGoodPosition(ctx, ctx->text.insertPos);
  if ( (ctx->text.s.left > lastPos) || (ctx->text.s.right > lastPos) )
    ctx->text.s.left = ctx->text.s.right = 0;

  _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
  if (IswIsRealized(w))
    DisplayTextWindow(w);
  _IswTextExecuteUpdate(ctx);
}

Widget
IswTextGetSource(Widget w)
{
  return ((TextWidget)w)->text.source;
}

Widget
IswTextGetSink(Widget w)
{
  return (((TextWidget)w)->text.sink);
}

void
IswTextDisplayCaret (Widget w,
#if NeedWidePrototypes
		    /* Boolean */ int display_caret)
#else
		    Boolean display_caret)
#endif
{
  TextWidget ctx = (TextWidget) w;

  if (ctx->text.display_caret == display_caret) return;

  if (IswIsRealized(w)) {
    _IswTextPrepareToUpdate(ctx);
    ctx->text.display_caret = display_caret;
    _IswTextExecuteUpdate(ctx);
  }
  else
    ctx->text.display_caret = display_caret;
}

/*	Function Name: IswTextSearch(w, dir, text).
 *	Description: searches for the given text block.
 *	Arguments: w - The text widget.
 *                 dir - The direction to search.
 *                 text - The text block containing info about the string
 *                        to search for.
 *	Returns: The position of the text found, or IswTextSearchError on
 *               an error.
 */

ISWTextPosition
IswTextSearch(Widget w,
#if NeedWidePrototypes
	    /* IswTextScanDirection */ int dir,
#else
	    IswTextScanDirection dir,
#endif
	    ISWTextBlock *text)
{
  TextWidget ctx = (TextWidget) w;

  return(SrcSearch(ctx->text.source, ctx->text.insertPos, dir, text));
}

TextClassRec textClassRec = {
  { /* core fields */
    /* superclass       */      (WidgetClass) &simpleClassRec,
    /* class_name       */      "Text",
    /* widget_size      */      sizeof(TextRec),
    /* class_initialize */      ClassInitialize,
    /* class_part_init  */	NULL,
    /* class_inited     */      FALSE,
    /* initialize       */      Initialize,
    /* initialize_hook  */	NULL,
    /* realize          */      Realize,
    /* actions          */      _IswTextActionsTable,
    /* num_actions      */      0,                /* Set in ClassInitialize. */
    /* resources        */      resources,
    /* num_ resource    */      IswNumber(resources),
    /* xrm_class        */      NULLQUARK,
    /* compress_motion  */      TRUE,
    /* compress_exposure*/      IswExposeGraphicsExpose | IswExposeNoExpose,
    /* compress_enterleave*/	TRUE,
    /* visible_interest */      FALSE,
    /* destroy          */      TextDestroy,
    /* resize           */      Resize,
    /* expose           */      (void (*)(Widget, IswEvent *, IswRegion))ProcessExposeRegion,
    /* set_values       */      SetValues,
    /* set_values_hook  */	NULL,
    /* set_values_almost*/	IswInheritSetValuesAlmost,
    /* get_values_hook  */	GetValuesHook,
    /* accept_focus     */      NULL,
    /* version          */	IswVersion,
    /* callback_private */      NULL,
    /* tm_table         */      NULL,    /* set in ClassInitialize */
    /* query_geometry   */	IswInheritQueryGeometry,
    /* display_accelerator*/	IswInheritDisplayAccelerator,
    /* extension	*/	NULL
  },
  { /* Simple fields */
    /* change_sensitive	*/	ChangeSensitive,
    /* hit_child	*/	TextHitChild,
    /* nth_windowless_child */	TextNthWindowlessChild
  },
  { /* text fields */
    /* empty            */	0
  }
};

WidgetClass textWidgetClass = (WidgetClass)&textClassRec;

