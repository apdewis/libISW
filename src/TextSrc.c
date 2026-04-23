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

*/

/*
 * TextSrc.c - UTF-8 text source for the Text widget.
 *
 * Single concrete class replacing the former abstract TextSrc plus its
 * AsciiSrc / MultiSrc subclasses. Applications use IswTextSource*
 * public functions; the AsciiSrc / MultiSrc class symbols survive as
 * deprecated aliases.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <ISW/StringDefs.h>
#include <X11/Xos.h>
#ifndef X_NOT_STDC_ENV
#include <stdlib.h>
#endif
#include <ISW/ISWInit.h>
#include <ISW/TextSrcP.h>
#include <ISW/ISWUtf8.h>
#include "ISWXcbDraw.h"
#include <string.h>

#ifdef L_tmpnam
#define TMPSIZ L_tmpnam
#else
#define TMPSIZ 32
#endif

#define MAGIC_VALUE ((ISWTextPosition) -1)
#define streq(a, b) (strcmp((a), (b)) == 0)

#if (defined(ASCII_STRING) || defined(ASCII_DISK))
#  include <ISW/AsciiText.h> /* for Widget Classes. */
#endif


/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

static int magic_value = MAGIC_VALUE;

#define offset(field) IswOffsetOf(TextSrcRec, text_src.field)

/* Edit-mode string→quark converter moved here from the old abstract TextSrc.c. */
static void CvtStringToEditMode(XrmValuePtr, Cardinal *, XrmValuePtr, XrmValuePtr);

static IswResource resources[] = {
    {IswNeditType, IswCEditType, IswREditMode, sizeof(IswTextEditType),
       offset(edit_mode), IswRString, "read"},
    {IswNstring, IswCString, IswRString, sizeof (char *),
       offset(string), IswRString, NULL},
    {IswNtype, IswCType, IswRAsciiType, sizeof (IswAsciiType),
       offset(type), IswRImmediate, (IswPointer)IswAsciiString},
    {IswNdataCompression, IswCDataCompression, IswRBoolean, sizeof (Boolean),
       offset(data_compression), IswRImmediate, (IswPointer) TRUE},
    {IswNpieceSize, IswCPieceSize, IswRInt, sizeof (ISWTextPosition),
       offset(piece_size), IswRImmediate, (IswPointer) BUFSIZ},
    {IswNcallback, IswCCallback, IswRCallback, sizeof(IswPointer),
       offset(callback), IswRCallback, (IswPointer)NULL},
    {IswNuseStringInPlace, IswCUseStringInPlace, IswRBoolean, sizeof (Boolean),
       offset(use_string_in_place), IswRImmediate, (IswPointer) FALSE},
    {IswNlength, IswCLength, IswRInt, sizeof (int),
       offset(ascii_length), IswRInt, (IswPointer) &magic_value},

#ifdef ASCII_DISK
    {IswNfile, IswCFile, IswRString, sizeof (String),
       offset(filename), IswRString, NULL},
#endif /* ASCII_DISK */
};
#undef offset

static ISWTextPosition Scan(Widget, ISWTextPosition, IswTextScanType,
                            IswTextScanDirection, int, Boolean);
static ISWTextPosition Search(Widget, ISWTextPosition, IswTextScanDirection,
                              ISWTextBlock *);
static ISWTextPosition ReadText(Widget, ISWTextPosition, ISWTextBlock *, int);
static int ReplaceText(Widget, ISWTextPosition, ISWTextPosition, ISWTextBlock *);
static Piece * FindPiece(TextSrcObject, ISWTextPosition, ISWTextPosition *);
static Piece * AllocNewPiece(TextSrcObject, Piece *);
static FILE * InitStringOrFile(TextSrcObject, Boolean);
static void FreeAllPieces(TextSrcObject);
static void RemovePiece(TextSrcObject, Piece *);
static void BreakPiece(TextSrcObject, Piece *);
static void LoadPieces(TextSrcObject, FILE *, char *);
static void RemoveOldStringOrFile(TextSrcObject, Boolean);
static void CvtStringToAsciiType(XrmValuePtr, Cardinal *, XrmValuePtr, XrmValuePtr);
static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void GetValuesHook(Widget, ArgList, Cardinal *);
static String MyStrncpy(char *, char *, int);
static char * StorePiecesInString(TextSrcObject);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean WriteToFile(_Xconst _IswString, _Xconst _IswString);
#ifdef X_NOT_STDC_ENV
extern int errno;
#endif

#ifdef X_NOT_POSIX
#define Off_t long
#define Size_t unsigned int
#else
#define Off_t off_t
#define Size_t size_t
#endif

/* SetSelection / ConvertSelection default stubs (formerly lived in the
 * abstract TextSrc class — inlined here since there is no abstract class
 * left to inherit from). */
/* ARGSUSED */
static void
DefaultSetSelection(Widget w, ISWTextPosition left, ISWTextPosition right,
                    xcb_atom_t selection)
{
  /* no-op */
}

/* ARGSUSED */
static Boolean
DefaultConvertSelection(Widget w, xcb_atom_t *selection, xcb_atom_t *target,
                        xcb_atom_t *type, IswPointer *value,
                        unsigned long *length, int *format)
{
  return FALSE;
}

#define superclass              (&objectClassRec)
TextSrcClassRec textSrcClassRec = {
  {
/* core_class fields */
    /* superclass               */      (WidgetClass) superclass,
    /* class_name               */      "TextSrc",
    /* widget_size              */      sizeof(TextSrcRec),
    /* class_initialize         */      ClassInitialize,
    /* class_part_initialize    */      NULL,
    /* class_inited             */      FALSE,
    /* initialize               */      Initialize,
    /* initialize_hook          */      NULL,
    /* realize                  */      NULL,
    /* actions                  */      NULL,
    /* num_actions              */      0,
    /* resources                */      resources,
    /* num_resources            */      IswNumber(resources),
    /* xrm_class                */      NULLQUARK,
    /* compress_motion          */      FALSE,
    /* compress_exposure        */      FALSE,
    /* compress_enterleave      */      FALSE,
    /* visible_interest         */      FALSE,
    /* destroy                  */      Destroy,
    /* resize                   */      NULL,
    /* expose                   */      NULL,
    /* set_values               */      SetValues,
    /* set_values_hook          */      NULL,
    /* set_values_almost        */      NULL,
    /* get_values_hook          */      GetValuesHook,
    /* accept_focus             */      NULL,
    /* version                  */      IswVersion,
    /* callback_private         */      NULL,
    /* tm_table                 */      NULL,
    /* query_geometry           */      NULL,
    /* display_accelerator      */      NULL,
    /* extension                */      NULL
  },
/* text_src_class fields */
  {
    /* Read                     */      ReadText,
    /* Replace                  */      ReplaceText,
    /* Scan                     */      Scan,
    /* Search                   */      Search,
    /* SetSelection             */      DefaultSetSelection,
    /* ConvertSelection         */      DefaultConvertSelection
  }
};

