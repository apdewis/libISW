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

/****************************************************************
 *
 * Callbacks
 *
 ****************************************************************/

typedef IswQResource **CallbackTable;

#define _IswCBCalling 1
#define _IswCBFreeAfterCalling 2

_XFUNCPROTOBEGIN

typedef struct internalCallbackRec {
    unsigned short count;
    char	   is_padded;	/* contains NULL padding for external form */
    char	   call_state;  /* combination of _IswCB{FreeAfter}Calling */
#if __STDC_VERSION__ >= 199901L
    /* When compiling with C99 or newer, use a flexible member to ensure
     * padding and alignment for sizeof(). */
    IswCallbackRec callbacks[];
#elif defined(LONG64)
    unsigned int   align_pad;	/* padding to align callback list */
#endif
    /* IswCallbackList */
} InternalCallbackRec, *InternalCallbackList;

typedef Boolean (*_IswConditionProc)(
    IswPointer	/* data */
);

extern void _IswAddCallback(
    InternalCallbackList*	/* callbacks */,
    IswCallbackProc		/* callback */,
    IswPointer 			/* closure */
);

extern void _IswAddCallbackOnce(
    InternalCallbackList*	/* callbacks */,
    IswCallbackProc		/* callback */,
    IswPointer 			/* closure */
);

extern InternalCallbackList _IswCompileCallbackList(
    IswCallbackList		/* xtcallbacks */
);

extern IswCallbackList _IswGetCallbackList(
    InternalCallbackList*	/* callbacks */
);

extern void _IswRemoveAllCallbacks(
    InternalCallbackList*	/* callbacks */
);

extern void _IswRemoveCallback(
    InternalCallbackList*	/* callbacks */,
    IswCallbackProc		/* callback */,
    IswPointer			/* closure */
);

extern void _IswPeekCallback(
    Widget			/* widget */,
    IswCallbackList		/* callbacks */,
    IswCallbackProc *		/* callback */,
    IswPointer *			/* closure */
);

extern void _IswCallConditionalCallbackList(
    Widget			/* widget */,
    IswCallbackList		/* callbacks */,
    IswPointer			/* call_data */,
    _IswConditionProc		/* cond_proc */
);

_XFUNCPROTOEND
