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

#include "ISWXcbDraw.h"
#include <ISW/Intrinsic.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include "ISWXcbDraw.h"
#include <ISW/TextP.h>
#include <ISW/ISWImP.h>
#include <ISW/IswArgMacros.h>
#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* XCB doesn't have these Xlib constants */
#ifndef Success
#define Success 0
#endif

#define SrcScan                IswTextSourceScan
#define FindDist               IswTextSinkFindDistance
#define FindPos                IswTextSinkFindPosition

#define IswTextActionMaxHexChars 100

/*
 * These are defined in TextPop.c
 */

extern void _IswTextInsertFileAction(Widget, IswEvent *, String *, Cardinal *);
extern void _IswTextInsertFile(Widget, IswEvent *, String *, Cardinal *);
extern void _IswTextSearch(Widget, IswEvent *, String *, Cardinal *);
extern void _IswTextDoSearchAction(Widget, IswEvent *, String *, Cardinal *);
extern void _IswTextDoReplaceAction(Widget, IswEvent *, String *, Cardinal *);
extern void _IswTextSetField(Widget, IswEvent *, String *, Cardinal *);
extern void _IswTextPopdownSearchAction(Widget, IswEvent *, String *, Cardinal *);

/*
 * These are defined in Text.c
 */

extern char * _IswTextGetText(TextWidget, ISWTextPosition, ISWTextPosition);
extern void _IswTextAlterSelection(TextWidget, IswTextSelectionMode,
                                   IswTextSelectionAction, String *, Cardinal *);
extern void _IswTextVScroll(TextWidget, int);
extern void _IswTextSetSelection(TextWidget, ISWTextPosition, ISWTextPosition,
                                 String *, Cardinal);
extern void _IswTextCheckResize(TextWidget);
extern void _IswTextExecuteUpdate(TextWidget);
extern void _IswTextSetScrollBars(TextWidget);
extern void _IswTextClearAndCenterDisplay(TextWidget);
extern IswSelectionId * _IswTextSelectionList(TextWidget, String *, Cardinal);
extern void _IswTextPrepareToUpdate(TextWidget);
extern int _IswTextReplace(TextWidget, ISWTextPosition, ISWTextPosition, ISWTextBlock *);

/*
 * These are defined here
 */

static void GetSelection(Widget, IswTime, String *, Cardinal);
void _IswTextZapSelection(TextWidget, IswEvent *, Boolean);


static void
StartAction(TextWidget ctx, IswEvent *iswev)
{
  _IswTextPrepareToUpdate(ctx);
  if (iswev != NULL) {
    switch (iswev->kind) {
    case IswButtonDown:
    case IswButtonUp:
      ctx->text.time = iswev->button.time;
      break;
    case IswKeyDown:
    case IswKeyUp:
      ctx->text.time = iswev->key.time;
      break;
    case IswMotion:
      ctx->text.time = iswev->motion.time;
      break;
    case IswEnter:
    case IswLeave:
      ctx->text.time = iswev->any.time;
      break;
    default:
      break;
    }
  }
}

static void
NotePosition(TextWidget ctx, IswEvent *iswev)
{
  switch (iswev->kind) {
  case IswKeyDown:
  case IswKeyUp:
    {
      IswRectangle cursor;
      IswTextSinkGetCursorBounds(ctx->text.sink, &cursor);
      ctx->text.ev_x = cursor.x + cursor.width / 2;
      ctx->text.ev_y = cursor.y + cursor.height / 2;
    }
    break;
  default:
    ctx->text.ev_x = IswEventX(iswev);
    ctx->text.ev_y = IswEventY(iswev);
    break;
  }
}

static void
EndAction(TextWidget ctx)
{
  _IswTextCheckResize(ctx);
  _IswTextExecuteUpdate(ctx);
  ctx->text.mult = 1;
}


struct _SelectionList {
    String* params;
    Cardinal count;
    IswTime time;
    Boolean CT_asked;	/* flag if asked for COMPOUND_TEXT */
    IswSelectionId selection;	/* selection id when asking for COMPOUND_TEXT */
};


/* ARGSUSED */
static void
_SelectionReceived(Widget w, IswPointer client_data, IswSelectionId *selection, IswSelectionId *type,
                   IswPointer value, unsigned long *length, int* format)
{
  TextWidget ctx = (TextWidget)w;
  ISWTextBlock text;

  if (*type == 0 /*ISW_CONVERT_FAIL*/ || *length == 0) {
    struct _SelectionList* list = (struct _SelectionList*)client_data;
    if (list != NULL) {
      if (list->CT_asked) {

	/* If we just asked for COMPOUND_TEXT and got a null
	response, we'll ask again, this time for STRING. */

	list->CT_asked = False;
        IswGetSelectionValue(w, list->selection,
                            _IswPlatformSelectionStdType(IswDisplayOf(w), ISW_SEL_STDTYPE_STRING),
                            _SelectionReceived, (IswPointer)list, list->time);
      } else {
	GetSelection(w, list->time, list->params, list->count);
	IswFree(client_data);
     }
    }
    return;
  }

  /* Many programs, especially old terminal emulators, give us multibyte text
but tell us it is COMPOUND_TEXT :(  The following routine checks to see if the
string is a legal multibyte string in our locale using a spooky heuristic :O
and if it is we can only assume the sending client is using the same locale as
we are, and convert it.  I also warn the user that the other client is evil. */

  StartAction( ctx, (IswEvent *) NULL );
      text.format = IswFmt8Bit;
  text.ptr = (char*)value;
  text.firstPos = 0;
  text.length = *length;
  if (_IswTextReplace(ctx, ctx->text.insertPos, ctx->text.insertPos, &text)) {
    xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
    xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
    return;
  }
  ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
				IswstPositions, IswsdRight, text.length, TRUE);

  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
  IswFree(client_data);
  IswFree(value);		/* the selection value should be freed with IswFree */
}


static void
GetSelection(Widget w, IswTime time, String *params, Cardinal num_params)
{
    IswSelectionId selection;
    struct _SelectionList* list;

    selection = _IswPlatformSelectionInternName(IswDisplayOf(w), *params, False);

    if (--num_params) {
	list = IswNew(struct _SelectionList);
	list->params = params + 1;
	list->count = num_params;
	list->time = time;
	list->CT_asked = True;
	list->selection = selection;
    } else list = NULL;
    IswGetSelectionValue(w, selection,
			_IswPlatformSelectionInternName(IswDisplayOf(w), "COMPOUND_TEXT", False),
			_SelectionReceived, (IswPointer)list, time);
}

static void
InsertSelection(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  StartAction((TextWidget)w, iswev); /* Get Time. */
  GetSelection(w, ((TextWidget)w)->text.time, params, *num_params);
  EndAction((TextWidget)w);
}

/************************************************************
 *
 * Routines for Moving Around.
 *
 ************************************************************/

