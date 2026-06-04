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

#ifndef IswCoreP_h
#define IswCoreP_h

#include <ISW/Core.h>

_XFUNCPROTOBEGIN

externalref int _IswInheritTranslations;

#define IswInheritTranslations  ((String) &_IswInheritTranslations)
#define IswInheritRealize ((IswRealizeProc) _IswInherit)
#define IswInheritResize ((IswWidgetProc) _IswInherit)
#define IswInheritExpose ((IswExposeProc) _IswInherit)
#define IswInheritSetValuesAlmost ((IswAlmostProc) _IswInherit)
#define IswInheritAcceptFocus ((IswAcceptFocusProc) _IswInherit)
#define IswInheritQueryGeometry ((IswGeometryHandler) _IswInherit)
#define IswInheritDisplayAccelerator ((IswStringProc) _IswInherit)

/***************************************************************
 * Widget Core Data Structures
 *
 *
 **************************************************************/

typedef struct _CorePart {
    Widget	    self;		/* pointer to widget itself	     */
    WidgetClass	    widget_class;	/* pointer to Widget's ClassRec	     */
    Widget	    parent;		/* parent widget	  	     */
    XrmName         xrm_name;		/* widget resource name quarkified   */
    Boolean         being_destroyed;	/* marked for destroy		     */
    IswCallbackList  destroy_callbacks;	/* who to call when widget destroyed */
    IswPointer       constraints;        /* constraint record                 */
    Position        x, y;		/* window position		     */
    Dimension       width, height;	/* window dimensions		     */
    Dimension       border_width;	/* window border width		     */
    Boolean         managed;            /* is widget geometry managed?       */
    Boolean	    sensitive;		/* is widget sensitive to user events*/
    Boolean         ancestor_sensitive;	/* are all ancestors sensitive?      */
    IswEventTable    event_table;	/* private to event dispatcher       */
    IswTMRec	    tm;                 /* translation management            */
    IswTranslations  accelerators;       /* accelerator translations          */
    Pixel	    border_pixel;	/* window border pixel		     */
    xcb_pixmap_t    border_pixmap;	/* window border pixmap or NULL      */
    WidgetList      popup_list;         /* list of popups                    */
    Cardinal        num_popups;         /* how many popups                   */
    String          name;		/* widget resource name		     */
    xcb_screen_t	*screen;		/* window's screen		     */
    xcb_connection_t *display;      /* window's display (XCB doesnt store a pointer in screen type like xlib does)*/
    xcb_colormap_t  colormap;           /* colormap                          */
    xcb_window_t	    window;		/* window ID			     */
    Cardinal        depth;		/* number of planes in window        */
    Pixel	    background_pixel;	/* window background pixel	     */
    xcb_pixmap_t    background_pixmap;	/* window background pixmap or NULL  */
    Boolean         visible;		/* is window mapped and not occluded?*/
    Boolean	    mapped_when_managed;/* map window if it's managed?       */
    Boolean         windowless;         /* no own X window; draws into
                                           nearest windowed ancestor         */
    Boolean         windowless_realized;/* windowless widget has been
                                           realized (no window to test)      */
    Boolean         windowless_mapped;  /* windowless equivalent of "the X
                                           window is mapped": the live shown
                                           state, driven by Isw{Map,Unmap}Widget
                                           exactly as map/unmap drive a real
                                           window.  The composite/paint/hit-test
                                           walks gate on this.                */
    Boolean         windowless_unmapped_explicit;
                                        /* app called IswUnmapWidget on this
                                           windowless widget while it was
                                           unrealized.  The realize-time map
                                           pass must honour it and leave the
                                           widget hidden, exactly as a windowed
                                           widget the app keeps unmapped stays
                                           off-screen.  Cleared by IswMapWidget. */
} CorePart;

typedef struct _WidgetRec {
    CorePart    core;
 } WidgetRec, CoreRec;



/******************************************************************
 *
 * Core Class Structure. Widgets, regardless of their class, will have
 * these fields.  All widgets of a given class will have the same values
 * for these fields.  Widgets of a given class may also have additional
 * common fields.  These additional fields are included in incremental
 * class structures, such as CommandClass.
 *
 * The fields that are specific to this subclass, as opposed to fields that
 * are part of the superclass, are called "subclass fields" below.  Many
 * procedures are responsible only for the subclass fields, and not for
 * any superclass fields.
 *
 ********************************************************************/

typedef struct _CoreClassPart {
    WidgetClass     superclass;		/* pointer to superclass ClassRec   */
    String          class_name;		/* widget resource class name       */
    Cardinal        widget_size;	/* size in bytes of widget record   */
    IswProc	    class_initialize;   /* class initialization proc	    */
    IswWidgetClassProc class_part_initialize; /* dynamic initialization	    */
    IswEnum          class_inited;       /* has class been initialized?      */
    IswInitProc      initialize;		/* initialize subclass fields       */
    IswArgsProc      initialize_hook;    /* notify that initialize called    */
    IswRealizeProc   realize;		/* XCreateWindow for widget	    */
    IswActionList    actions;		/* widget semantics name to proc map */
    Cardinal	    num_actions;	/* number of entries in actions     */
    IswResourceList  resources;		/* resources for subclass fields    */
    Cardinal        num_resources;      /* number of entries in resources   */
    XrmClass        xrm_class;		/* resource class quarkified	    */
    Boolean         compress_motion;    /* compress MotionNotify for widget */
    IswEnum          compress_exposure;  /* compress Expose events for widget*/
    Boolean         compress_enterleave;/* compress enter and leave events  */
    Boolean         visible_interest;   /* select for VisibilityNotify      */
    IswWidgetProc    destroy;		/* free data for subclass pointers  */
    IswWidgetProc    resize;		/* geom manager changed widget size */
    IswExposeProc    expose;		/* rediplay window		    */
    IswSetValuesFunc set_values;		/* set subclass resource values     */
    IswArgsFunc      set_values_hook;    /* notify that set_values called    */
    IswAlmostProc    set_values_almost;  /* set_values got "Almost" geo reply */
    IswArgsProc      get_values_hook;    /* notify that get_values called    */
    IswAcceptFocusProc accept_focus;     /* assign input focus to widget     */
    IswVersionType   version;	        /* version of intrinsics used	    */
    IswPointer       callback_private;   /* list of callback offsets       */
    String          tm_table;           /* state machine                    */
    IswGeometryHandler query_geometry;	/* return preferred geometry        */
    IswStringProc    display_accelerator;/* display your accelerator	    */
    IswPointer	    extension;		/* pointer to extension record      */
 } CoreClassPart;

typedef struct _WidgetClassRec {
    CoreClassPart core_class;
} WidgetClassRec, CoreClassRec;

externalref WidgetClassRec widgetClassRec;
#define coreClassRec widgetClassRec

_XFUNCPROTOEND

#endif /* _IswCoreP_h */
/* DON'T ADD STUFF AFTER THIS #endif */
