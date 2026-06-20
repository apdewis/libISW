/***********************************************************
Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts,

			All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name Digital not be
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

Copyright 1987, 1988, 1994, 1998  The Open Group

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

#ifndef _IswIntrinsic_h
#define _IswIntrinsic_h

//#include	<X11/Xlib.h>
#include	<ISW/IswTypes.h>
#include	<ISW/IswEvent.h>	/* platform-neutral event union */
/* Xresource.h replaced by custom IswQuark/IswValue/IswDatabase/IswOptions headers */
#include	<ISW/IswFuncproto.h>
//#include    "ISWPlatform.h"
#include <string.h>		/* for IswNewString, memcpy, etc. */
#include <stdlib.h>		/* for malloc, free */

#define IswSpecificationRelease 7

/*
 * As used in its function interface, the String type of libXt can be readonly.
 * But compiling libXt with this feature may require some internal changes,
 * e.g., casts and occasionally using a plain "char*".
 */
#ifdef _CONST_X_STRING
typedef const char *String;
#else
typedef char *String;
#endif

/* We do this in order to get "const" declarations to work right.  We
 * use _IswString instead of String so that C++ applications can
 * #define String to something else if they choose, to avoid conflicts
 * with other C++ libraries.
 */
#define _IswString char*

/* _Xt names are private to Xt implementation, do not use in client code */
#if NeedWidePrototypes
#define _IswBoolean	int
#define _IswDimension	unsigned int
#define _IswPosition	int
#define _IswEnum	unsigned int
#else
#define _IswBoolean	Boolean
#define _IswDimension	Dimension
#define _IswPosition	Position
#define _IswEnum	IswEnum
#endif /* NeedWidePrototypes */

#include <stddef.h>

#define externalref extern
#define externaldef(psect)

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#if __STDC_VERSION__ >= 199901L
#include <stdint.h>
typedef intptr_t	IswIntPtr;
typedef uintptr_t	IswUIntPtr;
#else
typedef long		IswIntPtr;
typedef unsigned long	IswUIntPtr;
#endif

#define IswNumber(arr)		((Cardinal) (sizeof(arr) / sizeof(arr[0])))

typedef struct _WidgetRec *Widget;
typedef Widget *WidgetList;
typedef struct _WidgetClassRec *WidgetClass;
typedef struct _CompositeRec *CompositeWidget;

/* Opaque platform handles (the ops vtables live in ISW/ISWPlatform.h).
   Declared here so the public Display/Screen/Window accessors below can use
   them without a circular include.  A platform backend maps these to its
   native types; toolkit and application code never dereference them. */
typedef struct _IswDisplay *IswDisplay;
typedef struct _IswScreen  *IswScreen;    /* a screen on a display       */
typedef struct _IswWindow  *IswWindow;    /* a window                    */
typedef struct _IswSurface *IswSurface;   /* a per-widget render surface */
typedef struct _IswActionsRec *IswActionList;
typedef struct _IswEventRec *IswEventTable;

typedef struct _IswAppStruct *IswAppContext;
typedef unsigned long	IswValueMask;

/* Window-creation attribute selectors for the IswValueMask passed to a Realize
   proc (and the positional values[] array beside it).  Numerically X11-compatible
   so the geometry engine and any X11 backend agree on the bit layout; the
   backend translates them at the actual window-create call. */
#define IswCWBackPixmap		(1u << 0)
#define IswCWBackPixel		(1u << 1)
#define IswCWBorderPixmap	(1u << 2)
#define IswCWBorderPixel	(1u << 3)
#define IswCWBitGravity		(1u << 4)
#define IswCWWinGravity		(1u << 5)
#define IswCWBackingStore	(1u << 6)
#define IswCWOverrideRedirect	(1u << 9)
#define IswCWSaveUnder		(1u << 10)
#define IswCWAttrEventMask	(1u << 11)
#define IswCWColormap		(1u << 13)
#define IswCWCursor		(1u << 14)

/* bit_gravity / win_gravity value: preserve content on geometry change. */
#define IswGravityNorthWest	1

/* backing_store values. */
#define IswBackingNotUseful	0
#define IswBackingWhenMapped	1
#define IswBackingAlways	2

/* "no cursor" value for IswCWCursor. */
#define IswCursorNone		0

/* Visual class values (X11-compatible) for the String->Visual resource
   converter and _IswPlatformMatchVisualInfo. */
#define IswVisualStaticGray	0
#define IswVisualGrayScale	1
#define IswVisualStaticColor	2
#define IswVisualPseudoColor	3
#define IswVisualTrueColor	4
#define IswVisualDirectColor	5

/* WM initial-state values (ICCCM WM_STATE) for the String->InitialState
   converter. */
#define IswWmStateNormal	1
#define IswWmStateIconic	3

typedef IswUIntPtr	IswIntervalId;
typedef IswUIntPtr	IswInputId;
typedef IswUIntPtr	IswWorkProcId;
typedef IswUIntPtr	IswSignalId;
typedef unsigned int	IswGeometryMask;
typedef unsigned long	IswGCMask;   /* Mask of values that are used by widget*/
typedef unsigned long	Pixel;	    /* Index into colormap		*/
typedef int		IswCacheType;
#define			IswCacheNone	  0x001
#define			IswCacheAll	  0x002
#define			IswCacheByDisplay  0x003
#define			IswCacheRefCount	  0x100

/****************************************************************
 *
 * System Dependent Definitions; see spec for specific range
 * requirements.  Do not assume every implementation uses the
 * same base types!
 *
 *
 * IswArgVal ought to be a union of IswPointer, char *, long, int *, and proc *
 * but casting to union types is not really supported.
 *
 * So the typedef for IswArgVal should be chosen such that
 *
 *	sizeof (IswArgVal) >=	sizeof(IswPointer)
 *				sizeof(char *)
 *				sizeof(long)
 *				sizeof(int *)
 *				sizeof(proc *)
 *
 * ArgLists rely heavily on the above typedef.
 *
 ****************************************************************/
typedef char		Boolean;
typedef IswIntPtr	IswArgVal;
typedef unsigned char	IswEnum;

typedef unsigned int	Cardinal;
typedef unsigned short	Dimension;  /* Size in pixels			*/
typedef short		Position;   /* Offset from 0 coordinate		*/

typedef void*		IswPointer;
#if __STDC_VERSION__ >= 201112L && !defined(__cplusplus)
_Static_assert(sizeof(IswArgVal) >= sizeof(IswPointer), "IswArgVal too small");
_Static_assert(sizeof(IswArgVal) >= sizeof(long), "IswArgVal too small");
#endif

/* XRM replacement headers - must come after IswPointer is defined */
#include <ISW/IswQuark.h>
#include <ISW/IswValue.h>
#include <ISW/IswDatabase.h>
#include <ISW/IswOptions.h>

/* The type Opaque is NOT part of the Xt standard, do NOT use it. */
/* (It remains here only for backward compatibility.) */
typedef IswPointer	Opaque;

#include <ISW/Core.h>
#include <ISW/Composite.h>
#include <ISW/Constraint.h>
#include <ISW/Object.h>
#include <ISW/RectObj.h>

typedef struct _TranslationData *IswTranslations;
typedef struct _TranslationData *IswAccelerators;
typedef uint16_t Modifiers;

