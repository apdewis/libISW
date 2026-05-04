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

/*
 * ShellP.h - Private definitions for Shell widget
 *
 * Author:	Paul Asente
 * 		Digital Equipment Corporation
 * 		Western Software Laboratory
 * Date:	Thu Dec 3, 1987
 */

#ifndef _IswShellPrivate_h
#define _IswShellPrivate_h

#include <xcb/xcb_icccm.h>
#include <ISW/Shell.h>

/* *****
 * ***** VendorP.h is included later on; it needs fields defined in the first
 * ***** part of this header file
 * *****
 */

_XFUNCPROTOBEGIN

/***********************************************************************
 *
 * Shell Widget Private Data
 *
 ***********************************************************************/

/* New fields for the Shell widget class record */

typedef struct {
    IswPointer       extension;          /* pointer to extension record      */
} ShellClassPart;

typedef struct {
    IswPointer next_extension;	/* 1st 4 mandated for all extension records */
    XrmQuark record_type;	/* NULLQUARK; on ShellClassPart */
    long version;		/* must be IswShellExtensionVersion */
    Cardinal record_size;	/* sizeof(ShellClassExtensionRec) */
    IswGeometryHandler root_geometry_manager;
} ShellClassExtensionRec, *ShellClassExtension;

#define IswShellExtensionVersion 1L
#define IswInheritRootGeometryManager ((IswGeometryHandler)_IswInherit)

typedef struct _ShellClassRec {
  	CoreClassPart      core_class;
	CompositeClassPart composite_class;
	ShellClassPart  shell_class;
} ShellClassRec;

externalref ShellClassRec shellClassRec;

/* New fields for the shell widget */

typedef struct {
	char       *geometry;
	IswCreatePopupChildProc	create_popup_child_proc;
	IswGrabKind	grab_kind;
	Boolean	    popped_up;
	Boolean	    allow_shell_resize;
	Boolean     client_specified; /* re-using old name */
#define _IswShellPositionValid	((Boolean)(1<<0))
#define _IswShellNotReparented	((Boolean)(1<<1))
#define _IswShellPPositionOK	((Boolean)(1<<2))
#define _IswShellGeometryParsed	((Boolean)(1<<3))
	Boolean	    save_under;
	Boolean	    override_redirect;

	IswCallbackList popup_callback;
	IswCallbackList popdown_callback;
	xcb_visualid_t visual;
} ShellPart;

typedef  struct {
	CorePart 	core;
	CompositePart 	composite;
	ShellPart 	shell;
} ShellRec, *ShellWidget;

/***********************************************************************
 *
 * OverrideShell Widget Private Data
 *
 ***********************************************************************/

/* New fields for the OverrideShell widget class record */

typedef struct {
    IswPointer       extension;          /* pointer to extension record      */
} OverrideShellClassPart;

typedef struct _OverrideShellClassRec {
  	CoreClassPart      core_class;
	CompositeClassPart composite_class;
	ShellClassPart  shell_class;
	OverrideShellClassPart  override_shell_class;
} OverrideShellClassRec;

externalref OverrideShellClassRec overrideShellClassRec;

/* No new fields for the override shell widget */

typedef struct {int frabjous;} OverrideShellPart;

typedef  struct {
	CorePart 	core;
	CompositePart 	composite;
	ShellPart 	shell;
	OverrideShellPart override;
} OverrideShellRec, *OverrideShellWidget;

/***********************************************************************
 *
 * WMShell Widget Private Data
 *
 ***********************************************************************/

/* New fields for the WMShell widget class record */

typedef struct {
    IswPointer       extension;          /* pointer to extension record      */
} WMShellClassPart;

typedef struct _WMShellClassRec {
  	CoreClassPart      core_class;
	CompositeClassPart composite_class;
	ShellClassPart  shell_class;
	WMShellClassPart wm_shell_class;
} WMShellClassRec;

externalref WMShellClassRec wmShellClassRec;

/* New fields for the WM shell widget */

/* Local replacement for Xlib's XClassHint */
typedef struct {
    const char *res_name;
    const char *res_class;
} XClassHint;

