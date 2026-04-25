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

#ifndef _ISW_IswLabel_h
#define _ISW_IswLabel_h

/***********************************************************************
 *
 * Label Widget
 *
 ***********************************************************************/

#include "ISWP.h"
#include <ISW/Simple.h>

/* Resources:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		Pixel		IswDefaultBackground
 bitmap		     Pixmap		Pixmap		None
 border		     BorderColor	Pixel		IswDefaultForeground
 borderWidth	     BorderWidth	Dimension	1
 cursor		     Cursor		Cursor		None
 cursorName	     Cursor		String		NULL
 destroyCallback     Callback		IswCallbackList	NULL
 encoding	     Encoding		unsigned char	IswTextEncoding8bit
 font		     Font		IswFontStruct*	IswDefaultFont
 foreground	     Foreground		Pixel		IswDefaultForeground
 height		     Height		Dimension	text height
 insensitiveBorder   Insensitive	Pixmap		Gray
 internalHeight	     Height		Dimension	2
 internalWidth	     Width		Dimension	4
 justify	     Justify		IswJustify	IswJustifyCenter
 label		     Label		String		NULL
 leftBitmap	     LeftBitmap		Pixmap		None
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 pointerColor	     Foreground		Pixel		IswDefaultForeground
 pointerColorBackground Background	Pixel		IswDefaultBackground
 resize		     Resize		Boolean		True
 sensitive	     Sensitive		Boolean		True
 width		     Width		Dimension	text width
 x		     Position		Position	0
 y		     Position		Position	0

*/

/* Ellipsize modes for text truncation */
typedef enum {
    IswEllipsizeNone,
    IswEllipsizeStart,
    IswEllipsizeMiddle,
    IswEllipsizeEnd
} IswEllipsize;

#define IswNellipsize "ellipsize"
#define IswCEllipsize "Ellipsize"
#define IswREllipsize "Ellipsize"

#define IswTextEncoding8bit	0
#define IswTextEncodingChar2b	1

#define IswNencoding "encoding"
#define IswCEncoding "Encoding"

#define IswNimage "image"
#define IswCImage "Image"
#define IswNleftImage "leftImage"
#define IswCLeftImage "LeftImage"

#ifndef _IswStringDefs_h_
#define IswNforeground "foreground"
#define IswNlabel "label"
#define IswNfont "font"
#define IswNinternalWidth "internalWidth"
#define IswNinternalHeight "internalHeight"
#define IswNresize "resize"
#define IswCResize "Resize"
#endif

/* Class record constants */

extern WidgetClass labelWidgetClass;

typedef struct _LabelClassRec *LabelWidgetClass;
typedef struct _LabelRec      *LabelWidget;

#endif /* _ISW_IswLabel_h */
