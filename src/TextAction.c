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
#include <X11/Intrinsic.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include "ISWXcbDraw.h"
#include <ISW/TextP.h>
#ifdef ISW_INTERNATIONALIZATION
#include <ISW/MultiSrcP.h>
#include <ISW/ISWImP.h>
#include "ISWI18n.h"
#endif
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

extern void _IswTextInsertFileAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
extern void _IswTextInsertFile(Widget, xcb_generic_event_t *, String *, Cardinal *);
extern void _IswTextSearch(Widget, xcb_generic_event_t *, String *, Cardinal *);
extern void _IswTextDoSearchAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
extern void _IswTextDoReplaceAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
extern void _IswTextSetField(Widget, xcb_generic_event_t *, String *, Cardinal *);
extern void _IswTextPopdownSearchAction(Widget, xcb_generic_event_t *, String *, Cardinal *);

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
extern xcb_atom_t * _IswTextSelectionList(TextWidget, String *, Cardinal);
extern void _IswTextPrepareToUpdate(TextWidget);
extern int _IswTextReplace(TextWidget, ISWTextPosition, ISWTextPosition, ISWTextBlock *);

/*
 * These are defined here
 */

static void GetSelection(Widget, xcb_timestamp_t, String *, Cardinal);
void _IswTextZapSelection(TextWidget, xcb_generic_event_t *, Boolean);

#ifdef ISW_INTERNATIONALIZATION
static void
ParameterError(Widget w, String param)
{
    String params[2];
    Cardinal num_params = 2;
    params[0] = XtName(w);
    params[1] = param;

    XtAppWarningMsg( XtWidgetToApplicationContext(w),
 "parameterError", "textAction", "IswError",
 "Widget: %s Parameter: %s",
 params, &num_params);
    xcb_bell(XtDisplay(w), 0); // 0 = default volume
    xcb_flush(XtDisplay(w));
}
#endif

static void
StartAction(TextWidget ctx, xcb_generic_event_t *event)
{
  _IswTextPrepareToUpdate(ctx);
  if (event != NULL) {
    uint8_t type = event->response_type & ~0x80;
    switch (type) {
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
      {
        xcb_button_press_event_t *bev = (xcb_button_press_event_t *)event;
        ctx->text.time = bev->time;
      }
      break;
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
      {
        xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;
        ctx->text.time = kev->time;
      }
      break;
    case XCB_MOTION_NOTIFY:
      {
        xcb_motion_notify_event_t *mev = (xcb_motion_notify_event_t *)event;
        ctx->text.time = mev->time;
      }
      break;
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
      {
        xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;
        ctx->text.time = cev->time;
      }
      break;
    }
  }
}

static void
NotePosition(TextWidget ctx, xcb_generic_event_t* event)
{
  uint8_t type = event->response_type & ~0x80;
  switch (type) {
  case XCB_BUTTON_PRESS:
  case XCB_BUTTON_RELEASE:
    {
      xcb_button_press_event_t *bev = (xcb_button_press_event_t *)event;
      ctx->text.ev_x = bev->event_x;
      ctx->text.ev_y = bev->event_y;
    }
    break;
  case XCB_KEY_PRESS:
  case XCB_KEY_RELEASE:
    {
      xcb_rectangle_t cursor;
      IswTextSinkGetCursorBounds(ctx->text.sink, &cursor);
      ctx->text.ev_x = cursor.x + cursor.width / 2;
      ctx->text.ev_y = cursor.y + cursor.height / 2;
    }
    break;
  case XCB_MOTION_NOTIFY:
    {
      xcb_motion_notify_event_t *mev = (xcb_motion_notify_event_t *)event;
      ctx->text.ev_x = mev->event_x;
      ctx->text.ev_y = mev->event_y;
    }
    break;
  case XCB_ENTER_NOTIFY:
  case XCB_LEAVE_NOTIFY:
    {
      xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;
      ctx->text.ev_x = cev->event_x;
      ctx->text.ev_y = cev->event_y;
    }
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
    xcb_timestamp_t time;
    Boolean CT_asked;	/* flag if asked XCB_ATOM_COMPOUND_TEXT */
    xcb_atom_t selection;	/* selection atom when asking XCB_ATOM_COMPOUND_TEXT */
};

#ifdef ISW_INTERNATIONALIZATION
static int
ProbablyMB(char *s)
{
    int escapes = 0;
    int has_hi_bit = False;

    /* if it has more than one ESC char, I assume it is COMPOUND_TEXT.
    If it has at least one hi bit set character, I pretend it is multibyte. */

    while ( (wchar_t)(*s) != (wchar_t)0 ) {
        if ( *s & 128 )
            has_hi_bit = True;
        if ( *s++ == '\033' )
            escapes++;
        if ( escapes >= 2 )
            return( 0 );
    }
    return( has_hi_bit );
}
#endif

/* ARGSUSED */
static void
_SelectionReceived(Widget w, XtPointer client_data, xcb_atom_t *selection, xcb_atom_t *type,
                   XtPointer value, unsigned long *length, int* format)
{
  TextWidget ctx = (TextWidget)w;
  ISWTextBlock text;

  if (*type == 0 /*XT_CONVERT_FAIL*/ || *length == 0) {
    struct _SelectionList* list = (struct _SelectionList*)client_data;
    if (list != NULL) {
      if (list->CT_asked) {

	/* If we just asked for a XCB_ATOM_COMPOUND_TEXT and got a null
	response, we'll ask again, this time for an XCB_ATOM_STRING. */

	list->CT_asked = False;
        XtGetSelectionValue(w, list->selection, XCB_ATOM_STRING, _SelectionReceived,
                            (XtPointer)list, list->time);
      } else {
	GetSelection(w, list->time, list->params, list->count);
	XtFree(client_data);
     }
    }
    return;
  }

  /* Many programs, especially old terminal emulators, give us multibyte text
but tell us it is COMPOUND_TEXT :(  The following routine checks to see if the
string is a legal multibyte string in our locale using a spooky heuristic :O
and if it is we can only assume the sending client is using the same locale as
we are, and convert it.  I also warn the user that the other client is evil. */

  StartAction( ctx, (xcb_generic_event_t*) NULL );
#ifdef ISW_INTERNATIONALIZATION
  if (_IswTextFormat(ctx) == IswFmtWide) {
#ifdef ISW_HAS_XIM
      XTextProperty textprop;
      xcb_connection_t *d = XtDisplay((Widget)ctx);
      wchar_t **wlist;
      int count;
      int try_CT = 1;

      /* IS THE SELECTION IN MULTIBYTE FORMAT? */

      if ( ProbablyMB( (char *) value ) ) {
          char * list[1];
          list[0] = (char *) value;
          if ( XmbTextListToTextProperty( d, (char**) list, 1,
				XCompoundTextStyle, &textprop ) == Success )
              try_CT = 0;
      }

      /* OR IN COMPOUND TEXT FORMAT? */

      if ( try_CT ) {
          textprop.encoding = XCB_ATOM_COMPOUND_TEXT(d);
          textprop.value = (unsigned char *)value;
          textprop.nitems = strlen(value);
          textprop.format = 8;
      }

      if ( XwcTextPropertyToTextList( d, &textprop, (wchar_t***) &wlist, &count )
		!=  Success) {
          XwcFreeStringList( (wchar_t**) wlist );

          /* Notify the user on strerr and in the insertion :) */
          textprop.value = (unsigned char *) " >> ILLEGAL SELECTION << ";
          count = 1;
          fprintf( stderr, "Isw Text Widget: An attempt was made to insert an illegal selection.\n" );

          if ( XwcTextPropertyToTextList( d, &textprop, (wchar_t***) &wlist, &count )
		!=  Success) return;
      }

      XFree(value);
      value = (XtPointer)wlist[0];

      *length = wcslen(wlist[0]);
      XtFree((XtPointer)wlist);
      text.format = IswFmtWide;
#else
      /* XCB: TextProperty I18N functions not available */
      /* Fall through to simple ASCII handling */
      fprintf(stderr, "Isw3d: Wide char selection not supported in XCB mode\n");
      /* Fall through to 8-bit handling below */
      text.format = IswFmt8Bit;
#endif
  } else
#endif
      text.format = IswFmt8Bit;
  text.ptr = (char*)value;
  text.firstPos = 0;
  text.length = *length;
  if (_IswTextReplace(ctx, ctx->text.insertPos, ctx->text.insertPos, &text)) {
    xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
    xcb_flush(XtDisplay(ctx));
    return;
  }
  ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
				IswstPositions, IswsdRight, text.length, TRUE);

  _IswTextSetScrollBars(ctx);
  EndAction(ctx);
  XtFree(client_data);
  XFree(value);		/* the selection value should be freed with XFree */
}


