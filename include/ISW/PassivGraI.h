/********************************************************

Copyright 1988 by Hewlett-Packard Company
Copyright 1987, 1988, 1989 by Digital Equipment Corporation, Maynard

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that copyright notice and this permission
notice appear in supporting documentation, and that the names of
Hewlett-Packard or Digital not be used in advertising or
publicity pertaining to distribution of the software without specific,
written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/

/*

Copyright 1987, 1988, 1989, 1998  The Open Group

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

*/

#ifndef _PDI_h_
#define _PDI_h_

#include "uthash.h"

#define KEYBOARD TRUE
#define POINTER  FALSE

_XFUNCPROTOBEGIN

typedef enum {
    IswNoServerGrab,
    IswPassiveServerGrab,
    IswActiveServerGrab,
    IswPseudoPassiveServerGrab,
    IswPseudoActiveServerGrab
}IswServerGrabType;

typedef struct _IswServerGrabRec {
    struct _IswServerGrabRec 	*next;
    Widget			widget;
    unsigned int		ownerEvents:1;
    unsigned int		pointerMode:1;
    unsigned int		keyboardMode:1;
    unsigned int		hasExt:1;
    unsigned int		confineToIsWidgetWin:1;
    xcb_keycode_t			keybut;
    unsigned short		modifiers;
    unsigned short		eventMask;
} IswServerGrabRec, *IswServerGrabPtr;

typedef struct _IswGrabExtRec {
    Mask			*pKeyButMask;
    Mask			*pModifiersMask;
    xcb_window_t			confineTo;
    xcb_cursor_t			cursor;
} IswServerGrabExtRec, *IswServerGrabExtPtr;

#define GRABEXT(p) ((IswServerGrabExtPtr)((p)+1))

typedef struct _IswDeviceRec{
    IswServerGrabRec	grab; 	/* need copy in order to protect
				   during grab */
    IswServerGrabType	grabType;
}IswDeviceRec, *IswDevice;

#define IswMyAncestor	0
#define IswMyDescendant	1
#define IswMyCousin	2
#define IswMySelf	3
#define IswUnrelated	4
typedef char IswGeneology; /* do not use an enum makes PerWidgetInput larger */

typedef struct {
    Widget		id;		/* hash key: widget pointer (NOT window ID) */
    Widget		focusKid;
    IswServerGrabPtr	keyList, ptrList;
    Widget		queryEventDescendant;
    unsigned int	map_handler_added:1;
    unsigned int	realize_handler_added:1;
    unsigned int	active_handler_added:1;
    unsigned int	haveFocus:1;
    IswGeneology		focalPoint;
    UT_hash_handle hh;
}IswPerWidgetInputRec, *IswPerWidgetInput;

typedef struct IswPerDisplayInputRec{
    IswGrabList 	grabList;
    IswDeviceRec keyboard, pointer;
    xcb_keycode_t	activatingKey;
    Widget 	*trace;
    int		traceDepth, traceMax;
    Widget 	focusWidget;
    Widget 	pointerWidget;	/* windowless widget under pointer, for
				   synthesized Enter/Leave crossing events */
}IswPerDisplayInputRec, *IswPerDisplayInput;

#define IsServerGrab(g) ((g == IswPassiveServerGrab) ||\
			 (g == IswActiveServerGrab))

#define IsAnyGrab(g) ((g == IswPassiveServerGrab) ||\
		      (g == IswActiveServerGrab)  ||\
		      (g == IswPseudoPassiveServerGrab))

#define IsEitherPassiveGrab(g) ((g == IswPassiveServerGrab) ||\
				(g == IswPseudoPassiveServerGrab))

#define IsPseudoGrab(g) ((g == IswPseudoPassiveServerGrab))

extern void _IswDestroyServerGrabs(
    Widget		/* w */,
    IswPointer		/* pwi */, /*IswPerWidgetInput*/
    IswPointer		/* call_data */
);

extern IswPerWidgetInput _IswGetPerWidgetInput(
    Widget	/* widget */,
    _IswBoolean	/* create */
);

extern IswServerGrabPtr _IswCheckServerGrabsOnWidget(
   xcb_generic_event_t*		/* event */,
    Widget		/* widget */,
    _IswBoolean		/* isKeyboard */
);

/*
extern IswGrabList* _IswGetGrabList( IswPerDisplayInput );
*/

#define _IswGetGrabList(pdi) (&(pdi)->grabList)

extern void _IswFreePerWidgetInput(
    Widget		/* w */,
    IswPerWidgetInput	/* pwi */
);

extern Widget _IswProcessKeyboardEvent(
    xcb_key_press_event_t*		/* event */,
    Widget		/* widget */,
    IswPerDisplayInput	/* pdi */
);

extern Widget _IswProcessPointerEvent(
    xcb_button_press_event_t*	/* event */,
    Widget		/* widget */,
    IswPerDisplayInput	/* pdi */
);

extern void _IswRegisterPassiveGrabs(
    Widget		/* widget */
);

extern void _IswClearAncestorCache(
    Widget		/* widget */
);

_XFUNCPROTOEND

#endif /* _PDI_h_ */