WidgetClass textSrcObjectClass = (WidgetClass)&textSrcClassRec;

/* Deprecated aliases: AsciiSrc / MultiSrc were the two legacy concrete
 * subclasses; both now resolve to the single TextSrc class. */
WidgetClass asciiSrcObjectClass = (WidgetClass)&textSrcClassRec;
WidgetClass multiSrcObjectClass = (WidgetClass)&textSrcClassRec;
TextSrcClassRec asciiSrcClassRec; /* unused, referenced by some externs */

/************************************************************
 *
 * Semi-Public Interfaces.
 *
 ************************************************************/

/*      Function Name: ClassInitialize
 *      Description: Class Initialize routine, called only once.
 *      Arguments: none.
 *      Returns: none.
 */

static void
ClassInitialize(void)
{
  IswInitializeWidgetSet();
  IswAddConverter( IswRString, IswRAsciiType, CvtStringToAsciiType,
		 NULL, (Cardinal) 0);
  IswAddConverter( IswRString, IswREditMode, CvtStringToEditMode,
                 NULL, (Cardinal) 0);
}

/* ARGSUSED */
static void
CvtStringToEditMode(XrmValuePtr args, Cardinal *num_args,
                    XrmValuePtr fromVal, XrmValuePtr toVal)
{
  static IswTextEditType editType;
  static XrmQuark QRead, QAppend, QEdit;
  XrmQuark q;
  char lowerName[40];
  static Boolean inited = FALSE;

  if (!inited) {
    QRead   = XrmPermStringToQuark(IswEtextRead);
    QAppend = XrmPermStringToQuark(IswEtextAppend);
    QEdit   = XrmPermStringToQuark(IswEtextEdit);
    inited = TRUE;
  }

  if (strlen((char*) fromVal->addr) < sizeof lowerName) {
    ISWCopyISOLatin1Lowered(lowerName, (char *)fromVal->addr);
    q = XrmStringToQuark(lowerName);

    if      (q == QRead)   editType = IswtextRead;
    else if (q == QAppend) editType = IswtextAppend;
    else if (q == QEdit)   editType = IswtextEdit;
    else {
      toVal->size = 0;
      toVal->addr = NULL;
      return;
    }
    toVal->size = sizeof editType;
    toVal->addr = (IswPointer) &editType;
    return;
  }
  toVal->size = 0;
  toVal->addr = NULL;
}

/*      Function Name: Initialize
 *      Description: Initializes the simple menu widget
 *      Arguments: request - the widget requested by the argument list.
 *                 new     - the new widget with both resource and non
 *                           resource values.
 *      Returns: none.
 */

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
  TextSrcObject src = (TextSrcObject) new;
  FILE * file;

/*
 * Set correct flags (override resources) depending upon widget class.
 */

  src->text_src.text_format = IswFmt8Bit;	/* data format. */

#ifdef ASCII_DISK
  if (IswIsSubclass(IswParent(new), asciiDiskWidgetClass)) {
    src->text_src.type = IswAsciiFile;
    src->text_src.string = src->text_src.filename;
  }
#endif

#ifdef ASCII_STRING
  if (IswIsSubclass(IswParent(new), asciiStringWidgetClass)) {
    src->text_src.use_string_in_place = TRUE;
    src->text_src.type = IswAsciiString;
  }
#endif

  src->text_src.changes = FALSE;
  src->text_src.allocated_string = FALSE;

  file = InitStringOrFile(src, src->text_src.type == IswAsciiFile);
  LoadPieces(src, file, NULL);

  if (file != NULL) fclose(file);
}

/*	Function Name: ReadText
 *	Description: This function reads the source.
 *	Arguments: w - the AsciiSource widget.
 *                 pos - position of the text to retreive.
 * RETURNED        text - text block that will contain returned text.
 *                 length - maximum number of characters to read.
 *	Returns: The number of characters read into the buffer.
 */

static ISWTextPosition
ReadText(Widget w, ISWTextPosition pos, ISWTextBlock *text, int length)
{
  TextSrcObject src = (TextSrcObject) w;
  ISWTextPosition count, start;
  Piece * piece = FindPiece(src, pos, &start);

  text->firstPos = pos;
  text->ptr = piece->text + (pos - start);
  count = piece->used - (pos - start);
  text->length = (length > count) ? count : length;
  return(pos + text->length);
}

/*	Function Name: ReplaceText.
 *	Description: Replaces a block of text with new text.
 *	Arguments: w - the AsciiSource widget.
 *                 startPos, endPos - ends of text that will be removed.
 *                 text - new text to be inserted into buffer at startPos.
 *	Returns: IswEditError or IswEditDone.
 */

