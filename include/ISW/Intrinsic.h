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
#include    <xcb/xcb.h>
#include    <xcb/xfixes.h>
#include    <xcb/xcbext.h>
#include    <xcb/xkb.h>
#include    <xcb/xcb_keysyms.h>
#include	<ISW/IswTypes.h>
/* Xresource.h replaced by custom IswQuark/IswValue/IswDatabase/IswOptions headers */
#include	<ISW/IswFuncproto.h>
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
#define _IswKeyCode	unsigned int
#define _IswPosition	int
#define _IswEnum	unsigned int
#else
#define _IswBoolean	Boolean
#define _IswDimension	Dimension
#define _IswKeyCode	xcb_keycode_t
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
typedef struct _IswActionsRec *IswActionList;
typedef struct _IswEventRec *IswEventTable;

typedef struct _IswAppStruct *IswAppContext;
typedef unsigned long	IswValueMask;
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
    xcb_generic_event_t*		/* event */,
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
    XrmValue*		/* value */
);

typedef struct {
    IswGeometryMask request_mode;
    Position x, y;
    Dimension width, height, border_width;
    Widget sibling;
    int stack_mode;   /* Above, Below, TopIf, BottomIf, Opposite, DontChange */
} IswWidgetGeometry;

/* Additions to Xlib geometry requests: ask what would happen, don't do it */
#define IswCWQueryOnly	(1 << 7)

/* Additions to Xlib stack modes: don't change stack order */
#define IswSMDontChange	5

typedef void (*IswConverter)( /* obsolete */
    XrmValue*		/* args */,
    Cardinal*		/* num_args */,
    XrmValue*		/* from */,
    XrmValue*		/* to */
);

typedef Boolean (*IswTypeConverter)(
    xcb_connection_t *		/* dpy */,
    XrmValue*		/* args */,
    Cardinal*		/* num_args */,
    XrmValue*		/* from */,
    XrmValue*		/* to */,
    IswPointer*		/* converter_data */
);

typedef void (*IswDestructor)(
    IswAppContext	/* app */,
    XrmValue*		/* to */,
    IswPointer 		/* converter_data */,
    XrmValue*		/* args */,
    Cardinal*		/* num_args */
);

typedef Opaque IswCacheRef;

typedef Opaque IswActionHookId;

typedef void (*IswActionHookProc)(
    Widget		/* w */,
    IswPointer		/* client_data */,
    String		/* action_name */,
    xcb_generic_event_t*		/* event */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

typedef IswUIntPtr IswBlockHookId;

typedef void (*IswBlockHookProc)(
    IswPointer		/* client_data */
);

typedef void (*IswKeyProc)(
    xcb_connection_t *		/* dpy */,
    _IswKeyCode 		/* keycode */,
    Modifiers		/* modifiers */,
    Modifiers*		/* modifiers_return */,
    xcb_keysym_t*		/* keysym_return */
);

typedef void (*IswCaseProc)(
    xcb_connection_t*		/* xcb_connection_t */,
    xcb_keysym_t		/* keysym */,
    xcb_keysym_t*		/* lower_return */,
    xcb_keysym_t*		/* upper_return */
);

typedef void (*IswEventHandler)(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    xcb_generic_event_t*		/* event */,
    Boolean*		/* continue_to_dispatch */
);
typedef unsigned long EventMask;

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
    XrmValue*	/* value */
);

