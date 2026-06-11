/***********************************************************

Copyright 1987, 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


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
 * Event.h - exported types and functions for toolkit event handler
 *
 * Author:	Charles Haynes
 * 		Digital Equipment Corporation
 * 		Western Software Laboratory
 * Date:	Sun Dec  6 1987
 */

#ifndef _Event_h_
#define _Event_h_

typedef struct _IswGrabRec  *IswGrabList;

#include "PassivGraI.h"

_XFUNCPROTOBEGIN

extern void _IswEventInitialize(void);

typedef struct _IswEventRec {
     IswEventTable	next;
     EventMask		mask;	/*  also select_data count for RecExt */
     IswEventHandler	proc;
     IswPointer		closure;
     unsigned int	select:1;
     unsigned int	has_type_specifier:1;
     unsigned int	async:1; /* not used, here for Digital extension? */
} IswEventRec;

typedef struct _IswGrabRec {
    IswGrabList next;
    Widget   widget;
    unsigned int exclusive:1;
}IswGrabRec;

typedef struct _BlockHookRec {
    struct _BlockHookRec* next;
    IswAppContext app;
    IswBlockHookProc proc;
    IswPointer closure;
} BlockHookRec, *BlockHook;

extern void _IswFreeEventTable(
    IswEventTable*	/* event_table */
);

extern Boolean _IswOnGrabList(
    Widget	/* widget */,
    IswGrabRec*	/* grabList */
);

extern void _IswRemoveAllInputs(
    IswAppContext /* app */
);

extern void _IswRefreshMapping(
    IswDisplay,
    xcb_generic_event_t*	/* event */,
    _IswBoolean	/* dispatch */
);

extern void _IswSendFocusEvent(
    Widget	/* child */,
    IswEventKind	/* kind */);

extern EventMask _IswConvertKindToMask(
    IswEventKind	/* kind */
);

/* Event.c: deepest windowless widget under (x,y) in a windowed widget's
   window coords; *dx,*dy returns the accumulated origin offset.  Returns the
   root itself if nothing windowless is under the point. */
extern Widget _IswFindWidgetAtPoint(Widget root, int x, int y,
				    int *dx, int *dy);

/* Paint a now-shown windowless widget (and descendants) and composite it. */
extern void _IswRepaintWindowless(Widget w);

/* EventUtil.c */
extern Widget _IswFindRemapWidget(IswEvent *event, Widget widget,
				 EventMask mask, IswPerDisplayInput pdi);
extern void _IswUngrabBadGrabs(IswEvent *event, Widget widget,
				 EventMask mask, IswPerDisplayInput pdi);
extern void _IswFillAncestorList(Widget **listPtr, int *maxElemsPtr,
				int *numElemsPtr, Widget start,
				Widget breakWidget);

/* NextEvent.c */
extern Boolean IswAppPeekEvent_SkipTimer;
extern void _IswFillEventQueue(IswAppContext /* app */);

_XFUNCPROTOEND

#endif /* _Event_h_ */