//#TODO replace usage of cut buffers with modern selection mechanism
static void
GetSelection(Widget w, xcb_timestamp_t time, String *params, Cardinal num_params)
{
    xcb_atom_t selection;
    int buffer;

    selection = IswXcbInternAtom(XtDisplay(w), *params, False);

    switch (selection) {
      case XCB_ATOM_CUT_BUFFER0: buffer = 0; break;
      case XCB_ATOM_CUT_BUFFER1: buffer = 1; break;
      case XCB_ATOM_CUT_BUFFER2: buffer = 2; break;
      case XCB_ATOM_CUT_BUFFER3: buffer = 3; break;
      case XCB_ATOM_CUT_BUFFER4: buffer = 4; break;
      case XCB_ATOM_CUT_BUFFER5: buffer = 5; break;
      case XCB_ATOM_CUT_BUFFER6: buffer = 6; break;
      case XCB_ATOM_CUT_BUFFER7: buffer = 7; break;
      default:	       buffer = -1;
    }
    if (buffer >= 0) {
	int nbytes;
	unsigned long length;
	int fmt8 = 8;
	xcb_atom_t type = XCB_ATOM_STRING;
	xcb_connection_t *connection = XtDisplay(w);
	xcb_screen_t *screen = XtScreen(w);
	char *line = NULL;

  xcb_get_property_cookie_t cookie = xcb_get_property(
      connection,
      0,                          // delete: false
      screen->root,               // window
      selection,       // property
      XCB_ATOM_STRING,            // type
      0,                          // long_offset
      4096                        // long_length (amount of data to fetch)
  );
  xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, NULL);
  if (reply) {
    nbytes = xcb_get_property_value_length(reply);
    if (nbytes > 0) {
        // Allocate space and copy the value
        line = XtMalloc(nbytes + 1);
        memcpy(line, xcb_get_property_value(reply), nbytes);
        line[nbytes] = '\0'; // Ensure null-termination
    }
    free(reply); // You must free the reply structure itself
  }
 

	if ((length = nbytes))
	    _SelectionReceived(w, (XtPointer) NULL, &selection, &type, (XtPointer)line,
			       &length, &fmt8);
	else if (num_params > 1)
	    GetSelection(w, time, params+1, num_params-1);
    } else {
	struct _SelectionList* list;
	if (--num_params) {
	    list = XtNew(struct _SelectionList);
	    list->params = params + 1;
	    list->count = num_params;
	    list->time = time;
	    list->CT_asked = True;
	    list->selection = selection;
	} else list = NULL;
	XtGetSelectionValue(w, selection, XCB_ATOM_COMPOUND_TEXT(XtDisplay(w)),
			    _SelectionReceived, (XtPointer)list, time);
    }
}

static void
InsertSelection(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  StartAction((TextWidget)w, event); /* Get Time. */
  GetSelection(w, ((TextWidget)w)->text.time, params, *num_params);
  EndAction((TextWidget)w);
}

/************************************************************
 *
 * Routines for Moving Around.
 *
 ************************************************************/

static void
Move(TextWidget ctx, xcb_generic_event_t *event, IswTextScanDirection dir,
     IswTextScanType type, Boolean include)
{
  StartAction(ctx, event);
  ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
				type, dir, ctx->text.mult, include);
  EndAction(ctx);
}

/*ARGSUSED*/
static void
MoveForwardChar(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
   Move((TextWidget) w, event, IswsdRight, IswstPositions, TRUE);
}

/*ARGSUSED*/
static void
MoveBackwardChar(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdLeft, IswstPositions, TRUE);
}

/*ARGSUSED*/
static void
MoveForwardWord(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdRight, IswstWhiteSpace, FALSE);
}

/*ARGSUSED*/
static void
MoveBackwardWord(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdLeft, IswstWhiteSpace, FALSE);
}

/*ARGSUSED*/
static void
MoveForwardParagraph(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdRight, IswstParagraph, FALSE);
}

/*ARGSUSED*/
static void
MoveBackwardParagraph(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdLeft, IswstParagraph, FALSE);
}

/*ARGSUSED*/
static void
MoveToLineEnd(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdRight, IswstEOL, FALSE);
}