typedef String (*IswLanguageProc)(
    xcb_connection_t *	/* dpy */,
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

typedef Boolean (*IswConvertSelectionProc)(
    Widget 		/* widget */,
    xcb_atom_t*		/* selection */,
    xcb_atom_t*		/* target */,
    xcb_atom_t*		/* type_return */,
    IswPointer*		/* value_return */,
    unsigned long*	/* length_return */,
    int*		/* format_return */
);

typedef void (*IswLoseSelectionProc)(
    Widget 		/* widget */,
    xcb_atom_t*		/* selection */
);

typedef void (*IswSelectionDoneProc)(
    Widget 		/* widget */,
    xcb_atom_t*		/* selection */,
    xcb_atom_t*		/* target */
);

typedef void (*IswSelectionCallbackProc)(
    Widget 		/* widget */,
    IswPointer 		/* closure */,
    xcb_atom_t*		/* selection */,
    xcb_atom_t*		/* type */,
    IswPointer 		/* value */,
    unsigned long*	/* length */,
    int*		/* format */
);

typedef void (*IswLoseSelectionIncrProc)(
    Widget 		/* widget */,
    xcb_atom_t*		/* selection */,
    IswPointer 		/* client_data */
);

typedef void (*IswSelectionDoneIncrProc)(
    Widget 		/* widget */,
    xcb_atom_t*		/* selection */,
    xcb_atom_t*		/* target */,
    IswRequestId*	/* receiver_id */,
    IswPointer 		/* client_data */
);

typedef Boolean (*IswConvertSelectionIncrProc)(
    Widget 		/* widget */,
    xcb_atom_t*		/* selection */,
    xcb_atom_t*		/* target */,
    xcb_atom_t*		/* type */,
    IswPointer*		/* value */,
    unsigned long*	/* length */,
    int*		/* format */,
    unsigned long*	/* max_length */,
    IswPointer 		/* client_data */,
    IswRequestId*	/* receiver_id */
);

typedef void (*IswCancelConvertSelectionProc)(
    Widget 		/* widget */,
    xcb_atom_t*		/* selection */,
    xcb_atom_t*		/* target */,
    IswRequestId*	/* receiver_id */,
    IswPointer 		/* client_data */
);

typedef Boolean (*IswEventDispatchProc)(
    xcb_generic_event_t*		/* event */,
    xcb_connection_t*           /* connection */
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
    XrmValue*		/* from */,
    _Xconst _IswString 	/* to_type */,
    XrmValue*		/* to_in_out */
);

extern Boolean IswCallConverter(
    xcb_connection_t*		/* dpy */,
    IswTypeConverter 	/* converter */,
    XrmValuePtr 	/* args */,
    Cardinal 		/* num_args */,
    XrmValuePtr 	/* from */,
    XrmValue*		/* to_in_out */,
    IswCacheRef*		/* cache_ref_return */
);

extern Boolean IswDispatchEvent(
    xcb_generic_event_t*, 
    xcb_connection_t *
);

extern Boolean IswCallAcceptFocus(
    Widget 		/* widget */,
    xcb_timestamp_t*		/* time */
);

extern Boolean IswAppPeekEvent(
    IswAppContext 	/* app_context */,
    xcb_generic_event_t*		/* event_return */
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
    xcb_atom_t 		/* selection */,
    xcb_timestamp_t 		/* time */,
    IswConvertSelectionProc /* convert */,
    IswLoseSelectionProc	/* lose */,
    IswSelectionDoneProc /* done */
);

extern Boolean IswOwnSelectionIncremental(
    Widget 		/* widget */,
    xcb_atom_t 		/* selection */,
    xcb_timestamp_t 		/* time */,
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

extern xcb_key_symbols_t* IswGetKeysymTable(
    xcb_connection_t *		/* dpy */,
    xcb_keycode_t *		/* min_keycode_return */,
    int*		/* keysyms_per_keycode_return */
);

extern void IswKeysymToKeycodeList(
    xcb_connection_t *		/* dpy */,
    xcb_keysym_t 		/* keysym */,
    xcb_keycode_t**		/* keycodes_return */,
    Cardinal*		/* keycount_return */
);

extern void IswDisplayStringConversionWarning(
    xcb_connection_t *	 	/* dpy */,
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

//extern void IswConvert( /* obsolete */
//    Widget 		/* widget */,
//    _Xconst _IswString 	/* from_type */,
//    XrmValue*		/* from */,
//    _Xconst _IswString 	/* to_type */,
//    XrmValue*		/* to_return */
//);
//
//extern void IswDirectConvert( /* obsolete */
//    IswConverter 	/* converter */,
//    XrmValuePtr 	/* args */,
//    Cardinal 		/* num_args */,
//    XrmValuePtr 	/* from */,
//    XrmValue*		/* to_return */
//);

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
    xcb_generic_event_t*		/* event */,
    String*		/* params */,
    Cardinal		/* num_params */
);

extern void IswRegisterGrabAction(
    IswActionProc 	/* action_proc */,
    _IswBoolean 		/* owner_events */,
    unsigned int 	/* event_mask */,
    int			/* pointer_mode */,
    int	 		/* keyboard_mode */
);

extern void IswSetMultiClickTime(
    xcb_connection_t *		/* dpy */,
    int 		/* milliseconds */
);

extern int IswGetMultiClickTime(
    xcb_connection_t *		/* dpy */
);

extern xcb_keysym_t IswGetActionKeysym(
    xcb_generic_event_t*		/* event */,
    Modifiers*		/* modifiers_return */,
    xcb_connection_t *
);

/***************************************************************
 *
 * Keycode and Keysym procedures for translation management
 *
 ****************************************************************/

extern void IswTranslateKeycode(
    xcb_connection_t *		/* dpy */,
    _IswKeyCode 		/* keycode */,
    Modifiers 		/* modifiers */,
    Modifiers*		/* modifiers_return */,
    xcb_keysym_t*		/* keysym_return */
);

extern void IswTranslateKey(
    xcb_connection_t *		/* dpy */,
    _IswKeyCode		/* keycode */,
    Modifiers		/* modifiers */,
    Modifiers*		/* modifiers_return */,
    xcb_keysym_t*		/* keysym_return */
);

extern void IswSetKeyTranslator(
    xcb_connection_t *		/* dpy */,
    IswKeyProc 		/* proc */
);

extern void IswRegisterCaseConverter(
    xcb_connection_t *		/* dpy */,
    IswCaseProc 		/* proc */,
    xcb_keysym_t 		/* start */,
    xcb_keysym_t 		/* stop */
);

extern void IswConvertCase(
    xcb_connection_t *		/* dpy */,
    xcb_keysym_t 		/* keysym */,
    xcb_keysym_t*		/* lower_return */,
    xcb_keysym_t*		/* upper_return */
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
    xcb_connection_t *		/* dpy */,
    int			/* event_type */,
    IswEventDispatchProc	/* proc */
);

extern Boolean IswDispatchEventToWidget(
    Widget		                /* widget */,
    xcb_generic_event_t*		/* event */
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
    xcb_connection_t *		/* dpy */,
    int			/* min_event_type */,
    int			/* max_event_type */,
    IswExtensionSelectProc /* proc */,
    IswPointer		/* client_data */
);

extern void IswAddGrab(
    Widget 		/* widget */,
    _IswBoolean 		/* exclusive */,
    _IswBoolean 		/* spring_loaded */
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

void get_region_bounding_box(
    xcb_connection_t*, 
    xcb_xfixes_region_t, 
    xcb_rectangle_t*
);

extern void IswAddExposureToRegion(
    xcb_connection_t *, 
    xcb_generic_event_t *, 
    xcb_xfixes_region_t
);

extern void IswSetKeyboardFocus(
    Widget		/* subtree */,
    Widget 		/* descendent */
);

extern Widget IswGetKeyboardFocusWidget(
    Widget		/* widget */
);

extern xcb_generic_event_t* IswLastEventProcessed(
    xcb_connection_t *		/* dpy */
);

extern xcb_timestamp_t IswLastTimestampProcessed(
    xcb_connection_t *		/* dpy */
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
    xcb_generic_event_t* 		/* event */
);

extern void IswAppNextEvent(
    IswAppContext 	/* app_context */,
    xcb_generic_event_t*		/* event_return */
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

extern Widget IswWindowToWidget(
    xcb_connection_t*		/* xcb_connection_t */,
    xcb_window_t 		/* window */
);

extern IswPointer IswGetClassExtension(
    WidgetClass		/* object_class */,
    Cardinal		/* byte_offset */,
    XrmQuark		/* type */,
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

extern xcb_connection_t *IswDisplay(
    Widget 		/* widget */
);

extern xcb_connection_t *IswDisplayOfObject(
    Widget 		/* object */
);

extern xcb_screen_t *IswScreen(
    Widget 		/* widget */
);

extern xcb_screen_t *IswScreenOfObject(
    Widget 		/* object */
);

extern xcb_window_t IswWindow(
    Widget 		/* widget */
);

extern xcb_window_t IswWindowOfObject(
    Widget 		/* object */
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

#undef IswMapWidget
extern void IswMapWidget(Widget /* w */);
#define IswMapWidget(widget)	do { xcb_map_window(IswDisplay(widget), IswWindow(widget)); xcb_flush(IswDisplay(widget)); } while(0)

#undef IswUnmapWidget
extern void IswUnmapWidget(Widget /* w */);
#define IswUnmapWidget(widget)	do { xcb_unmap_window(IswDisplay(widget), IswWindow(widget)); xcb_flush(IswDisplay(widget)); } while(0)

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

extern void IswPopupSpringLoaded(
    Widget 		/* popup_shell */
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
    xcb_generic_event_t*		/* event */,
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
    xcb_connection_t*		/* xcb_connection_t */,
    ArgList 		/* args */,
    Cardinal 		/* num_args */
);

extern Widget IswVaAppCreateShell(
    _Xconst _IswString	/* application_name */,
    _Xconst _IswString	/* application_class */,
    WidgetClass		/* widget_class */,
    xcb_connection_t*		/* xcb_connection_t */,
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
    xcb_connection_t *		/* dpy */,
    _Xconst _IswString	/* application_name */,
    _Xconst _IswString	/* application_class */,
    //XrmOptionDescRec* 	/* options */,
    Cardinal 		/* num_options */,
    int*		/* argc */,
    _IswString*		/* argv */
);

extern Widget IswOpenApplication(
    IswAppContext*	/* app_context_return */,
    _Xconst _IswString	/* application_class */,
    XrmOptionDescList 	/* options */,
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
    XrmOptionDescList	/* options */,
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
    XrmOptionDescList 	/* options */,
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
    XrmOptionDescList	/* options */,
    Cardinal		/* num_options */,
    int*		/* argc_in_out */,
    _IswString*		/* argv_in_out */,
    String*		/* fallback_resources */,
    ...
) _X_SENTINEL(0);

extern xcb_connection_t *IswOpenDisplay(
    IswAppContext 	/* app_context */,
    _Xconst _IswString	/* display_string */,
    _Xconst _IswString	/* application_name */,
    _Xconst _IswString	/* application_class */,
    XrmOptionDescRec*	/* options */,
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
    xcb_connection_t *		/* dpy */
);

extern XrmDatabase IswDatabase(
    xcb_connection_t *		/* dpy */
);

extern XrmDatabase IswScreenDatabase(
    xcb_screen_t*		/* xcb_screen_t */
);

extern void IswCloseDisplay(
    xcb_connection_t *		/* dpy */
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

#define IswUnspecifiedPixmap	((xcb_pixmap_t)2)
#define IswUnspecifiedShellInt	(-1)
#define IswUnspecifiedWindow	((xcb_window_t)2)
#define IswUnspecifiedWindowGroup ((xcb_window_t)3)
#define IswCurrentDirectory	"IswCurrentDirectory"
#define IswDefaultForeground	"IswDefaultForeground"
#define IswDefaultBackground	"IswDefaultBackground"
#define IswDefaultFont		"IswDefaultFont"
#define IswDefaultFontSet	"IswDefaultFontSet"

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

extern XrmDatabase *IswAppGetErrorDatabase(
    IswAppContext 	/* app_context */
);

extern XrmDatabase *IswGetErrorDatabase( /* obsolete */
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
    XrmDatabase 	/* database */
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
    char*		/* ptr */
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
    char *	/* ptr */
);

extern Boolean _IswIsValidPointer( /* implementation-private */
    char *	/* ptr */);

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

extern void IswSetWMColormapWindows(
    Widget 		/* widget */,
    Widget*		/* list */,
    Cardinal		/* count */
);

extern _IswString IswFindFile(
    _Xconst _IswString	/* path */,
    Substitution	/* substitutions */,
    Cardinal 		/* num_substitutions */,
    IswFilePredicate	/* predicate */
);

extern _IswString IswResolvePathname(
    xcb_connection_t *		/* dpy */,
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

#define ISW_CONVERT_FAIL (xcb_atom_t)0x80000001

extern void IswDisownSelection(
    Widget 		/* widget */,
    xcb_atom_t 		/* selection */,
    xcb_timestamp_t 		/* time */
);

extern void IswGetSelectionValue(
    Widget 		/* widget */,
    xcb_atom_t 		/* selection */,
    xcb_atom_t 		/* target */,
    IswSelectionCallbackProc /* callback */,
    IswPointer 		/* closure */,
    xcb_timestamp_t 		/* time */
);

extern void IswGetSelectionValues(
    Widget 		/* widget */,
    xcb_atom_t 		/* selection */,
    xcb_atom_t*		/* targets */,
    int 		/* count */,
    IswSelectionCallbackProc /* callback */,
    IswPointer*		/* closures */,
    xcb_timestamp_t 		/* time */
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

extern xcb_selection_request_event_t *IswGetSelectionRequest(
    Widget 		/* widget */,
    xcb_atom_t 		/* selection */,
    IswRequestId 	/* request_id */
);

extern void IswGetSelectionValueIncremental(
    Widget 		/* widget */,
    xcb_atom_t 		/* selection */,
    xcb_atom_t 		/* target */,
    IswSelectionCallbackProc /* selection_callback */,
    IswPointer 		/* client_data */,
    xcb_timestamp_t 		/* time */
);

extern void IswGetSelectionValuesIncremental(
    Widget 		/* widget */,
    xcb_atom_t 		/* selection */,
    xcb_atom_t*		/* targets */,
    int 		/* count */,
    IswSelectionCallbackProc /* callback */,
    IswPointer*		/* client_data */,
    xcb_timestamp_t 		/* time */
);

extern void IswSetSelectionParameters(
    Widget		/* requestor */,
    xcb_atom_t		/* selection */,
    xcb_atom_t		/* type */,
    IswPointer		/* value */,
    unsigned long	/* length */,
    int			/* format */
);

extern void IswGetSelectionParameters(
    Widget		/* owner */,
    xcb_atom_t		/* selection */,
    IswRequestId		/* request_id */,
    xcb_atom_t*		/* type_return */,
    IswPointer*		/* value_return */,
    unsigned long*	/* length_return */,
    int*		/* format_return */
);

extern void IswCreateSelectionRequest(
    Widget		/* requestor */,
    xcb_atom_t		/* selection */
);

extern void IswSendSelectionRequest(
    Widget		/* requestor */,
    xcb_atom_t		/* selection */,
    xcb_timestamp_t		/* time */
);

extern void IswCancelSelectionRequest(
    Widget		/* requestor */,
    xcb_atom_t		/* selection */
);

extern xcb_atom_t IswReservePropertyAtom(
    Widget		/* widget */
);

extern void IswReleasePropertyAtom(
    Widget		/* widget */,
    xcb_atom_t		/* selection */
);

extern void IswGrabKey(
    Widget 		/* widget */,
    _IswKeyCode 		/* keycode */,
    Modifiers	 	/* modifiers */,
    _IswBoolean 		/* owner_events */,
    int 		/* pointer_mode */,
    int 		/* keyboard_mode */
);

extern void IswUngrabKey(
    Widget 		/* widget */,
    _IswKeyCode 		/* keycode */,
    Modifiers	 	/* modifiers */
);

extern int IswGrabKeyboard(
    Widget 		/* widget */,
    _IswBoolean 		/* owner_events */,
    int 		/* pointer_mode */,
    int 		/* keyboard_mode */,
    xcb_timestamp_t 		/* time */
);

extern void IswUngrabKeyboard(
    Widget 		/* widget */,
    xcb_timestamp_t 		/* time */
);

extern void IswGrabButton(
    Widget 		/* widget */,
    int 		/* button */,
    Modifiers	 	/* modifiers */,
    _IswBoolean 		/* owner_events */,
    unsigned int	/* event_mask */,
    int 		/* pointer_mode */,
    int 		/* keyboard_mode */,
    xcb_window_t 		/* confine_to */,
    xcb_cursor_t 		/* cursor */
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
    int 		/* pointer_mode */,
    int 		/* keyboard_mode */,
    xcb_window_t 		/* confine_to */,
    xcb_cursor_t 		/* cursor */,
    xcb_timestamp_t 		/* time */
);

extern void IswUngrabPointer(
    Widget 		/* widget */,
    xcb_timestamp_t 		/* time */
);

extern void IswGetApplicationNameAndClass(
    xcb_connection_t *		/* dpy */,
    String*		/* name_return */,
    String*		/* class_return */
);

extern void IswRegisterDrawable(
    xcb_connection_t *		/* dpy */,
    xcb_drawable_t		/* drawable */,
    Widget		/* widget */
);

extern void IswUnregisterDrawable(
    xcb_connection_t *		/* dpy */,
    xcb_drawable_t		/* drawable */
);

extern Widget IswHooksOfDisplay(
    xcb_connection_t *		/* dpy */
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
    uint32_t changes_x;
    uint32_t changes_y;
    uint32_t changes_h;
    uint32_t changes_w;
    uint32_t changes_bw;
    uint32_t changes_sm;
    uint32_t changes_sb;
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
    xcb_connection_t ***		/* dpy_return */,
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
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToAtom(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Display */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToBool(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToBoolean(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToCommandArgArray(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToCursor(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Display */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToDimension(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToDirectoryString(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToDisplay(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFile(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFloat(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFont(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Display */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFontSet(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Display, locale */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToFontStruct(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Display */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToGravity(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToInitialState(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToInt(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToPixel(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Screen, Colormap */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

#define IswCvtStringToPosition IswCvtStringToShort


extern Boolean IswCvtStringToShort(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToTranslationTable(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToUnsignedChar(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtStringToVisual(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Screen, depth */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

/* int converters */

extern Boolean IswCvtIntToBool(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToBoolean(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToColor(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* Screen, Colormap */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

#define IswCvtIntToDimension IswCvtIntToShort

extern Boolean IswCvtIntToFloat(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToFont(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToPixel(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToPixmap(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

#define IswCvtIntToPosition IswCvtIntToShort

extern Boolean IswCvtIntToShort(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

extern Boolean IswCvtIntToUnsignedChar(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

/* Color converter */

extern Boolean IswCvtColorToPixel(
    xcb_connection_t *	/* dpy */,
    XrmValuePtr /* args */,	/* none */
    Cardinal*   /* num_args */,
    XrmValuePtr	/* fromVal */,
    XrmValuePtr	/* toVal */,
    IswPointer*	/* closure_ret */
);

/* Pixel converter */

#define IswCvtPixelToColor IswCvtIntToColor


_XFUNCPROTOEND

#endif /*_IswIntrinsic_h*/
/* DON'T ADD STUFF AFTER THIS #endif */
