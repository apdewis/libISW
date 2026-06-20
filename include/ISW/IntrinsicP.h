/***********************************************************

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

#ifndef _IswintrinsicP_h
#define _IswintrinsicP_h

#include <ISW/Intrinsic.h>

/*
 * Field sizes and offsets of IswQResource must match those of IswResource.
 * Type long is used instead of IswQuark here because IswQuark and String
 * are not the same size on all systems.
 */
typedef struct {
    IswIntPtr	xrm_name;	  /* Resource name quark		*/
    IswIntPtr	xrm_class;	  /* Resource class quark		*/
    IswIntPtr	xrm_type;	  /* Resource representation type quark */
    Cardinal	xrm_size;	  /* Size in bytes of representation	*/
    int		xrm_offset;	  /* -offset-1				*/
    IswIntPtr	xrm_default_type; /* Default representation type quark	*/
    IswPointer	xrm_default_addr; /* Default resource address		*/
} IswQResource, *IswQResourceList;
#if __STDC_VERSION__ >= 201112L && !defined(__cplusplus)
_Static_assert(IswOffsetOf(IswQResource, xrm_default_addr) ==
                   IswOffsetOf(IswResource, default_addr),
               "Field offset mismatch");
#endif

typedef unsigned long IswVersionType;

#define ISW_VERSION 11
#ifndef ISW_REVISION
#define ISW_REVISION 6
#endif
#define IswVersion (ISW_VERSION * 1000 + ISW_REVISION)
#define IswVersionDontCheck 0

typedef void (*IswProc)(
    void
);

typedef void (*IswWidgetClassProc)(
    WidgetClass /* class */
);

typedef void (*IswWidgetProc)(
    Widget	/* widget */
);

typedef Boolean (*IswAcceptFocusProc)(
    Widget	/* widget */,
    IswTime*	/* time */
);

typedef void (*IswArgsProc)(
    Widget	/* widget */,
    ArgList	/* args */,
    Cardinal*	/* num_args */
);

typedef void (*IswInitProc)(
    Widget	/* request */,
    Widget	/* new */,
    ArgList	/* args */,
    Cardinal*	/* num_args */
);

typedef Boolean (*IswSetValuesFunc)(
    Widget 	/* old */,
    Widget 	/* request */,
    Widget 	/* new */,
    ArgList 	/* args */,
    Cardinal*	/* num_args */
);

typedef Boolean (*IswArgsFunc)(
    Widget	/* widget */,
    ArgList	/* args */,
    Cardinal*	/* num_args */
);

typedef void (*IswAlmostProc)(
    Widget		/* old */,
    Widget		/* new */,
    IswWidgetGeometry*	/* request */,
    IswWidgetGeometry*	/* reply */
);

typedef void (*IswExposeProc)(
    Widget	/* widget */,
    IswEvent*	/* event */,
    IswRegion	/* region */
);

/* compress_exposure options*/
#define IswExposeNoCompress		((IswEnum)False)
#define IswExposeCompressSeries		((IswEnum)True)
#define IswExposeCompressMultiple	2
#define IswExposeCompressMaximal		3

/* modifiers */
#define IswExposeGraphicsExpose	  	0x10
#define IswExposeGraphicsExposeMerged	0x20
#define IswExposeNoExpose	  	0x40
#define IswExposeNoRegion		0x80

typedef void (*IswRealizeProc)(
    IswDisplay		  /* display */,
    Widget 		  /* widget */,
    IswValueMask* 	  /* mask */,
    uint32_t* /* attributes */
);

typedef IswGeometryResult (*IswGeometryHandler)(
    Widget		/* widget */,
    IswWidgetGeometry*	/* request */,
    IswWidgetGeometry*	/* reply */
);

typedef void (*IswStringProc)(
    Widget	/* widget */,
    String	/* str */
);

typedef struct {
    String	name;	/* resource name */
    String	type;	/* representation type name */
    IswArgVal	value;	/* representation */
    int		size;	/* size of representation */
} IswTypedArg, *IswTypedArgList;

typedef void (*IswAllocateProc)(
    WidgetClass		/* widget_class */,
    Cardinal *		/* constraint_size */,
    Cardinal *		/* more_bytes */,
    ArgList		/* args */,
    Cardinal *		/* num_args */,
    IswTypedArgList	/* typed_args */,
    Cardinal *		/* num_typed_args */,
    Widget *		/* widget_return */,
    IswPointer *		/* more_bytes_return */
);

typedef void (*IswDeallocateProc)(
    Widget		/* widget */,
    IswPointer		/* more_bytes */
);

struct _IswStateRec;	/* Forward declare before use for C++ */

typedef struct _IswTMRec {
    IswTranslations  translations;	/* private to Translation Manager    */
    IswBoundActions  proc_table;		/* procedure bindings for actions    */
    struct _IswStateRec *current_state;  /* Translation Manager state ptr     */
    unsigned long   lastEventTime;
} IswTMRec, *IswTM;

