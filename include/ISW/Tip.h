/*
 * Copyright (c) 1999 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo C�sar Pereira de Andrade
 */

#ifndef _ISW_IswTip_h
#define _ISW_IswTip_h

/*
 * Tip Widget
 */

#include "ISWP.h"
#include <ISW/Simple.h>

/* Resources:

  Name		    Class		RepType		Default Value
  ----		    -----		-------		-------------
  background	    Background		Pixel		IswDefaultBackground
  backgroundPixmap  BackgroundPixmap	Pixmap		IswUnspecifiedPixmap
  border	    BorderColor		Pixel		IswDefaultForeground
  borderWidth	    BorderWidth		Dimension	1
  destroyCallback   Callback		IswCallbackList	NULL
  font		    Font		IswFontStruct*	IswDefaultFont
  foreground	    Foreground		Pixel		IswDefaultForeground
  height	    Height		Dimension	text height
  internalHeight    Height		Dimension	2
  internalWidth     Width		Dimension	2
  label		    Label		String		NULL
  timeout	    Timeout		Int		500
  width		    Width		Dimension	text width
  x		    Position		Position	0
  y		    Position		Position	0

*/

typedef struct _TipClassRec *TipWidgetClass;
typedef struct _TipRec *TipWidget;

extern WidgetClass tipWidgetClass;

#define IswTextEncoding8bit	0
#define IswTextEncodingChar2b	1

#define IswNencoding "encoding"
#define IswNtimeout "timeout"
#define IswNtip "tip"

#ifdef ISW_INTERNATIONALIZATION
#ifndef IswNfontSet
#define IswNfontSet "fontSet"
#endif
#ifndef IswCFontSet
#define IswCFontSet "FontSet"
#endif
#endif

#define IswCEncoding "Encoding"
#define IswCTimeout "Timeout"
#define IswCTip "Tip"

#ifndef _IswStringDefs_h_
#define IswNforeground "foreground"
#define IswNlabel "label"
#define IswNfont "font"
#define IswNinternalWidth "internalWidth"
#define IswNinternalHeight "internalHeight"
#endif

/*
 * Public Functions
 */

/*
 * Function:
 *	IswTipEnable
 *
 * Parameters:
 *	Widget - widget for tooltip
 *	String - tooltip label
 *
 * Description:
 *	Enables the tip event handler for this widget.
 */
void IswTipEnable(
 Widget,
 String
);

/*
 * Function:
 *	IswTipDisable
 *
 * Parameters:
 *	Widget - widget for tooltip
 *
 * Description:
 *	Disables the tip event handler for this widget.
 */
void IswTipDisable(
 Widget
);

#endif /* _ISW_IswTip_h */
