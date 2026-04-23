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

******************************************************************/

/*
 * TextSinkP.h - Private definitions for TextSink object
 *
 * Single concrete UTF-8 text sink. Replaces the old abstract-TextSink +
 * concrete-AsciiSink/MultiSink hierarchy.
 */

#ifndef _ISW_IswTextSinkP_h
#define _ISW_IswTextSinkP_h

#include <xcb/xcb.h>

#include <ISW/TextSink.h>
#include <ISW/TextP.h>
#include <ISW/TextSrcP.h>
#include <ISW/ISWRender.h>

/* Class part (vtable) — kept so existing IswTextSink* dispatch functions
 * continue to work unchanged. There is only one concrete implementation
 * now, so the vtable entries are always populated by textSinkClassRec. */

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
    _IswSinkSetTabsProc SetTabs;
    _IswSinkGetCursorBoundsProc GetCursorBounds;
} TextSinkClassPart;

typedef struct _TextSinkClassRec {
    ObjectClassPart     object_class;
    TextSinkClassPart   text_sink_class;
} TextSinkClassRec;

extern TextSinkClassRec textSinkClassRec;

/* Instance struct — public resources and private state for the concrete
 * TextSink. Absorbs the former AsciiSinkPart fields. */
typedef struct {
    /* public resources */
    Pixel foreground;
    Pixel background;
    IswFontStruct *font;
    Boolean echo;
    Boolean display_nonprinting;

    /* private state */
    Position *tabs;
    short    *char_tabs;
    int       tab_count;

    xcb_pixmap_t insertCursorOn;
    IswTextInsertState laststate;
    short cursor_x, cursor_y;
    ISWRenderContext *render_ctx;
} TextSinkPart;

typedef struct _TextSinkRec {
  ObjectPart    object;
  TextSinkPart  text_sink;
} TextSinkRec;

/* The old IswInherit* sentinels still need to exist for any caller that
 * references them, but with a single concrete class nothing resolves
 * through them. */
#define IswInheritDisplayText       ((_IswSinkDisplayTextProc)_IswInherit)
#define IswInheritInsertCursor      ((_IswSinkInsertCursorProc)_IswInherit)
#define IswInheritClearToBackground ((_IswSinkClearToBackgroundProc)_IswInherit)
#define IswInheritFindPosition      ((_IswSinkFindPositionProc)_IswInherit)
#define IswInheritFindDistance      ((_IswSinkFindDistanceProc)_IswInherit)
#define IswInheritResolve           ((_IswSinkResolveProc)_IswInherit)
#define IswInheritMaxLines          ((_IswSinkMaxLinesProc)_IswInherit)
#define IswInheritMaxHeight         ((_IswSinkMaxHeightProc)_IswInherit)
#define IswInheritSetTabs           ((_IswSinkSetTabsProc)_IswInherit)
#define IswInheritGetCursorBounds   ((_IswSinkGetCursorBoundsProc)_IswInherit)

#endif /* _ISW_IswTextSinkP_h */