static void
Move(TextWidget ctx, IswEvent *iswev, IswTextScanDirection dir,
     IswTextScanType type, Boolean include)
{
  StartAction(ctx, iswev);
  ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
				type, dir, ctx->text.mult, include);
  EndAction(ctx);
}

/*ARGSUSED*/
static void
MoveForwardChar(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
   Move((TextWidget) w, iswev, IswsdRight, IswstPositions, TRUE);
}

/*ARGSUSED*/
static void
MoveBackwardChar(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdLeft, IswstPositions, TRUE);
}

/*ARGSUSED*/
static void
MoveForwardWord(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdRight, IswstWhiteSpace, FALSE);
}

/*ARGSUSED*/
static void
MoveBackwardWord(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdLeft, IswstWhiteSpace, FALSE);
}

/*ARGSUSED*/
static void
MoveForwardParagraph(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdRight, IswstParagraph, FALSE);
}

/*ARGSUSED*/
static void
MoveBackwardParagraph(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdLeft, IswstParagraph, FALSE);
}

/*ARGSUSED*/
static void
MoveToLineEnd(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdRight, IswstEOL, FALSE);
}

/*ARGSUSED*/
static void
MoveToLineStart(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdLeft, IswstEOL, FALSE);
}


static void
MoveLine(TextWidget ctx, IswEvent *iswev, IswTextScanDirection dir)
{
  ISWTextPosition new, next_line, junk;
  int from_left, garbage;

  StartAction(ctx, iswev);

  if (dir == IswsdLeft)
    ctx->text.mult++;

  new = SrcScan(ctx->text.source, ctx->text.insertPos,
		IswstEOL, IswsdLeft, 1, FALSE);

  FindDist(ctx->text.sink, new, ctx->text.margin.left, ctx->text.insertPos,
	   &from_left, &junk, &garbage);

  new = SrcScan(ctx->text.source, ctx->text.insertPos, IswstEOL, dir,
		ctx->text.mult, (dir == IswsdRight));

  next_line = SrcScan(ctx->text.source, new, IswstEOL, IswsdRight, 1, FALSE);

  FindPos(ctx->text.sink, new, ctx->text.margin.left, from_left, FALSE,
	  &(ctx->text.insertPos), &garbage, &garbage);

  if (ctx->text.insertPos > next_line)
    ctx->text.insertPos = next_line;

  EndAction(ctx);
}

/*ARGSUSED*/
static void
MoveNextLine(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  MoveLine( (TextWidget) w, iswev, IswsdRight);
}

/*ARGSUSED*/
static void
MovePreviousLine(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  MoveLine( (TextWidget) w, iswev, IswsdLeft);
}

/*ARGSUSED*/
static void
MoveBeginningOfFile(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdLeft, IswstAll, TRUE);
}

/*ARGSUSED*/
static void
MoveEndOfFile(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Move((TextWidget) w, iswev, IswsdRight, IswstAll, TRUE);
}

static void
Scroll(TextWidget ctx, IswEvent *iswev, IswTextScanDirection dir)
{
  StartAction(ctx, iswev);

  if (dir == IswsdLeft)
    _IswTextVScroll(ctx, ctx->text.mult);
  else
    _IswTextVScroll(ctx, -ctx->text.mult);

  EndAction(ctx);
}

/*ARGSUSED*/
static void
ScrollOneLineUp(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Scroll( (TextWidget) w, iswev, IswsdLeft);
}

/*ARGSUSED*/
static void
ScrollOneLineDown(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  Scroll( (TextWidget) w, iswev, IswsdRight);
}

static void
MovePage(TextWidget ctx, IswEvent *iswev, IswTextScanDirection dir)
{
  int scroll_val = Max(1, ctx->text.lt.lines - 2);

  if (dir == IswsdLeft)
    scroll_val = -scroll_val;

  StartAction(ctx, iswev);
  _IswTextVScroll(ctx, scroll_val);
  ctx->text.insertPos = ctx->text.lt.top;
  EndAction(ctx);
}

/*ARGSUSED*/
static void
MoveNextPage(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  MovePage((TextWidget) w, iswev, IswsdRight);
}

/*ARGSUSED*/
static void
MovePreviousPage(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  MovePage((TextWidget) w, iswev, IswsdLeft);
}

/************************************************************
 *
 * Delete Routines.
 *
 ************************************************************/

static Boolean
MatchSelection(IswSelectionId selection, IswTextSelection *s)
{
    IswSelectionId    *match;
    int	    count;

    for (count = 0, match = s->selections; count < s->id_count; match++, count++)
	  if (*match == selection)
	      return True;
    return False;
}

#define SrcCvtSel	IswTextSourceConvertSelection

/*
 * IswConvertStandardSelection - Stub for standard widget selection conversion
 *
 * In a full implementation, this would handle standard selection targets like
 * TIMESTAMP, HOSTNAME, etc. For now, we return an empty list and let the text
 * widget provide its own targets.
 */
static Boolean
IswConvertStandardSelection(Widget w, IswTime time, IswSelectionId *selection,
                           IswSelectionId *target, IswSelectionId *type,
                           IswPointer *value, unsigned long *length, int *format)
{
    (void)time; (void)selection; (void)target; (void)format;

    /* Return empty list - the caller will add text-specific targets */
    *value = (IswPointer)IswMalloc(0);
    *length = 0;
    *type = _IswPlatformSelectionStdType(IswDisplayOf(w), ISW_SEL_STDTYPE_ID_LIST);
    return True;
}

