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
    IswQuarkName         xrm_name;		/* widget resource name quarkified   */
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
    WidgetList      popup_list;         /* list of popups                    */
    Cardinal        num_popups;         /* how many popups                   */
    String          name;		/* widget resource name		     */
    IswScreen	    screen;		/* window's screen (opaque handle)   */
    IswDisplay      display;        /* window's display (opaque handle)  */
    IswColormap     colormap;           /* colormap (opaque handle)          */
    IswSurface	    surface;		/* per-widget render surface (opaque
					   handle).  The core has no window —
					   widgets render to this surface and the
					   platform layer blits the composited
					   result to the real top-level window,
					   which the platform alone owns.
					   Created at realize, destroyed with the
					   widget.                             */
    Cardinal        depth;		/* number of planes in window        */
    Pixel	    background_pixel;	/* window background pixel	     */
    String          background_image;   /* image file path (resource)        */
    struct _ISWImage *background_image_handle; /* private: loaded image      */
    Boolean         visible;		/* is window mapped and not occluded?*/
    Boolean	    mapped_when_managed;/* map window if it's managed?       */
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
    Boolean         composite_clip;     /* a composite clip is set (below)    */
    int             composite_clip_x, composite_clip_y,
                    composite_clip_w, composite_clip_h;
                                        /* when composite_clip: confine this
                                           widget to this rect (PARENT content
                                           coords) as it folds into its parent.
                                           Used by scrolling containers
                                           (Viewport).  Persists across the
                                           surface's create/destroy, so it lives
                                           on the widget, not the surface.       */
    Boolean         virtual_origin;     /* back surface covers a sub-region   */
    int             virtual_origin_x, virtual_origin_y,
                    virtual_origin_w, virtual_origin_h;
                                        /* when virtual_origin: the back surface
                                           is sized to (w x h) and maps to
                                           widget-local rect [x..x+w, y..y+h].
                                           surface_begin translates so the child
                                           draws at local coords while only this
                                           tile is rasterised.  Set by Viewport
                                           for oversized scrolled children.      */
    Boolean         composite_dirty;    /* surface changed since last fold:
                                           re-run this container's expose proc
                                           on the next composite pass.  Starts
                                           True so the first pass paints.        */
    Boolean         composite_lazy_root;/* windowed root the composite pass
                                           created a surface for itself (bare
                                           Box/Form/Shell with no own-content
                                           expose): background-fill it each pass. */
    Boolean         composite_presented;/* this root has presented a pass that
                                           folded real child content (gates the
                                           startup no-op-pass skip).             */
    Boolean         windowless_overlay; /* windowless widget that must composite
                                           ABOVE all normal content in its
                                           windowed root (a popup menu shown
                                           in-window).  The composite pass folds
                                           overlays in a final top pass, after the
                                           regular subtree, so they are never
                                           painted over by later-in-tree siblings.*/
    Dimension       border_top;         /* per-side border widths (logical px).
                                           When any is non-zero the backend draws
                                           four independent edges instead of a
                                           uniform ring from border_width.       */
    Dimension       border_right;
    Dimension       border_bottom;
    Dimension       border_left;
} CorePart;

typedef struct _WidgetRec {
    CorePart    core;
 } WidgetRec, CoreRec;

struct _ISWRenderContext;
extern void _IswCoreDrawBackground(Widget w, struct _ISWRenderContext *ctx);

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
    IswQuarkClass        xrm_class;		/* resource class quarkified	    */
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

typedef struct {
    int top, right, bottom, left;
} IswBorderSides;

static inline IswBorderSides
_IswGetBorderSides(Widget w)
{
    IswBorderSides s;
    int bw = (int) w->core.border_width;

    if (w->core.border_top  || w->core.border_right ||
        w->core.border_bottom || w->core.border_left) {
        s.top    = (int) w->core.border_top;
        s.right  = (int) w->core.border_right;
        s.bottom = (int) w->core.border_bottom;
        s.left   = (int) w->core.border_left;
        return s;
    }
    s.top = s.right = s.bottom = s.left = bw;
    return s;
}

#define _IswBorderHoriz(s) ((s).left + (s).right)
#define _IswBorderVert(s)  ((s).top + (s).bottom)
#define _IswBorderIsUniform(s) \
    ((s).top == (s).right && (s).right == (s).bottom && (s).bottom == (s).left)

_XFUNCPROTOEND

#endif /* _IswCoreP_h */
/* DON'T ADD STUFF AFTER THIS #endif */