/*ARGSUSED*/
static int
ReplaceText (Widget w, ISWTextPosition startPos, ISWTextPosition endPos,
             ISWTextBlock *text)
{
  TextSrcObject src = (TextSrcObject) w;
  Piece *start_piece, *end_piece, *temp_piece;
  ISWTextPosition start_first, end_first;
  int length, firstPos;

/*
 * Editing a read only source is not allowed.
 */

  if (src->text_src.edit_mode == IswtextRead)
    return(IswEditError);

  start_piece = FindPiece(src, startPos, &start_first);
  end_piece = FindPiece(src, endPos, &end_first);

  src->text_src.changes = TRUE; /* We have changed the buffer. */

/*
 * Remove Old Stuff.
 */

  if (start_piece != end_piece) {
    temp_piece = start_piece->next;

/*
 * If empty and not the only piece then remove it.
 */

    if ( ((start_piece->used = startPos - start_first) == 0) &&
	 !((start_piece->next == NULL) && (start_piece->prev == NULL)) )
      RemovePiece(src, start_piece);

    while (temp_piece != end_piece) {
      temp_piece = temp_piece->next;
      RemovePiece(src, temp_piece->prev);
    }
    end_piece->used -= endPos - end_first;
    if (end_piece->used != 0)
      MyStrncpy(end_piece->text, (end_piece->text + endPos - end_first),
		(int) end_piece->used);
  }
  else {			/* We are fully in one piece. */
    if ( (start_piece->used -= endPos - startPos) == 0) {
      if ( !((start_piece->next == NULL) && (start_piece->prev == NULL)) )
	RemovePiece(src, start_piece);
    }
    else {
      MyStrncpy(start_piece->text + (startPos - start_first),
		start_piece->text + (endPos - start_first),
		(int) (start_piece->used - (startPos - start_first)) );
      if ( src->text_src.use_string_in_place &&
	   ((src->text_src.length - (endPos - startPos)) <
	    (src->text_src.piece_size - 1)) )
	start_piece->text[src->text_src.length - (endPos - startPos)] = '\0';
    }
  }

  src->text_src.length += -(endPos - startPos) + text->length;

  if ( text->length != 0) {

    /*
     * Put in the New Stuff.
     */

    start_piece = FindPiece(src, startPos, &start_first);

    length = text->length;
    firstPos = text->firstPos;

    while (length > 0) {
      char * ptr;
      int fill;

      if (src->text_src.use_string_in_place) {
	if (start_piece->used == (src->text_src.piece_size - 1)) {
	  /*
	   * If we are in ascii string emulation mode. Then the
	   *  string is not allowed to grow.
	   */
	  start_piece->used = src->text_src.length =
	                                         src->text_src.piece_size - 1;
	  start_piece->text[src->text_src.length] = '\0';
	  return(IswEditError);
	}
      }


      if (start_piece->used == src->text_src.piece_size) {
	BreakPiece(src, start_piece);
	start_piece = FindPiece(src, startPos, &start_first);
      }

      fill = IswMin((int)(src->text_src.piece_size - start_piece->used), length);

      ptr = start_piece->text + (startPos - start_first);
      MyStrncpy(ptr + fill, ptr,
		(int) start_piece->used - (startPos - start_first));
      strncpy(ptr, text->ptr + firstPos, fill);

      startPos += fill;
      firstPos += fill;
      start_piece->used += fill;
      length -= fill;
    }
  }

  if (src->text_src.use_string_in_place)
    start_piece->text[start_piece->used] = '\0';

  IswCallCallbacks(w, IswNcallback, NULL); /* Call callbacks, we have changed
					    the buffer. */

  return(IswEditDone);
}

/*	Function Name: Scan
 *	Description: Scans the text source for the number and type
 *                   of item specified.
 *	Arguments: w - the AsciiSource widget.
 *                 position - the position to start scanning.
 *                 type - type of thing to scan for.
 *                 dir - direction to scan.
 *                 count - which occurance if this thing to search for.
 *                 include - whether or not to include the character found in
 *                           the position that is returned.
 *	Returns: the position of the item found.
 *
 * Note: While there are only 'n' characters in the file there are n+1
 *       possible cursor positions (one before the first character and
 *       one after the last character.
 */