#include <ISW/CoreP.h>
#include <ISW/CompositeP.h>
#include <ISW/ConstrainP.h>
#include <ISW/ObjectP.h>
#include <ISW/RectObjP.h>

#define IswDisplayOf(widget)	((widget)->core.display)
#define IswScreenOf(widget)	((widget)->core.screen)
/* Every widget owns its own render surface — core widgets never reference a
   window.  The single real top-level window is owned by the platform layer
   (the shell's root surface); the platform blits composited surfaces to it.
   Read-only: assign w->core.surface directly to set. */
#define IswSurfaceOf(widget)	((widget)->core.surface)

#define IswClass(widget)		((widget)->core.widget_class)
#define IswSuperclass(widget)	(IswClass(widget)->core_class.superclass)
/* Realization is surface-based, not window-based: a realized widget has been
   created (its surface and geometry are valid) but not necessarily shown. */
#define IswIsRealized(object) \
    (IswIsRectObj(object) ? (object)->core.windowless_realized : False)
#define IswParent(widget)	((widget)->core.parent)

#undef IswIsRectObj
extern Boolean IswIsRectObj(Widget);
#define IswIsRectObj(obj) \
    (((Object)(obj))->object.widget_class->core_class.class_inited & 0x02)

#undef IswIsWidget
extern Boolean IswIsWidget(Widget);
#define IswIsWidget(obj) \
    (((Object)(obj))->object.widget_class->core_class.class_inited & 0x04)

#undef IswIsComposite
extern Boolean IswIsComposite(Widget);
#define IswIsComposite(obj) \
    (((Object)(obj))->object.widget_class->core_class.class_inited & 0x08)

#undef IswIsConstraint
extern Boolean IswIsConstraint(Widget);
#define IswIsConstraint(obj) \
    (((Object)(obj))->object.widget_class->core_class.class_inited & 0x10)

#undef IswIsShell
extern Boolean IswIsShell(Widget);
#define IswIsShell(obj) \
    (((Object)(obj))->object.widget_class->core_class.class_inited & 0x20)

#undef IswIsWMShell
extern Boolean IswIsWMShell(Widget);
#define IswIsWMShell(obj) \
    (((Object)(obj))->object.widget_class->core_class.class_inited & 0x40)

#undef IswIsTopLevelShell
extern Boolean IswIsTopLevelShell(Widget);
#define IswIsTopLevelShell(obj) \
    (((Object)(obj))->object.widget_class->core_class.class_inited & 0x80)

#ifdef DEBUG
#define IswCheckSubclass(w, widget_class_ptr, message)	\
	if (!IswIsSubclass(((Widget)(w)), (widget_class_ptr))) {	\
	    String dbgArgV[3];				\
	    Cardinal dbgArgC = 3;			\
	    dbgArgV[0] = ((Widget)(w))->core.widget_class->core_class.class_name;\
	    dbgArgV[1] = (widget_class_ptr)->core_class.class_name;	     \
	    dbgArgV[2] = (message);					     \
	    IswAppErrorMsg(IswWidgetToApplicationContext((Widget)(w)),	     \
		    "subclassMismatch", "xtCheckSubclass", "IswToolkitError", \
		    "Widget class %s found when subclass of %s expected: %s",\
		    dbgArgV, &dbgArgC);			\
	}
#else
#define IswCheckSubclass(w, widget_class, message)	/* nothing */
#endif

_XFUNCPROTOBEGIN

/* Nearest widget ancestor of a non-widget object (used to read display/screen).
   No window semantics — core widgets are surface-based. */
extern Widget _IswWidgetAncestor(
    Widget 		/* object */
);

/* Aggregate event mask to select on a windowed widget's window: its own
   mask plus the masks of windowless descendants sharing the window. */
extern EventMask _IswWindowSelectMask(
    Widget 		/* windowed widget */
);

#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(LIBXT_COMPILATION)
__declspec(dllimport)
#else
extern
#endif
void _IswInherit(
    void
);

extern void _IswHandleFocus(
    Widget		/* widget */,
    IswPointer		/* client_data */,
   IswEvent *		/* event */,
    Boolean *		/* cont */);

extern void IswResizeWidget(
    Widget 		/* widget */,
    _IswDimension	/* width */,
    _IswDimension	/* height */,
    _IswDimension	/* border_width */
);

extern void IswMoveWidget(
    Widget 		/* widget */,
    _IswPosition		/* x */,
    _IswPosition		/* y */
);

extern void IswConfigureWidget(
    Widget 		/* widget */,
    _IswPosition		/* x */,
    _IswPosition		/* y */,
    _IswDimension	/* width */,
    _IswDimension	/* height */,
    _IswDimension	/* border_width */
);

extern void IswResizeWindow(
    Widget 		/* widget */
);

extern void IswProcessLock(
    void
);

extern void IswProcessUnlock(
    void
);

_XFUNCPROTOEND

#endif /* _IswIntrinsicP_h */
/* DON'T ADD STUFF AFTER THIS #endif */