static Boolean
ConvertSelection(Widget w, IswSelectionId *selection, IswSelectionId *target, IswSelectionId *type,
                 IswPointer* value, unsigned long *length, int *format)
{
  IswDisplay d = IswDisplayOf(w);
  TextWidget ctx = (TextWidget)w;
  Widget src = ctx->text.source;
  IswTextEditType edit_mode;
  IswTextSelectionSalt	*salt = NULL;
  IswTextSelection  *s;

  /* Intern the selection-target ids once through the platform selection op. */
  IswSelectionId a_targets  = _IswPlatformSelectionInternName(d, "TARGETS", False);
  IswSelectionId a_text     = _IswPlatformSelectionInternName(d, "TEXT", False);
  IswSelectionId a_ctext    = _IswPlatformSelectionInternName(d, "COMPOUND_TEXT", False);
  IswSelectionId a_length   = _IswPlatformSelectionInternName(d, "LENGTH", False);
  IswSelectionId a_listlen  = _IswPlatformSelectionInternName(d, "LIST_LENGTH", False);
  IswSelectionId a_charpos  = _IswPlatformSelectionInternName(d, "CHARACTER_POSITION", False);
  IswSelectionId a_delete   = _IswPlatformSelectionInternName(d, "DELETE", False);
  IswSelectionId str_type   = _IswPlatformSelectionStdType(d, ISW_SEL_STDTYPE_STRING);

  if (*target == a_targets) {
    IswSelectionId  idlist_type = _IswPlatformSelectionStdType(d, ISW_SEL_STDTYPE_ID_LIST);
    IswSelectionId* targetP, * std_targets;
    unsigned long std_length;

    if ( SrcCvtSel(src, selection, target, type, value, length, format) )
	return True;

    IswConvertStandardSelection(w, ctx->text.time, selection,
				target, type, (IswPointer*)&std_targets,
				&std_length, format);

    *value = IswMalloc((unsigned) sizeof(IswSelectionId)*(std_length + 7));
    targetP = *(IswSelectionId**)value;

    *length = std_length + 6;
    *targetP++ = str_type;
    *targetP++ = a_text;
    *targetP++ = a_ctext;
    *targetP++ = a_length;
    *targetP++ = a_listlen;
    *targetP++ = a_charpos;

    {
      IswArgBuilder ab = IswArgBuilderInit();
      IswArgEditType(&ab, (IswArgVal)&edit_mode);
      IswGetValues(src, ab.args, ab.count);
    }

    if (edit_mode == IswtextEdit) {
      *targetP++ = a_delete;
      (*length)++;
    }
    memcpy((char*)targetP, (char*)std_targets, sizeof(IswSelectionId)*std_length);
    IswFree((char*)std_targets);
    *type = idlist_type;
    *format = 32;
    return True;
  }

  if ( SrcCvtSel(src, selection, target, type, value, length, format) )
    return True;

  for (salt = ctx->text.salt2; salt; salt = salt->next)
    if (MatchSelection (*selection, &salt->s))
      break;
  if (!salt)
    return False;
  s = &salt->s;
  if (*target == str_type ||
      *target == a_text ||
      *target == a_ctext) {
	if (*target == a_text) {
		*type = str_type;
	} else {
	    *type = *target;
	}
	/*
	 * If salt is True, the salt->contents stores CT string,
	 * its length is measured in bytes.
	 * Refer to _IswTextSaltAwaySelection().
	 *
	 * by Li Yuhong, Mar. 20, 1991.
	 */
	if (!salt) {
	    *value = (char *)_IswTextGetSTRING(ctx, s->left, s->right);
	    {
		*length = strlen(*value);
	    }
	} else {
	    *value = IswMalloc((salt->length + 1) * sizeof(unsigned char));
	    strcpy (*value, salt->contents);
	    *length = salt->length;
	}
	*format = 8;
	return True;
  }

  if ( (*target == a_listlen) || (*target == a_length) ) {
    long * temp;

    temp = (long *) IswMalloc(sizeof(long));
    if (*target == a_listlen)
      *temp = 1L;
    else			/* *target == a_length */
      *temp = (long) (s->right - s->left);

    *value = (IswPointer) temp;
    *type = _IswPlatformSelectionInternName(d, "INTEGER", False);
    *length = 1L;
    *format = 32;
    return True;
  }

  if (*target == a_charpos) {
    long * temp;

    temp = (long *) IswMalloc(2 * sizeof(long));
    temp[0] = (long) (s->left + 1);
    temp[1] = s->right;
    *value = (IswPointer) temp;
    *type = _IswPlatformSelectionInternName(d, "SPAN", False);
    *length = 2L;
    *format = 32;
    return True;
  }

  if (*target == a_delete) {
    if (!salt)
	_IswTextZapSelection( ctx, (IswEvent *) NULL, TRUE);
    *value = NULL;
    *type = ISW_SELECTION_NONE;
    *length = 0;
    *format = 32;
    return True;
  }

  if (IswConvertStandardSelection(w, ctx->text.time, selection, target, type,
				  (IswPointer *)value, length, format))
    return True;

  /* else */
  return False;
}

static void
LoseSelection(Widget w, IswSelectionId *selection)
{
  TextWidget ctx = (TextWidget) w;
  IswSelectionId* idP;
  int i;
  IswTextSelectionSalt	*salt, *prevSalt, *nextSalt;

    prevSalt = 0;
    for (salt = ctx->text.salt2; salt; salt = nextSalt)
    {
    	idP = salt->s.selections;
	nextSalt = salt->next;
    	for (i = 0 ; i < salt->s.id_count; i++, idP++)
	    if (*selection == *idP)
		*idP = ISW_SELECTION_NONE;

    	while (salt->s.id_count &&
	       salt->s.selections[salt->s.id_count-1] == 0)
	{
	    salt->s.id_count--;
	}

    	/*
    	 * Must walk the selection list in opposite order from UnsetSelection.
    	 */

    	idP = salt->s.selections;
    	for (i = 0 ; i < salt->s.id_count; i++, idP++)
    	    if (*idP == ISW_SELECTION_NONE)
 	    {
      	      *idP = salt->s.selections[--salt->s.id_count];
      	      while (salt->s.id_count &&
	     	     salt->s.selections[salt->s.id_count-1] == 0)
    	    	salt->s.id_count--;
    	    }
	if (salt->s.id_count == 0)
	{
	    IswFree ((char *) salt->s.selections);

            /* WARNING: the next line frees memory not allocated in Isw. */
            /* Could be a serious bug.  Someone look into it. */
	    IswFree (salt->contents);
	    if (prevSalt)
		prevSalt->next = nextSalt;
	    else
		ctx->text.salt2 = nextSalt;
	    IswFree ((char *) salt);
	}
	else
	    prevSalt = salt;
    }
}

static void
_DeleteOrKill(TextWidget ctx, ISWTextPosition from, ISWTextPosition to, Boolean	kill)
{
  ISWTextBlock text;

  if (kill && from < to) {
    IswTextSelectionSalt    *salt;
    IswSelectionId selection = _IswPlatformSelectionInternName(IswDisplayOf(ctx), "SECONDARY", False);

    LoseSelection ((Widget) ctx, &selection);
    salt = (IswTextSelectionSalt *) IswMalloc (sizeof (IswTextSelectionSalt));
    if (!salt)
	return;
    salt->s.selections = (IswSelectionId *) IswMalloc (sizeof (IswSelectionId));
    if (!salt->s.selections)
    {
	IswFree ((char *) salt);
	return;
    }
    salt->s.left = from;
    salt->s.right = to;
    salt->contents = (char *)_IswTextGetSTRING(ctx, from, to);
       salt->length = strlen (salt->contents);
    salt->next = ctx->text.salt2;
    ctx->text.salt2 = salt;
    salt->s.selections[0] = selection;
    IswOwnSelection ((Widget) ctx, selection, ctx->text.time,
		    ConvertSelection, LoseSelection, NULL);
    salt->s.id_count = 1;
/*
    XStoreBuffer(IswDisplayOf(ctx), ptr, strlen(ptr), 1);
    IswFree(ptr);
*/
  }
  text.length = 0;
  text.firstPos = 0;

  text.format = _IswTextFormat(ctx);
  text.ptr = (char *)"";	/* These two lines needed to make legal TextBlock */

  if (_IswTextReplace(ctx, from, to, &text)) {
    xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
    xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
    return;
  }
  ctx->text.insertPos = from;
  ctx->text.showposition = TRUE;
}

