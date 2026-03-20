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

#ifndef _ISW_IswText_h
#define _ISW_IswText_h

#include <ISW/Simple.h>

/*
 Text widget

 Class: 	textWidgetClass
 Class Name:	Text
 Superclass:	Simple

 Resources added by the Text widget:

 Name		     Class	     RepType		Default Value
 ----		     -----	     -------		-------------
 autoFill	    AutoFill	     Boolean		False
 bottomMargin	    Margin	     Position		2
 displayPosition    TextPosition     ISWTextPosition	0
 insertPosition	    TextPosition     ISWTextPosition	0
 leftMargin	    Margin	     Position		2
 resize		    Resize	     IswTextResizeMode	IswTextResizeNever
 rightMargin	    Margin	     Position		4
 scrollHorizontal   Scroll	     IswTextScrollMode	IswtextScrollNever
 scrollVertical     Scroll	     IswTextScrollMode  IswtextScrollNever
 selectTypes        SelectTypes      Pointer            see documentation
 textSink	    TextSink	     Widget		NULL
 textSource	    TextSource	     Widget		NULL
 topMargin	    Margin	     Position		2
 unrealizeCallback  Callback	     Callback		NULL
 wrap		    Wrap	     IswTextWrapMode	IswTextWrapNever

*/

typedef long ISWTextPosition;

typedef enum { IswtextScrollNever,
	       IswtextScrollWhenNeeded, IswtextScrollAlways} IswTextScrollMode;

typedef enum { IswtextWrapNever,
	       IswtextWrapLine, IswtextWrapWord} IswTextWrapMode;

typedef enum { IswtextResizeNever, IswtextResizeWidth,
	       IswtextResizeHeight, IswtextResizeBoth} IswTextResizeMode;

typedef enum {IswsdLeft, IswsdRight} IswTextScanDirection;
typedef enum {IswtextRead, IswtextAppend, IswtextEdit} IswTextEditType;
typedef enum {IswselectNull, IswselectPosition, IswselectChar, IswselectWord,
    IswselectLine, IswselectParagraph, IswselectAll} IswTextSelectType;

typedef struct {
    int  firstPos;
    int  length;
    char *ptr;
    unsigned long format;
    } ISWTextBlock, *IswTextBlockPtr;

#include <ISW/TextSink.h>
#include <ISW/TextSrc.h>

#define XtEtextScrollNever "never"
#define XtEtextScrollWhenNeeded "whenneeded"
#define XtEtextScrollAlways "always"

#define XtEtextWrapNever "never"
#define XtEtextWrapLine "line"
#define XtEtextWrapWord "word"

#define XtEtextResizeNever "never"
#define XtEtextResizeWidth "width"
#define XtEtextResizeHeight "height"
#define XtEtextResizeBoth "both"

#define XtNautoFill "autoFill"
#define XtNbottomMargin "bottomMargin"
#define XtNdialogHOffset "dialogHOffset"
#define XtNdialogVOffset "dialogVOffset"
#define XtNdisplayCaret "displayCaret"
#define XtNdisplayPosition "displayPosition"
#define XtNleftMargin "leftMargin"
#define XtNrightMargin "rightMargin"
#define XtNscrollVertical "scrollVertical"
#define XtNscrollHorizontal "scrollHorizontal"
#define XtNselectTypes "selectTypes"
#define XtNtopMargin "topMargin"
#define XtNwrap "wrap"

#define XtCAutoFill "AutoFill"
#define XtCScroll "Scroll"
#define XtCSelectTypes "SelectTypes"
#define XtCWrap "Wrap"

#ifndef _XtStringDefs_h_
#define XtNinsertPosition "insertPosition"
#define XtNresize "resize"
#define XtNselection "selection"
#define XtCResize "Resize"
#endif

/* Return Error code for IswTextSearch */

#define IswTextSearchError      (-12345L)

/* Return codes from IswTextReplace */

#define IswReplaceError	       -1
#define IswEditDone		0
#define IswEditError		1
#define IswPositionError	2

extern unsigned long FMT8BIT;
extern unsigned long IswFmt8Bit;
extern unsigned long IswFmtWide;

/* Class record constants */

extern WidgetClass textWidgetClass;

typedef struct _TextClassRec *TextWidgetClass;
typedef struct _TextRec      *TextWidget;

_XFUNCPROTOBEGIN

extern XrmQuark _IswTextFormat(
    TextWidget		/* tw */
);

extern void IswTextDisplay(
    Widget		/* w */
);

extern void IswTextEnableRedisplay(
    Widget		/* w */
);

extern void IswTextDisableRedisplay(
    Widget		/* w */
);

extern void IswTextSetSelectionArray(
    Widget		/* w */,
    IswTextSelectType*	/* sarray */
);

extern void IswTextGetSelectionPos(
    Widget		/* w */,
    ISWTextPosition*	/* begin_return */,
    ISWTextPosition*	/* end_return */
);

extern void IswTextSetSource(
    Widget		/* w */,
    Widget		/* source */,
    ISWTextPosition	/* position */
);

extern int IswTextReplace(
    Widget		/* w */,
    ISWTextPosition	/* start */,
    ISWTextPosition	/* end */,
    ISWTextBlock*	/* text */
);

extern ISWTextPosition IswTextTopPosition(
    Widget		/* w */
);

extern void IswTextSetInsertionPoint(
    Widget		/* w */,
    ISWTextPosition	/* position */
);

extern ISWTextPosition IswTextGetInsertionPoint(
    Widget		/* w */
);

extern void IswTextUnsetSelection(
    Widget		/* w */
);

extern void IswTextSetSelection(
    Widget		/* w */,
    ISWTextPosition	/* left */,
    ISWTextPosition	/* right */
);

extern void IswTextInvalidate(
    Widget		/* w */,
    ISWTextPosition	/* from */,
    ISWTextPosition	/* to */
);

extern Widget IswTextGetSource(
    Widget		/* w */
);

extern Widget IswTextGetSink(
    Widget		/* w */
);

extern ISWTextPosition IswTextSearch(
    Widget			/* w */,
#if NeedWidePrototypes
    /* IswTextScanDirection */ int /* dir */,
#else
    IswTextScanDirection	/* dir */,
#endif
    ISWTextBlock*		/* text */
);

extern void IswTextDisplayCaret(
    Widget		/* w */,
#if NeedWidePrototypes
    /* Boolean */ int	/* visible */
#else
    Boolean		/* visible */
#endif
);

_XFUNCPROTOEND

/*
 * For R3 compatability only.
 */

#include <ISW/AsciiSrc.h>
#include <ISW/AsciiSink.h>

#endif /* _ISW_IswText_h */
/* DON'T ADD STUFF AFTER THIS #endif */