typedef void (*IswActionProc)(
    Widget 		/* widget */,
    IswEvent*		/* event */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

typedef IswActionProc* IswBoundActions;

typedef struct _IswActionsRec{
    String	 string;
    IswActionProc proc;
} IswActionsRec;

typedef enum {
/* address mode		parameter representation    */
/* ------------		------------------------    */
    IswAddress,		/* address		    */
    IswBaseOffset,	/* offset		    */
    IswImmediate,	/* constant		    */
    IswResourceString,	/* resource name string	    */
    IswResourceQuark,	/* resource name quark	    */
    IswWidgetBaseOffset,	/* offset from ancestor	    */
    IswProcedureArg	/* procedure to invoke	    */
} IswAddressMode;

typedef struct {
    IswAddressMode   address_mode;
    IswPointer	    address_id;
    Cardinal	    size;
} IswConvertArgRec, *IswConvertArgList;

typedef void (*IswConvertArgProc)(
    Widget 		/* widget */,
    Cardinal*		/* size */,
    IswValueRec*		/* value */
);

typedef struct {
    IswGeometryMask request_mode;
    Position x, y;
    Dimension width, height, border_width;
    Widget sibling;
    int stack_mode;   /* Above, Below, TopIf, BottomIf, Opposite, DontChange */
} IswWidgetGeometry;

/* Geometry-request field selectors for IswWidgetGeometry.request_mode.  Each
   bit selects one field of the request the backend should honour.  Values are
   platform-neutral (a backend maps them to its own configure mechanism); they
   are numerically X11-compatible so the geometry engine and any X11 backend
   agree on the bit layout. */
#define IswCWX		(1 << 0)
#define IswCWY		(1 << 1)
#define IswCWWidth	(1 << 2)
#define IswCWHeight	(1 << 3)
#define IswCWBorderWidth (1 << 4)
#define IswCWSibling	(1 << 5)
#define IswCWStackMode	(1 << 6)

/* Additions to Xlib geometry requests: ask what would happen, don't do it */
#define IswCWQueryOnly	(1 << 7)

/* Stack modes for IswWidgetGeometry.stack_mode (values match the X11 wire
   protocol so the backend forwards them directly). */
#define IswSMAbove	0
#define IswSMBelow	1
#define IswSMTopIf	2
#define IswSMBottomIf	3
#define IswSMOpposite	4
/* Additions to Xlib stack modes: don't change stack order */
#define IswSMDontChange	5

typedef void (*IswConverter)( /* obsolete */
    IswValueRec*		/* args */,
    Cardinal*		/* num_args */,
    IswValueRec*		/* from */,
    IswValueRec*		/* to */
);

typedef Boolean (*IswTypeConverter)(
    IswDisplay 		/* dpy */,
    IswValueRec*		/* args */,
    Cardinal*		/* num_args */,
    IswValueRec*		/* from */,
    IswValueRec*		/* to */,
    IswPointer*		/* converter_data */
);

typedef void (*IswDestructor)(
    IswAppContext	/* app */,
    IswValueRec*		/* to */,
    IswPointer 		/* converter_data */,
    IswValueRec*		/* args */,
    Cardinal*		/* num_args */
);

typedef Opaque IswCacheRef;

typedef Opaque IswActionHookId;

typedef void (*IswActionHookProc)(
    Widget		/* w */,
    IswPointer		/* client_data */,
    String		/* action_name */,
    IswEvent*		/* event */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

typedef IswUIntPtr IswBlockHookId;

typedef void (*IswBlockHookProc)(
    IswPointer		/* client_data */
);

typedef void (*IswKeyProc)(
    IswDisplay 		/* dpy */,
    IswKeyCode 		/* keycode */,
    Modifiers		/* modifiers */,
    Modifiers*		/* modifiers_return */,
    IswKeySym *		/* keysym_return */
);

typedef void (*IswCaseProc)(
    IswDisplay 		/* dpy */,
    IswKeySym		/* keysym */,
    IswKeySym *		/* lower_return */,
    IswKeySym *		/* upper_return */
);

typedef void (*IswEventHandler)(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    IswEvent*		/* event */,
    Boolean*		/* continue_to_dispatch */
);
typedef unsigned long EventMask;

/* Neutral event-selection mask bits for IswAddEventHandler / window event
   masks.  Values match the X11 wire protocol so the XCB backend forwards them
   directly; toolkit and widget code names these instead of any XCB_EVENT_MASK_*
   symbol. */
#define IswNoEventMask			0L
#define IswKeyPressMask			(1L<<0)
#define IswKeyReleaseMask		(1L<<1)
#define IswButtonPressMask		(1L<<2)
#define IswButtonReleaseMask		(1L<<3)
#define IswEnterWindowMask		(1L<<4)
#define IswLeaveWindowMask		(1L<<5)
#define IswPointerMotionMask		(1L<<6)
#define IswPointerMotionHintMask	(1L<<7)
#define IswButton1MotionMask		(1L<<8)
#define IswButton2MotionMask		(1L<<9)
#define IswButton3MotionMask		(1L<<10)
#define IswButton4MotionMask		(1L<<11)
#define IswButton5MotionMask		(1L<<12)
#define IswButtonMotionMask		(1L<<13)
#define IswKeymapStateMask		(1L<<14)
#define IswExposureMask			(1L<<15)
#define IswVisibilityChangeMask		(1L<<16)
#define IswStructureNotifyMask		(1L<<17)
#define IswResizeRedirectMask		(1L<<18)
#define IswSubstructureNotifyMask	(1L<<19)
#define IswSubstructureRedirectMask	(1L<<20)
#define IswFocusChangeMask		(1L<<21)
#define IswPropertyChangeMask		(1L<<22)
#define IswColormapChangeMask		(1L<<23)
#define IswOwnerGrabButtonMask		(1L<<24)

/* "Any modifier" wildcard for passive grab registration (matches the X
   AnyModifier value so the backend passes it through unchanged). */
#define IswAnyModifier			(1L<<15)

/* Neutral grab-attempt status (returned by IswGrabKeyboard / IswGrabPointer;
   values match the X grab-status wire codes the backend passes through). */
#define IswGrabSuccess			0
#define IswGrabAlreadyGrabbed		1
#define IswGrabInvalidTime		2
#define IswGrabNotViewable		3
#define IswGrabFrozen			4

typedef enum {IswListHead, IswListTail } IswListPosition;

typedef unsigned long	IswInputMask;
#define IswInputNoneMask		0L
#define IswInputReadMask		(1L<<0)
#define IswInputWriteMask	(1L<<1)
#define IswInputExceptMask	(1L<<2)

typedef void (*IswTimerCallbackProc)(
    IswPointer 		/* closure */,
    IswIntervalId*	/* id */
);

typedef void (*IswInputCallbackProc)(
    IswPointer 		/* closure */,
    int*		/* source */,
    IswInputId*		/* id */
);

typedef void (*IswSignalCallbackProc)(
    IswPointer		/* closure */,
    IswSignalId*		/* id */
);

typedef struct {
    String	name;
    IswArgVal	value;
} Arg, *ArgList;

typedef IswPointer	IswVarArgsList;

typedef void (*IswCallbackProc)(
    Widget 		/* widget */,
    IswPointer 		/* closure */,	/* data the application registered */
    IswPointer 		/* call_data */	/* callback specific data */
);

typedef struct _IswCallbackRec {
    IswCallbackProc  callback;
    IswPointer	    closure;
} IswCallbackRec, *IswCallbackList;

typedef enum {
	IswCallbackNoList,
	IswCallbackHasNone,
	IswCallbackHasSome
} IswCallbackStatus;

typedef enum  {
    IswGeometryYes,	  /* Request accepted. */
    IswGeometryNo,	  /* Request denied. */
    IswGeometryAlmost,	  /* Request denied, but willing to take replyBox. */
    IswGeometryDone	  /* Request accepted and done. */
} IswGeometryResult;

typedef enum {IswGrabNone, IswGrabNonexclusive, IswGrabExclusive} IswGrabKind;

typedef struct {
    Widget  shell_widget;
    Widget  enable_widget;
} IswPopdownIDRec, *IswPopdownID;

typedef struct _IswResource {
    String	resource_name;	/* Resource name			    */
    String	resource_class;	/* Resource class			    */
    String	resource_type;	/* Representation type desired		    */
    Cardinal	resource_size;	/* Size in bytes of representation	    */
    Cardinal	resource_offset;/* Offset from base to put resource value   */
    String	default_type;	/* representation type of specified default */
    IswPointer	default_addr;	/* Address of default resource		    */
} IswResource, *IswResourceList;

typedef void (*IswResourceDefaultProc)(
    Widget	/* widget */,
    int		/* offset */,
    IswValueRec*	/* value */
);

typedef String (*IswLanguageProc)(
    IswDisplay 	/* dpy */,
    String	/* xnl */,
    IswPointer	/* client_data */
);

typedef void (*IswErrorMsgHandler)(
    String 		/* name */,
    String		/* type */,
    String		/* class */,
    String		/* default */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

typedef void (*IswErrorHandler)(
  String		/* msg */
);

typedef void (*IswCreatePopupChildProc)(
    Widget	/* shell */
);

typedef Boolean (*IswWorkProc)(
    IswPointer 		/* closure */	/* data the application registered */
);

typedef struct {
    char match;
    _IswString substitution;
} SubstitutionRec, *Substitution;

typedef Boolean (*IswFilePredicate)(
   String /* filename */
);

typedef IswPointer IswRequestId;

/*
 * Neutral selection vocabulary.  A selection, a conversion target, a conversion
 * type and an exchange property are all named by an opaque IswSelectionId — the
 * selection engine and its consumers never name them with an X11 atom.  A
 * backend maps a name to an IswSelectionId (IswInternSelection) and back; on X11
 * the id is numerically an interned atom, but a non-X11 backend assigns its own.
 * The full ops live in ISW/ISWPlatform.h; these types are declared here because
 * the public selection API and its callbacks use them.
 */
typedef uint32_t IswSelectionId;
#define ISW_SELECTION_NONE ((IswSelectionId) 0)

typedef enum {
    ISW_SEL_EVENT_OTHER = 0,     /* not a selection-protocol event           */
    ISW_SEL_EVENT_CLEAR,         /* ownership lost (another client took over) */
    ISW_SEL_EVENT_REQUEST,       /* a requestor asks us to convert            */
    ISW_SEL_EVENT_NOTIFY,        /* a conversion we requested is ready/refused*/
    ISW_SEL_EVENT_PROP_NEW,      /* exchange property got a new value (INCR)  */
    ISW_SEL_EVENT_PROP_DELETE    /* exchange property was deleted (INCR)      */
} IswSelectionEventKind;

/* A requestor's conversion request, in neutral terms.  Carries the identity an
   owner needs to answer (and that IswGetSelectionRequest hands to owner procs).
   `property` is ISW_SELECTION_NONE for an obsolete requestor. */
typedef struct {
    IswWindow      requestor;
    IswWindow      owner;
    IswSelectionId selection;
    IswSelectionId target;
    IswSelectionId property;
    IswTime        time;
} IswSelectionRequest;

/* A decoded selection-protocol event.  The backend fills the fields meaningful
   for `kind`; the rest are ISW_SELECTION_NONE / 0. */
typedef struct {
    IswSelectionEventKind kind;
    IswWindow             requestor;
    IswSelectionId        selection;
    IswSelectionId        target;
    IswSelectionId        property;
    IswTime               time;
    unsigned long         serial;
    IswSelectionRequest   request;
} IswSelectionEvent;

/* Standard selection value-types the engine and widgets name by role rather
   than by any backend's wire name.  The backend resolves each to its own id
   (on X11: the predefined ATOM / STRING / INTEGER atoms). */
typedef enum {
    ISW_SEL_STDTYPE_ID_LIST = 0,   /* a list of selection ids (X11: ATOM)    */
    ISW_SEL_STDTYPE_STRING,        /* a text string           (X11: STRING)  */
    ISW_SEL_STDTYPE_INTEGER        /* an integer              (X11: INTEGER) */
} IswSelectionStdType;

/*
 * Simplified selection API — widget-level offer/request.
 *
 * The widget says "I have text" (offer) or "give me text" (request).
 * The platform backend decides which selection channels to use (PRIMARY,
 * CLIPBOARD, etc.) and handles all protocol detail (TARGETS negotiation,
 * INCR chunking, format conversion).
 */

/* The offer callback: platform asks the widget for its current text.
   Widget fills *value with IswMalloc'd UTF-8 string and *length with
   its byte count.  Returns True if text is available, False otherwise. */
typedef Boolean (*IswSelectionOfferProc)(
    Widget		/* widget */,
    IswPointer*		/* value_return — IswMalloc'd UTF-8, caller frees */,
    unsigned long*	/* length_return — byte count */
);

/* The lose callback: platform informs the widget it no longer owns the
   selection (another client took it). */
typedef void (*IswSelectionLoseProc)(
    Widget		/* widget */
);

/* The receive callback: platform delivers text from the selection owner.
   value is IswMalloc'd UTF-8 (or NULL on failure); length is its byte count.
   The callback must IswFree value when done. */
typedef void (*IswSelectionReceiveProc)(
    Widget		/* widget */,
    IswPointer		/* closure */,
    const char*		/* value — IswMalloc'd UTF-8, callback frees */,
    unsigned long	/* length — byte count */
);

extern Boolean IswSelectionOffer(
    Widget			/* widget */,
    IswTime			/* time */,
    IswSelectionOfferProc	/* offer */,
    IswSelectionLoseProc	/* lose */
);

extern void IswSelectionDisown(
    Widget		/* widget */,
    IswTime		/* time */
);

extern void IswSelectionRequestText(
    Widget			/* widget */,
    IswTime			/* time */,
    IswSelectionReceiveProc	/* receive */,
    IswPointer			/* closure */
);

/* ---- Legacy selection API (IswSelectionId-based) ----
 * These remain for the ICCCM protocol engine in Selection.c and for widgets
 * that haven't migrated.  New code should use the simplified API above.
 */

typedef Boolean (*IswConvertSelectionProc)(
    Widget 		/* widget */,
    IswSelectionId*	/* selection */,
    IswSelectionId*	/* target */,
    IswSelectionId*	/* type_return */,
    IswPointer*		/* value_return */,
    unsigned long*	/* length_return */,
    int*		/* format_return */
);

typedef void (*IswLoseSelectionProc)(
    Widget 		/* widget */,
    IswSelectionId*	/* selection */
);

typedef void (*IswSelectionDoneProc)(
    Widget 		/* widget */,
    IswSelectionId*	/* selection */,
    IswSelectionId*	/* target */
);

typedef void (*IswSelectionCallbackProc)(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    IswSelectionId*	/* selection */,
    IswSelectionId*	/* type */,
    IswPointer 		/* value */,
    unsigned long*	/* length */,
    int*		/* format */
);

typedef void (*IswLoseSelectionIncrProc)(
    Widget 		/* widget */,
    IswSelectionId*	/* selection */,
    IswPointer 		/* client_data */
);

typedef void (*IswSelectionDoneIncrProc)(
    Widget 		/* widget */,
    IswSelectionId*	/* selection */,
    IswSelectionId*	/* target */,
    IswRequestId*	/* receiver_id */,
    IswPointer 		/* client_data */
);

typedef Boolean (*IswConvertSelectionIncrProc)(
    Widget 		/* widget */,
    IswSelectionId*	/* selection */,
    IswSelectionId*	/* target */,
    IswSelectionId*	/* type */,
    IswPointer*		/* value */,
    unsigned long*	/* length */,
    int*		/* format */,
    unsigned long*	/* max_length */,
    IswPointer 		/* client_data */,
    IswRequestId*	/* receiver_id */
);

typedef void (*IswCancelConvertSelectionProc)(
    Widget 		/* widget */,
    IswSelectionId*	/* selection */,
    IswSelectionId*	/* target */,
    IswRequestId*	/* receiver_id */,
    IswPointer 		/* client_data */
);

typedef Boolean (*IswEventDispatchProc)(
    IswEvent*		/* event */,
    IswDisplay            /* connection */
);

typedef void (*IswExtensionSelectProc)(
    Widget		/* widget */,
    int*		/* event_types */,
    IswPointer*		/* select_data */,
    int			/* count */,
    IswPointer		/* client_data */
);

/***************************************************************
 *
 * Exported Interfaces
 *
 ****************************************************************/

_XFUNCPROTOBEGIN

extern Boolean IswConvertAndStore(
    Widget 		/* widget */,
    _Xconst _IswString 	/* from_type */,
    IswValueRec*		/* from */,
    _Xconst _IswString 	/* to_type */,
    IswValueRec*		/* to_in_out */
);

extern Boolean IswCallConverter(
    IswDisplay 		/* dpy */,
    IswTypeConverter 	/* converter */,
    IswValuePtr 	/* args */,
    Cardinal 		/* num_args */,
    IswValuePtr 	/* from */,
    IswValueRec*		/* to_in_out */,
    IswCacheRef*		/* cache_ref_return */
);

extern Boolean IswDispatchEvent(
    IswEvent*,
    IswDisplay
);

extern Boolean IswCallAcceptFocus(
    Widget 		/* widget */,
    IswTime*		/* time */
);

extern Boolean IswAppPeekEvent(
    IswAppContext 	/* app_context */,
    IswEvent*		/* event_return */
);

extern Boolean IswIsSubclass(
    Widget 		/* widget */,
    WidgetClass 	/* widgetClass */
);

extern Boolean IswIsObject(
    Widget 		/* object */
);

extern Boolean _IswCheckSubclassFlag( /* implementation-private */
    Widget		/* object */,
    _IswEnum		/* type_flag */
);

extern Boolean _IswIsSubclassOf( /* implementation-private */
    Widget		/* object */,
    WidgetClass		/* widget_class */,
    WidgetClass		/* flag_class */,
    _IswEnum		/* type_flag */
);

extern Boolean IswIsManaged(
    Widget 		/* rectobj */
);

extern Boolean IswIsRealized(
    Widget 		/* widget */
);

extern Boolean IswIsSensitive(
    Widget 		/* widget */
);

extern Boolean IswOwnSelection(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswTime 		/* time */,
    IswConvertSelectionProc /* convert */,
    IswLoseSelectionProc	/* lose */,
    IswSelectionDoneProc /* done */
);

extern Boolean IswOwnSelectionIncremental(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswTime 		/* time */,
    IswConvertSelectionIncrProc	/* convert_callback */,
    IswLoseSelectionIncrProc	/* lose_callback */,
    IswSelectionDoneIncrProc	/* done_callback */,
    IswCancelConvertSelectionProc /* cancel_callback */,
    IswPointer 		/* client_data */
);

extern IswGeometryResult IswMakeResizeRequest(
    Widget 		/* widget */,
    _IswDimension	/* width */,
    _IswDimension	/* height */,
    Dimension*		/* width_return */,
    Dimension*		/* height_return */
);

extern void IswTranslateCoords(
    Widget 		/* widget */,
    _IswPosition		/* x */,
    _IswPosition		/* y */,
    Position*		/* rootx_return */,
    Position*		/* rooty_return */
);

extern void IswKeysymToKeycodeList(
    IswDisplay 		/* dpy */,
    IswKeySym 		/* keysym */,
    IswKeyCode **		/* keycodes_return */,
    Cardinal*		/* keycount_return */
);

extern void IswDisplayStringConversionWarning(
    IswDisplay 	 	/* dpy */,
    _Xconst _IswString	/* from_value */,
    _Xconst _IswString	/* to_type */
);

externalref IswConvertArgRec const colorConvertArgs[];
externalref IswConvertArgRec const screenConvertArg[];

extern void IswAppAddConverter( /* obsolete */
    IswAppContext	/* app_context */,
    _Xconst _IswString	/* from_type */,
    _Xconst _IswString	/* to_type */,
    IswConverter 	/* converter */,
    IswConvertArgList	/* convert_args */,
    Cardinal 		/* num_args */
);

extern void IswAddConverter( /* obsolete */
    _Xconst _IswString	/* from_type */,
    _Xconst _IswString 	/* to_type */,
    IswConverter 	/* converter */,
    IswConvertArgList 	/* convert_args */,
    Cardinal 		/* num_args */
);

extern void IswSetTypeConverter(
    _Xconst _IswString 	/* from_type */,
    _Xconst _IswString 	/* to_type */,
    IswTypeConverter 	/* converter */,
    IswConvertArgList 	/* convert_args */,
    Cardinal 		/* num_args */,
    IswCacheType 	/* cache_type */,
    IswDestructor 	/* destructor */
);

extern void IswAppSetTypeConverter(
    IswAppContext 	/* app_context */,
    _Xconst _IswString 	/* from_type */,
    _Xconst _IswString 	/* to_type */,
    IswTypeConverter 	/* converter */,
    IswConvertArgList 	/* convert_args */,
    Cardinal 		/* num_args */,
    IswCacheType 	/* cache_type */,
    IswDestructor 	/* destructor */
);

extern void IswConvert(
    Widget 		/* widget */,
    _Xconst _IswString 	/* from_type */,
    IswValueRec*		/* from */,
    _Xconst _IswString 	/* to_type */,
    IswValueRec*		/* to_return */
);

extern void IswDirectConvert(
    IswConverter 	/* converter */,
    IswValuePtr 	/* args */,
    Cardinal 		/* num_args */,
    IswValuePtr 	/* from */,
    IswValueRec*		/* to_return */
);

/****************************************************************
 *
 * Translation Management
 *
 ****************************************************************/

extern IswTranslations IswParseTranslationTable(
    _Xconst _IswString	/* table */
);

extern IswAccelerators IswParseAcceleratorTable(
    _Xconst _IswString	/* source */
);

extern void IswOverrideTranslations(
    Widget 		/* widget */,
    IswTranslations 	/* translations */
);

extern void IswAugmentTranslations(
    Widget 		/* widget */,
    IswTranslations 	/* translations */
);

extern void IswInstallAccelerators(
    Widget 		/* destination */,
    Widget		/* source */
);

extern void IswInstallAllAccelerators(
    Widget 		/* destination */,
    Widget		/* source */
);

extern void IswUninstallTranslations(
    Widget 		/* widget */
);

extern void IswAppAddActions(
    IswAppContext 	/* app_context */,
    IswActionList 	/* actions */,
    Cardinal 		/* num_actions */
);

extern IswActionHookId IswAppAddActionHook(
    IswAppContext 	/* app_context */,
    IswActionHookProc 	/* proc */,
    IswPointer 		/* client_data */
);

extern void IswRemoveActionHook(
    IswActionHookId 	/* id */
);

extern void IswGetActionList(
    WidgetClass		/* widget_class */,
    IswActionList*	/* actions_return */,
    Cardinal*		/* num_actions_return */
);

extern void IswCallActionProc(
    Widget		/* widget */,
    _Xconst _IswString	/* action */,
    IswEvent*		/* event */,
    String*		/* params */,
    Cardinal		/* num_params */
);

extern void IswRegisterGrabAction(
    IswActionProc 	/* action_proc */,
    _IswBoolean 		/* owner_events */,
    unsigned int 	/* event_mask */
);

extern void IswSetMultiClickTime(
    IswDisplay 		/* dpy */,
    int 		/* milliseconds */
);

extern int IswGetMultiClickTime(
    IswDisplay 		/* dpy */
);

extern IswKeySym IswGetActionKeysym(
    IswEvent*		/* event */,
    Modifiers*		/* modifiers_return */,
    IswDisplay
);

/***************************************************************
 *
 * Keycode and Keysym procedures for translation management
 *
 ****************************************************************/

extern void IswTranslateKeycode(
    IswDisplay 		/* dpy */,
    IswKeyCode 		/* keycode */,
    Modifiers 		/* modifiers */,
    Modifiers*		/* modifiers_return */,
    IswKeySym *		/* keysym_return */
);

extern void IswTranslateKey(
    IswDisplay 		/* dpy */,
    IswKeyCode		/* keycode */,
    Modifiers		/* modifiers */,
    Modifiers*		/* modifiers_return */,
    IswKeySym *		/* keysym_return */
);

extern void IswSetKeyTranslator(
    IswDisplay 		/* dpy */,
    IswKeyProc 		/* proc */
);

extern void IswRegisterCaseConverter(
    IswDisplay 		/* dpy */,
    IswCaseProc 		/* proc */,
    IswKeySym 		/* start */,
    IswKeySym 		/* stop */
);

extern void IswConvertCase(
    IswDisplay 		/* dpy */,
    IswKeySym 		/* keysym */,
    IswKeySym *		/* lower_return */,
    IswKeySym *		/* upper_return */
);

/****************************************************************
 *
 * Event Management
 *
 ****************************************************************/

/* IswAllEvents is valid only for IswRemoveEventHandler and
 * IswRemoveRawEventHandler; don't use it to select events!
 */
#define IswAllEvents ((EventMask) -1L)

extern void IswAddEventHandler(
    Widget 		/* widget */,
    EventMask 		/* event_mask */,
    _IswBoolean 		/* nonmaskable */,
    IswEventHandler 	/* proc */,
    IswPointer 		/* closure */
);

extern void IswRemoveEventHandler(
    Widget 		/* widget */,
    EventMask 		/* event_mask */,
    _IswBoolean 		/* nonmaskable */,
    IswEventHandler 	/* proc */,
    IswPointer 		/* closure */
);

extern void IswAddRawEventHandler(
    Widget 		/* widget */,
    EventMask 		/* event_mask */,
    _IswBoolean 		/* nonmaskable */,
    IswEventHandler 	/* proc */,
    IswPointer 		/* closure */
);

extern void IswRemoveRawEventHandler(
    Widget 		/* widget */,
    EventMask 		/* event_mask */,
    _IswBoolean 		/* nonmaskable */,
    IswEventHandler 	/* proc */,
    IswPointer 		/* closure */
);

extern void IswInsertEventHandler(
    Widget 		/* widget */,
    EventMask 		/* event_mask */,
    _IswBoolean 		/* nonmaskable */,
    IswEventHandler 	/* proc */,
    IswPointer 		/* closure */,
    IswListPosition 	/* position */
);

extern void IswInsertRawEventHandler(
    Widget 		/* widget */,
    EventMask 		/* event_mask */,
    _IswBoolean 		/* nonmaskable */,
    IswEventHandler 	/* proc */,
    IswPointer 		/* closure */,
    IswListPosition 	/* position */
);

extern IswEventDispatchProc IswSetEventDispatcher(
    IswDisplay 		/* dpy */,
    int			/* event_type */,
    IswEventDispatchProc	/* proc */
);

extern Boolean IswDispatchEventToWidget(
    Widget		                /* widget */,
    IswEvent*		/* event */
);

extern void IswInsertEventTypeHandler(
    Widget		/* widget */,
    int			/* type */,
    IswPointer		/* select_data */,
    IswEventHandler	/* proc */,
    IswPointer		/* closure */,
    IswListPosition	/* position */
);

extern void IswRemoveEventTypeHandler(
    Widget		/* widget */,
    int			/* type */,
    IswPointer		/* select_data */,
    IswEventHandler	/* proc */,
    IswPointer		/* closure */
);

extern EventMask IswBuildEventMask(
    Widget 		/* widget */
);

extern void IswRegisterExtensionSelector(
    IswDisplay 		/* dpy */,
    int			/* min_event_type */,
    int			/* max_event_type */,
    IswExtensionSelectProc /* proc */,
    IswPointer		/* client_data */
);

extern void IswAddGrab(
    Widget 		/* widget */,
    _IswBoolean 		/* exclusive */
);

extern void IswRemoveGrab(
    Widget 		/* widget */
);

extern void IswAppProcessEvent(
    IswAppContext 		/* app_context */,
    IswInputMask 		/* mask */
);

extern void IswAppMainLoop(
    IswAppContext 		/* app_context */
);

extern void IswSetKeyboardFocus(
    Widget		/* subtree */,
    Widget 		/* descendent */
);

extern Widget IswGetKeyboardFocusWidget(
    Widget		/* widget */
);

extern IswEvent* IswLastEventProcessed(
    IswDisplay 		/* dpy */
);

extern IswTime IswLastTimestampProcessed(
    IswDisplay 		/* dpy */
);

/****************************************************************
 *
 * Event Gathering Routines
 *
 ****************************************************************/

extern IswIntervalId IswAppAddTimeOut(
    IswAppContext 	/* app_context */,
    unsigned long 	/* interval */,
    IswTimerCallbackProc /* proc */,
    IswPointer 		/* closure */
);

extern void IswRemoveTimeOut(
    IswIntervalId 	/* timer */
);

extern IswInputId IswAppAddInput(
    IswAppContext       	/* app_context */,
    int 		/* source */,
    IswPointer 		/* condition */,
    IswInputCallbackProc /* proc */,
    IswPointer 		/* closure */
);

extern void IswRemoveInput(
    IswInputId 		/* id */
);

extern IswSignalId IswAddSignal(
    IswSignalCallbackProc /* proc */,
    IswPointer		/* closure */);

extern IswSignalId IswAppAddSignal(
    IswAppContext       	/* app_context */,
    IswSignalCallbackProc /* proc */,
    IswPointer 		/* closure */
);

extern void IswRemoveSignal(
    IswSignalId 		/* id */
);

extern void IswNoticeSignal(
    IswSignalId		/* id */
);

extern void IswNextEvent( /* obsolete */
    IswEvent* 		/* event */
);

extern void IswAppNextEvent(
    IswAppContext 	/* app_context */,
    IswEvent*		/* event_return */
);

#define IswIMXEvent		1
#define IswIMTimer		2
#define IswIMAlternateInput	4
#define IswIMSignal		8
#define IswIMAll (IswIMXEvent | IswIMTimer | IswIMAlternateInput | IswIMSignal)

extern Boolean IswPending( /* obsolete */
    void
);

extern IswInputMask IswAppPending(
    IswAppContext 	/* app_context */
);

extern IswBlockHookId IswAppAddBlockHook(
    IswAppContext 	/* app_context */,
    IswBlockHookProc 	/* proc */,
    IswPointer 		/* client_data */
);

extern void IswRemoveBlockHook(
    IswBlockHookId 	/* id */
);

/****************************************************************
 *
 * Random utility routines
 *
 ****************************************************************/

#define IswIsRectObj(object)	(_IswCheckSubclassFlag(object, (IswEnum)0x02))
#define IswIsWidget(object)	(_IswCheckSubclassFlag(object, (IswEnum)0x04))
#define IswIsComposite(widget)	(_IswCheckSubclassFlag(widget, (IswEnum)0x08))
#define IswIsConstraint(widget)	(_IswCheckSubclassFlag(widget, (IswEnum)0x10))
#define IswIsShell(widget)	(_IswCheckSubclassFlag(widget, (IswEnum)0x20))

#undef IswIsOverrideShell
extern Boolean IswIsOverrideShell(Widget /* object */);
#define IswIsOverrideShell(widget) \
    (_IswIsSubclassOf(widget, (WidgetClass)overrideShellWidgetClass, \
		     (WidgetClass)shellWidgetClass, (IswEnum)0x20))

#define IswIsWMShell(widget)	(_IswCheckSubclassFlag(widget, (IswEnum)0x40))

#undef IswIsVendorShell
extern Boolean IswIsVendorShell(Widget /* object */);
#define IswIsVendorShell(widget)	\
    (_IswIsSubclassOf(widget, (WidgetClass)vendorShellWidgetClass, \
		     (WidgetClass)wmShellWidgetClass, (IswEnum)0x40))

#undef IswIsTransientShell
extern Boolean IswIsTransientShell(Widget /* object */);
#define IswIsTransientShell(widget) \
    (_IswIsSubclassOf(widget, (WidgetClass)transientShellWidgetClass, \
		     (WidgetClass)wmShellWidgetClass, (IswEnum)0x40))
#define IswIsTopLevelShell(widget) (_IswCheckSubclassFlag(widget, (IswEnum)0x80))

#undef IswIsApplicationShell
extern Boolean IswIsApplicationShell(Widget /* object */);
#define IswIsApplicationShell(widget) \
    (_IswIsSubclassOf(widget, (WidgetClass)applicationShellWidgetClass, \
		     (WidgetClass)topLevelShellWidgetClass, (IswEnum)0x80))

extern void IswRealizeWidget(
    Widget 		/* widget */
);

void IswUnrealizeWidget(
    Widget 		/* widget */
);

extern void IswDestroyWidget(
    Widget 		/* widget */
);

extern void IswSetSensitive(
    Widget 		/* widget */,
    _IswBoolean 		/* sensitive */
);

extern void IswSetMappedWhenManaged(
    Widget 		/* widget */,
    _IswBoolean 		/* mapped_when_managed */
);

extern Widget IswNameToWidget(
    Widget 		/* reference */,
    _Xconst _IswString	/* names */
);

/* Window→widget association table (X11 backend; window-facing API used by
   backend protocol code — selections, tray, shell.  The toolkit core is
   windowless and does not use these). */
extern Widget IswWindowToWidget(
    IswDisplay 		/* dpy */,
    IswWindow 		/* window */
);

extern IswPointer IswGetClassExtension(
    WidgetClass		/* object_class */,
    Cardinal		/* byte_offset */,
    IswQuark		/* type */,
    long		/* version */,
    Cardinal		/* record_size */
);

/***************************************************************
 *
 * Arg lists
 *
 ****************************************************************/


#define IswSetArg(arg, n, d) \
    ((void)( (arg).name = (n), (arg).value = (IswArgVal)(d) ))


#define ISW_ARGBUILDER_MAX 32

typedef struct _IswArgBuilder IswArgBuilder;

struct _IswArgBuilder {
    Arg                 args[ISW_ARGBUILDER_MAX];
    Cardinal            count;
};

static inline void IswArgBuilderAdd(IswArgBuilder *ab, String name, IswArgVal value)
{
    if (ab->count < ISW_ARGBUILDER_MAX) {
        IswSetArg(ab->args[ab->count], name, value);
        ab->count++;
    }
}

#define IswArgBuilderInit() \
    { .args = {{0}}, .count = 0}

#define IswArgBuilderSet(ab, name, val) \
    ((ab)->add((ab), (name), (IswArgVal)(val)))

static inline void
IswArgBuilderReset(IswArgBuilder *ab)
{
    ab->count = 0;
}

extern ArgList IswMergeArgLists(
    ArgList 		/* args1 */,
    Cardinal 		/* num_args1 */,
    ArgList 		/* args2 */,
    Cardinal 		/* num_args2 */
);

/***************************************************************
 *
 * Vararg lists
 *
 ****************************************************************/

#define IswVaNestedList  "IswVaNestedList"
#define IswVaTypedArg    "IswVaTypedArg"

extern IswVarArgsList IswVaCreateArgsList(
    IswPointer		/*unused*/, ...
) _X_SENTINEL(0);

/*************************************************************
 *
 * Information routines
 *
 ************************************************************/

#ifndef _IswIntrinsicP_h

/* We're not included from the private file, so define these */

extern IswDisplay IswDisplayOf(
    Widget 		/* widget */
);

extern IswDisplay IswDisplayOfObject(
    Widget 		/* object */
);

extern IswScreen IswScreenOf(
    Widget 		/* widget */
);

extern IswScreen IswScreenOfObject(
    Widget 		/* object */
);

/* The widget's own render surface (core.surface).  Core widgets have no window
   — they render to this surface, which the platform composites and blits to the
   single top-level window it owns.  May be NULL before the surface is created
   (unrealized / zero-sized). */
extern IswSurface IswSurfaceOf(
    Widget 		/* widget */
);

extern String IswName(
    Widget 		/* object */
);

extern WidgetClass IswSuperclass(
    Widget 		/* object */
);

extern WidgetClass IswClass(
    Widget 		/* object */
);

extern Widget IswParent(
    Widget 		/* widget */
);

#endif /*_IswIntrinsicP_h*/

/* Call the functions (Functions.c), which handle both windowed widgets and
   windowless widgets.  The old inline macros called xcb_map_window on
   IswWindow(), which for a windowless widget is the SHARED ancestor window —
   wrong: it never hid/showed the widget itself and broke e.g. Tabs page
   switching.  No inline fast path: map/unmap is not hot, and the macro can't
   tell a shell from a windowless widget from the public header anyway. */
#undef IswMapWidget
extern void IswMapWidget(Widget /* w */);

#undef IswUnmapWidget
extern void IswUnmapWidget(Widget /* w */);

extern void IswAddCallback(
    Widget 		/* widget */,
    _Xconst _IswString 	/* callback_name */,
    IswCallbackProc 	/* callback */,
    IswPointer 		/* closure */
);

extern void IswRemoveCallback(
    Widget 		/* widget */,
    _Xconst _IswString 	/* callback_name */,
    IswCallbackProc 	/* callback */,
    IswPointer 		/* closure */
);

extern void IswAddCallbacks(
    Widget 		/* widget */,
    _Xconst _IswString	/* callback_name */,
    IswCallbackList 	/* callbacks */
);

extern void IswRemoveCallbacks(
    Widget 		/* widget */,
    _Xconst _IswString 	/* callback_name */,
    IswCallbackList 	/* callbacks */
);

extern void IswRemoveAllCallbacks(
    Widget 		/* widget */,
    _Xconst _IswString 	/* callback_name */
);


extern void IswCallCallbacks(
    Widget 		/* widget */,
    _Xconst _IswString 	/* callback_name */,
    IswPointer 		/* call_data */
);

extern void IswCallCallbackList(
    Widget		/* widget */,
    IswCallbackList 	/* callbacks */,
    IswPointer 		/* call_data */
);

extern IswCallbackStatus IswHasCallbacks(
    Widget 		/* widget */,
    _Xconst _IswString 	/* callback_name */
);

/****************************************************************
 *
 * Geometry Management
 *
 ****************************************************************/


extern IswGeometryResult IswMakeGeometryRequest(
    Widget 		/* widget */,
    IswWidgetGeometry*	/* request */,
    IswWidgetGeometry*	/* reply_return */
);

extern IswGeometryResult IswQueryGeometry(
    Widget 		/* widget */,
    IswWidgetGeometry*	/* intended */,
    IswWidgetGeometry*	/* preferred_return */
);

extern Widget IswCreatePopupShell(
    _Xconst _IswString	/* name */,
    WidgetClass 	/* widgetClass */,
    Widget 		/* parent */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern Widget IswVaCreatePopupShell(
    _Xconst _IswString	/* name */,
    WidgetClass		/* widgetClass */,
    Widget		/* parent */,
    ...
) _X_SENTINEL(0);

extern void IswPopup(
    Widget 		/* popup_shell */,
    IswGrabKind 		/* grab_kind */
);

extern void IswCallbackNone(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    IswPointer 		/* call_data */
);

extern void IswCallbackNonexclusive(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    IswPointer 		/* call_data */
);

extern void IswCallbackExclusive(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    IswPointer 		/* call_data */
);

extern void IswPopdown(
    Widget 		/* popup_shell */
);

extern void IswCallbackPopdown(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    IswPointer 		/* call_data */
);

extern void IswMenuPopupAction(
    Widget 		/* widget */,
    IswEvent*		/* event */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

extern Widget IswCreateWidget(
    _Xconst _IswString 	/* name */,
    WidgetClass 	/* widget_class */,
    Widget 		/* parent */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern Widget IswCreateManagedWidget(
    _Xconst _IswString 	/* name */,
    WidgetClass 	/* widget_class */,
    Widget 		/* parent */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern Widget IswVaCreateWidget(
    _Xconst _IswString	/* name */,
    WidgetClass		/* widget */,
    Widget		/* parent */,
    ...
) _X_SENTINEL(0);

extern Widget IswVaCreateManagedWidget(
    _Xconst _IswString	/* name */,
    WidgetClass		/* widget_class */,
    Widget		/* parent */,
    ...
) _X_SENTINEL(0);

extern Widget IswAppCreateShell(
    _Xconst _IswString	/* application_name */,
    _Xconst _IswString	/* application_class */,
    WidgetClass 	/* widget_class */,
    IswDisplay 		/* dpy */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern Widget IswVaAppCreateShell(
    _Xconst _IswString	/* application_name */,
    _Xconst _IswString	/* application_class */,
    WidgetClass		/* widget_class */,
    IswDisplay 		/* dpy */,
    ...
) _X_SENTINEL(0);

/****************************************************************
 *
 * Toolkit initialization
 *
 ****************************************************************/

extern void IswToolkitInitialize(
    void
);

extern IswLanguageProc IswSetLanguageProc(
    IswAppContext	/* app_context */,
    IswLanguageProc	/* proc */,
    IswPointer		/* client_data */
);

extern void IswDisplayInitialize(
    IswAppContext 	/* app_context */,
    IswDisplay 		/* dpy */,
    _Xconst _IswString	/* application_name */,
    _Xconst _IswString	/* application_class */,
    //IswOptionDescRec* 	/* options */,
    Cardinal 		/* num_options */,
    int*		/* argc */,
    _IswString*		/* argv */
);

extern Widget IswOpenApplication(
    IswAppContext*	/* app_context_return */,
    _Xconst _IswString	/* application_class */,
    IswOptionDescList 	/* options */,
    Cardinal 		/* num_options */,
    int*		/* argc_in_out */,
    _IswString*		/* argv_in_out */,
    String*		/* fallback_resources */,
    WidgetClass		/* widget_class */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern Widget IswVaOpenApplication(
    IswAppContext*	/* app_context_return */,
    _Xconst _IswString	/* application_class */,
    IswOptionDescList	/* options */,
    Cardinal		/* num_options */,
    int*		/* argc_in_out */,
    _IswString*		/* argv_in_out */,
    String*		/* fallback_resources */,
    WidgetClass		/* widget_class */,
    ...
) _X_SENTINEL(0);

extern Widget IswAppInitialize( /* obsolete */
    IswAppContext*	/* app_context_return */,
    _Xconst _IswString	/* application_class */,
    IswOptionDescList 	/* options */,
    Cardinal 		/* num_options */,
    int*		/* argc_in_out */,
    _IswString*		/* argv_in_out */,
    String*		/* fallback_resources */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern Widget IswVaAppInitialize( /* obsolete */
    IswAppContext*	/* app_context_return */,
    _Xconst _IswString	/* application_class */,
    IswOptionDescList	/* options */,
    Cardinal		/* num_options */,
    int*		/* argc_in_out */,
    _IswString*		/* argv_in_out */,
    String*		/* fallback_resources */,
    ...
) _X_SENTINEL(0);

extern IswDisplay IswOpenDisplay(
    IswAppContext 	/* app_context */,
    _Xconst _IswString	/* display_string */,
    _Xconst _IswString	/* application_name */,
    _Xconst _IswString	/* application_class */,
    IswOptionDescRec*	/* options */,
    Cardinal 		/* num_options */,
    int*		/* argc */,
    _IswString*		/* argv */
);

extern IswAppContext IswCreateApplicationContext(
    void
);

extern void IswAppSetFallbackResources(
    IswAppContext 	/* app_context */,
    String*		/* specification_list */
);

extern void IswDestroyApplicationContext(
    IswAppContext 	/* app_context */
);

extern void IswInitializeWidgetClass(
    WidgetClass 	/* widget_class */
);

extern IswAppContext IswWidgetToApplicationContext(
    Widget 		/* widget */
);

extern IswAppContext IswDisplayToApplicationContext(
    IswDisplay 		/* dpy */
);

extern IswDatabaseHandle IswDatabase(
    IswDisplay 		/* dpy */
);

extern IswDatabaseHandle IswScreenDatabase(
    IswScreen 		/* screen */
);

extern void IswCloseDisplay(
    IswDisplay 		/* dpy */
);

extern void IswGetApplicationResources(
    Widget 		/* widget */,
    IswPointer 		/* base */,
    IswResourceList 	/* resources */,
    Cardinal 		/* num_resources */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern void IswVaGetApplicationResources(
    Widget		/* widget */,
    IswPointer		/* base */,
    IswResourceList	/* resources */,
    Cardinal		/* num_resources */,
    ...
) _X_SENTINEL(0);

extern void IswGetSubresources(
    Widget 		/* widget */,
    IswPointer 		/* base */,
    _Xconst _IswString 	/* name */,
    _Xconst _IswString 	/* class */,
    IswResourceList 	/* resources */,
    Cardinal 		/* num_resources */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern void IswVaGetSubresources(
    Widget		/* widget */,
    IswPointer		/* base */,
    _Xconst _IswString	/* name */,
    _Xconst _IswString	/* class */,
    IswResourceList	/* resources */,
    Cardinal		/* num_resources */,
    ...
) _X_SENTINEL(0);

extern void IswSetValues(
    Widget 		/* widget */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern void IswReloadResources(
    Widget		/* subtree_root */
);

extern void IswReloadScreenDatabase(
    IswScreen 	/* screen */
);

extern void IswVaSetValues(
    Widget		/* widget */,
    ...
) _X_SENTINEL(0);

extern void IswGetValues(
    Widget 		/* widget */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern void IswVaGetValues(
    Widget		/* widget */,
    ...
) _X_SENTINEL(0);

extern void IswSetSubvalues(
    IswPointer 		/* base */,
    IswResourceList 	/* resources */,
    Cardinal 		/* num_resources */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern void IswVaSetSubvalues(
    IswPointer		/* base */,
    IswResourceList	/* resources */,
    Cardinal		/* num_resources */,
    ...
) _X_SENTINEL(0);

extern void IswGetSubvalues(
    IswPointer 		/* base */,
    IswResourceList 	/* resources */,
    Cardinal 		/* num_resources */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern void IswVaGetSubvalues(
    IswPointer		/* base */,
    IswResourceList	/* resources */,
    Cardinal		/* num_resources */,
    ...
) _X_SENTINEL(0);

extern void IswGetResourceList(
    WidgetClass 	/* widget_class */,
    IswResourceList*	/* resources_return */,
    Cardinal*		/* num_resources_return */
);

extern void IswGetConstraintResourceList(
    WidgetClass 	/* widget_class */,
    IswResourceList*	/* resources_return */,
    Cardinal*		/* num_resources_return */
);

#define IswUnspecifiedPixmap	((IswPixmap)2)
#define IswUnspecifiedShellInt	(-1)
#define IswUnspecifiedWindow	((IswWindow)(uintptr_t)2)
#define IswUnspecifiedWindowGroup ((IswWindow)(uintptr_t)3)
#define IswCurrentDirectory	((IswPointer)"IswCurrentDirectory")
#define IswDefaultForeground	((IswPointer)"IswDefaultForeground")
#define IswDefaultBackground	((IswPointer)"IswDefaultBackground")
#define IswDefaultFont		((IswPointer)"IswDefaultFont")
#define IswDefaultFontSet	((IswPointer)"IswDefaultFontSet")

#define IswOffset(p_type,field) \
	((Cardinal) (((char *) (&(((p_type)NULL)->field))) - ((char *) NULL)))

#ifdef offsetof
#define IswOffsetOf(s_type,field) offsetof(s_type,field)
#else
#define IswOffsetOf(s_type,field) IswOffset(s_type*,field)
#endif

/*************************************************************
 *
 * Error Handling
 *
 ************************************************************/

extern IswErrorMsgHandler IswAppSetErrorMsgHandler(
    IswAppContext 	/* app_context */,
    IswErrorMsgHandler 	/* handler */ _X_NORETURN
);

extern void IswSetErrorMsgHandler( /* obsolete */
    IswErrorMsgHandler 	/* handler */ _X_NORETURN
);

extern IswErrorMsgHandler IswAppSetWarningMsgHandler(
    IswAppContext 	/* app_context */,
    IswErrorMsgHandler 	/* handler */
);

extern void IswSetWarningMsgHandler( /* obsolete */
    IswErrorMsgHandler 	/* handler */
);

extern void IswAppErrorMsg(
    IswAppContext 	/* app_context */,
    _Xconst _IswString 	/* name */,
    _Xconst _IswString	/* type */,
    _Xconst _IswString	/* class */,
    _Xconst _IswString	/* default */,
    String*		/* params */,
    Cardinal*		/* num_params */
) _X_NORETURN;

extern void IswErrorMsg( /* obsolete */
    _Xconst _IswString 	/* name */,
    _Xconst _IswString	/* type */,
    _Xconst _IswString	/* class */,
    _Xconst _IswString	/* default */,
    String*		/* params */,
    Cardinal*		/* num_params */
) _X_NORETURN;

extern void IswAppWarningMsg(
    IswAppContext 	/* app_context */,
    _Xconst _IswString 	/* name */,
    _Xconst _IswString 	/* type */,
    _Xconst _IswString 	/* class */,
    _Xconst _IswString 	/* default */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

extern void IswWarningMsg( /* obsolete */
    _Xconst _IswString	/* name */,
    _Xconst _IswString	/* type */,
    _Xconst _IswString	/* class */,
    _Xconst _IswString	/* default */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

extern IswErrorHandler IswAppSetErrorHandler(
    IswAppContext 	/* app_context */,
    IswErrorHandler 	/* handler */ _X_NORETURN
);

extern IswErrorHandler IswAppSetWarningHandler(
    IswAppContext 	/* app_context */,
    IswErrorHandler 	/* handler */
);

extern void IswAppError(
    IswAppContext 	/* app_context */,
    _Xconst _IswString	/* message */
) _X_NORETURN;

extern void IswError( /* obsolete */
    _Xconst _IswString	/* message */
) _X_NORETURN;

extern void IswAppWarning(
    IswAppContext 	/* app_context */,
    _Xconst _IswString	/* message */
);

extern void IswWarning( /* obsolete */
    _Xconst _IswString	/* message */
);

extern IswDatabaseHandle *IswAppGetErrorDatabase(
    IswAppContext 	/* app_context */
);

extern IswDatabaseHandle *IswGetErrorDatabase( /* obsolete */
    void
);

extern void IswAppGetErrorDatabaseText(
    IswAppContext 	/* app_context */,
    _Xconst _IswString	/* name */,
    _Xconst _IswString	/* type */,
    _Xconst _IswString	/* class */,
    _Xconst _IswString 	/* default */,
    _IswString 		/* buffer_return */,
    int 		/* nbytes */,
    IswDatabaseHandle 	/* database */
);

extern void IswGetErrorDatabaseText( /* obsolete */
    _Xconst _IswString	/* name */,
    _Xconst _IswString	/* type */,
    _Xconst _IswString	/* class */,
    _Xconst _IswString 	/* default */,
    _IswString 		/* buffer_return */,
    int 		/* nbytes */
);

/****************************************************************
 *
 * Memory Management
 *
 ****************************************************************/

extern char *IswMalloc(
    Cardinal 		/* size */
);

extern char *IswCalloc(
    Cardinal		/* num */,
    Cardinal 		/* size */
);

extern char *IswRealloc(
    char* 		/* ptr */,
    Cardinal 		/* num */
);

extern void *IswReallocArray(
    void * 		/* ptr */,
    Cardinal 		/* num */,
    Cardinal 		/* size */
);

extern void IswFree(
    const void*		/* ptr */
);

#ifndef _X_RESTRICT_KYWD
# define _X_RESTRICT_KYWD
#endif
extern Cardinal IswAsprintf(
    _IswString *new_string,
    _Xconst char * _X_RESTRICT_KYWD format,
    ...
) _X_ATTRIBUTE_PRINTF(2,3);

#ifdef XTTRACEMEMORY

extern char *_IswMalloc( /* implementation-private */
    Cardinal	/* size */,
    const char */* file */,
    int	        /* line */
);

extern char *_IswRealloc( /* implementation-private */
    char *	/* ptr */,
    Cardinal    /* size */,
    const char */* file */,
    int		/* line */
);

extern char *_IswReallocArray( /* implementation-private */
    char *	/* ptr */,
    Cardinal	/* num */,
    Cardinal    /* size */,
    const char */* file */,
    int		/* line */
);

extern char *_IswCalloc( /* implementation-private */
    Cardinal	/* num */,
    Cardinal 	/* size */,
    const char */* file */,
    int		/* line */
);

extern void _IswFree( /* implementation-private */
    const void *	/* ptr */
);

extern Boolean _IswIsValidPointer( /* implementation-private */
    const void *	/* ptr */);

extern void _IswPrintMemory( /* implementation-private */
    const char */* filename */);

#define IswMalloc(size) _IswMalloc(size, __FILE__, __LINE__)
#define IswRealloc(ptr,size) _IswRealloc(ptr, size, __FILE__, __LINE__)
#define IswMallocArray(num,size) _IswReallocArray(NULL, num, size, __FILE__, __LINE__)
#define IswReallocArray(ptr,num,size) _IswReallocArray(ptr, num, size, __FILE__, __LINE__)
#define IswCalloc(num,size) _IswCalloc(num, size, __FILE__, __LINE__)
#define IswFree(ptr) _IswFree(ptr)

#else

#define IswMallocArray(num,size) IswReallocArray(NULL, num, size)

#endif /* ifdef XTTRACEMEMORY */

#define IswNew(type) ((type *) IswMalloc((unsigned) sizeof(type)))

#undef IswNewString
extern String IswNewString(String /* str */);
#define IswNewString(str) \
    ((str) != NULL ? (strcpy(IswMalloc((unsigned)strlen(str) + 1), str)) : NULL)

/* Copy `src` to `dst`, lowercasing ISO Latin-1 (neutral string utility,
   replacement for libXmu's XmuCopyISOLatin1Lowered). */
extern void ISWCopyISOLatin1Lowered(char * /* dst */, const char * /* src */);

/* Case-insensitive string compare (ASCII case folding), strcmp-like result. */
extern int ISWCompareISOLatin1(const char * /* first */, const char * /* second */);

/*************************************************************
 *
 *  Work procs
 *
 **************************************************************/

extern IswWorkProcId IswAddWorkProc( /* obsolete */
    IswWorkProc 		/* proc */,
    IswPointer 		/* closure */
);

extern IswWorkProcId IswAppAddWorkProc(
    IswAppContext 	/* app_context */,
    IswWorkProc 		/* proc */,
    IswPointer 		/* closure */
);

extern void  IswRemoveWorkProc(
    IswWorkProcId 	/* id */
);




extern void IswAppReleaseCacheRefs(
    IswAppContext	/* app_context */,
    IswCacheRef*		/* cache_ref */
);

extern void IswCallbackReleaseCacheRef(
    Widget 		/* widget */,
    IswPointer 		/* closure */,	/* IswCacheRef */
    IswPointer 		/* call_data */
);

extern void IswCallbackReleaseCacheRefList(
    Widget 		/* widget */,
    IswPointer 		/* closure */,	/* IswCacheRef* */
    IswPointer 		/* call_data */
);

extern _IswString IswFindFile(
    _Xconst _IswString	/* path */,
    Substitution	/* substitutions */,
    Cardinal 		/* num_substitutions */,
    IswFilePredicate	/* predicate */
);

extern _IswString IswResolvePathname(
    IswDisplay 		/* dpy */,
    _Xconst _IswString	/* type */,
    _Xconst _IswString	/* filename */,
    _Xconst _IswString	/* suffix */,
    _Xconst _IswString	/* path */,
    Substitution	/* substitutions */,
    Cardinal		/* num_substitutions */,
    IswFilePredicate 	/* predicate */
);

/****************************************************************
 *
 * Selections
 *
 *****************************************************************/

#define ISW_CONVERT_FAIL (Atom)0x80000001

extern void IswDisownSelection(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswTime 		/* time */
);

extern void IswGetSelectionValue(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswSelectionId 	/* target */,
    IswSelectionCallbackProc /* callback */,
    IswPointer 		/* closure */,
    IswTime 		/* time */
);

extern void IswGetSelectionValues(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswSelectionId*	/* targets */,
    int 		/* count */,
    IswSelectionCallbackProc /* callback */,
    IswPointer*		/* closures */,
    IswTime 		/* time */
);

extern void IswAppSetSelectionTimeout(
    IswAppContext 	/* app_context */,
    unsigned long 	/* timeout */
);

extern void IswSetSelectionTimeout( /* obsolete */
    unsigned long 	/* timeout */
);

extern unsigned long IswAppGetSelectionTimeout(
    IswAppContext 	/* app_context */
);

extern unsigned long IswGetSelectionTimeout( /* obsolete */
    void
);

extern IswSelectionRequest *IswGetSelectionRequest(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswRequestId 	/* request_id */
);

extern void IswGetSelectionValueIncremental(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswSelectionId 	/* target */,
    IswSelectionCallbackProc /* selection_callback */,
    IswPointer 		/* client_data */,
    IswTime 		/* time */
);

extern void IswGetSelectionValuesIncremental(
    Widget 		/* widget */,
    IswSelectionId 	/* selection */,
    IswSelectionId*	/* targets */,
    int 		/* count */,
    IswSelectionCallbackProc /* callback */,
    IswPointer*		/* client_data */,
    IswTime 		/* time */
);

extern void IswSetSelectionParameters(
    Widget		/* requestor */,
    IswSelectionId	/* selection */,
    IswSelectionId	/* type */,
    IswPointer		/* value */,
    unsigned long	/* length */,
    int			/* format */
);

extern void IswGetSelectionParameters(
    Widget		/* owner */,
    IswSelectionId	/* selection */,
    IswRequestId		/* request_id */,
    IswSelectionId*	/* type_return */,
    IswPointer*		/* value_return */,
    unsigned long*	/* length_return */,
    int*		/* format_return */
);

extern void IswCreateSelectionRequest(
    Widget		/* requestor */,
    IswSelectionId	/* selection */
);

extern void IswSendSelectionRequest(
    Widget		/* requestor */,
    IswSelectionId	/* selection */,
    IswTime		/* time */
);

extern void IswCancelSelectionRequest(
    Widget		/* requestor */,
    IswSelectionId	/* selection */
);

extern Atom IswReservePropertyAtom(
    Widget		/* widget */
);

extern void IswReleasePropertyAtom(
    Widget		/* widget */,
    Atom		/* selection */
);

extern void IswGrabKey(
    Widget 		/* widget */,
    IswKeyCode 		/* keycode */,
    Modifiers	 	/* modifiers */,
    _IswBoolean 		/* owner_events */
);

extern void IswUngrabKey(
    Widget 		/* widget */,
    IswKeyCode 		/* keycode */,
    Modifiers	 	/* modifiers */
);

extern int IswGrabKeyboard(
    Widget 		/* widget */,
    _IswBoolean 		/* owner_events */,
    IswTime 		/* time */
);

extern void IswUngrabKeyboard(
    Widget 		/* widget */,
    IswTime 		/* time */
);

extern void IswGrabButton(
    Widget 		/* widget */,
    int 		/* button */,
    Modifiers	 	/* modifiers */,
    _IswBoolean 		/* owner_events */,
    unsigned int	/* event_mask */,
    IswCursor 		/* cursor */
);

extern void IswUngrabButton(
    Widget 		/* widget */,
    unsigned int	/* button */,
    Modifiers	 	/* modifiers */
);

extern int IswGrabPointer(
    Widget 		/* widget */,
    _IswBoolean 		/* owner_events */,
    unsigned int	/* event_mask */,
    IswCursor 		/* cursor */,
    IswTime 		/* time */
);

extern void IswUngrabPointer(
    Widget 		/* widget */,
    IswTime 		/* time */
);

extern void IswGetApplicationNameAndClass(
    IswDisplay 		/* dpy */,
    String*		/* name_return */,
    String*		/* class_return */
);

extern void IswRegisterDrawable(
    IswDisplay 		/* dpy */,
    IswWindow		/* drawable */,
    Widget		/* widget */
);

extern void IswUnregisterDrawable(
    IswDisplay 		/* dpy */,
    IswWindow		/* drawable */
);

extern Widget IswHooksOfDisplay(
    IswDisplay 		/* dpy */
);

typedef struct {
    String type;
    Widget widget;
    ArgList args;
    Cardinal num_args;
} IswCreateHookDataRec, *IswCreateHookData;

typedef struct {
    String type;
    Widget widget;
    IswPointer event_data;
    Cardinal num_event_data;
} IswChangeHookDataRec, *IswChangeHookData;

typedef struct {
    Widget old, req;
    ArgList args;
    Cardinal num_args;
} IswChangeHookSetValuesDataRec, *IswChangeHookSetValuesData;

typedef struct {
    String type;
    Widget widget;
    IswGeometryMask changeMask;
    int32_t changes_x;
    int32_t changes_y;
    int32_t changes_h;
    int32_t changes_w;
    int32_t changes_bw;
    int32_t changes_sm;
    int32_t changes_sb;
} IswConfigureHookDataRec, *IswConfigureHookData;

typedef struct {
    String type;
    Widget widget;
    IswWidgetGeometry* request;
    IswWidgetGeometry* reply;
    IswGeometryResult result;
} IswGeometryHookDataRec, *IswGeometryHookData;

typedef struct {
    String type;
    Widget widget;
} IswDestroyHookDataRec, *IswDestroyHookData;

extern void IswGetDisplays(
    IswAppContext	/* app_context */,
    IswDisplay **		/* dpy_return */,
    Cardinal*		/* num_dpy_return */
);

extern Boolean IswToolkitThreadInitialize(
    void
);

extern void IswAppSetExitFlag(
    IswAppContext	/* app_context */
);

extern Boolean IswAppGetExitFlag(
    IswAppContext	/* app_context */
);

extern void IswAppLock(
    IswAppContext	/* app_context */
);

extern void IswAppUnlock(
    IswAppContext	/* app_context */
);

/*
 *	Predefined Resource Converters
 */


/* String converters */

extern Boolean IswCvtStringToAcceleratorTable(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToBool(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToBoolean(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToCommandArgArray(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToCursor(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* Display */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToDimension(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToDirectoryString(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToDisplay(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFile(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFloat(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFont(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* Display */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFontStruct(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* Display */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToGravity(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToInitialState(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToInt(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToPixel(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* Screen, Colormap */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

#define IswCvtStringToPosition IswCvtStringToShort


extern Boolean IswCvtStringToShort(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToTranslationTable(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToUnsignedChar(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToVisual(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* Screen, depth */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

/* Widget-set value converters (neutral; libXmu replacements). */
extern Boolean ISWCvtStringToOrientation(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean ISWCvtStringToJustify(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean ISWCvtStringToWidget(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* parent widget */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

/* int converters */

extern Boolean IswCvtIntToBool(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToBoolean(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToColor(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* Screen, Colormap */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

#define IswCvtIntToDimension IswCvtIntToShort

extern Boolean IswCvtIntToFloat(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToFont(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToPixel(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToPixmap(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

#define IswCvtIntToPosition IswCvtIntToShort

extern Boolean IswCvtIntToShort(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToUnsignedChar(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

/* Color converter */

extern Boolean IswCvtColorToPixel(
    IswDisplay 	/* dpy */,
    IswValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    IswValuePtr	/* fromVal */,
    IswValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

/* Pixel converter */

#define IswCvtPixelToColor IswCvtIntToColor


_XFUNCPROTOEND

#endif /*_IswIntrinsic_h*/
/* DON'T ADD STUFF AFTER THIS #endif */