static void
DeleteOrKill(TextWidget ctx, IswEvent *iswev, IswTextScanDirection dir,
             IswTextScanType type, Boolean include, Boolean kill)
{
  ISWTextPosition from, to;

  StartAction(ctx, iswev);
  to = SrcScan(ctx->text.source, ctx->text.insertPos,
	       type, dir, ctx->text.mult, include);

/*
 * If no movement actually happened, then bump the count and try again.
 * This causes the character position at the very beginning and end of
 * a boundary to act correctly.
 */

  if (to == ctx->text.insertPos)
      to = SrcScan(ctx->text.source, ctx->text.insertPos,
		   type, dir, ctx->text.mult + 1, include);

  if (dir == IswsdLeft) {
    from = to;
    to = ctx->text.insertPos;
  }
  else
    from = ctx->text.insertPos;

  _DeleteOrKill(ctx, from, to, kill);
  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
}

/*ARGSUSED*/
static void
DeleteForwardChar(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, iswev, IswsdRight, IswstPositions, TRUE, FALSE);
}

/*ARGSUSED*/
static void
DeleteBackwardChar(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, iswev, IswsdLeft, IswstPositions, TRUE, FALSE);
}

/*ARGSUSED*/
static void
DeleteForwardWord(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, iswev,
	       IswsdRight, IswstWhiteSpace, FALSE, FALSE);
}

/*ARGSUSED*/
static void
DeleteBackwardWord(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, iswev,
	       IswsdLeft, IswstWhiteSpace, FALSE, FALSE);
}

/*ARGSUSED*/
static void
KillForwardWord(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, iswev,
	       IswsdRight, IswstWhiteSpace, FALSE, TRUE);
}

/*ARGSUSED*/
static void
KillBackwardWord(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, iswev,
	       IswsdLeft, IswstWhiteSpace, FALSE, TRUE);
}

/*ARGSUSED*/
static void
KillToEndOfLine(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition end_of_line;

  StartAction(ctx, iswev);
  end_of_line = SrcScan(ctx->text.source, ctx->text.insertPos, IswstEOL,
			IswsdRight, ctx->text.mult, FALSE);
  if (end_of_line == ctx->text.insertPos)
    end_of_line = SrcScan(ctx->text.source, ctx->text.insertPos, IswstEOL,
			  IswsdRight, ctx->text.mult, TRUE);

  _DeleteOrKill(ctx, ctx->text.insertPos, end_of_line, TRUE);
  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
}

/*ARGSUSED*/
static void
KillToEndOfParagraph(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, iswev, IswsdRight, IswstParagraph, FALSE, TRUE);
}

void
_IswTextZapSelection(TextWidget ctx, IswEvent *iswev, Boolean kill)
{
   StartAction(ctx, iswev);
   _DeleteOrKill(ctx, ctx->text.s.left, ctx->text.s.right, kill);
  _IswTextSetScrollBars(ctx);
   EndAction(ctx);
}

/*ARGSUSED*/
static void
KillCurrentSelection(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;

  StartAction(ctx, iswev);
  /* Snapshot selection to CLIPBOARD before deleting */
  if (ctx->text.s.left < ctx->text.s.right) {
    IswSelectionId clip = _IswPlatformSelectionInternName(IswDisplayOf(w), "CLIPBOARD", False);
    _IswTextSaltAwaySelection(ctx, &clip, 1);
  }
  _DeleteOrKill(ctx, ctx->text.s.left, ctx->text.s.right, TRUE);
  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
}

/*ARGSUSED*/
static void
DeleteCurrentSelection(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  _IswTextZapSelection( (TextWidget) w, iswev, FALSE);
}

/************************************************************
 *
 * Insertion Routines.
 *
 ************************************************************/

static int
InsertNewLineAndBackupInternal(TextWidget ctx)
{
  int count, error = IswEditDone;
  ISWTextBlock text;

  text.format = _IswTextFormat(ctx);
  text.length = ctx->text.mult;
  text.firstPos = 0;

  {
      text.ptr = IswMalloc(sizeof(char) * ctx->text.mult);
      for (count = 0; count < ctx->text.mult; count++ )
          text.ptr[count] = IswLF;
  }

  if (_IswTextReplace(ctx, ctx->text.insertPos, ctx->text.insertPos, &text)) {
    xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
    xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
    error = IswEditError;
  }
  else
    ctx->text.showposition = TRUE;

  IswFree( text.ptr );
  return( error );
}

/*ARGSUSED*/
static void
InsertNewLineAndBackup(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  StartAction( (TextWidget) w, iswev );
  (void) InsertNewLineAndBackupInternal( (TextWidget) w );
  _IswTextSetScrollBars( (TextWidget) w);
  EndAction( (TextWidget) w );
}

static int
LocalInsertNewLine(TextWidget ctx, IswEvent *iswev)
{
  StartAction(ctx, iswev);
  if (InsertNewLineAndBackupInternal(ctx) == IswEditError)
    return(IswEditError);
  ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
			     IswstPositions, IswsdRight, ctx->text.mult, TRUE);
  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
  return(IswEditDone);
}

/*ARGSUSED*/
static void
InsertNewLine(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  (void) LocalInsertNewLine( (TextWidget) w, iswev);
}

/*ARGSUSED*/
static void
InsertNewLineAndIndent(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  ISWTextBlock text;
  ISWTextPosition pos1;
  int length;
  TextWidget ctx = (TextWidget) w;
  String line_to_ip;

  StartAction(ctx, iswev);
  pos1 = SrcScan(ctx->text.source, ctx->text.insertPos,
		 IswstEOL, IswsdLeft, 1, FALSE);

  line_to_ip = _IswTextGetText(ctx, pos1, ctx->text.insertPos);

  text.format = _IswTextFormat(ctx);
  text.firstPos = 0;

  {
     char *ptr;
     length = strlen(line_to_ip);
     /* The current line + \0 and LF will be copied to this
	buffer. Before my fix, only length + 1 bytes were
	allocated, causing on machine with non-wasteful
	malloc implementation segmentation violations by
	overwriting the bypte after the allocated area

	-gustaf neumann
      */
     text.ptr = IswMalloc( ( 2 + length ) * sizeof( char ) );

     ptr = text.ptr;
     ptr[0] = IswLF;
     strcpy( ++ptr, line_to_ip );

     length++;
     while ( length && ( isspace(*ptr) || ( *ptr == IswTAB ) ) )
         ptr++, length--;
     *ptr = '\0';
     text.length = strlen(text.ptr);
  }
  IswFree( line_to_ip );

  if (_IswTextReplace(ctx,ctx->text.insertPos, ctx->text.insertPos, &text)) {
    xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
    xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
    IswFree(text.ptr);
    EndAction(ctx);
    return;
  }
  IswFree(text.ptr);
  ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
				IswstPositions, IswsdRight, text.length, TRUE);
  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
}