/*ARGSUSED*/
static void
MoveToLineStart(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdLeft, IswstEOL, FALSE);
}


static void
MoveLine(TextWidget ctx, xcb_generic_event_t *event, IswTextScanDirection dir)
{
  ISWTextPosition new, next_line, junk;
  int from_left, garbage;

  StartAction(ctx, event);

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
MoveNextLine(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  MoveLine( (TextWidget) w, event, IswsdRight);
}

/*ARGSUSED*/
static void
MovePreviousLine(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  MoveLine( (TextWidget) w, event, IswsdLeft);
}

/*ARGSUSED*/
static void
MoveBeginningOfFile(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdLeft, IswstAll, TRUE);
}

/*ARGSUSED*/
static void
MoveEndOfFile(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Move((TextWidget) w, event, IswsdRight, IswstAll, TRUE);
}

static void
Scroll(TextWidget ctx, xcb_generic_event_t *event, IswTextScanDirection dir)
{
  StartAction(ctx, event);

  if (dir == IswsdLeft)
    _IswTextVScroll(ctx, ctx->text.mult);
  else
    _IswTextVScroll(ctx, -ctx->text.mult);

  EndAction(ctx);
}

/*ARGSUSED*/
static void
ScrollOneLineUp(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Scroll( (TextWidget) w, event, IswsdLeft);
}

/*ARGSUSED*/
static void
ScrollOneLineDown(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  Scroll( (TextWidget) w, event, IswsdRight);
}

static void
MovePage(TextWidget ctx, xcb_generic_event_t *event, IswTextScanDirection dir)
{
  int scroll_val = Max(1, ctx->text.lt.lines - 2);

  if (dir == IswsdLeft)
    scroll_val = -scroll_val;

  StartAction(ctx, event);
  _IswTextVScroll(ctx, scroll_val);
  ctx->text.insertPos = ctx->text.lt.top;
  EndAction(ctx);
}

/*ARGSUSED*/
static void
MoveNextPage(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  MovePage((TextWidget) w, event, IswsdRight);
}

/*ARGSUSED*/
static void
MovePreviousPage(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  MovePage((TextWidget) w, event, IswsdLeft);
}

/************************************************************
 *
 * Delete Routines.
 *
 ************************************************************/

static Boolean
MatchSelection(xcb_atom_t selection, IswTextSelection *s)
{
    xcb_atom_t    *match;
    int	    count;

    for (count = 0, match = s->selections; count < s->atom_count; match++, count++)
	  if (*match == selection)
	      return True;
    return False;
}

#define SrcCvtSel	IswTextSourceConvertSelection

/*
 * IswConvertStandardSelection - Stub for standard widget selection conversion
 *
 * In a full implementation, this would handle standard Xt selection targets like
 * TIMESTAMP, HOSTNAME, etc. For now, we return an empty list and let the text
 * widget provide its own targets.
 */
static Boolean
IswConvertStandardSelection(Widget w, xcb_timestamp_t time, xcb_atom_t *selection,
                           xcb_atom_t *target, xcb_atom_t *type,
                           XtPointer *value, unsigned long *length, int *format)
{
    (void)w; (void)time; (void)selection; (void)target; (void)format;
    
    /* Return empty list - the caller will add text-specific targets */
    *value = (XtPointer)XtMalloc(0);
    *length = 0;
    *type = XCB_ATOM_ATOM;
    return True;
}

