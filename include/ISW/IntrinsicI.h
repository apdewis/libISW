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

#ifndef _IswintrinsicI_h
#define _IswintrinsicI_h

#include "Iswos.h"
#include "IntrinsicP.h"
#ifdef WIN32
#define _WILLWINSOCK_
#endif
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "Object.h"
#include "RectObj.h"
#include "ObjectP.h"
#include "RectObjP.h"

#include "ConvertI.h"
#include "TranslateI.h"

#define RectObjClassFlag	0x02
#define WidgetClassFlag		0x04
#define CompositeClassFlag	0x08
#define ConstraintClassFlag	0x10
#define ShellClassFlag		0x20
#define WMShellClassFlag	0x40
#define TopLevelClassFlag	0x80

/*
 * The following macros, though very handy, are not suitable for
 * IntrinsicP.h as they violate the rule that arguments are to
 * be evaluated exactly once.
 */

/* For a non-widget object (e.g. a shell extension), resolve display/screen
   through its nearest widget ancestor.  Core never resolves a *window* this
   way — widgets render to surfaces, not windows. */
#define IswDisplayOfObject(object) \
    (IswIsWidget(object) ? (object)->core.display : \
    _IswIsHookObject(object) ? ((HookObject)(object))->hooks.display : \
    _IswWidgetAncestor(object)->core.display)

#define IswScreenOfObject(object) \
    (IswIsWidget(object) ? (object)->core.screen : \
    _IswIsHookObject(object) ? ((HookObject)(object))->hooks.screen : \
    _IswWidgetAncestor(object)->core.screen)

#define IswIsManaged(object) \
    (IswIsRectObj(object) ? (object)->core.managed : False)

#define IswIsSensitive(object) \
    (IswIsRectObj(object) ? ((object)->core.sensitive && \
			    (object)->core.ancestor_sensitive) : False)

/****************************************************************
 *
 * Bit utilities
 *
 ****************************************************************/
#define IswSetBits(dst,src,len)  dst = (((1U << (len)) - 1) & (unsigned)(src))
#define IswSetBit(dst,src)  IswSetBits(dst,src,1)

/****************************************************************
 *
 * Byte utilities
 *
 ****************************************************************/

/* Use standard C library functions instead of X11/Xfuncs.h */
#define bcopy(src, dst, size) memmove((dst), (src), (size))
#define bzero(dst, size) memset((dst), 0, (size))
#define _IswBcopy(src, dst, size) memmove((dst), (src), (size))

#define IswMemmove(dst, src, size)	\
    if ((const void *)(dst) != (const void *)(src)) {		    \
	(void) memcpy((void *) (dst), (const void *) (src), (size_t) (size)); \
    }

#define IswBZero(dst, size) 	\
	memset((void *) (dst), 0, (size_t) (size))

#define IswMemcmp(b1, b2, size) 		\
	memcmp((const void *) (b1), (const void *) (b2), (size_t) (size))

/* gettimeofday wrapper - was in X11/Xos.h */
#include <sys/time.h>
#define X_GETTIMEOFDAY(t) gettimeofday(t, NULL)


/****************************************************************
 *
 * Stack cache allocation/free
 *
 ****************************************************************/

#define IswStackAlloc(size, stack_cache_array)     \
    ((size) <= sizeof(stack_cache_array)	  \
    ?  (IswPointer)(stack_cache_array)		  \
    :  IswMalloc((Cardinal)(size)))

#define IswStackFree(pointer, stack_cache_array) \
    { if ((pointer) != ((IswPointer)(stack_cache_array))) IswFree(pointer); }

/***************************************************************
 *
 * Filename defines
 *
 **************************************************************/

/* used by IswResolvePathname */
#ifndef XFILESEARCHPATHDEFAULT
#define XFILESEARCHPATHDEFAULT "/usr/lib/X11/%L/%T/%N%S:/usr/lib/X11/%l/%T/%N%S:/usr/lib/X11/%T/%N%S"
#endif

/* the following two were both "X Toolkit " prior to R4 */
#ifndef XTERROR_PREFIX
#define XTERROR_PREFIX ""
#endif

#ifndef XTWARNING_PREFIX
#define XTWARNING_PREFIX ""
#endif

#ifndef ERRORDB
#define ERRORDB "/usr/lib/X11/XtErrorDB"
#endif

_XFUNCPROTOBEGIN

extern String IswCIswToolkitError;

extern void _IswAllocError(
    String	/* alloc_type */
) _X_NORETURN;

extern void _IswCompileResourceList(
    IswResourceList 	/* resources */,
    Cardinal 		/* num_resources */
);

extern IswGeometryResult _IswMakeGeometryRequest(
    Widget 		/* widget */,
    IswWidgetGeometry*	/* request */,
    IswWidgetGeometry*	/* reply_return */,
    Boolean*		/* clear_rect_obj */
);

extern Boolean _IswIsHookObject(
    Widget      /* widget */
);

extern IswCursor _IswLoadThemedCursor(
    IswDisplay		/* dpy */,
    IswScreen		/* screen */,
    const char *	/* name */,
    unsigned int	/* shape (fallback glyph) */
);

extern void _IswAddShellToHookObj(
    Widget      /* widget */
);

/** GeoTattler stuff */

#ifdef ISW_GEO_TATTLER

extern void _IswGeoTab (int);
extern void _IswGeoTrace (
			    Widget widget,
			    const char *,
			    ...
) _X_ATTRIBUTE_PRINTF(2,3);

#define CALLGEOTAT(f) f

#else /* ISW_GEO_TATTLER */

#define CALLGEOTAT(f)

#endif /* ISW_GEO_TATTLER */

#ifndef XTTRACEMEMORY

extern char* __XtMalloc (
    unsigned	/* size */
);
extern char* __XtCalloc (
    unsigned	/* num */,
    unsigned	/* size */
);

#else

#define __XtMalloc IswMalloc
#define __XtCalloc IswCalloc
#endif

_XFUNCPROTOEND

#endif /* _IswintrinsicI_h */
/* DON'T ADD STUFF AFTER THIS #endif */