/************************************************************
 *
 * Selection Routines.
 *
 *************************************************************/

static void
SelectWord(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition l, r;

  StartAction(ctx, iswev);
  l = SrcScan(ctx->text.source, ctx->text.insertPos,
	      IswstWhiteSpace, IswsdLeft, 1, FALSE);
  r = SrcScan(ctx->text.source, l, IswstWhiteSpace, IswsdRight, 1, FALSE);
  _IswTextSetSelection(ctx, l, r, params, *num_params);
  EndAction(ctx);
}

static void
SelectAll(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;

  StartAction(ctx, iswev);
  _IswTextSetSelection(ctx,zeroPosition,ctx->text.lastPos,params,*num_params);
  EndAction(ctx);
}

static void
ModifySelection(TextWidget ctx, IswEvent *iswev, IswTextSelectionMode mode,
                IswTextSelectionAction action, String *params, Cardinal *num_params)
{
  StartAction(ctx, iswev);
  NotePosition(ctx, iswev);
  _IswTextAlterSelection(ctx, mode, action, params, num_params);
  EndAction(ctx);
}

/* ARGSUSED */
static void
SelectStart(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  Widget shell;

  for (shell = w; !IswIsShell(shell); shell = IswParent(shell))
    ;
  IswSetKeyboardFocus(shell, w);
  ModifySelection((TextWidget) w, iswev,
		  IswsmTextSelect, IswactionStart, params, num_params);
}

/* ARGSUSED */
static void
SelectAdjust(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, iswev,
		  IswsmTextSelect, IswactionAdjust, params, num_params);
}

static void
SelectEnd(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, iswev,
		  IswsmTextSelect, IswactionEnd, params, num_params);
}

/* ARGSUSED */
static void
ExtendStart(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, iswev,
		  IswsmTextExtend, IswactionStart, params, num_params);
}

/* ARGSUSED */
static void
ExtendAdjust(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, iswev,
		  IswsmTextExtend, IswactionAdjust, params, num_params);
}

static void
ExtendEnd(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, iswev,
		  IswsmTextExtend, IswactionEnd, params, num_params);
}

static void
SelectSave(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    int	    num_ids;
    IswSelectionId*   sel;
    IswDisplay dpy = IswDisplayOf(w);
    IswSelectionId    selections[256];

    StartAction(  (TextWidget) w, iswev );
    num_ids = *num_params;
    if (num_ids > 256) num_ids = 256;
    for (sel=selections; --num_ids >= 0; sel++, params++)
	    *sel = _IswPlatformSelectionInternName(dpy, *params, False);
    num_ids = *num_params;
    _IswTextSaltAwaySelection( (TextWidget) w, selections, num_ids );
    EndAction(  (TextWidget) w );
}

static void
CopySelection(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  int num_ids;
  IswSelectionId *sel;
  IswDisplay dpy = IswDisplayOf(w);
  IswSelectionId selections[256];

  StartAction(ctx, iswev);
  if (ctx->text.s.left < ctx->text.s.right) {
    num_ids = *num_params;
    if (num_ids > 256) num_ids = 256;
    for (sel = selections; --num_ids >= 0; sel++, params++)
      *sel = _IswPlatformSelectionInternName(dpy, *params, False);
    num_ids = *num_params;
    _IswTextSaltAwaySelection(ctx, selections, num_ids);
  }
  EndAction(ctx);
}

/************************************************************
 *
 * Misc. Routines.
 *
 ************************************************************/

/* ARGSUSED */
static void
RedrawDisplay(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  StartAction( (TextWidget) w, iswev);
  _IswTextClearAndCenterDisplay((TextWidget) w);
  EndAction( (TextWidget) w);
}

/*ARGSUSED*/
static void
TextFocusIn (Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;

  if (iswev->kind != IswFocusIn) {
    return;
  }

  /* Let the input method know focus has arrived. */
  _IswImSetFocusValues (w, NULL, 0);
  if (iswev->focus.source == IswFocusByPointer) {
      return;
  }

  _IswTextPrepareToUpdate(ctx);
  ctx->text.hasfocus = TRUE;
  _IswTextExecuteUpdate(ctx);
}

/*ARGSUSED*/
static void
TextFocusOut(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;

  if (iswev->kind != IswFocusOut) {
    return;
  }

  /* Let the input method know focus has left.*/
  _IswImUnsetFocus(w);
  if (iswev->focus.source == IswFocusByPointer) {
      return;
  }
  _IswTextPrepareToUpdate(ctx);
  ctx->text.hasfocus = FALSE;
  _IswTextExecuteUpdate(ctx);
}

/*ARGSUSED*/
static void
TextEnterWindow(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  /* INFERIOR-crossing distinction has no neutral equivalent; reach the
     native crossing event just for the detail field. */
  ISW_NATIVE_EVENT(iswev);
  TextWidget ctx = (TextWidget) w;
  xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;

  if ((cev->detail != XCB_NOTIFY_DETAIL_INFERIOR) && !ctx->text.hasfocus)
    _IswImSetFocusValues(w, NULL, 0);
}

/*ARGSUSED*/
static void
TextLeaveWindow(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  /* INFERIOR-crossing distinction has no neutral equivalent; reach the
     native crossing event just for the detail field. */
  ISW_NATIVE_EVENT(iswev);
  TextWidget ctx = (TextWidget) w;
  xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;

  if ((cev->detail != XCB_NOTIFY_DETAIL_INFERIOR) && !ctx->text.hasfocus)
    _IswImUnsetFocus(w);
}

/* XComposeStatus removed - not available in XCB */

/*	Function Name: AutoFill
 *	Description: Breaks the line at the previous word boundry when
 *                   called inside InsertChar.
 *	Arguments: ctx - The text widget.
 *	Returns: none
 */

static void
AutoFill(TextWidget ctx)
{
  int width, height, x, line_num, max_width;
  ISWTextPosition ret_pos;
  ISWTextBlock text;

  if ( !((ctx->text.auto_fill) && (ctx->text.mult == 1)) )
    return;

  for ( line_num = 0; line_num < ctx->text.lt.lines ; line_num++)
    if ( ctx->text.lt.info[line_num].position >= ctx->text.insertPos )
      break;
  line_num--;			/* backup a line. */

  max_width = Max(0, (int)(ctx->core.width - HMargins(ctx)));

  x = ctx->text.margin.left;
  IswTextSinkFindPosition( ctx->text.sink,ctx->text.lt.info[line_num].position,
			  x, max_width, TRUE, &ret_pos, &width, &height);

  if ( ret_pos >= ctx->text.insertPos )
    return;

  text.format = IswFmt8Bit;
    text.ptr = (char *)"\n";
  text.length = 1;
  text.firstPos = 0;

  if (_IswTextReplace(ctx, ret_pos - 1, ret_pos, &text)) {
      xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
      xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
  }
}