static Boolean
ConvertSelection(Widget w, xcb_atom_t *selection, xcb_atom_t *target, xcb_atom_t *type,
                 XtPointer* value, unsigned long *length, int *format)
{
  xcb_connection_t* d = XtDisplay(w);
  TextWidget ctx = (TextWidget)w;
  Widget src = ctx->text.source;
  IswTextEditType edit_mode;
  Arg args[1];
  IswTextSelectionSalt	*salt = NULL;
  IswTextSelection  *s;

  if (*target == XCB_ATOM_TARGETS(d)) {
    xcb_atom_t* targetP, * std_targets;
    unsigned long std_length;

    if ( SrcCvtSel(src, selection, target, type, value, length, format) )
	return True;

    IswConvertStandardSelection(w, ctx->text.time, selection,
				target, type, (XtPointer*)&std_targets,
				&std_length, format);

    *value = XtMalloc((unsigned) sizeof(xcb_atom_t)*(std_length + 7));
    targetP = *(xcb_atom_t**)value;

    *length = std_length + 6;
    *targetP++ = XCB_ATOM_STRING;
    *targetP++ = XCB_ATOM_TEXT(d);
    *targetP++ = XCB_ATOM_COMPOUND_TEXT(d);
    *targetP++ = XCB_ATOM_LENGTH(d);
    *targetP++ = XCB_ATOM_LIST_LENGTH(d);
    *targetP++ = XCB_ATOM_CHARACTER_POSITION(d);

    XtSetArg(args[0], XtNeditType,&edit_mode);
    XtGetValues(src, args, 1);

    if (edit_mode == IswtextEdit) {
      *targetP++ = XCB_ATOM_DELETE(d);
      (*length)++;
    }
    memcpy((char*)targetP, (char*)std_targets, sizeof(xcb_atom_t)*std_length);
    XtFree((char*)std_targets);
    *type = XCB_ATOM_ATOM;
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
  if (*target == XCB_ATOM_STRING ||
      *target == XCB_ATOM_TEXT(d) ||
      *target == XCB_ATOM_COMPOUND_TEXT(d)) {
	if (*target == XCB_ATOM_TEXT(d)) {
#ifdef ISW_INTERNATIONALIZATION
	    if (_IswTextFormat(ctx) == IswFmtWide)
		*type = XCB_ATOM_COMPOUND_TEXT(d);
	    else
#endif
		*type = XCB_ATOM_STRING;
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
#ifdef ISW_INTERNATIONALIZATION
	    if (_IswTextFormat(ctx) == IswFmtWide) {
#ifdef ISW_HAS_XIM
		XTextProperty textprop;
		if (XwcTextListToTextProperty(d, (wchar_t**)value, 1,
					      XCompoundTextStyle, &textprop)
			< Success) {
		    XtFree(*value);
		    return False;
		}
		XtFree(*value);
		*value = (XtPointer)textprop.value;
		*length = textprop.nitems;
#else
		/* XCB: TextProperty I18N functions not available */
		/* For now, return the raw wide char data as-is */
		fprintf(stderr, "Isw3d: Wide char selection conversion not supported in XCB mode\n");
		/* Fall through to length calculation below */
		*length = wcslen((wchar_t*)*value);
#endif
	    } else
#endif
	    {
		*length = strlen(*value);
	    }
	} else {
	    *value = XtMalloc((salt->length + 1) * sizeof(unsigned char));
	    strcpy (*value, salt->contents);
	    *length = salt->length;
	}
#ifdef ISW_INTERNATIONALIZATION
	if (_IswTextFormat(ctx) == IswFmtWide && *type == XCB_ATOM_STRING) {
#ifdef ISW_HAS_XIM
	    XTextProperty textprop;
	    wchar_t** wlist;
	    int count;
	    textprop.encoding = XCB_ATOM_COMPOUND_TEXT(d);
	    textprop.value = (unsigned char *)*value;
	    textprop.nitems = strlen(*value);
	    textprop.format = 8;
	    if (XwcTextPropertyToTextList(d, &textprop, (wchar_t***)&wlist, &count)
			< Success) {
		XtFree(*value);
		return False;
	    }
	    XtFree(*value);
	    if (XwcTextListToTextProperty(d, (wchar_t**)wlist, 1,
					  XStringStyle, &textprop) < Success) {
		XwcFreeStringList( (wchar_t**) wlist );
		return False;
	    }
	    *value = (XtPointer)textprop.value;
	    *length = textprop.nitems;
	    XwcFreeStringList( (wchar_t**) wlist );
#else
	    /* XCB: TextProperty I18N functions not available */
	    fprintf(stderr, "Isw3d: Wide char selection conversion not supported in XCB mode\n");
	    /* Leave value and length as-is */
#endif
	}
#endif
	*format = 8;
	return True;
  }

  if ( (*target == XCB_ATOM_LIST_LENGTH(d)) || (*target == XCB_ATOM_LENGTH(d)) ) {
    long * temp;

    temp = (long *) XtMalloc(sizeof(long));
    if (*target == XCB_ATOM_LIST_LENGTH(d))
      *temp = 1L;
    else			/* *target == XCB_ATOM_LENGTH(d) */
      *temp = (long) (s->right - s->left);

    *value = (XtPointer) temp;
    *type = XCB_ATOM_INTEGER;
    *length = 1L;
    *format = 32;
    return True;
  }

  if (*target == XCB_ATOM_CHARACTER_POSITION(d)) {
    long * temp;

    temp = (long *) XtMalloc(2 * sizeof(long));
    temp[0] = (long) (s->left + 1);
    temp[1] = s->right;
    *value = (XtPointer) temp;
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(d, 0, 5, "SPAN");
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(d, cookie, NULL);
    if(reply) {
      *type = reply->atom;
      free(reply);
    } else {
      *type = XCB_ATOM_NONE;
    }
    *length = 2L;
    *format = 32;
    return True;
  }

  if (*target == XCB_ATOM_DELETE(d)) {
    if (!salt)
	_IswTextZapSelection( ctx, (xcb_generic_event_t *) NULL, TRUE);
    *value = NULL;
    *type = XCB_ATOM_NONE;
    *length = 0;
    *format = 32;
    return True;
  }

  if (IswConvertStandardSelection(w, ctx->text.time, selection, target, type,
				  (XtPointer *)value, length, format))
    return True;

  /* else */
  return False;
}

static void
LoseSelection(Widget w, xcb_atom_t *selection)
{
  TextWidget ctx = (TextWidget) w;
  xcb_atom_t* atomP;
  int i;
  IswTextSelectionSalt	*salt, *prevSalt, *nextSalt;

    prevSalt = 0;
    for (salt = ctx->text.salt2; salt; salt = nextSalt)
    {
    	atomP = salt->s.selections;
	nextSalt = salt->next;
    	for (i = 0 ; i < salt->s.atom_count; i++, atomP++)
	    if (*selection == *atomP)
		*atomP = (xcb_atom_t)0;

    	while (salt->s.atom_count &&
	       salt->s.selections[salt->s.atom_count-1] == 0)
	{
	    salt->s.atom_count--;
	}

    	/*
    	 * Must walk the selection list in opposite order from UnsetSelection.
    	 */

    	atomP = salt->s.selections;
    	for (i = 0 ; i < salt->s.atom_count; i++, atomP++)
    	    if (*atomP == (xcb_atom_t)0)
 	    {
      	      *atomP = salt->s.selections[--salt->s.atom_count];
      	      while (salt->s.atom_count &&
	     	     salt->s.selections[salt->s.atom_count-1] == 0)
    	    	salt->s.atom_count--;
    	    }
	if (salt->s.atom_count == 0)
	{
	    XtFree ((char *) salt->s.selections);

            /* WARNING: the next line frees memory not allocated in Isw. */
            /* Could be a serious bug.  Someone look into it. */
	    XtFree (salt->contents);
	    if (prevSalt)
		prevSalt->next = nextSalt;
	    else
		ctx->text.salt2 = nextSalt;
	    XtFree ((char *) salt);
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
    xcb_atom_t selection = IswXcbInternAtom(XtDisplay(ctx), "SECONDARY", False);

    LoseSelection ((Widget) ctx, &selection);
    salt = (IswTextSelectionSalt *) XtMalloc (sizeof (IswTextSelectionSalt));
    if (!salt)
	return;
    salt->s.selections = (xcb_atom_t *) XtMalloc (sizeof (xcb_atom_t));
    if (!salt->s.selections)
    {
	XtFree ((char *) salt);
	return;
    }
    salt->s.left = from;
    salt->s.right = to;
    salt->contents = (char *)_IswTextGetSTRING(ctx, from, to);
#ifdef ISW_INTERNATIONALIZATION
    if (_IswTextFormat(ctx) == IswFmtWide) {
#ifdef ISW_HAS_XIM
	XTextProperty textprop;
	if (XwcTextListToTextProperty(XtDisplay((Widget)ctx),
			(wchar_t**)(&(salt->contents)), 1, XCompoundTextStyle,
			&textprop) <  Success) {
	    XtFree(salt->contents);
	    salt->length = 0;
	    return;
	}
	XtFree(salt->contents);
	salt->contents = (char *)textprop.value;
	salt->length = textprop.nitems;
#else
	/* XCB: TextProperty I18N functions not available */
	fprintf(stderr, "Isw3d: Wide char selection conversion not supported in XCB mode\n");
	salt->length = wcslen((wchar_t*)salt->contents);
#endif
    } else
#endif
       salt->length = strlen (salt->contents);
    salt->next = ctx->text.salt2;
    ctx->text.salt2 = salt;
    salt->s.selections[0] = selection;
    XtOwnSelection ((Widget) ctx, selection, ctx->text.time,
		    ConvertSelection, LoseSelection, NULL);
    salt->s.atom_count = 1;
/*
    XStoreBuffer(XtDisplay(ctx), ptr, strlen(ptr), 1);
    XtFree(ptr);
*/
  }
  text.length = 0;
  text.firstPos = 0;

  text.format = _IswTextFormat(ctx);
  text.ptr = "";	/* These two lines needed to make legal TextBlock */

  if (_IswTextReplace(ctx, from, to, &text)) {
    xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
    xcb_flush(XtDisplay(ctx));
    return;
  }
  ctx->text.insertPos = from;
  ctx->text.showposition = TRUE;
}

static void
DeleteOrKill(TextWidget ctx, xcb_generic_event_t *event, IswTextScanDirection dir,
             IswTextScanType type, Boolean include, Boolean kill)
{
  ISWTextPosition from, to;

  StartAction(ctx, event);
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
DeleteForwardChar(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, event, IswsdRight, IswstPositions, TRUE, FALSE);
}

/*ARGSUSED*/
static void
DeleteBackwardChar(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, event, IswsdLeft, IswstPositions, TRUE, FALSE);
}

/*ARGSUSED*/
static void
DeleteForwardWord(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, event,
	       IswsdRight, IswstWhiteSpace, FALSE, FALSE);
}

