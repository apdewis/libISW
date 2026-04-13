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

#ifndef _ISW_IswForm_h
#define _ISW_IswForm_h

#include <ISW/Constraint.h>

/***********************************************************************
 *
 * Form Widget
 *
 ***********************************************************************/

/* Parameters:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		Pixel		IswDefaultBackground
 border		     BorderColor	Pixel		IswDefaultForeground
 borderWidth	     BorderWidth	Dimension	1
 defaultDistance     Thickness		int		4
 destroyCallback     Callback		Pointer		NULL
 height		     Height		Dimension	computed at realize
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 sensitive	     Sensitive		Boolean		True
 width		     Width		Dimension	computed at realize
 x		     Position		Position	0
 y		     Position		Position	0

*/

/* Constraint parameters:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 bottom		     Edge		IswEdgeType	IswRubber
 fromHoriz	     Widget		Widget		(left edge of form)
 fromVert	     Widget		Widget		(top of form)
 horizDistance	     Thickness		int		defaultDistance
 left		     Edge		IswEdgeType	IswRubber
 resizable	     Boolean		Boolean		False
 right		     Edge		IswEdgeType	IswRubber
 top		     Edge		IswEdgeType	IswRubber
 vertDistance	     Thickness		int		defaultDistance

*/


#ifndef _IswStringDefs_h_
#define IswNtop "top"
#define IswRWidget "Widget"
#endif

#define IswNdefaultDistance "defaultDistance"
#define IswNbottom "bottom"
#define IswNleft "left"
#define IswNright "right"
#define IswNfromHoriz "fromHoriz"
#define IswNfromVert "fromVert"
#define IswNhorizDistance "horizDistance"
#define IswNvertDistance "vertDistance"
#define IswNresizable "resizable"

#define IswCEdge "Edge"
#define IswCWidget "Widget"

#ifndef _IswEdgeType_e
#define _IswEdgeType_e
typedef enum {
    IswChainTop,		/* Keep this edge a constant distance from
				   the top of the form */
    IswChainBottom,		/* Keep this edge a constant distance from
				   the bottom of the form */
    IswChainLeft,		/* Keep this edge a constant distance from
				   the left of the form */
    IswChainRight,		/* Keep this edge a constant distance from
				   the right of the form */
    IswRubber			/* Keep this edge a proportional distance
				   from the edges of the form*/
} IswEdgeType;
#endif /* _IswEdgeType_e */

/*
 * Unfortunatly I missed this definition for R4, so I cannot
 * protect it with XAW_BC, it looks like this particular problem is
 * one that we will have to live with for a while.
 *
 * Chris D. Peterson - 3/23/90.
 */

#define IswEdgeType IswEdgeType

#define IswChainTop IswChainTop
#define IswChainBottom IswChainBottom
#define IswChainLeft IswChainLeft
#define IswChainRight IswChainRight
#define IswRubber IswRubber

typedef struct _FormClassRec	*FormWidgetClass;
typedef struct _FormRec		*FormWidget;

extern WidgetClass formWidgetClass;

_XFUNCPROTOBEGIN

extern void IswFormDoLayout(
    Widget		/* w */,
#if NeedWidePrototypes
    /* Boolean */ int	/* do_layout */
#else
    Boolean		/* do_layout */
#endif
);

_XFUNCPROTOEND

#endif /* _ISW_IswForm_h */
