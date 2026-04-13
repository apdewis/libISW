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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "StringDefs.h"
#include "CreateI.h"

/******************************************************************
 *
 * Rectangle Object Resources
 *
 ******************************************************************/

static void IswCopyAncestorSensitive(Widget, int, XrmValue *);

/* *INDENT-OFF* */
static IswResource resources[] = {

    {IswNancestorSensitive, IswCSensitive, IswRBoolean, sizeof(Boolean),
      IswOffsetOf(RectObjRec,rectangle.ancestor_sensitive),IswRCallProc,
      (IswPointer)IswCopyAncestorSensitive},
    {IswNx, IswCPosition, IswRPosition, sizeof(Position),
         IswOffsetOf(RectObjRec,rectangle.x), IswRImmediate, (IswPointer)0},
    {IswNy, IswCPosition, IswRPosition, sizeof(Position),
         IswOffsetOf(RectObjRec,rectangle.y), IswRImmediate, (IswPointer)0},
    {IswNwidth, IswCWidth, IswRDimension, sizeof(Dimension),
         IswOffsetOf(RectObjRec,rectangle.width), IswRImmediate, (IswPointer)0},
    {IswNheight, IswCHeight, IswRDimension, sizeof(Dimension),
         IswOffsetOf(RectObjRec,rectangle.height), IswRImmediate, (IswPointer)0},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
         IswOffsetOf(RectObjRec,rectangle.border_width), IswRImmediate,
         (IswPointer)1},
    {IswNsensitive, IswCSensitive, IswRBoolean, sizeof(Boolean),
         IswOffsetOf(RectObjRec,rectangle.sensitive), IswRImmediate,
         (IswPointer)True}
};
/* *INDENT-ON* */

static void RectClassPartInitialize(WidgetClass);
static void RectSetValuesAlmost(Widget, Widget, IswWidgetGeometry *,
                                IswWidgetGeometry *);

/* *INDENT-OFF* */
externaldef(rectobjclassrec) RectObjClassRec rectObjClassRec = {
  {
    /* superclass            */ (WidgetClass)&objectClassRec,
    /* class_name            */ "Rect",
    /* widget_size           */ sizeof(RectObjRec),
    /* class_initialize      */ NULL,
    /* class_part_initialize */ RectClassPartInitialize,
    /* class_inited          */ FALSE,
    /* initialize            */ NULL,
    /* initialize_hook       */ NULL,
    /* realize               */ NULL,
    /* actions               */ NULL,
    /* num_actions           */ 0,
    /* resources             */ resources,
    /* num_resources         */ IswNumber(resources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ FALSE,
    /* compress_exposure     */ TRUE,
    /* compress_enterleave   */ FALSE,
    /* visible_interest      */ FALSE,
    /* destroy               */ NULL,
    /* resize                */ NULL,
    /* expose                */ NULL,
    /* set_values            */ NULL,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ RectSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* version               */ IswVersion,
    /* callback_offsets      */ NULL,
    /* tm_table              */ NULL,
    /* query_geometry        */ NULL,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL
  }
};
/* *INDENT-ON* */

externaldef(rectObjClass)
WidgetClass rectObjClass = (WidgetClass) &rectObjClassRec;

static void
IswCopyAncestorSensitive(Widget widget, int offset _X_UNUSED, XrmValue *value)
{
    static Boolean sensitive;
    Widget parent = widget->core.parent;

    if (parent == NULL) {
        /* Top-level widget with no parent - default to TRUE */
        sensitive = TRUE;
    } else {
        sensitive = (parent->core.ancestor_sensitive & parent->core.sensitive);
    }
    value->addr = (IswPointer) (&sensitive);
}

/*
 * Start of rectangle object methods
 */

static void
RectClassPartInitialize(register WidgetClass wc)
{
    register RectObjClass roc = (RectObjClass) wc;
    register RectObjClass super = ((RectObjClass) roc->rect_class.superclass);

    /* We don't need to check for null super since we'll get to object
       eventually, and it had better define them!  */

    if (roc->rect_class.resize == IswInheritResize) {
        roc->rect_class.resize = super->rect_class.resize;
    }

    if (roc->rect_class.expose == IswInheritExpose) {
        roc->rect_class.expose = super->rect_class.expose;
    }

    if (roc->rect_class.set_values_almost == IswInheritSetValuesAlmost) {
        roc->rect_class.set_values_almost = super->rect_class.set_values_almost;
    }

    if (roc->rect_class.query_geometry == IswInheritQueryGeometry) {
        roc->rect_class.query_geometry = super->rect_class.query_geometry;
    }
}

/*
 * Why there isn't an Initialize Method:
 *
 * Initialization of the RectObj non-Resource field is done by the
 * intrinsics in _IswCreateWidget in order that the field is initialized
 * for use by converters during instance resource resolution.
 */

static void
RectSetValuesAlmost(Widget old _X_UNUSED,
                    Widget new _X_UNUSED,
                    IswWidgetGeometry *request,
                    IswWidgetGeometry *reply)
{
    *request = *reply;
}
