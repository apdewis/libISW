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

#ifndef _IswComposite_h
#define _IswComposite_h

typedef struct _CompositeClassRec *CompositeWidgetClass;

typedef Cardinal (*IswOrderProc)(
    Widget 	/* child */
);

_XFUNCPROTOBEGIN

extern void IswManageChildren(
    WidgetList 		/* children */,
    Cardinal 		/* num_children */
);

extern void IswManageChild(
    Widget 		/* child */
);

extern void IswUnmanageChildren(
    WidgetList 		/* children */,
    Cardinal 		/* num_children */
);

extern void IswUnmanageChild(
    Widget 		/* child */
);

typedef void (*IswDoChangeProc)(
    Widget		/* composite_parent */,
    WidgetList		/* unmanage_children */,
    Cardinal *		/* num_unmanage_children */,
    WidgetList		/* manage_children */,
    Cardinal *		/* num_manage_children */,
    IswPointer		/* client_data */
);

extern void IswChangeManagedSet(
    WidgetList		/* unmanage_children */,
    Cardinal		/* num_unmanage_children */,
    IswDoChangeProc	/* do_change_proc */,
    IswPointer		/* client_data */,
    WidgetList		/* manage_children */,
    Cardinal		/* num_manage_children */
);

_XFUNCPROTOEND

externalref WidgetClass compositeWidgetClass;

#endif /* _IswComposite_h */
/* DON'T ADD STUFF AFTER THIS #endif */