static
ISWTextPosition
Scan (Widget w, ISWTextPosition position, IswTextScanType type,
      IswTextScanDirection dir, int count, Boolean include)
{
  TextSrcObject src = (TextSrcObject) w;
  int inc;
  Piece* piece;
  ISWTextPosition first, first_eol_position = 0;
  char* ptr;

  if (type == IswstAll) {	/* Optomize this common case. */
    if (dir == IswsdRight)
      return(src->text_src.length);
    return(0);			/* else. */
  }

  if (position > src->text_src.length)
    position = src->text_src.length;

  if ( dir == IswsdRight ) {
    if (position == src->text_src.length)
/*
 * Scanning right from src->text_src.length???
 */
      return(src->text_src.length);
    inc = 1;
  }
  else {
    if (position == 0)
      return(0);		/* Scanning left from 0??? */
    inc = -1;
    position--;
  }

  piece = FindPiece(src, position, &first);

/*
 * If the buffer is empty then return 0.
 */

  if ( piece->used == 0 ) return(0);

  ptr = (position - first) + piece->text;

  switch (type) {
  case IswstEOL:
  case IswstParagraph:
  case IswstWhiteSpace:
    for ( ; count > 0 ; count-- ) {
      Boolean non_space = FALSE, first_eol = TRUE;
      /* CONSTCOND */
      while (TRUE) {
	unsigned char c = *ptr;

	ptr += inc;
	position += inc;

	if (type == IswstWhiteSpace) {
	  if (isspace(c)) {
	    if (non_space)
	      break;
	  }
	  else
	    non_space = TRUE;
	}
	else if (type == IswstEOL) {
	  if (c == '\n') break;
	}
	else { /* IswstParagraph */
	  if (first_eol) {
	    if (c == '\n') {
	      first_eol_position = position;
	      first_eol = FALSE;
	    }
	  }
	  else
	    if ( c == '\n')
	      break;
	    else if ( !isspace(c) )
	      first_eol = TRUE;
	}


	if ( ptr < piece->text ) {
	  piece = piece->prev;
	  if (piece == NULL)	/* Begining of text. */
	    return(0);
	  ptr = piece->text + piece->used - 1;
	}
	else if ( ptr >= (piece->text + piece->used) ) {
	  piece = piece->next;
	  if (piece == NULL)	/* End of text. */
	    return(src->text_src.length);
	  ptr = piece->text;
	}
      }
    }
    if (!include) {
      if ( type == IswstParagraph)
	position = first_eol_position;
      position -= inc;
    }
    break;
  case IswstPositions:
    /* Step by codepoints, not bytes. Walk piece-by-piece. */
    {
      ISWTextPosition max_pos = src->text_src.length;
      if (dir == IswsdRight) {
        /* We started with position pointing at the codepoint to consume.
         * Skip `count` codepoints forward. */
        for (int steps = 0; steps < count && position < max_pos; steps++) {
          int rem_in_piece = (int)(piece->used - (ptr - piece->text));
          int n;
          if (rem_in_piece >= 4) {
            n = _IswUtf8CharLen(ptr, rem_in_piece);
          } else {
            /* Codepoint may straddle a piece boundary — copy up to 4 bytes
             * across pieces and decode. */
            unsigned char tmp[4];
            int k = 0;
            Piece *p2 = piece;
            char *q = ptr;
            while (k < 4 && p2) {
              if (q >= p2->text + p2->used) {
                p2 = p2->next;
                if (!p2) break;
                q = p2->text;
                continue;
              }
              tmp[k++] = (unsigned char)*q++;
            }
            n = _IswUtf8CharLen((char *)tmp, k);
          }
          if (n <= 0) n = 1;
          position += n;
          ptr += n;
          while (piece && ptr >= piece->text + piece->used) {
            int over = (int)(ptr - (piece->text + piece->used));
            piece = piece->next;
            if (!piece) break;
            ptr = piece->text + over;
          }
        }
      } else {
        /* Leftward. On entry the caller has already done `position--`
         * (so `position` is one less than the caret) and the post-switch
         * `position++` below will add one back. Net: we need to leave
         * `position` pointing one byte BEFORE the lead byte of the
         * target codepoint. Walk back `count` codepoints to the lead
         * byte, then back up one more byte so the `+1` compensation
         * yields the lead-byte offset. */
        for (int steps = 0; steps < count; steps++) {
          /* At entry to each step, `ptr`/`position` sit on some byte
           * inside (or just past) the current codepoint. Back up while
           * we're on a continuation byte; stop on the first lead byte. */
          int scanned = 0;
          while (scanned < 4 && _IswUtf8IsCont((unsigned char)*ptr)) {
            if (position <= 0) { position = -1; goto done_left; }
            position--;
            if (ptr == piece->text) {
              piece = piece->prev;
              if (!piece) { position = -1; goto done_left; }
              ptr = piece->text + piece->used - 1;
            } else {
              ptr--;
            }
            scanned++;
          }
          /* `position` is now the byte offset of a lead byte. For every
           * step except the last, advance past it so the next iteration
           * starts looking at the byte to its left. */
          if (steps + 1 < count) {
            if (position <= 0) { position = -1; goto done_left; }
            position--;
            if (ptr == piece->text) {
              piece = piece->prev;
              if (!piece) { position = -1; goto done_left; }
              ptr = piece->text + piece->used - 1;
            } else {
              ptr--;
            }
          }
        }
        /* Final `position` is the lead-byte offset of the target
         * codepoint. The post-switch `position++` would turn that into
         * lead+1, which is wrong — back up one more so the `+1` cancels. */
        if (position > 0) position--;
      done_left: ;
      }
    }
    break;
  case IswstAll:		/* handled in special code above */
  default:
    break;
  }

  if ( dir == IswsdLeft )
    position++;

  if (position >= src->text_src.length)
    return(src->text_src.length);
  if (position < 0)
    return(0);

  return(position);
}

/*	Function Name: Search
 *	Description: Searchs the text source for the text block passed
 *	Arguments: w - the AsciiSource Widget.
 *                 position - the position to start scanning.
 *                 dir - direction to scan.
 *                 text - the text block to search for.
 *	Returns: the position of the item found.
 */

static ISWTextPosition
Search(Widget w, ISWTextPosition position, IswTextScanDirection dir,
       ISWTextBlock *text)
{
  TextSrcObject src = (TextSrcObject) w;
  int inc, count = 0;
  char * ptr;
  Piece * piece;
  char * buf;
  ISWTextPosition first;

  if ( dir == IswsdRight )
    inc = 1;
  else {
    inc = -1;
    if (position == 0)
      return(IswTextSearchError);	/* scanning left from 0??? */
    position--;
  }

  buf = IswMalloc((unsigned)sizeof(unsigned char) * text->length);
  strncpy(buf, (text->ptr + text->firstPos), text->length);
  piece = FindPiece(src, position, &first);
  ptr = (position - first) + piece->text;

  /* CONSTCOND */
  while (TRUE) {
    if (*ptr == ((dir == IswsdRight) ? *(buf + count)
		                     : *(buf + text->length - count - 1)) ) {
      if (count == (text->length - 1))
	break;
      else
	count++;
    }
    else {
      if (count != 0) {
	position -=inc * count;
	ptr -= inc * count;
      }
      count = 0;
    }

    ptr += inc;
    position += inc;

    while ( ptr < piece->text ) {
      piece = piece->prev;
      if (piece == NULL) {	/* Begining of text. */
	IswFree(buf);
	return(IswTextSearchError);
      }
      ptr = piece->text + piece->used - 1;
    }

    while ( ptr >= (piece->text + piece->used) ) {
      piece = piece->next;
      if (piece == NULL) {	/* End of text. */
	IswFree(buf);
	return(IswTextSearchError);
      }
      ptr = piece->text;
    }
  }

  IswFree(buf);
  if (dir == IswsdLeft)
    return(position);
  return(position - (text->length - 1));
}

/*	Function Name: SetValues
 *	Description: Sets the values for the AsciiSource.
 *	Arguments: current - current state of the widget.
 *                 request - what was requested.
 *                 new - what the widget will become.
 *	Returns: True if redisplay is needed.
 */

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args,
          Cardinal * num_args)
{
  TextSrcObject src =      (TextSrcObject) new;
  TextSrcObject old_src = (TextSrcObject) current;
  Boolean total_reset = FALSE, string_set = FALSE;
  FILE * file;
  int i;

  if ( old_src->text_src.use_string_in_place !=
       src->text_src.use_string_in_place ) {
      IswAppWarning( IswWidgetToApplicationContext(new),
	   "AsciiSrc: The IswNuseStringInPlace resource may not be changed.");
       src->text_src.use_string_in_place =
	   old_src->text_src.use_string_in_place;
  }

  for (i = 0; i < *num_args ; i++ )
      if (streq(args[i].name, IswNstring)) {
	  string_set = TRUE;
	  break;
      }

  if ( string_set || (old_src->text_src.type != src->text_src.type) ) {
    RemoveOldStringOrFile(old_src, string_set); /* remove old info. */
    file = InitStringOrFile(src, string_set);	/* Init new info. */
    LoadPieces(src, file, NULL);    /* load new info into internal buffers. */
    if (file != NULL) fclose(file);
    IswTextSetSource( IswParent(new), new, 0);   /* Tell text widget
						   what happened. */
    total_reset = TRUE;
  }

  if ( old_src->text_src.ascii_length != src->text_src.ascii_length )
      src->text_src.piece_size = src->text_src.ascii_length;

  if ( !total_reset &&
      (old_src->text_src.piece_size != src->text_src.piece_size) ) {
      String string = StorePiecesInString(old_src);
      FreeAllPieces(old_src);
      LoadPieces(src, NULL, string);
      IswFree(string);
  }

  return(FALSE);
}

