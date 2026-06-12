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
 * TextSrcP.h - Private definitions for TextSrc object
 *
 * Single concrete UTF-8 text source.
 */

#ifndef _ISW_IswTextSrcP_h
#define _ISW_IswTextSrcP_h


#include <ISW/TextSrc.h>
#include <ISW/TextP.h>

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
     (Widget, ISWTextPosition, ISWTextPosition, IswSelectionId);

typedef Boolean (*_IswSrcConvertSelectionProc)
     (Widget, IswSelectionId*, IswSelectionId*, IswSelectionId*, IswPointer*, unsigned long*, int*);

typedef struct _TextSrcClassPart {
    _IswSrcReadProc Read;
    _IswSrcReplaceProc Replace;
    _IswSrcScanProc Scan;
    _IswSrcSearchProc Search;
    _IswSrcSetSelectionProc SetSelection;
    _IswSrcConvertSelectionProc ConvertSelection;
} TextSrcClassPart;

typedef struct _TextSrcClassRec {
    ObjectClassPart  object_class;
    TextSrcClassPart text_src_class;
} TextSrcClassRec;

extern TextSrcClassRec textSrcClassRec;

/* Piece of the text buffer (used by the piece-table storage). */
typedef struct _Piece {
  char           *text;
  ISWTextPosition used;
  struct _Piece  *prev, *next;
} Piece;

/* Instance struct — source state for the unified concrete TextSrc. */
typedef struct _TextSrcPart {
    /* resources */
    IswTextEditType   edit_mode;
    XrmQuark          text_format;  /* always FMT8BIT now; kept for compat */

    char             *string;       /* either the string or the file name */
    IswTextSourceType type;
    ISWTextPosition  piece_size;
    Boolean          data_compression;
    IswCallbackList  callback;
    Boolean          use_string_in_place;
    int              text_length;

    /* private state */
    Boolean          is_tempfile;
    Boolean          changes;
    Boolean          allocated_string;
    ISWTextPosition  length;
    Piece           *first_piece;
} TextSrcPart;

typedef struct _TextSrcRec {
  ObjectPart  object;
  TextSrcPart text_src;
} TextSrcRec;

/* Legacy inherit sentinels kept for ABI-compat; with a single concrete
 * class nothing resolves through them. */
#define IswInheritRead             ((_IswSrcReadProc) _IswInherit)
#define IswInheritReplace          ((_IswSrcReplaceProc) _IswInherit)
#define IswInheritScan             ((_IswSrcScanProc) _IswInherit)
#define IswInheritSearch           ((_IswSrcSearchProc) _IswInherit)
#define IswInheritSetSelection     ((_IswSrcSetSelectionProc) _IswInherit)
#define IswInheritConvertSelection ((_IswSrcConvertSelectionProc) _IswInherit)

#define IswTextSrcExtVersion       1
#define IswTextSrcExtTypeString    "ISW_TEXTSRC_EXT"

#endif /* _ISW_IswTextSrcP_h */