/*ARGSUSED*/
static void
DeleteBackwardWord(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, event,
	       IswsdLeft, IswstWhiteSpace, FALSE, FALSE);
}

/*ARGSUSED*/
static void
KillForwardWord(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, event,
	       IswsdRight, IswstWhiteSpace, FALSE, TRUE);
}

/*ARGSUSED*/
static void
KillBackwardWord(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, event,
	       IswsdLeft, IswstWhiteSpace, FALSE, TRUE);
}

/*ARGSUSED*/
static void
KillToEndOfLine(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition end_of_line;

  StartAction(ctx, event);
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
KillToEndOfParagraph(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  DeleteOrKill((TextWidget) w, event, IswsdRight, IswstParagraph, FALSE, TRUE);
}

void
_IswTextZapSelection(TextWidget ctx, xcb_generic_event_t *event, Boolean kill)
{
   StartAction(ctx, event);
   _DeleteOrKill(ctx, ctx->text.s.left, ctx->text.s.right, kill);
  _IswTextSetScrollBars(ctx);
   EndAction(ctx);
}

/*ARGSUSED*/
static void
KillCurrentSelection(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  _IswTextZapSelection( (TextWidget) w, event, TRUE);
}

/*ARGSUSED*/
static void
DeleteCurrentSelection(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  _IswTextZapSelection( (TextWidget) w, event, FALSE);
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

#ifdef ISW_INTERNATIONALIZATION
  if ( text.format == IswFmtWide ) {
      wchar_t* wptr;
      text.ptr =  XtMalloc(sizeof(wchar_t) * ctx->text.mult);
      wptr = (wchar_t *)text.ptr;
      for (count = 0; count < ctx->text.mult; count++ )
          wptr[count] = _Isw_atowc(IswLF);
  }
  else
#endif
  {
      text.ptr = XtMalloc(sizeof(char) * ctx->text.mult);
      for (count = 0; count < ctx->text.mult; count++ )
          text.ptr[count] = IswLF;
  }

  if (_IswTextReplace(ctx, ctx->text.insertPos, ctx->text.insertPos, &text)) {
    xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
    xcb_flush(XtDisplay(ctx));
    error = IswEditError;
  }
  else
    ctx->text.showposition = TRUE;

  XtFree( text.ptr );
  return( error );
}

/*ARGSUSED*/
static void
InsertNewLineAndBackup(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  StartAction( (TextWidget) w, event );
  (void) InsertNewLineAndBackupInternal( (TextWidget) w );
  _IswTextSetScrollBars( (TextWidget) w);
  EndAction( (TextWidget) w );
}

static int
LocalInsertNewLine(TextWidget ctx, xcb_generic_event_t *event)
{
  StartAction(ctx, event);
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
InsertNewLine(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  (void) LocalInsertNewLine( (TextWidget) w, event);
}

/*ARGSUSED*/
static void
InsertNewLineAndIndent(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  ISWTextBlock text;
  ISWTextPosition pos1;
  int length;
  TextWidget ctx = (TextWidget) w;
  String line_to_ip;

  StartAction(ctx, event);
  pos1 = SrcScan(ctx->text.source, ctx->text.insertPos,
		 IswstEOL, IswsdLeft, 1, FALSE);

  line_to_ip = _IswTextGetText(ctx, pos1, ctx->text.insertPos);

  text.format = _IswTextFormat(ctx);
  text.firstPos = 0;

#ifdef ISW_INTERNATIONALIZATION
  if ( text.format == IswFmtWide ) {
     wchar_t* ptr;
     text.ptr = XtMalloc( ( 2 + wcslen((wchar_t*)line_to_ip) ) * sizeof(wchar_t) );

     ptr = (wchar_t*)text.ptr;
     ptr[0] = _Isw_atowc( IswLF );
     wcscpy( (wchar_t*) ++ptr, (wchar_t*) line_to_ip );

     length = wcslen((wchar_t*)text.ptr);
     while ( length && ( iswspace(*ptr) || ( *ptr == _Isw_atowc(IswTAB) ) ) )
         ptr++, length--;
     *ptr = (wchar_t)0;
     text.length = wcslen((wchar_t*)text.ptr);

  } else
#endif
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
     text.ptr = XtMalloc( ( 2 + length ) * sizeof( char ) );

     ptr = text.ptr;
     ptr[0] = IswLF;
     strcpy( ++ptr, line_to_ip );

     length++;
     while ( length && ( isspace(*ptr) || ( *ptr == IswTAB ) ) )
         ptr++, length--;
     *ptr = '\0';
     text.length = strlen(text.ptr);
  }
  XtFree( line_to_ip );

  if (_IswTextReplace(ctx,ctx->text.insertPos, ctx->text.insertPos, &text)) {
    xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
    xcb_flush(XtDisplay(ctx));
    XtFree(text.ptr);
    EndAction(ctx);
    return;
  }
  XtFree(text.ptr);
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
SelectWord(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition l, r;

  StartAction(ctx, event);
  l = SrcScan(ctx->text.source, ctx->text.insertPos,
	      IswstWhiteSpace, IswsdLeft, 1, FALSE);
  r = SrcScan(ctx->text.source, l, IswstWhiteSpace, IswsdRight, 1, FALSE);
  _IswTextSetSelection(ctx, l, r, params, *num_params);
  EndAction(ctx);
}

static void
SelectAll(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;

  StartAction(ctx, event);
  _IswTextSetSelection(ctx,zeroPosition,ctx->text.lastPos,params,*num_params);
  EndAction(ctx);
}

static void
ModifySelection(TextWidget ctx, xcb_generic_event_t *event, IswTextSelectionMode mode,
                IswTextSelectionAction action, String *params, Cardinal *num_params)
{
  StartAction(ctx, event);
  NotePosition(ctx, event);
  _IswTextAlterSelection(ctx, mode, action, params, num_params);
  EndAction(ctx);
}

/* ARGSUSED */
static void
SelectStart(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, event,
		  IswsmTextSelect, IswactionStart, params, num_params);
}

/* ARGSUSED */
static void
SelectAdjust(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, event,
		  IswsmTextSelect, IswactionAdjust, params, num_params);
}