/*	Function Name: GetValuesHook
 *	Description: This is a get values hook routine that sets the
 *                   values specific to the ascii source.
 *	Arguments: w - the AsciiSource Widget.
 *                 args - the argument list.
 *                 num_args - the number of args.
 *	Returns: none.
 */

static void
GetValuesHook(Widget w, ArgList args, Cardinal * num_args)
{
  TextSrcObject src = (TextSrcObject) w;
  int i;

  if (src->text_src.type == IswAsciiString) {
    for (i = 0; i < *num_args ; i++ )
      if (streq(args[i].name, IswNstring)) {
	  if (src->text_src.use_string_in_place) {
	      *((char **) args[i].value) = src->text_src.first_piece->text;
	  }
	  else {
	      if (IswAsciiSave(w))	/* If save sucessful. */
		  *((char **) args[i].value) = src->text_src.string;
	  }
	break;
      }
  }
}

/*	Function Name: Destroy
 *	Description: Destroys an ascii source (frees all data)
 *	Arguments: src - the Ascii source Widget to free.
 *	Returns: none.
 */

static void
Destroy (Widget w)
{
  RemoveOldStringOrFile((TextSrcObject) w, True);
}

/************************************************************
 *
 * Public routines
 *
 ************************************************************/

/*	Function Name: IswAsciiSourceFreeString
 *	Description: Frees the string returned by a get values call
 *                   on the string when the source is of type string.
 *	Arguments: w - the AsciiSrc widget.
 *	Returns: none.
 */

void
IswAsciiSourceFreeString(Widget w)
{
  TextSrcObject src = (TextSrcObject) w;

  if ( !IswIsSubclass( w, asciiSrcObjectClass ) ) {
      IswErrorMsg("bad argument", "asciiSource", "IswError",
            "IswAsciiSourceFreeString's parameter must be an asciiSrc.",
	     NULL, NULL);
  }

  if (src->text_src.allocated_string && src->text_src.type != IswAsciiFile) {
    src->text_src.allocated_string = FALSE;
    IswFree(src->text_src.string);
    src->text_src.string = NULL;
  }
}

/*	Function Name: IswAsciiSave
 *	Description: Saves all the pieces into a file or string as required.
 *	Arguments: w - the asciiSrc Widget.
 *	Returns: TRUE if the save was successful.
 */

Boolean
IswAsciiSave(Widget w)
{
  TextSrcObject src = (TextSrcObject) w;

  if ( !IswIsSubclass( w, asciiSrcObjectClass ) ) {
      	IswErrorMsg("bad argument", "asciiSource", "IswError",
		"IswAsciiSave's parameter must be an asciiSrc.",
		   NULL, NULL);
  }

/*
 * If using the string in place then there is no need to play games
 * to get the internal info into a readable string.
 */

  if (src->text_src.use_string_in_place)
    return(TRUE);

  if (src->text_src.type == IswAsciiFile) {
    char * string;

    if (!src->text_src.changes) 		/* No changes to save. */
      return(TRUE);

    string = StorePiecesInString(src);

    if (WriteToFile(string, src->text_src.string) == FALSE) {
      IswFree(string);
      return(FALSE);
    }
    IswFree(string);
  }
  else {
    if (src->text_src.allocated_string == TRUE)
      IswFree(src->text_src.string);
    else
      src->text_src.allocated_string = TRUE;

    src->text_src.string = StorePiecesInString(src);
  }
  src->text_src.changes = FALSE;
  return(TRUE);
}

/*	Function Name: IswAsciiSaveAsFile
 *	Description: Save the current buffer as a file.
 *	Arguments: w - the AsciiSrc widget.
 *                 name - name of the file to save this file into.
 *	Returns: True if the save was sucessful.
 */

Boolean
IswAsciiSaveAsFile(Widget w, _Xconst char* name)
{
  TextSrcObject src = (TextSrcObject) w;
  String string;
  Boolean ret;

  if ( !IswIsSubclass( w, asciiSrcObjectClass ) ) {
      	IswErrorMsg("bad argument", "asciiSource", "IswError",
		"IswAsciiSaveAsFile's 1st parameter must be an asciiSrc.",
		   NULL, NULL);
  }

  string = StorePiecesInString(src);

  ret = WriteToFile(string, name);
  IswFree(string);
  return(ret);
}

/*	Function Name: IswAsciiSourceChanged
 *	Description: Returns true if the source has changed since last saved.
 *	Arguments: w - the ascii source widget.
 *	Returns: a Boolean (see description).
 */

Boolean
IswAsciiSourceChanged(Widget w)
{
  if ( IswIsSubclass( w, asciiSrcObjectClass ) )
      return( ( (TextSrcObject) w)->text_src.changes );

  IswErrorMsg("bad argument", "asciiSource", "IswError",
		"IswAsciiSourceChanged parameter must be an asciiSrc.",
		   NULL, NULL);

  return( True ); /* for gcc -Wall */
}

/************************************************************
 *
 * TextSource public dispatch API.
 * Previously lived in the abstract TextSrc class; kept because application
 * code calls these directly, and internal callers rely on the vtable
 * indirection even though there is now only one concrete class.
 *
 ************************************************************/

ISWTextPosition
IswTextSourceRead(Widget w, ISWTextPosition pos, ISWTextBlock *text, int length)
{
  TextSrcObjectClass class = (TextSrcObjectClass) w->core.widget_class;

  if (!IswIsSubclass(w, textSrcObjectClass))
      IswErrorMsg("bad argument", "textSource", "IswError",
                  "IswTextSourceRead's 1st parameter must be a textSrc.",
                  NULL, NULL);

  return (*class->text_src_class.Read)(w, pos, text, length);
}

