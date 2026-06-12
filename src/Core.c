/***********************************************************
Copyright (c) 1993, Oracle and/or its affiliates.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

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

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>

#include "IntrinsicP.h"
#include "EventI.h"
#include "ConvertI.h"
#include "TranslateI.h"
#include "ResourceI.h"
#include "RectObj.h"
#include "RectObjP.h"
#include "ThreadsI.h"
#include "StringDefs.h"

/******************************************************************
 *
 * CoreWidget Resources
 *
 ******************************************************************/

externaldef(xtinherittranslations)
int _IswInheritTranslations = 0;
extern String IswCIswToolkitError;        /* from IntrinsicI.h */
static void
IswCopyScreen(Widget, int, XrmValue *);

static IswResource resources[] = {
    {IswNscreen, IswCScreen, IswRScreen, sizeof(IswScreen),
     IswOffsetOf(CoreRec, core.screen), IswRCallProc, (IswPointer) IswCopyScreen},
/*_IswCopyFromParent does not work for screen because the Display
parameter is not passed through to the IswRCallProc routines */
    {IswNdepth, IswCDepth, IswRInt, sizeof(int),
     IswOffsetOf(CoreRec, core.depth),
     IswRCallProc, (IswPointer) _IswCopyFromParent},
    {IswNcolormap, IswCColormap, IswRColormap, sizeof(IswColormap),
     IswOffsetOf(CoreRec, core.colormap),
     IswRCallProc, (IswPointer) _IswCopyFromParent},
    {IswNbackground, IswCBackground, IswRPixel, sizeof(Pixel),
     IswOffsetOf(CoreRec, core.background_pixel),
     IswRString, (IswPointer) "IswDefaultBackground"},
    {IswNborderColor, IswCBorderColor, IswRPixel, sizeof(Pixel),
     IswOffsetOf(CoreRec, core.border_pixel),
     IswRString, (IswPointer) "IswDefaultForeground"},
    {IswNmappedWhenManaged, IswCMappedWhenManaged, IswRBoolean, sizeof(Boolean),
     IswOffsetOf(CoreRec, core.mapped_when_managed),
     IswRImmediate, (IswPointer) True},
    {IswNtranslations, IswCTranslations, IswRTranslationTable,
     sizeof(IswTranslations), IswOffsetOf(CoreRec, core.tm.translations),
     IswRTranslationTable, (IswPointer) NULL},
    {IswNaccelerators, IswCAccelerators, IswRAcceleratorTable,
     sizeof(IswTranslations), IswOffsetOf(CoreRec, core.accelerators),
     IswRTranslationTable, (IswPointer) NULL}
};

static void CoreInitialize(Widget, Widget, ArgList, Cardinal *);
static void CoreClassPartInitialize(WidgetClass);
static void CoreDestroy(Widget);
static void CoreRealize(IswDisplay, Widget, IswValueMask *, uint32_t *);
static Boolean CoreSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void CoreSetValuesAlmost(Widget, Widget, IswWidgetGeometry *,
                                IswWidgetGeometry *);

static RectObjClassRec unNamedObjClassRec = {
    {
     /* superclass         */ (WidgetClass) &rectObjClassRec,
     /* class_name         */ "UnNamedObj",
     /* widget_size        */ 0,
     /* class_initialize   */ NULL,
     /* class_part_initialize */ NULL,
     /* class_inited       */ FALSE,
     /* initialize         */ NULL,
     /* initialize_hook    */ NULL,
     /* realize            */ (IswProc) IswInheritRealize,
     /* actions            */ NULL,
     /* num_actions        */ 0,
     /* resources          */ NULL,
     /* num_resources      */ 0,
     /* xrm_class          */ NULLQUARK,
     /* compress_motion    */ FALSE,
     /* compress_exposure  */ FALSE,
     /* compress_enterleave */ FALSE,
     /* visible_interest   */ FALSE,
     /* destroy            */ NULL,
     /* resize             */ NULL,
     /* expose             */ NULL,
     /* set_values         */ NULL,
     /* set_values_hook    */ NULL,
     /* set_values_almost  */ IswInheritSetValuesAlmost,
     /* get_values_hook    */ NULL,
     /* accept_focus       */ NULL,
     /* version            */ IswVersion,
     /* callback_offsets   */ NULL,
     /* tm_table           */ NULL,
     /* query_geometry       */ NULL,
     /* display_accelerator  */ NULL,
     /* extension            */ NULL
     }
};

externaldef(widgetclassrec)
WidgetClassRec widgetClassRec = {
    {
     /* superclass         */ (WidgetClass) &unNamedObjClassRec,
     /* class_name         */ "Core",
     /* widget_size        */ sizeof(WidgetRec),
     /* class_initialize   */ NULL,
     /* class_part_initialize */ CoreClassPartInitialize,
     /* class_inited       */ FALSE,
     /* initialize         */ CoreInitialize,
     /* initialize_hook    */ NULL,
     /* realize            */ CoreRealize,
     /* actions            */ NULL,
     /* num_actions        */ 0,
     /* resources          */ resources,
     /* num_resources      */ IswNumber(resources),
     /* xrm_class          */ NULLQUARK,
     /* compress_motion    */ FALSE,
     /* compress_exposure  */ TRUE,
     /* compress_enterleave */ FALSE,
     /* visible_interest   */ FALSE,
     /* destroy            */ CoreDestroy,
     /* resize             */ NULL,
     /* expose             */ NULL,
     /* set_values         */ CoreSetValues,
     /* set_values_hook    */ NULL,
     /* set_values_almost  */ CoreSetValuesAlmost,
     /* get_values_hook    */ NULL,
     /* accept_focus       */ NULL,
     /* version            */ IswVersion,
     /* callback_offsets   */ NULL,
     /* tm_table           */ NULL,
     /* query_geometry       */ NULL,
     /* display_accelerator  */ NULL,
     /* extension            */ NULL
     }
};

