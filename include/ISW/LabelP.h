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
 * LabelP.h - Private definitions for Label widget
 *
 */

#ifndef _ISW_IswLabelP_h
#define _ISW_IswLabelP_h

/***********************************************************************
 *
 * Label Widget Private Data
 *
 ***********************************************************************/

#include "ISWP.h"
#include <ISW/Label.h>
#include <ISW/SimpleP.h>
#include <ISW/ISWXftCompat.h>  /* ISWFontSet typedef only */
#include <ISW/ISWRender.h>     /* For ISWRenderContext */
#include <ISW/ISWSVG.h>        /* For ISWSVGImage */

/* XtJustify type is now centrally defined in Isw3dP.h and IswXcbDraw.h */
/* with proper include guards to prevent redefinition */

/* New fields for the Label widget class record */

typedef struct {int foo;} LabelClassPart;

/* Full class record declaration */
typedef struct _LabelClassRec {
    CoreClassPart	core_class;
    SimpleClassPart	simple_class;
    LabelClassPart	label_class;
} LabelClassRec;

extern LabelClassRec labelClassRec;

/* New fields for the Label widget record */
typedef struct {
    /* resources */
    Pixel	foreground;
    XFontStruct	*font;
#ifdef ISW_INTERNATIONALIZATION
    ISWFontSet	*fontset;  /* Phase 3.2: Changed from XFontSet to ISWFontSet* */
#endif
    char	*label;
    XtJustify	justify;
    Dimension	internal_width;
    Dimension	internal_height;
    Pixmap	pixmap;
    Boolean	resize;
    unsigned char encoding;
    Pixmap	left_bitmap;

    /* private state */
    xcb_gcontext_t		normal_GC;
    xcb_gcontext_t          gray_GC;
    Pixmap	stipple;
    Position	label_x;
    Position	label_y;
    Dimension	label_width;
    Dimension	label_height;
    Dimension	label_len;
    int		lbm_y;			/* where in label */
    unsigned int lbm_width, lbm_height;	/* size of pixmap */
    unsigned int depth;			/* depth of pixmaps */

    /* Cairo rendering context */
    ISWRenderContext *render_ctx;

    /* SVG image support */
    String	svg_data;		/* inline SVG XML string */
    String	svg_file;		/* path to SVG file */
    ISWSVGImage	*svg_image;		/* parsed SVG (private) */
    unsigned char *svg_raster;		/* cached RGBA raster (private) */
    unsigned int svg_raster_w;		/* cached raster width */
    unsigned int svg_raster_h;		/* cached raster height */
} LabelPart;


/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _LabelRec {
    CorePart	core;
    SimplePart	simple;
    LabelPart	label;
} LabelRec;

#define LEFT_OFFSET(lw) ((lw)->label.left_bitmap && (lw)->label.pixmap == None \
			 ? (lw)->label.lbm_width + (lw)->label.internal_width \
			 : 0)

#endif /* _ISW_IswLabelP_h */
