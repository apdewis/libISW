/*
Copyright (c) 1989, 1994  X Consortium

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
 *
 * Author:  Chris D. Peterson, MIT X Consortium
 */

/*
 * SmeP.h - Private definitions for Sme object
 *
 */

#ifndef _ISW_IswSmeBSBP_h
#define _ISW_IswSmeBSBP_h

/***********************************************************************
 *
 * Sme Object Private Data
 *
 ***********************************************************************/

#include "ISWP.h"
#include <ISW/SmeP.h>
#include <ISW/SmeBSB.h>
#include <ISW/ISWXftCompat.h>  /* ISWFontSet typedef */
#include <ISW/ISWRender.h>
#include <ISW/ISWImage.h>

/* XtJustify is missing from XCB-based libXt, define it here */
#ifndef _IswXtJustify_defined
#define _IswXtJustify_defined
typedef enum {
    XtJustifyLeft,
    XtJustifyCenter,
    XtJustifyRight
} XtJustify;
#endif

/************************************************************
 *
 * New fields for the Sme Object class record.
 *
 ************************************************************/

typedef struct _SmeBSBClassPart {
  XtPointer extension;
} SmeBSBClassPart;

/* Full class record declaration */
typedef struct _SmeBSBClassRec {
    RectObjClassPart	rect_class;
    SmeClassPart	sme_class;
    SmeBSBClassPart	sme_bsb_class;
} SmeBSBClassRec;

extern SmeBSBClassRec smeBSBClassRec;

/* New fields for the Sme Object record */
typedef struct {
    /* resources */
    String label;		/* The entry label. */
    int vert_space;		/* extra vert space to leave, as a percentage
				   of the font height of the label. */
    String left_image_source;   /* left icon: file path or inline SVG */
    String right_image_source;  /* right icon: file path or inline SVG */
    ISWImage *left_image;
    ISWImage *right_image;
    Dimension left_margin, right_margin; /* left and right margins. */
    Pixel foreground;		/* foreground color. */
    XFontStruct * font;		/* The font to show label in. */
#ifdef ISW_INTERNATIONALIZATION
    ISWFontSet *fontset;		/* or fontset */
#endif
    XtJustify justify;		/* Justification for the label. */
    int underline;		/* index of letter to underline in label. */

    /* private state. */
    Boolean set_values_area_cleared; /* Remember if we need to unhighlight. */
    Dimension left_image_width;
    Dimension left_image_height;
    Dimension right_image_width;
    Dimension right_image_height;
    String menu_name;		/* name of nested sub-menu or NULL */
} SmeBSBPart;

/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _SmeBSBRec {
    ObjectPart		object;
    RectObjPart		rectangle;
    SmePart		sme;
    SmeBSBPart		sme_bsb;
} SmeBSBRec;

/************************************************************
 *
 * Private declarations.
 *
 ************************************************************/

#endif /* _ISW_IswSmeBSBP_h */