/*ARGSUSED*/
static void
InsertChar(Widget w, IswEvent *iswev, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;
  char *ptr, strbuf[BUFSIZ];
  int count, error;
  ISWTextBlock text;

  {
    /* The neutral translator already resolved the key identity and the
       UTF-8 text it produced; read those instead of decoding keysyms. */
    if (iswev->key.key == IswKeyReturn) {
      strbuf[0] = '\r';
      text.length = 1;
    } else if (iswev->key.key == IswKeyTab) {
      strbuf[0] = '\t';
      text.length = 1;
    } else if (iswev->key.text[0] != '\0') {
      text.length = strlen(iswev->key.text);
      memcpy(strbuf, iswev->key.text, text.length);
    } else {
      /* Non-printable or special key */
      text.length = 0;
    }
  }

  if (text.length == 0)
      return;

  text.format = _IswTextFormat( ctx );
  { /* == IswFmt8Bit */
      text.ptr = ptr = IswMalloc( sizeof(char) * text.length * ctx->text.mult );
      for ( count = 0; count < ctx->text.mult; count++ ) {
          strncpy( ptr, strbuf, text.length );
          ptr += text.length;
      }
  }

  text.length = text.length * ctx->text.mult;
  text.firstPos = 0;

  StartAction(ctx, iswev);

  error = _IswTextReplace(ctx, ctx->text.insertPos,ctx->text.insertPos, &text);

  if (error == IswEditDone) {
      ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
	      IswstPositions, IswsdRight, text.length, TRUE);
      AutoFill(ctx);
  }
  else {
      xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
      xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
  }

  IswFree(text.ptr);
  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
}


/* IfHexConvertHexElseReturnParam() - called by InsertString
 *
 * i18n requires the ability to specify multiple characters in a hexa-
 * decimal string at once.  Since Insert was already too long, I made
 * this a seperate routine.
 *
 * A legal hex string in MBNF: '0' 'x' ( HEX-DIGIT HEX-DIGIT )+ '\0'
 *
 * WHEN:    the passed param is a legal hex string
 * RETURNS: a pointer to that converted, null terminated hex string;
 *          len_return holds the character count of conversion result
 *
 * WHEN:    the passed param is not a legal hex string:
 * RETURNS: the parameter passed;
 *          len_return holds the char count of param.
 *
 * NOTE:    In neither case will there be strings to free. */

static char*
IfHexConvertHexElseReturnParam(const char *param, int *len_return)
{
  const char *p;               /* steps through param char by char */
  char c;                      /* holds the character pointed to by p */

  int ind;		       /* steps through hexval buffer char by char */
  static char hexval[ IswTextActionMaxHexChars ];
  Boolean first_digit;

  /* reject if it doesn't begin with 0x and at least one more character. */

  if ( ( param[0] != '0' ) || ( param[1] != 'x' ) || ( param[2] == '\0' ) ) {
      *len_return = strlen( param );
      return (char *)param;
  }

  /* Skip the 0x; go character by character shifting and adding. */

  first_digit = True;
  ind = 0;
  hexval[ ind ] = '\0';

  for ( p = param+2; ( c = *p ); p++ ) {
      hexval[ ind ] *= 16;
      if (c >= '0' && c <= '9')
          hexval[ ind ] += c - '0';
      else if (c >= 'a' && c <= 'f')
          hexval[ ind ] += c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
          hexval[ ind ] += c - 'A' + 10;
      else break;

      /* If we didn't break in preceding line, it was a good hex char. */

      if ( first_digit )
          first_digit = False;
      else {
          first_digit = True;
          if ( ++ind < IswTextActionMaxHexChars )
              hexval[ ind ] = '\0';
          else {
              *len_return = strlen( param );
              return (char *)param;
          }
      }
  }

  /* We quit the above loop becasue we hit a non hex.  If that char is \0... */

  if ( ( c == '\0' ) && first_digit ) {
      *len_return = strlen( hexval );
      return( hexval );       /* ...it was a legal hex string, so return it.*/
  }

  /* Else, there were non-hex chars or odd digit count, so... */

  *len_return = strlen( param );
  return (char *)param;			   /* ...return the verbatim string. */
}


/* InsertString() - action
 *
 * Mostly rewritten for R6 i18n.
 *
 * Each parameter, in turn, will be insert at the inputPos
 * and the inputPos advances to the insertion's end.
 *
 * The exception is that parameters composed of the two
 * characters 0x, followed only by an even number of
 * hexadecimal digits will be converted to characters. */

/*ARGSUSED*/
static void
InsertString(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextBlock text;
  int	   i;

  text.firstPos = 0;
  text.format = _IswTextFormat( ctx );

  StartAction(ctx, iswev);
  for ( i = *num_params; i; i--, params++ ) { /* DO FOR EACH PARAMETER */

      text.ptr = IfHexConvertHexElseReturnParam( *params, &text.length );

      if ( text.length == 0 ) continue;

      if ( _IswTextReplace( ctx, ctx->text.insertPos,
			    ctx->text.insertPos, &text ) ) {
          xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
          xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
          EndAction( ctx );
          return;
      }

      /* Advance insertPos to the end of the string we just inserted. */
      ctx->text.insertPos = SrcScan( ctx->text.source, ctx->text.insertPos,
			    IswstPositions, IswsdRight, text.length, TRUE );

  } /* DO FOR EACH PARAMETER */

  EndAction( ctx );
}


/* DisplayCaret() - action
 *
 * The parameter list should contain one boolean value.  If the
 * argument is true, the cursor will be displayed.  If false, not.
 *
 * The exception is that EnterNotify and LeaveNotify events may
 * have a second argument, "always".  If they do not, the cursor
 * is only affected if the focus member of the event is true.	*/

static void
DisplayCaret(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  /* The crossing event's focus flag (same_screen_focus) has no neutral
     equivalent; reach the native event just for that read. */
  ISW_NATIVE_EVENT(iswev);
  TextWidget ctx = (TextWidget)w;
  Boolean display_caret = True;

  if  ( ( iswev->kind == IswEnter || iswev->kind == IswLeave ) &&
        ( ( *num_params >= 2 ) && ( strcmp( params[1], "always" ) == 0 ) ) &&
        ( ((xcb_enter_notify_event_t *)event)->same_screen_focus) )
      return;

  if (*num_params > 0) {	/* default arg is "True" */
      XrmValue from, to;
      Boolean converted_value;
      from.size = strlen(from.addr = (IswPointer)params[0]);
      to.size = sizeof(Boolean);
      to.addr = (IswPointer)&converted_value;
      
      if ( IswConvertAndStore( w, IswRString, &from, IswRBoolean, &to ) )
          display_caret = converted_value;
      if ( ctx->text.display_caret == display_caret )
          return;
  }
  StartAction(ctx, iswev);
  ctx->text.display_caret = display_caret;
  EndAction(ctx);
}


