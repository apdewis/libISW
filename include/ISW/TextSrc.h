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

#ifndef _ISW_IswTextSrc_h
#define _ISW_IswTextSrc_h

/***********************************************************************
 *
 * TextSrc Object
 *
 ***********************************************************************/

#include <ISW/Text.h>

/* Resources:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 editType	     EditType		IswTextEditType	IswtextRead

*/

/* Class record constants */

extern WidgetClass textSrcObjectClass;

typedef struct _TextSrcClassRec *TextSrcObjectClass;
typedef struct _TextSrcRec      *TextSrcObject;

typedef enum {IswstPositions, IswstWhiteSpace, IswstEOL, IswstParagraph,
              IswstAll} IswTextScanType;
typedef enum {Normal, Selected }highlightType;
typedef enum {IswsmTextSelect, IswsmTextExtend} IswTextSelectionMode;
typedef enum {IswactionStart, IswactionAdjust, IswactionEnd}
    IswTextSelectionAction;

/*
 * Error Conditions:
 */

#define IswTextReadError -1
#define IswTextScanError -1

/************************************************************
 *
 * Public Functions.
 *
 ************************************************************/

_XFUNCPROTOBEGIN

/*	Function Name: IswTextSourceRead
 *	Description: This function reads the source.
 *	Arguments: w - the TextSrc Object.
 *                 pos - position of the text to retreive.
 * RETURNED        text - text block that will contain returned text.
 *                 length - maximum number of characters to read.
 *	Returns: The number of characters read into the buffer.
 */

extern ISWTextPosition IswTextSourceRead(
    Widget		/* w */,
    ISWTextPosition	/* pos */,
    ISWTextBlock*	/* text_return */,
    int			/* length */
);

/*	Function Name: IswTextSourceReplace.
 *	Description: Replaces a block of text with new text.
 *	Arguments: src - the Text Source Object.
 *                 startPos, endPos - ends of text that will be removed.
 *                 text - new text to be inserted into buffer at startPos.
 *	Returns: IswEditError or IswEditDone.
 */

extern int IswTextSourceReplace (
    Widget		/* w */,
    ISWTextPosition	/* start */,
    ISWTextPosition	/* end */,
    ISWTextBlock*	/* text */
);

/*	Function Name: IswTextSourceScan
 *	Description: Scans the text source for the number and type
 *                   of item specified.
 *	Arguments: w - the TextSrc Object.
 *                 position - the position to start scanning.
 *                 type - type of thing to scan for.
 *                 dir - direction to scan.
 *                 count - which occurance if this thing to search for.
 *                 include - whether or not to include the character found in
 *                           the position that is returned.
 *	Returns: The position of the text.
 *
 */

extern ISWTextPosition IswTextSourceScan(
    Widget		/* w */,
    ISWTextPosition	/* position */,
#if NeedWidePrototypes
    /* IswTextScanType */ int		/* type */,
    /* IswTextScanDirection */ int	/* dir */,
#else
    IswTextScanType	/* type */,
    IswTextScanDirection /* dir */,
#endif
    int			/* count */,
#if NeedWidePrototypes
    /* Boolean */ int	/* include */
#else
    Boolean		/* include */
#endif
);

/*	Function Name: IswTextSourceSearch
 *	Description: Searchs the text source for the text block passed
 *	Arguments: w - the TextSource Object.
 *                 position - the position to start scanning.
 *                 dir - direction to scan.
 *                 text - the text block to search for.
 *	Returns: The position of the text we are searching for or
 *               IswTextSearchError.
 */

extern ISWTextPosition IswTextSourceSearch(
    Widget		/* w */,
    ISWTextPosition	/* position */,
#if NeedWidePrototypes
    /* IswTextScanDirection */ int	/* dir */,
#else
    IswTextScanDirection /* dir */,
#endif
    ISWTextBlock*	/* text */
);

/*	Function Name: IswTextSourceConvertSelection
 *	Description: Dummy selection converter.
 *	Arguments: w - the TextSrc object.
 *                 selection - the current selection atom.
 *                 target    - the current target atom.
 *                 type      - the type to conver the selection to.
 * RETURNED        value, length - the return value that has been converted.
 * RETURNED        format    - the format of the returned value.
 *	Returns: TRUE if the selection has been converted.
 *
 */

extern Boolean IswTextSourceConvertSelection(
    Widget		/* w */,
    xcb_atom_t*		/* selection */,
    xcb_atom_t*		/* target */,
    xcb_atom_t*		/* type */,
    XtPointer*		/* value_return */,
    unsigned long*	/* length_return */,
    int*		/* format_return */
);

/*	Function Name: IswTextSourceSetSelection
 *	Description: allows special setting of the selection.
 *	Arguments: w - the TextSrc object.
 *                 left, right - bounds of the selection.
 *                 selection - the selection atom.
 *	Returns: none
 */

extern void IswTextSourceSetSelection(
    Widget		/* w */,
    ISWTextPosition	/* start */,
    ISWTextPosition	/* end */,
    xcb_atom_t		/* selection */
);

_XFUNCPROTOEND

#endif /* _ISW_IswTextSrc_h */
/* DON'T ADD STUFF AFTER THIS #endif */