static void
SelectEnd(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, event,
		  IswsmTextSelect, IswactionEnd, params, num_params);
}

/* ARGSUSED */
static void
ExtendStart(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, event,
		  IswsmTextExtend, IswactionStart, params, num_params);
}

/* ARGSUSED */
static void
ExtendAdjust(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, event,
		  IswsmTextExtend, IswactionAdjust, params, num_params);
}

static void
ExtendEnd(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  ModifySelection((TextWidget) w, event,
		  IswsmTextExtend, IswactionEnd, params, num_params);
}

static void
SelectSave(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    int	    num_atoms;
    xcb_atom_t*   sel;
    xcb_connection_t* dpy = XtDisplay(w);
    xcb_atom_t    selections[256];

    StartAction(  (TextWidget) w, event );
    num_atoms = *num_params;
    if (num_atoms > 256) num_atoms = 256;
    for (sel=selections; --num_atoms >= 0; sel++, params++)
	    *sel = IswXcbInternAtom(dpy, *params, False);// XInternAtom(dpy, *params, False);
    num_atoms = *num_params;
    _IswTextSaltAwaySelection( (TextWidget) w, selections, num_atoms );
    EndAction(  (TextWidget) w );
}

/************************************************************
 *
 * Misc. Routines.
 *
 ************************************************************/

/* ARGSUSED */
static void
RedrawDisplay(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  StartAction( (TextWidget) w, event);
  _IswTextClearAndCenterDisplay((TextWidget) w);
  EndAction( (TextWidget) w);
}

/*ARGSUSED*/
static void
TextFocusIn (Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;

  if ((event->response_type & ~0x80) != XCB_FOCUS_IN) {
    return;
  }
  xcb_focus_in_event_t *fev = (xcb_focus_in_event_t *)event;

  /* Let the input method know focus has arrived. */
#ifdef ISW_INTERNATIONALIZATION
  _IswImSetFocusValues (w, NULL, 0);
#endif
  if (fev->detail == XCB_NOTIFY_DETAIL_POINTER) {
      return;
  }

  ctx->text.hasfocus = TRUE;
}

/*ARGSUSED*/
static void
TextFocusOut(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;

  if ((event->response_type & ~0x80) != XCB_FOCUS_IN) {
    return;
  }
  xcb_focus_in_event_t *fev = (xcb_focus_in_event_t *)event;

  /* Let the input method know focus has left.*/
#ifdef ISW_INTERNATIONALIZATION
  _IswImUnsetFocus(w);
#endif
  if (fev->detail == XCB_NOTIFY_DETAIL_POINTER) {
      return;
  }
  ctx->text.hasfocus = FALSE;
}

/*ARGSUSED*/
static void
TextEnterWindow(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
#ifdef ISW_INTERNATIONALIZATION
  TextWidget ctx = (TextWidget) w;
  xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;

  if ((cev->detail != XCB_NOTIFY_DETAIL_INFERIOR) && (cev->same_screen_focus & 1) &&
      !ctx->text.hasfocus) {
	_IswImSetFocusValues(w, NULL, 0);
  }
#endif
}

/*ARGSUSED*/
static void
TextLeaveWindow(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
#ifdef ISW_INTERNATIONALIZATION
  TextWidget ctx = (TextWidget) w;
  xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;

  if ((cev->detail != XCB_NOTIFY_DETAIL_INFERIOR) && (cev->same_screen_focus & 1) &&
      !ctx->text.hasfocus) {
	_IswImUnsetFocus(w);
  }
#endif
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
#ifdef ISW_INTERNATIONALIZATION
  if (_IswTextFormat(ctx) == IswFmtWide) {
    text.format = IswFmtWide;
    text.ptr =  (char *)XtMalloc(sizeof(wchar_t) * 2);
    ((wchar_t*)text.ptr)[0] = _Isw_atowc(IswLF);
    ((wchar_t*)text.ptr)[1] = 0;
  } else
#endif
    text.ptr = "\n";
  text.length = 1;
  text.firstPos = 0;

  if (_IswTextReplace(ctx, ret_pos - 1, ret_pos, &text)) {
      xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
      xcb_flush(XtDisplay(ctx));
  }
}