/*ARGSUSED*/
int
IswTextSourceReplace(Widget w, ISWTextPosition startPos,
                     ISWTextPosition endPos, ISWTextBlock *text)
{
  TextSrcObjectClass class = (TextSrcObjectClass) w->core.widget_class;

  if (!IswIsSubclass(w, textSrcObjectClass))
      IswErrorMsg("bad argument", "textSource", "IswError",
                  "IswTextSourceReplace's 1st parameter must be a textSrc.",
                  NULL, NULL);

  return (*class->text_src_class.Replace)(w, startPos, endPos, text);
}

ISWTextPosition
IswTextSourceScan(Widget w, ISWTextPosition position,
#if NeedWidePrototypes
                  int type, int dir,
#else
                  IswTextScanType type, IswTextScanDirection dir,
#endif
                  int count,
#if NeedWidePrototypes
                  int include)
#else
                  Boolean include)
#endif
{
  TextSrcObjectClass class = (TextSrcObjectClass) w->core.widget_class;

  if (!IswIsSubclass(w, textSrcObjectClass))
      IswErrorMsg("bad argument", "textSource", "IswError",
                  "IswTextSourceScan's 1st parameter must be a textSrc.",
                  NULL, NULL);

  return (*class->text_src_class.Scan)(w, position, type, dir, count, include);
}

ISWTextPosition
IswTextSourceSearch(Widget w, ISWTextPosition position,
#if NeedWidePrototypes
                    int dir,
#else
                    IswTextScanDirection dir,
#endif
                    ISWTextBlock *text)
{
  TextSrcObjectClass class = (TextSrcObjectClass) w->core.widget_class;

  if (!IswIsSubclass(w, textSrcObjectClass))
      IswErrorMsg("bad argument", "textSource", "IswError",
                  "IswTextSourceSearch's 1st parameter must be a textSrc.",
                  NULL, NULL);

  return (*class->text_src_class.Search)(w, position, dir, text);
}

Boolean
IswTextSourceConvertSelection(Widget w, xcb_atom_t *selection, xcb_atom_t *target,
                              xcb_atom_t *type, IswPointer *value,
                              unsigned long *length, int *format)
{
  TextSrcObjectClass class = (TextSrcObjectClass) w->core.widget_class;

  if (!IswIsSubclass(w, textSrcObjectClass))
      IswErrorMsg("bad argument", "textSource", "IswError",
                  "IswTextSourceConvertSelection's 1st parameter must be a textSrc.",
                  NULL, NULL);

  return (*class->text_src_class.ConvertSelection)(w, selection, target, type,
                                                   value, length, format);
}

void
IswTextSourceSetSelection(Widget w, ISWTextPosition left,
                          ISWTextPosition right, xcb_atom_t selection)
{
  TextSrcObjectClass class = (TextSrcObjectClass) w->core.widget_class;

  if (!IswIsSubclass(w, textSrcObjectClass))
      IswErrorMsg("bad argument", "textSource", "IswError",
                  "IswTextSourceSetSelection's 1st parameter must be a textSrc.",
                  NULL, NULL);

  (*class->text_src_class.SetSelection)(w, left, right, selection);
}

/*	TextFormat():
 *	  returns the format quark of the source text. Always FMT8BIT now
 *	  (the former FMTWIDE path was tied to MultiSrc, which is gone). */
XrmQuark
_IswTextFormat(TextWidget tw)
{
  return ((TextSrcObject)(tw->text.source))->text_src.text_format;
}

/* Aliases for the legacy Ascii-named public functions so new code can use
 * the preferred names. */
void    IswTextSourceFreeString(Widget w)                 { IswAsciiSourceFreeString(w); }
Boolean IswTextSourceSave(Widget w)                        { return IswAsciiSave(w); }
Boolean IswTextSourceSaveAsFile(Widget w, _Xconst char *n) { return IswAsciiSaveAsFile(w, n); }
Boolean IswTextSourceChanged(Widget w)                     { return IswAsciiSourceChanged(w); }

/************************************************************
 *
 * Private Functions.
 *
 ************************************************************/

static void
RemoveOldStringOrFile(TextSrcObject src, Boolean checkString)
{
  FreeAllPieces(src);

  if (checkString && src->text_src.allocated_string) {
    IswFree(src->text_src.string);
    src->text_src.allocated_string = False;
    src->text_src.string = NULL;
  }
}

/*	Function Name: WriteToFile
 *	Description: Write the string specified to the begining of the file
 *                   specified.
 *	Arguments: string - string to write.
 *                 name - the name of the file
 *	Returns: returns TRUE if sucessful, FALSE otherwise.
 */

static Boolean
WriteToFile(_Xconst _IswString string, _Xconst _IswString name)
{
  int fd;

  if ( ((fd = creat(name, 0666)) == -1 ) ||
       (write(fd, string, sizeof(unsigned char) * strlen(string)) == -1) )
    return(FALSE);

  if ( close(fd) == -1 )
    return(FALSE);

  return(TRUE);
}

/*	Function Name: StorePiecesInString
 *	Description: store the pieces in memory into a standard ascii string.
 *	Arguments: data - the ascii pointer data.
 *	Returns: none.
 */

static char *
StorePiecesInString(TextSrcObject src)
{
  char *string;
  ISWTextPosition first;
  Piece * piece;

  string = IswMalloc((unsigned) sizeof(unsigned char) *
		    src->text_src.length + 1);

  for (first = 0, piece = src->text_src.first_piece ; piece != NULL;
       first += piece->used, piece = piece->next)
    strncpy(string + first, piece->text, piece->used);

  string[src->text_src.length] = '\0';	/* NULL terminate this sucker. */

/*
 * This will refill all pieces to capacity.
 */

  if (src->text_src.data_compression) {
    FreeAllPieces(src);
    LoadPieces(src, NULL, string);
  }

  return(string);
}

/*	Function Name: InitStringOrFile.
 *	Description: Initializes the string or file.
 *	Arguments: src - the AsciiSource.
 *	Returns: none - May exit though.
 */