externaldef(WidgetClass)
WidgetClass widgetClass = &widgetClassRec;

externaldef(WidgetClass)
WidgetClass coreWidgetClass = &widgetClassRec;

static void
IswCopyScreen(Widget widget, int offset _X_UNUSED, XrmValue *value)
{
    value->addr = (IswPointer) (&widget->core.screen);
}

/*
 * Start of Core methods
 */

static void
CoreClassPartInitialize(register WidgetClass wc)
{
    /* We don't need to check for null super since we'll get to object
       eventually, and it had better define them!  */

    register WidgetClass super = wc->core_class.superclass;

    LOCK_PROCESS;
    if (wc->core_class.realize == IswInheritRealize) {
        wc->core_class.realize = super->core_class.realize;
    }

    if (wc->core_class.resize == IswInheritResize) {
        wc->core_class.resize = super->core_class.resize;
    }

    if (wc->core_class.expose == IswInheritExpose) {
        wc->core_class.expose = super->core_class.expose;
    }

    if (wc->core_class.set_values_almost == IswInheritSetValuesAlmost) {
        wc->core_class.set_values_almost = super->core_class.set_values_almost;
    }

    if (wc->core_class.query_geometry == IswInheritQueryGeometry) {
        wc->core_class.query_geometry = super->core_class.query_geometry;
    }

    if (wc->core_class.accept_focus == IswInheritAcceptFocus) {
        wc->core_class.accept_focus = super->core_class.accept_focus;
    }

    if (wc->core_class.display_accelerator == IswInheritDisplayAccelerator) {
        wc->core_class.display_accelerator =
            super->core_class.display_accelerator;
    }

    if (wc->core_class.tm_table == IswInheritTranslations) {
        wc->core_class.tm_table =
            wc->core_class.superclass->core_class.tm_table;
    }
    else if (wc->core_class.tm_table != NULL) {
        wc->core_class.tm_table =
            (String) IswParseTranslationTable(wc->core_class.tm_table);
    }

    if (wc->core_class.actions != NULL) {
        Boolean inPlace;

        if (wc->core_class.version == IswVersionDontCheck)
            inPlace = True;
        else
            inPlace = (wc->core_class.version < IswVersion) ? False : True;

        /* Compile the action table into a more efficient form */
        wc->core_class.actions =
            (IswActionList) _IswInitializeActionData(wc->core_class.actions,
                                                   wc->core_class.num_actions,
                                                   inPlace);
    }
    UNLOCK_PROCESS;
}

static void
CoreInitialize(Widget requested_widget _X_UNUSED,
               register Widget new_widget,
               ArgList args _X_UNUSED,
               Cardinal *num_args _X_UNUSED)
{
    IswTranslations save1, save2;

    new_widget->core.event_table = NULL;
    new_widget->core.tm.proc_table = NULL;
    new_widget->core.tm.lastEventTime = 0;
    /* magic semi-resource fetched by GetResources */
    save1 = (IswTranslations) new_widget->core.tm.current_state;
    new_widget->core.tm.current_state = NULL;
    save2 = new_widget->core.tm.translations;
    LOCK_PROCESS;
    new_widget->core.tm.translations =
        (IswTranslations) new_widget->core.widget_class->core_class.tm_table;
    UNLOCK_PROCESS;
    if (save1)
        _IswMergeTranslations(new_widget, save1, save1->operation);
    if (save2)
        _IswMergeTranslations(new_widget, save2, save2->operation);
}

static void
CoreRealize(IswDisplay display _X_UNUSED,
            Widget widget _X_UNUSED,
            IswValueMask *value_mask _X_UNUSED,
            uint32_t *attributes _X_UNUSED)
{
}                               /* CoreRealize */

static void
CoreDestroy(Widget widget)
{
    _IswFreeEventTable(&widget->core.event_table);
    _IswDestroyTMData(widget);

    if (widget->core.popup_list != NULL)
        IswFree((char *) widget->core.popup_list);

}                               /* CoreDestroy */

static Boolean
CoreSetValues(Widget old,
              Widget reference _X_UNUSED,
              Widget new,
              ArgList args _X_UNUSED,
              Cardinal *num_args _X_UNUSED)
{
    Boolean redisplay = FALSE;

    if (old->core.tm.translations != new->core.tm.translations) {
        IswTranslations save = new->core.tm.translations;
        new->core.tm.translations = old->core.tm.translations;
        _IswMergeTranslations(new, save, IswTableReplace);
    }

    /* Widgets are windowless: there is no X window whose attributes to change.
       The widget draws into its windowed ancestor, so a background change just
       needs a repaint via the widget's own expose. */
    if (IswIsRealized(old)) {
        if (old->core.background_pixel != new->core.background_pixel) {
            redisplay = TRUE;
        }

        if (old->core.depth != new->core.depth) {
            IswAppWarningMsg(IswWidgetToApplicationContext(old),
                            "invalidDepth", "setValues", IswCIswToolkitError,
                            "Can't change widget depth", NULL, NULL);
            new->core.depth = old->core.depth;
        }

        if (old->core.mapped_when_managed != new->core.mapped_when_managed) {
            Boolean mapped_when_managed = new->core.mapped_when_managed;
            new->core.mapped_when_managed = !mapped_when_managed;
            IswSetMappedWhenManaged(new, mapped_when_managed);
        }
    }

    return redisplay;
}                               /* CoreSetValues */

static void
CoreSetValuesAlmost(Widget old _X_UNUSED,
                    Widget new _X_UNUSED,
                    IswWidgetGeometry *request,
                    IswWidgetGeometry *reply)
{
    *request = *reply;
}
