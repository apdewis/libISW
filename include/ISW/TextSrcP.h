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
 * TextSrcP.h - Private definitions for TextSrc object
 *
 */

#ifndef _ISW_IswTextSrcP_h
#define _ISW_IswTextSrcP_h

/***********************************************************************
 *
 * TextSrc Object Private Data
 *
 ***********************************************************************/

#include <xcb/xcb.h>
#include <ISW/TextSrc.h>
#include <ISW/TextP.h>	/* This source works with the Text widget. */

/************************************************************
 *
 * New fields for the TextSrc object class record.
 *
 ************************************************************/

#if 0	/* no longer used */
typedef struct {
  XtPointer		next_extension;
  XrmQuark		record_type;
  long			version;
  Cardinal		record_size;
  int			(*Input)();
} TextSrcExtRec, *TextSrcExt;
#endif

typedef ISWTextPosition (*_IswSrcReadProc)
     (Widget, ISWTextPosition, ISWTextBlock*, int);

typedef int (*_IswSrcReplaceProc)
     (Widget, ISWTextPosition, ISWTextPosition, ISWTextBlock*);

typedef ISWTextPosition (*_IswSrcScanProc)
     (Widget, ISWTextPosition, IswTextScanType, IswTextScanDirection,
      int, Boolean);

typedef ISWTextPosition (*_IswSrcSearchProc)
     (Widget, ISWTextPosition, IswTextScanDirection, ISWTextBlock*);

typedef void (*_IswSrcSetSelectionProc)
     (Widget, ISWTextPosition, ISWTextPosition, xcb_atom_t);

typedef Boolean (*_IswSrcConvertSelectionProc)
     (Widget, xcb_atom_t*, xcb_atom_t*, xcb_atom_t*, XtPointer*, unsigned long*, int*);

typedef struct _TextSrcClassPart {
    _IswSrcReadProc Read;
    _IswSrcReplaceProc Replace;
    _IswSrcScanProc Scan;
    _IswSrcSearchProc Search;
    _IswSrcSetSelectionProc SetSelection;
    _IswSrcConvertSelectionProc ConvertSelection;
} TextSrcClassPart;

/* Full class record declaration */
typedef struct _TextSrcClassRec {
    ObjectClassPart     object_class;
    TextSrcClassPart	textSrc_class;
} TextSrcClassRec;

extern TextSrcClassRec textSrcClassRec;

/* New fields for the TextSrc object record */
typedef struct {
    /* resources */
  IswTextEditType	edit_mode;
  XrmQuark		text_format;	/* 2 formats: FMT8BIT for Ascii */
					/*            FMTWIDE for ISO 10646 */
} TextSrcPart;

/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _TextSrcRec {
  ObjectPart    object;
  TextSrcPart	textSrc;
} TextSrcRec;

/******************************************************************
 *
 * Semiprivate declarations of functions used in other modules
 *
 ******************************************************************/

char* _ISWTextWCToMB(
    xcb_connection_t* /* d */,
    wchar_t* /* wstr */,
    int*     /* len_in_out */
);

wchar_t* _ISWTextMBToWC(
    xcb_connection_t*  /* d */,
    char*     /* str */,
    int*      /* len_in_out */
);

/************************************************************
 *
 * Private declarations.
 *
 ************************************************************/

#if 0	/* no longer used */
typedef ISWTextPosition (*_IswTextPositionFunc)();
#endif

#define XtInheritInput                ((_IswTextPositionFunc) _XtInherit)
#define XtInheritRead                 ((_IswSrcReadProc) _XtInherit)
#define XtInheritReplace              ((_IswSrcReplaceProc) _XtInherit)
#define XtInheritScan                 ((_IswSrcScanProc) _XtInherit)
#define XtInheritSearch               ((_IswSrcSearchProc) _XtInherit)
#define XtInheritSetSelection         ((_IswSrcSetSelectionProc) _XtInherit)
#define XtInheritConvertSelection     ((_IswSrcConvertSelectionProc) _XtInherit)
#define XtTextSrcExtVersion	      1
#define XtTextSrcExtTypeString        "XT_TEXTSRC_EXT"

#endif /* _ISW_IswTextSrcP_h */