/* Multiply() - action
 *
 * The parameter list may contain either a number or the string 'Reset'.
 *
 * A number will multiply the current multiplication factor by that number.
 * Many of the text widget actions will will perform n actions, where n is
 * the multiplication factor.
 *
 * The string reset will reset the mutiplication factor to 1. */

/* ARGSUSED */
static void
Multiply(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  int mult;

  if (*num_params != 1) {
      IswAppError( IswWidgetToApplicationContext( w ),
	       "Isw Text Widget: multiply() takes exactly one argument.");
      xcb_bell(_IswXcbConn(IswDisplayOf(w)), 0); // 0 = default volume
      xcb_flush(_IswXcbConn(IswDisplayOf(w)));
      return;
  }

  if ( ( params[0][0] == 'r' ) || ( params[0][0] == 'R' ) ) {
      xcb_bell(_IswXcbConn(IswDisplayOf(w)), 0); // 0 = default volume
      xcb_flush(_IswXcbConn(IswDisplayOf(w)));
      ctx->text.mult = 1;
      return;
  }

  if ( ( mult = atoi( params[0] ) ) == 0 ) {
      char buf[ BUFSIZ ];
      sprintf(buf, "%s %s", "Isw Text Widget: multiply() argument",
	    "must be a number greater than zero, or 'Reset'." );
      IswAppError( IswWidgetToApplicationContext( w ), buf );
      xcb_bell(_IswXcbConn(IswDisplayOf(w)), 0); // 0 = default volume
      xcb_flush(_IswXcbConn(IswDisplayOf(w)));
      return;
  }

  ctx->text.mult *= mult;
}


/* StripOutOldCRs() - called from FormRegion
 *
 * removes CRs in widget ctx, from from to to.
 *
 * RETURNS: the new ending location (we may add some characters),
 * or IswReplaceError if the widget can't be written to. */

static ISWTextPosition
StripOutOldCRs(TextWidget ctx, ISWTextPosition from, ISWTextPosition to)
{
  ISWTextPosition startPos, endPos, eop_begin, eop_end, temp;
  Widget src = ctx->text.source;
  ISWTextBlock text;
  char *buf;

  /* Initialize our TextBlock with two spaces. */

  text.firstPos = 0;
  text.format = _IswTextFormat(ctx);
  if ( text.format == IswFmt8Bit )
      text.ptr= (char *)"  ";

  /* Strip out CR's. */

  eop_begin = eop_end = startPos = endPos = from;
  /* CONSTCOND */
  while (TRUE) {
      endPos=SrcScan(src, startPos, IswstEOL, IswsdRight, 1, FALSE);

      temp = SrcScan(src, endPos, IswstWhiteSpace, IswsdLeft, 1, FALSE);
      temp = SrcScan(src, temp,   IswstWhiteSpace, IswsdRight,1, FALSE);

      if (temp > startPos)
          endPos = temp;

      if (endPos >= to)
          break;

      if (endPos >= eop_begin) {
          startPos = eop_end;
          eop_begin=SrcScan(src, startPos, IswstParagraph, IswsdRight, 1,FALSE);
          eop_end = SrcScan(src, startPos, IswstParagraph, IswsdRight, 1, TRUE);
      }
    else {
      ISWTextPosition periodPos, next_word;
      int i, len;

      periodPos= SrcScan(src, endPos, IswstPositions, IswsdLeft, 1, TRUE);
      next_word = SrcScan(src, endPos, IswstWhiteSpace, IswsdRight, 1, FALSE);

      len = next_word - periodPos;

      text.length = 1;
      buf = _IswTextGetText(ctx, periodPos, next_word);
        if ( (periodPos < endPos) && (buf[0] == '.') )
	  text.length++;	/* Put in two spaces. */

      /*
       * Remove all extra spaces.
       */

      for (i = 1 ; i < len; i++)
	  if ( !isspace(buf[i]) || ((periodPos + i) >= to) ) {
	      break;
	  }

      IswFree(buf);

      to -= (i - text.length - 1);
      startPos = SrcScan(src, periodPos, IswstPositions, IswsdRight, i, TRUE);
      if (_IswTextReplace(ctx, endPos, startPos, &text) != IswEditDone)
	  return IswReplaceError;
      startPos -= i - text.length;
    }
  }
  return(to);
}


/* InsertNewCRs() - called from FormRegion
 *
 * inserts new CRs for FormRegion, thus for FormParagraph action */

static void
InsertNewCRs(TextWidget ctx, ISWTextPosition from, ISWTextPosition to)
{
  ISWTextPosition startPos, endPos, space, eol;
  ISWTextBlock text;
  int i, width, height, len;
  char * buf;

  text.firstPos = 0;
  text.length = 1;
  text.format = _IswTextFormat( ctx );

  if ( text.format == IswFmt8Bit )
      text.ptr = (char *)"\n";

  startPos = from;
  /* CONSTCOND */
  while (TRUE) {
      IswTextSinkFindPosition( ctx->text.sink, startPos,
			    (int) ctx->text.margin.left,
			    (int) (ctx->core.width - HMargins(ctx)),
			    TRUE, &eol, &width, &height);
      if (eol >= to)
          break;

      eol  = SrcScan(ctx->text.source, eol, IswstPositions, IswsdLeft, 1, TRUE);
      space= SrcScan(ctx->text.source, eol, IswstWhiteSpace,IswsdRight,1, TRUE);

      startPos = endPos = eol;
      if (eol == space)
          return;

      len = (int) (space - eol);
      buf = _IswTextGetText(ctx, eol, space);
      for ( i = 0 ; i < len ; i++)
          if (!isspace(buf[i]))
              break;

      to -= (i - 1);
      endPos = SrcScan(ctx->text.source, endPos,
		     IswstPositions, IswsdRight, i, TRUE);
      IswFree(buf);

      if (_IswTextReplace(ctx, startPos, endPos, &text))
          return;

      startPos = SrcScan(ctx->text.source, startPos,
		       IswstPositions, IswsdRight, 1, TRUE);
  }
}


/* FormRegion() - called by FormParagraph
 *
 * oversees the work of paragraph-forming a region
 *
 * RETURNS: IswEditDone if successful, or IswReplaceError. */

static int
FormRegion(TextWidget ctx, ISWTextPosition from, ISWTextPosition to)
{
  if ( from >= to ) return IswEditDone;

  if ( ( to = StripOutOldCRs( ctx, from, to ) ) == IswReplaceError )
      return IswReplaceError;

  /* insure that the insertion point is within legal bounds */
  if ( ctx->text.insertPos > SrcScan( ctx->text.source, 0,
				       IswstAll, IswsdRight, 1, TRUE ) )
      ctx->text.insertPos = to;

  InsertNewCRs(ctx, from, to);
  _IswTextBuildLineTable(ctx, ctx->text.lt.top, TRUE);
  return IswEditDone;
}


/* FormParagraph() - action
 *
 * removes and reinserts CRs to maximize line length without clipping */