static FILE *
InitStringOrFile(TextSrcObject src, Boolean newString)
{
    char * open_mode = NULL;
    FILE * file;
    char fileName[TMPSIZ];

    if (src->text_src.type == IswAsciiString) {

	if (src->text_src.string == NULL)
	    src->text_src.length = 0;

	else if (! src->text_src.use_string_in_place) {
	    src->text_src.string = IswNewString(src->text_src.string);
	    src->text_src.allocated_string = True;
	    src->text_src.length = strlen(src->text_src.string);
	}

	if (src->text_src.use_string_in_place) {
	    src->text_src.length = strlen(src->text_src.string);
	    /* In case the length resource is incorrectly set */
	    if (src->text_src.length > src->text_src.ascii_length)
		src->text_src.ascii_length = src->text_src.length;

	    if (src->text_src.ascii_length == MAGIC_VALUE)
		src->text_src.piece_size = src->text_src.length;
	    else
		src->text_src.piece_size = src->text_src.ascii_length + 1;
	}

	return(NULL);
    }

/*
 * type is IswAsciiFile.
 */

    src->text_src.is_tempfile = FALSE;

    switch (src->text_src.edit_mode) {
    case IswtextRead:
	if (src->text_src.string == NULL)
	    IswErrorMsg("NoFile", "asciiSourceCreate", "IswError",
		     "Creating a read only disk widget and no file specified.",
		       NULL, 0);
	open_mode = "r";
	break;
    case IswtextAppend:
    case IswtextEdit:
	if (src->text_src.string == NULL) {
	    src->text_src.string = fileName;
	    (void) tmpnam(src->text_src.string);
	    src->text_src.is_tempfile = TRUE;
	    open_mode = "w";
	} else
	    open_mode = "r+";
	break;
    default:
	IswErrorMsg("badMode", "asciiSourceCreate", "IswError",
		"Bad editMode for ascii source; must be Read, Append or Edit.",
		   NULL, NULL);
    }

    /* Allocate new memory for the temp filename, because it is held in
     * a stack variable, not static memory.  This widget does not need
     * to keep the private state field is_tempfile -- it is only accessed
     * in this routine, and its former setting is unused.
     */
    if (newString || src->text_src.is_tempfile) {
	src->text_src.string = IswNewString(src->text_src.string);
	src->text_src.allocated_string = TRUE;
    }

    if (!src->text_src.is_tempfile) {
	if ((file = fopen(src->text_src.string, open_mode)) != 0) {
	    (void) fseek(file, (Off_t)0, 2);
	    src->text_src.length = (ISWTextPosition) ftell(file);
	    return file;
	} else {
	    String params[2];
	    Cardinal num_params = 2;

	    params[0] = src->text_src.string;
	    params[1] = strerror(errno);
	    IswAppWarningMsg(IswWidgetToApplicationContext((Widget)src),
			    "openError", "asciiSourceCreate", "IswWarning",
			    "Cannot open file %s; %s", params, &num_params);
	}
    }
    src->text_src.length = 0;
    return((FILE *)NULL);
}

static void
LoadPieces(TextSrcObject src, FILE * file, char * string)
{
  char *local_str, *ptr;
  Piece * piece = NULL;
  ISWTextPosition left;

  if (string == NULL) {
    if (src->text_src.type == IswAsciiFile) {
      local_str = IswMalloc((unsigned) (src->text_src.length + 1)
			   * sizeof(unsigned char));
      if (src->text_src.length != 0) {
	fseek(file, (Off_t)0, 0);
	src->text_src.length = fread(local_str, (Size_t)sizeof(unsigned char),
				      (Size_t)src->text_src.length, file);
	if (src->text_src.length <= 0)
	  IswErrorMsg("readError", "asciiSourceCreate", "IswError",
		     "fread returned error.", NULL, NULL);
      }
      local_str[src->text_src.length] = '\0';
    }
    else
      local_str = src->text_src.string;
  }
  else
    local_str = string;

  if (src->text_src.use_string_in_place) {
    piece = AllocNewPiece(src, piece);
    piece->used = IswMin(src->text_src.length, src->text_src.piece_size);
    piece->text = src->text_src.string;
    return;
  }

  ptr = local_str;
  left = src->text_src.length;

  do {
    piece = AllocNewPiece(src, piece);

    piece->text = IswMalloc((unsigned)src->text_src.piece_size
			   * sizeof(unsigned char));
    piece->used = IswMin(left, src->text_src.piece_size);
    if (piece->used != 0)
      strncpy(piece->text, ptr, piece->used);

    left -= piece->used;
    ptr += piece->used;
  } while (left > 0);

  if ( (src->text_src.type == IswAsciiFile) && (string == NULL) )
    IswFree(local_str);
}

/*	Function Name: AllocNewPiece
 *	Description: Allocates a new piece of memory.
 *	Arguments: src - The AsciiSrc Widget.
 *                 prev - the piece just before this one, or NULL.
 *	Returns: the allocated piece.
 */

static Piece *
AllocNewPiece(TextSrcObject src, Piece * prev)
{
  Piece * piece = IswNew(Piece);

  if (prev == NULL) {
    src->text_src.first_piece = piece;
    piece->next = NULL;
  }
  else {
    if (prev->next != NULL)
      (prev->next)->prev = piece;
    piece->next = prev->next;
    prev->next = piece;
  }

  piece->prev = prev;

  return(piece);
}

/*	Function Name: FreeAllPieces
 *	Description: Frees all the pieces
 *	Arguments: src - The AsciiSrc Widget.
 *	Returns: none.
 */

static void
FreeAllPieces(TextSrcObject src)
{
  Piece * next, * first = src->text_src.first_piece;

  if (first->prev != NULL)
    (void) printf("Isw AsciiSrc Object: possible memory leak in FreeAllPieces().\n");

  for ( ; first != NULL ; first = next ) {
    next = first->next;
    RemovePiece(src, first);
  }
}

/*	Function Name: RemovePiece
 *	Description: Removes a piece from the list.
 *	Arguments:
 *                 piece - the piece to remove.
 *	Returns: none.
 */

