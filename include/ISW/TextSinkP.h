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

/*
 * TextSinkP.h - Private definitions for TextSink object
 *
 */

#ifndef _ISW_IswTextSinkP_h
#define _ISW_IswTextSinkP_h

/***********************************************************************
 *
 * TextSink Object Private Data
 *
 ***********************************************************************/

#include <xcb/xcb.h>

#include <ISW/TextSink.h>
#include <ISW/TextP.h>	/* This source works with the Text widget. */
#include <ISW/TextSrcP.h>	/* This source works with the Text Source. */

/************************************************************
 *
 * New fields for the TextSink object class record.
 *
 ************************************************************/

typedef void (*_IswSinkDisplayTextProc)
     (Widget, Position, Position, ISWTextPosition, ISWTextPosition, Boolean);

typedef void (*_IswSinkInsertCursorProc)
     (Widget, Position, Position, IswTextInsertState);

typedef void (*_IswSinkClearToBackgroundProc)
     (Widget, Position, Position, Dimension, Dimension);

typedef void (*_IswSinkFindPositionProc)
     (Widget, ISWTextPosition, int, int, Boolean, ISWTextPosition*, int*, int*);

typedef void (*_IswSinkFindDistanceProc)
     (Widget, ISWTextPosition, int, ISWTextPosition, int*, ISWTextPosition*, int*);

typedef void (*_IswSinkResolveProc)
     (Widget, ISWTextPosition, int, int, ISWTextPosition*);

typedef int  (*_IswSinkMaxLinesProc)
     (Widget, Dimension);

typedef int  (*_IswSinkMaxHeightProc)
     (Widget, int);

typedef void (*_IswSinkSetTabsProc)
     (Widget, int, short*);

typedef void (*_IswSinkGetCursorBoundsProc)
     (Widget, xcb_rectangle_t*);

typedef struct _TextSinkClassPart {
    _IswSinkDisplayTextProc DisplayText;
    _IswSinkInsertCursorProc InsertCursor;
    _IswSinkClearToBackgroundProc ClearToBackground;
    _IswSinkFindPositionProc FindPosition;
    _IswSinkFindDistanceProc FindDistance;
    _IswSinkResolveProc Resolve;
    _IswSinkMaxLinesProc MaxLines;
    _IswSinkMaxHeightProc MaxHeight;
    _IswSinkSetTabsProc	SetTabs;
    _IswSinkGetCursorBoundsProc GetCursorBounds;
} TextSinkClassPart;

/* Full class record declaration */
typedef struct _TextSinkClassRec {
    ObjectClassPart     object_class;
    TextSinkClassPart	text_sink_class;
} TextSinkClassRec;

extern TextSinkClassRec textSinkClassRec;

/* New fields for the TextSink object record */
typedef struct {
    /* resources */
    Pixel foreground;		/* Foreground color. */
    Pixel background;		/* Background color. */

    /* private state. */
    Position *tabs;		/* The tab stops as pixel values. */
    short    *char_tabs;	/* The tabs stops as character values. */
    int      tab_count;		/* number of items in tabs */

} TextSinkPart;

/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _TextSinkRec {
  ObjectPart    object;
  TextSinkPart	text_sink;
} TextSinkRec;

/************************************************************
 *
 * Private declarations.
 *
 ************************************************************/

#define XtInheritDisplayText	   ((_IswSinkDisplayTextProc)_XtInherit)
#define XtInheritInsertCursor	   ((_IswSinkInsertCursorProc)_XtInherit)
#define XtInheritClearToBackground ((_IswSinkClearToBackgroundProc)_XtInherit)
#define XtInheritFindPosition	   ((_IswSinkFindPositionProc)_XtInherit)
#define XtInheritFindDistance	   ((_IswSinkFindDistanceProc)_XtInherit)
#define XtInheritResolve	   ((_IswSinkResolveProc)_XtInherit)
#define XtInheritMaxLines	   ((_IswSinkMaxLinesProc)_XtInherit)
#define XtInheritMaxHeight	   ((_IswSinkMaxHeightProc)_XtInherit)
#define XtInheritSetTabs	   ((_IswSinkSetTabsProc)_XtInherit)
#define XtInheritGetCursorBounds   ((_IswSinkGetCursorBoundsProc)_XtInherit)

#endif /* _ISW_IswTextSinkP_h */