/*ARGSUSED*/
static void
InsertChar(Widget w, xcb_generic_event_t *event, String *p, Cardinal *n)
{
  TextWidget ctx = (TextWidget) w;
  char *ptr, strbuf[BUFSIZ];
  int count, error;
  KeySym keysym;
  ISWTextBlock text;

#ifdef ISW_INTERNATIONALIZATION
  if (XtIsSubclass (ctx->text.source, (WidgetClass) multiSrcObjectClass)) {
    Status status;
    xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;
    text.length = _IswImWcLookupString (w, (XKeyPressedEvent*)kev,
		(wchar_t*) strbuf, BUFSIZ, &keysym, &status);
  } else
#endif
  {
    /* Non-I18N path: convert XCB key event to character using XCB keysyms */
    xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;
    xcb_connection_t *conn = XtDisplay(w);
    static xcb_key_symbols_t *keysyms = NULL;
    
    /* Initialize key symbols context if needed */
    if (keysyms == NULL) {
      keysyms = xcb_key_symbols_alloc(conn);
    }
    
    if (keysyms != NULL) {
      /* Determine shift state from modifier mask */
      int col = 0;
      if (kev->state & ShiftMask)
        col = 1;

      /* Get keysym from keycode, using shift column */
      xcb_keysym_t sym = xcb_key_symbols_get_keysym(keysyms, kev->detail, col);

      /* Fall back to unshifted if shifted column has no symbol */
      if (sym == XCB_NO_SYMBOL && col == 1)
        sym = xcb_key_symbols_get_keysym(keysyms, kev->detail, 0);

      /* Handle CapsLock: toggle case for alphabetic keys */
      if ((kev->state & LockMask) && !(kev->state & ShiftMask)) {
        /* CapsLock without Shift: get shifted (uppercase) for letters */
        if (sym >= 'a' && sym <= 'z') {
          xcb_keysym_t usym = xcb_key_symbols_get_keysym(keysyms, kev->detail, 1);
          if (usym != XCB_NO_SYMBOL)
            sym = usym;
        }
      } else if ((kev->state & LockMask) && (kev->state & ShiftMask)) {
        /* CapsLock with Shift: get unshifted (lowercase) for letters */
        if (sym >= 'A' && sym <= 'Z') {
          xcb_keysym_t lsym = xcb_key_symbols_get_keysym(keysyms, kev->detail, 0);
          if (lsym != XCB_NO_SYMBOL)
            sym = lsym;
        }
      }

      keysym = sym;

      /* Convert keysym to character */
      if (sym >= 0x20 && sym <= 0x7E) {
        /* Printable ASCII */
        strbuf[0] = (char)sym;
        text.length = 1;
      } else if (sym >= 0xA0 && sym <= 0xFF) {
        /* Latin-1 supplement */
        strbuf[0] = (char)sym;
        text.length = 1;
      } else if (sym == 0xFF0D || sym == 0xFF8D) {
        /* Return/Enter key */
        strbuf[0] = '\r';
        text.length = 1;
      } else if (sym == 0xFF09) {
        /* Tab key */
        strbuf[0] = '\t';
        text.length = 1;
      } else {
        /* Non-printable or special key */
        text.length = 0;
      }
    } else {
      text.length = 0;
      keysym = 0;
    }
  }

  if (text.length == 0)
      return;

  text.format = _IswTextFormat( ctx );
#ifdef ISW_INTERNATIONALIZATION
  if ( text.format == IswFmtWide ) {
      text.ptr = ptr = XtMalloc(sizeof(wchar_t) * text.length * ctx->text.mult );
      for (count = 0; count < ctx->text.mult; count++ ) {
          memcpy((char*) ptr, (char *)strbuf, sizeof(wchar_t) * text.length );
          ptr += sizeof(wchar_t) * text.length;
      }

  } else
#endif
  { /* == IswFmt8Bit */
      text.ptr = ptr = XtMalloc( sizeof(char) * text.length * ctx->text.mult );
      for ( count = 0; count < ctx->text.mult; count++ ) {
          strncpy( ptr, strbuf, text.length );
          ptr += text.length;
      }
  }

  text.length = text.length * ctx->text.mult;
  text.firstPos = 0;

  StartAction(ctx, event);

  error = _IswTextReplace(ctx, ctx->text.insertPos,ctx->text.insertPos, &text);

  if (error == IswEditDone) {
      ctx->text.insertPos = SrcScan(ctx->text.source, ctx->text.insertPos,
	      IswstPositions, IswsdRight, text.length, TRUE);
      AutoFill(ctx);
  }
  else {
      xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
      xcb_flush(XtDisplay(ctx));
  }

  XtFree(text.ptr);
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
IfHexConvertHexElseReturnParam(char *param, int *len_return)
{
  char *p;                     /* steps through param char by char */
  char c;                      /* holds the character pointed to by p */

  int ind;		       /* steps through hexval buffer char by char */
  static char hexval[ IswTextActionMaxHexChars ];
  Boolean first_digit;

  /* reject if it doesn't begin with 0x and at least one more character. */

  if ( ( param[0] != '0' ) || ( param[1] != 'x' ) || ( param[2] == '\0' ) ) {
      *len_return = strlen( param );
      return( param );
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
              return( param );
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
  return( param );			   /* ...return the verbatim string. */
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
InsertString(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
#ifdef ISW_INTERNATIONALIZATION
  XtAppContext app_con = XtWidgetToApplicationContext(w);
#endif
  ISWTextBlock text;
  int	   i;

  text.firstPos = 0;
  text.format = _IswTextFormat( ctx );

  StartAction(ctx, event);
  for ( i = *num_params; i; i--, params++ ) { /* DO FOR EACH PARAMETER */

      text.ptr = IfHexConvertHexElseReturnParam( *params, &text.length );

      if ( text.length == 0 ) continue;

#ifdef ISW_INTERNATIONALIZATION
      if ( _IswTextFormat( ctx ) == IswFmtWide ) { /* convert to WC */

          int temp_len;
          text.ptr = (char*) _ISWTextMBToWC( XtDisplay(w), text.ptr,
					      &text.length );

          if ( text.ptr == NULL ) { /* conversion error */
              XtAppWarningMsg( app_con,
		"insertString", "textAction", "IswError",
		"insert-string()'s parameter contents not legal in this locale.",
		NULL, NULL );
              ParameterError( w, *params );
              continue;
          }

          /* Double check that the new input is legal: try to convert to MB. */

          temp_len = text.length;      /* _ISWTextWCToMB's 3rd arg is in_out */
          if ( _ISWTextWCToMB( XtDisplay(w), (wchar_t*)text.ptr, &temp_len ) == NULL ) {
              XtAppWarningMsg( app_con,
		"insertString", "textAction", "IswError",
		"insert-string()'s parameter contents not legal in this locale.",
				NULL, NULL );
              ParameterError( w, *params );
              continue;
          }
      } /* convert to WC */
#endif

      if ( _IswTextReplace( ctx, ctx->text.insertPos,
			    ctx->text.insertPos, &text ) ) {
          xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
          xcb_flush(XtDisplay(ctx));
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
DisplayCaret(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget)w;
  Boolean display_caret = True;
  uint32_t eventType = event->response_type & ~0x80;

  if  ( ( eventType == XCB_ENTER_NOTIFY || eventType == XCB_LEAVE_NOTIFY ) &&
        ( ( *num_params >= 2 ) && ( strcmp( params[1], "always" ) == 0 ) ) &&
        ( ((xcb_enter_notify_event_t *)event)->same_screen_focus) )
      return;

  if (*num_params > 0) {	/* default arg is "True" */
      XrmValue from, to;
      Boolean converted_value;
      from.size = strlen(from.addr = params[0]);
      to.size = sizeof(Boolean);
      to.addr = (XPointer)&converted_value;
      
      if ( XtConvertAndStore( w, XtRString, &from, XtRBoolean, &to ) )
          display_caret = converted_value;
      if ( ctx->text.display_caret == display_caret )
          return;
  }
  StartAction(ctx, event);
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
Multiply(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  int mult;

  if (*num_params != 1) {
      XtAppError( XtWidgetToApplicationContext( w ),
	       "Isw Text Widget: multiply() takes exactly one argument.");
      xcb_bell(XtDisplay(w), 0); // 0 = default volume
      xcb_flush(XtDisplay(w));
      return;
  }

  if ( ( params[0][0] == 'r' ) || ( params[0][0] == 'R' ) ) {
      xcb_bell(XtDisplay(w), 0); // 0 = default volume
      xcb_flush(XtDisplay(w));
      ctx->text.mult = 1;
      return;
  }

  if ( ( mult = atoi( params[0] ) ) == 0 ) {
      char buf[ BUFSIZ ];
      sprintf(buf, "%s %s", "Isw Text Widget: multiply() argument",
	    "must be a number greater than zero, or 'Reset'." );
      XtAppError( XtWidgetToApplicationContext( w ), buf );
      xcb_bell(XtDisplay(w), 0); // 0 = default volume
      xcb_flush(XtDisplay(w));
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
      text.ptr= "  ";
#ifdef ISW_INTERNATIONALIZATION
  else {
      static wchar_t wc_two_spaces[ 3 ];
      wc_two_spaces[0] = _Isw_atowc(IswSP);
      wc_two_spaces[1] = _Isw_atowc(IswSP);
      wc_two_spaces[2] = 0;
      text.ptr = (char*) wc_two_spaces;
  }
#endif

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
#ifdef ISW_INTERNATIONALIZATION
      if (text.format == IswFmtWide) {
        if ( (periodPos < endPos) && (((wchar_t*)buf)[0] == _Isw_atowc('.')))
          text.length++;
      } else
#endif
        if ( (periodPos < endPos) && (buf[0] == '.') )
	  text.length++;	/* Put in two spaces. */

      /*
       * Remove all extra spaces.
       */

      for (i = 1 ; i < len; i++)
#ifdef ISW_INTERNATIONALIZATION
        if (text.format ==  IswFmtWide) {
          if ( !iswspace(((wchar_t*)buf)[i]) || ((periodPos + i) >= to) ) {
             break;
          }
        } else
#endif
	  if ( !isspace(buf[i]) || ((periodPos + i) >= to) ) {
	      break;
	  }

      XtFree(buf);

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
      text.ptr = "\n";
#ifdef ISW_INTERNATIONALIZATION
  else {
      static wchar_t wide_CR[ 2 ];
      wide_CR[0] = _Isw_atowc(IswLF);
      wide_CR[1] = 0;
      text.ptr = (char*) wide_CR;
  }
#endif

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
#ifdef ISW_INTERNATIONALIZATION
      if (text.format == IswFmtWide) {
          if (!iswspace(((wchar_t*)buf)[i]))
              break;
      } else
#endif
          if (!isspace(buf[i]))
              break;

      to -= (i - 1);
      endPos = SrcScan(ctx->text.source, endPos,
		     IswstPositions, IswsdRight, i, TRUE);
      XtFree(buf);

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
FormParagraph(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition from, to;

  StartAction(ctx, event);

  from =  SrcScan( ctx->text.source, ctx->text.insertPos,
		  IswstParagraph, IswsdLeft, 1, FALSE );
  to  =  SrcScan( ctx->text.source, from,
		 IswstParagraph, IswsdRight, 1, FALSE );

  if ( FormRegion( ctx, from, to ) == IswReplaceError ) {
      xcb_bell(XtDisplay(w), 0); // 0 = default volume
      xcb_flush(XtDisplay(w));
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
TransposeCharacters(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) w;
  ISWTextPosition start, end;
  ISWTextBlock text;
  char* buf;
  int i;

  StartAction(ctx, event);

  /* Get bounds. */

  start = SrcScan( ctx->text.source, ctx->text.insertPos, IswstPositions,
		  IswsdLeft, 1, TRUE );
  end = SrcScan( ctx->text.source, ctx->text.insertPos, IswstPositions,
		IswsdRight, ctx->text.mult, TRUE );

  /* Make sure we aren't at the very beginning or end of the buffer. */

  if ( ( start == ctx->text.insertPos ) || ( end == ctx->text.insertPos ) ) {
      xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
      xcb_flush(XtDisplay(ctx));
      EndAction( ctx );
      return;
  }

  ctx->text.insertPos = end;

  text.firstPos = 0;
  text.format = _IswTextFormat(ctx);

  /* Retrieve text and swap the characters. */

#ifdef ISW_INTERNATIONALIZATION
  if ( text.format == IswFmtWide) {
      wchar_t wc;
      wchar_t* wbuf;

      wbuf = (wchar_t*) _IswTextGetText(ctx, start, end);
      text.length = wcslen( wbuf );
      wc = wbuf[ 0 ];
      for ( i = 1; i < text.length; i++ )
          wbuf[ i-1 ] = wbuf[ i ];
      wbuf[ i-1 ] = wc;
      buf = (char*) wbuf; /* so that it gets assigned and freed */

  } else
#endif
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
    xcb_bell(XtDisplay(ctx), 0); // 0 = default volume
    xcb_flush(XtDisplay(ctx));
  }

  XtFree((char *) buf);
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
NoOp(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    if (*num_params != 1)
	return;

    switch(params[0][0]) {
    case 'R':
    case 'r':
	xcb_bell(XtDisplay(w), 0); // 0 = default volume
  xcb_flush(XtDisplay(w));
    default:			/* Fall Through */
	break;
    }
}

/* Reconnect() - action
 * This reconnects to the input method.  The user will typically call
 * this action if/when connection has been severed, or when the app
 * was started up before an IM was started up.
 */

#ifdef ISW_INTERNATIONALIZATION
/*ARGSUSED*/
static void
Reconnect(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    _IswImReconnect( w );
}
#endif


XtActionsRec _IswTextActionsTable[] = {

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
#ifdef ISW_INTERNATIONALIZATION
  {"reconnect-im",       Reconnect} /* Li Yuhong, Omron KK, 1991 */
#endif
};

Cardinal _IswTextActionsTableCount = XtNumber(_IswTextActionsTable);