typedef struct {
	char	   *title;
	int 	    wm_timeout;
	Boolean	    wait_for_wm;
	Boolean	    transient;
	Boolean     urgency;
	Widget      client_leader;
	String      window_role;
	struct _OldXSizeHints {	/* pre-R4 Xlib structure */
	    long flags;
	    int x, y;
	    int width, height;
	    int min_width, min_height;
	    int max_width, max_height;
	    int width_inc, height_inc;
	    struct {
		    int x;
		    int y;
	    } min_aspect, max_aspect;
	} size_hints;
	xcb_icccm_wm_hints_t wm_hints;
	int base_width, base_height;
	int win_gravity;
	xcb_atom_t title_encoding;
} WMShellPart;

typedef  struct {
	CorePart 	core;
	CompositePart 	composite;
	ShellPart 	shell;
	WMShellPart	wm;
} WMShellRec, *WMShellWidget;

_XFUNCPROTOEND

#include <ISW/VendorP.h>

_XFUNCPROTOBEGIN

/***********************************************************************
 *
 * TransientShell Widget Private Data
 *
 ***********************************************************************/

/* New fields for the TransientShell widget class record */

typedef struct {
    IswPointer       extension;          /* pointer to extension record      */
} TransientShellClassPart;

typedef struct _TransientShellClassRec {
  	CoreClassPart      core_class;
	CompositeClassPart composite_class;
	ShellClassPart  shell_class;
	WMShellClassPart   wm_shell_class;
	VendorShellClassPart vendor_shell_class;
	TransientShellClassPart transient_shell_class;
} TransientShellClassRec;

externalref TransientShellClassRec transientShellClassRec;

/* New fields for the transient shell widget */

typedef struct {
	Widget transient_for;
} TransientShellPart;

typedef  struct {
	CorePart 	core;
	CompositePart 	composite;
	ShellPart 	shell;
	WMShellPart	wm;
	VendorShellPart	vendor;
	TransientShellPart transient;
} TransientShellRec, *TransientShellWidget;

/***********************************************************************
 *
 * TopLevelShell Widget Private Data
 *
 ***********************************************************************/

/* New fields for the TopLevelShell widget class record */

typedef struct {
    IswPointer       extension;          /* pointer to extension record      */
} TopLevelShellClassPart;

typedef struct _TopLevelShellClassRec {
  	CoreClassPart      core_class;
	CompositeClassPart composite_class;
	ShellClassPart  shell_class;
	WMShellClassPart   wm_shell_class;
	VendorShellClassPart vendor_shell_class;
	TopLevelShellClassPart top_level_shell_class;
} TopLevelShellClassRec;

externalref TopLevelShellClassRec topLevelShellClassRec;

/* New fields for the top level shell widget */

typedef struct {
	char	   *icon_name;
	Boolean	    iconic;
	xcb_atom_t	    icon_name_encoding;
} TopLevelShellPart;

typedef  struct {
	CorePart 	core;
	CompositePart 	composite;
	ShellPart 	shell;
	WMShellPart	wm;
	VendorShellPart	vendor;
	TopLevelShellPart topLevel;
} TopLevelShellRec, *TopLevelShellWidget;

/***********************************************************************
 *
 * ApplicationShell Widget Private Data
 *
 ***********************************************************************/

/* New fields for the ApplicationShell widget class record */

typedef struct {
    IswPointer       extension;          /* pointer to extension record      */
} ApplicationShellClassPart;

typedef struct _ApplicationShellClassRec {
  	CoreClassPart      core_class;
	CompositeClassPart composite_class;
	ShellClassPart  shell_class;
	WMShellClassPart   wm_shell_class;
	VendorShellClassPart vendor_shell_class;
	TopLevelShellClassPart top_level_shell_class;
	ApplicationShellClassPart application_shell_class;
} ApplicationShellClassRec;

externalref ApplicationShellClassRec applicationShellClassRec;

/* New fields for the application shell widget */

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    const char *c_class;
#else
    const char *class;
#endif
    //XrmClass xrm_class;
    int argc;
    _IswString *argv;
} ApplicationShellPart;

typedef  struct {
	CorePart 	core;
	CompositePart 	composite;
	ShellPart 	shell;
	WMShellPart	wm;
	VendorShellPart	vendor;
	TopLevelShellPart topLevel;
	ApplicationShellPart application;
} ApplicationShellRec, *ApplicationShellWidget;

_XFUNCPROTOEND

#endif /* _IswShellPrivate_h */