static void
RemovePiece(TextSrcObject src, Piece * piece)
{
  if (piece->prev == NULL)
    src->text_src.first_piece = piece->next;
  else
    (piece->prev)->next = piece->next;

  if (piece->next != NULL)
    (piece->next)->prev = piece->prev;

  if (!src->text_src.use_string_in_place)
    IswFree(piece->text);

  IswFree((char *)piece);
}

/*	Function Name: FindPiece
 *	Description: Finds the piece containing the position indicated.
 *	Arguments: src - The AsciiSrc Widget.
 *                 position - the position that we are searching for.
 * RETURNED        first - the position of the first character in this piece.
 *	Returns: piece - the piece that contains this position.
 */

static Piece *
FindPiece(TextSrcObject src, ISWTextPosition position, ISWTextPosition * first)
{
  Piece * old_piece = NULL, * piece = src->text_src.first_piece;
  ISWTextPosition temp;

  for ( temp = 0 ; piece != NULL ; temp += piece->used, piece = piece->next ) {
    *first = temp;
    old_piece = piece;

    if ((temp + piece->used) > position)
      return(piece);
  }
  return(old_piece);	  /* if we run off the end the return the last piece */
}

/*	Function Name: MyStrncpy
 *	Description: Just like string copy, but slower and will always
 *                   work on overlapping strings.
 *	Arguments: (same as strncpy) - s1, s2 - strings to copy (2->1).
 *                  n - the number of chars to copy.
 *	Returns: s1.
 */

static String
MyStrncpy(char * s1, char * s2, int n)
{
  char buf[256];
  char* temp;

  if (n == 0) return s1;

  if (n < sizeof buf) temp = buf;
  else temp = IswMalloc((unsigned)sizeof(unsigned char) * n);

  strncpy(temp, s2, n);		/* Saber has a bug that causes it to generate*/
  strncpy(s1, temp, n);		/* a bogus warning message here (CDP 6/32/89)*/

  if (temp != buf) IswFree(temp);
  return s1;
}

/*	Function Name: BreakPiece
 *	Description: Breaks a full piece into two new pieces.
 *	Arguments: src - The AsciiSrc Widget.
 *                 piece - the piece to break.
 *	Returns: none.
 */

#define HALF_PIECE (src->text_src.piece_size/2)

static void
BreakPiece(TextSrcObject src, Piece * piece)
{
  Piece * new = AllocNewPiece(src, piece);

  new->text = IswMalloc(src->text_src.piece_size * sizeof(unsigned char));
  strncpy(new->text, piece->text + HALF_PIECE,
	  src->text_src.piece_size - HALF_PIECE);
  piece->used = HALF_PIECE;
  new->used = src->text_src.piece_size - HALF_PIECE;
}

/* ARGSUSED */
static void
CvtStringToAsciiType(XrmValuePtr args, Cardinal * num_args, XrmValuePtr fromVal,
                     XrmValuePtr toVal)
{
  static IswAsciiType type;
  static XrmQuark  IswQEstring = NULLQUARK;
  static XrmQuark  IswQEfile;
  XrmQuark q;
  char lowerName[40];

  if (IswQEstring == NULLQUARK) {
    IswQEstring = XrmPermStringToQuark(IswEstring);
    IswQEfile   = XrmPermStringToQuark(IswEfile);
  }

  if (strlen ((char*)fromVal->addr) < sizeof lowerName) {
    ISWCopyISOLatin1Lowered(lowerName, (char *) fromVal->addr);
    q = XrmStringToQuark(lowerName);

    if (q == IswQEstring)     type = IswAsciiString;
    else if (q == IswQEfile)  type = IswAsciiFile;
    else {
      toVal->size = 0;
      toVal->addr = NULL;
      return;
    }
    toVal->size = sizeof type;
    toVal->addr = (IswPointer) &type;
    return;
  }
  toVal->size = 0;
  toVal->addr = NULL;
}

#if (defined(ASCII_STRING) || defined(ASCII_DISK))
#  include <ISW/Cardinals.h>
#endif

#ifdef ASCII_STRING
/************************************************************
 *
 * Compatability functions.
 *
 ************************************************************/

/*	Function Name: AsciiStringSourceCreate
 *	Description: Creates a string source.
 *	Arguments: parent - the widget that will own this source.
 *                 args, num_args - the argument list.
 *	Returns: a pointer to the new text source.
 */

Widget
IswStringSourceCreate(Widget parent, ArgList args, Cardinal num_args)
{
  IswTextSource src;
  ArgList ascii_args;
  Arg temp[2];

  IswSetArg(temp[0], IswNtype, IswAsciiString);
  IswSetArg(temp[1], IswNuseStringInPlace, TRUE);
  ascii_args = IswMergeArgLists(temp, TWO, args, num_args);

  src = IswCreateWidget("genericAsciiString", asciiSrcObjectClass, parent,
		       ascii_args, num_args + TWO);
  IswFree((char *)ascii_args);
  return(src);
}

/*
 * This is hacked up to try to emulate old functionality, it
 * may not work, as I have not old code to test it on.
 *
 * Chris D. Peterson  8/31/89.
 */

void
IswTextSetLastPos (Widget w, ISWTextPosition lastPos)
{
  TextSrcObject src = (TextSrcObject) IswTextGetSource(w);

  src->text_src.piece_size = lastPos;
}
#endif /* ASCII_STRING */

#ifdef ASCII_DISK
/*	Function Name: AsciiDiskSourceCreate
 *	Description: Creates a disk source.
 *	Arguments: parent - the widget that will own this source.
 *                 args, num_args - the argument list.
 *	Returns: a pointer to the new text source.
 */

Widget
IswDiskSourceCreate(Widget parent, ArgList args, Cardinal num_args)
{
  IswTextSource src;
  ArgList ascii_args;
  Arg temp[1];
  int i;

  IswSetArg(temp[0], IswNtype, IswAsciiFile);
  ascii_args = IswMergeArgLists(temp, ONE, args, num_args);
  num_args++;

  for (i = 0; i < num_args; i++)
    if (streq(ascii_args[i].name, IswNfile) ||
	          streq(ascii_args[i].name, IswCFile))
      ascii_args[i].name = IswNstring;

  src = IswCreateWidget("genericAsciiDisk", asciiSrcObjectClass, parent,
		       ascii_args, num_args);
  IswFree((char *)ascii_args);
  return(src);
}
#endif /* ASCII_DISK */