/* ARGSUSED */
static void
FormParagraph(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition from, to;

  StartAction(ctx, iswev);

  from =  SrcScan( ctx->text.source, ctx->text.insertPos,
		  IswstParagraph, IswsdLeft, 1, FALSE );
  to  =  SrcScan( ctx->text.source, from,
		 IswstParagraph, IswsdRight, 1, FALSE );

  if ( FormRegion( ctx, from, to ) == IswReplaceError ) {
      xcb_bell(_IswXcbConn(IswDisplayOf(w)), 0); // 0 = default volume
      xcb_flush(_IswXcbConn(IswDisplayOf(w)));
  }
  _IswTextSetScrollBars( ctx );
  EndAction( ctx );
}


/* TransposeCharacters() - action
 *
 * Swaps the character to the left of the mark
 * with the character to the right of the mark. */

/* ARGSUSED */
static void
TransposeCharacters(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition start, end;
  ISWTextBlock text;
  char* buf;
  int i;

  StartAction(ctx, iswev);

  /* Get bounds. */

  start = SrcScan( ctx->text.source, ctx->text.insertPos, IswstPositions,
		  IswsdLeft, 1, TRUE );
  end = SrcScan( ctx->text.source, ctx->text.insertPos, IswstPositions,
		IswsdRight, ctx->text.mult, TRUE );

  /* Make sure we aren't at the very beginning or end of the buffer. */

  if ( ( start == ctx->text.insertPos ) || ( end == ctx->text.insertPos ) ) {
      xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
      xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
      EndAction( ctx );
      return;
  }

  ctx->text.insertPos = end;

  text.firstPos = 0;
  text.format = _IswTextFormat(ctx);

  /* Retrieve text and swap the characters. */

  { /* thus text.format == IswFmt8Bit */
      char c;
      buf = _IswTextGetText( ctx, start, end );
      text.length = strlen( buf );
      c = buf[ 0 ];
      for ( i = 1; i < text.length; i++ )
          buf[ i-1 ] = buf[ i ];
      buf[ i-1 ] = c;
  }

  text.ptr = buf;

  /* Store new text in source. */

  if (_IswTextReplace (ctx, start, end, &text))	{/* Unable to edit, complain. */
    xcb_bell(_IswXcbConn(IswDisplayOf(ctx)), 0); // 0 = default volume
    xcb_flush(_IswXcbConn(IswDisplayOf(ctx)));
  }

  IswFree((char *) buf);
  EndAction(ctx);
}


/* NoOp() - action
 * This action performs no action, and allows the user or
 * application programmer to unbind a translation.
 *
 * Note: If the parameter list contains the string "RingBell" then
 *       this action will ring the bell.
 */

/*ARGSUSED*/
static void
NoOp(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    if (*num_params != 1)
	return;

    switch(params[0][0]) {
    case 'R':
    case 'r':
	xcb_bell(_IswXcbConn(IswDisplayOf(w)), 0); // 0 = default volume
  xcb_flush(_IswXcbConn(IswDisplayOf(w)));
    default:			/* Fall Through */
	break;
    }
}

/* Reconnect() - action
 * This reconnects to the input method.  The user will typically call
 * this action if/when connection has been severed, or when the app
 * was started up before an IM was started up.
 */

/*ARGSUSED*/
static void
Reconnect(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    _IswImReconnect( w );
}


IswActionsRec _IswTextActionsTable[] = {

/* motion bindings */

  {"forward-character", 	MoveForwardChar},
  {"backward-character", 	MoveBackwardChar},
  {"forward-word", 		MoveForwardWord},
  {"backward-word", 		MoveBackwardWord},
  {"forward-paragraph", 	MoveForwardParagraph},
  {"backward-paragraph", 	MoveBackwardParagraph},
  {"beginning-of-line", 	MoveToLineStart},
  {"end-of-line", 		MoveToLineEnd},
  {"next-line", 		MoveNextLine},
  {"previous-line", 		MovePreviousLine},
  {"next-page", 		MoveNextPage},
  {"previous-page", 		MovePreviousPage},
  {"beginning-of-file", 	MoveBeginningOfFile},
  {"end-of-file", 		MoveEndOfFile},
  {"scroll-one-line-up", 	ScrollOneLineUp},
  {"scroll-one-line-down", 	ScrollOneLineDown},

/* delete bindings */

  {"delete-next-character", 	DeleteForwardChar},
  {"delete-previous-character", DeleteBackwardChar},
  {"delete-next-word", 		DeleteForwardWord},
  {"delete-previous-word", 	DeleteBackwardWord},
  {"delete-selection", 		DeleteCurrentSelection},

/* kill bindings */

  {"kill-word", 		KillForwardWord},
  {"backward-kill-word", 	KillBackwardWord},
  {"kill-selection", 		KillCurrentSelection},
  {"kill-to-end-of-line", 	KillToEndOfLine},
  {"kill-to-end-of-paragraph", 	KillToEndOfParagraph},

/* new line stuff */

  {"newline-and-indent", 	InsertNewLineAndIndent},
  {"newline-and-backup", 	InsertNewLineAndBackup},
  {"newline", 			InsertNewLine},

/* Selection stuff */

  {"select-word", 		SelectWord},
  {"select-all", 		SelectAll},
  {"select-start", 		SelectStart},
  {"select-adjust", 		SelectAdjust},
  {"select-end", 		SelectEnd},
  {"select-save",		SelectSave},
  {"extend-start", 		ExtendStart},
  {"extend-adjust", 		ExtendAdjust},
  {"extend-end", 		ExtendEnd},
  {"insert-selection",		InsertSelection},
  {"copy-selection",		CopySelection},

/* Miscellaneous */

  {"redraw-display", 		RedrawDisplay},
  {"insert-file", 		_IswTextInsertFile},
  {"search",		        _IswTextSearch},
  {"insert-char", 		InsertChar},
  {"insert-string",		InsertString},
  {"focus-in", 	 	        TextFocusIn},
  {"focus-out", 		TextFocusOut},
  {"enter-window", 	 	TextEnterWindow},
  {"leave-window", 		TextLeaveWindow},
  {"display-caret",		DisplayCaret},
  {"multiply",		        Multiply},
  {"form-paragraph",            FormParagraph},
  {"transpose-characters",      TransposeCharacters},
  {"no-op",                     NoOp},

/* Action to bind special translations for text Dialogs. */

  {"InsertFileAction",          _IswTextInsertFileAction},
  {"DoSearchAction",            _IswTextDoSearchAction},
  {"DoReplaceAction",           _IswTextDoReplaceAction},
  {"SetField",                  _IswTextSetField},
  {"PopdownSearchAction",       _IswTextPopdownSearchAction},

/* Reconnect to Input Method */
  {"reconnect-im",       Reconnect} /* Li Yuhong, Omron KK, 1991 */
};

Cardinal _IswTextActionsTableCount = IswNumber(_IswTextActionsTable);
