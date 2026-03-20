/*
 * multi_stubs.c - Stub implementations for Multi* widget classes
 *
 * This file provides stub implementations for multiSinkObjectClass and
 * multiSrcObjectClass which are referenced by libXaw3d.so but not yet
 * fully ported to XCB.
 *
 * These stubs allow the demo to link and run, demonstrating the widgets
 * that HAVE been ported to XCB. The Multi* widgets themselves won't be
 * functional until they are fully converted.
 *
 * NOTE: This is a temporary workaround. DO NOT USE in production code.
 */

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/Object.h>

/*
 * Stub widget class records for Multi* widgets.
 * These minimal implementations satisfy the linker without
 * providing actual Multi* widget functionality.
 */

/* Forward declarations */
typedef struct _MultiSinkClassRec *MultiSinkObjectClass;
typedef struct _MultiSrcClassRec *MultiSrcObjectClass;

/* Minimal class structures */
typedef struct {
    XtPointer extension;
} MultiSinkClassPart;

typedef struct {
    XtPointer extension;
} MultiSrcClassPart;

struct _MultiSinkClassRec {
    ObjectClassPart object_class;
    MultiSinkClassPart multisink_class;
};

struct _MultiSrcClassRec {
    ObjectClassPart object_class;
    MultiSrcClassPart multisrc_class;
};

/* Create minimal class records */
static struct _MultiSinkClassRec multiSinkClassRec = {
    {
        /* superclass         */ (WidgetClass) &objectClassRec,
        /* class_name         */ "MultiSink",
        /* widget_size        */ sizeof(ObjectRec),
        /* class_initialize   */ NULL,
        /* class_part_init    */ NULL,
        /* class_inited       */ FALSE,
        /* initialize         */ NULL,
        /* initialize_hook    */ NULL,
        /* realize            */ NULL,
        /* actions            */ NULL,
        /* num_actions        */ 0,
        /* resources          */ NULL,
        /* num_resources      */ 0,
        /* xrm_class          */ NULLQUARK,
        /* compress_motion    */ FALSE,
        /* compress_exposure  */ FALSE,
        /* compress_enterleave*/ FALSE,
        /* visible_interest   */ FALSE,
        /* destroy            */ NULL,
        /* resize             */ NULL,
        /* expose             */ NULL,
        /* set_values         */ NULL,
        /* set_values_hook    */ NULL,
        /* set_values_almost  */ NULL,
        /* get_values_hook    */ NULL,
        /* accept_focus       */ NULL,
        /* version            */ XtVersion,
        /* callback_private   */ NULL,
        /* tm_table           */ NULL,
        /* query_geometry     */ NULL,
        /* display_accelerator*/ NULL,
        /* extension          */ NULL
    },
    {
        /* extension          */ NULL
    }
};

static struct _MultiSrcClassRec multiSrcClassRec = {
    {
        /* superclass         */ (WidgetClass) &objectClassRec,
        /* class_name         */ "MultiSrc",
        /* widget_size        */ sizeof(ObjectRec),
        /* class_initialize   */ NULL,
        /* class_part_init    */ NULL,
        /* class_inited       */ FALSE,
        /* initialize         */ NULL,
        /* initialize_hook    */ NULL,
        /* realize            */ NULL,
        /* actions            */ NULL,
        /* num_actions        */ 0,
        /* resources          */ NULL,
        /* num_resources      */ 0,
        /* xrm_class          */ NULLQUARK,
        /* compress_motion    */ FALSE,
        /* compress_exposure  */ FALSE,
        /* compress_enterleave*/ FALSE,
        /* visible_interest   */ FALSE,
        /* destroy            */ NULL,
        /* resize             */ NULL,
        /* expose             */ NULL,
        /* set_values         */ NULL,
        /* set_values_hook    */ NULL,
        /* set_values_almost  */ NULL,
        /* get_values_hook    */ NULL,
        /* accept_focus       */ NULL,
        /* version            */ XtVersion,
        /* callback_private   */ NULL,
        /* tm_table           */ NULL,
        /* query_geometry     */ NULL,
        /* display_accelerator*/ NULL,
        /* extension          */ NULL
    },
    {
        /* extension          */ NULL
    }
};

/* Export the class pointers that libXaw3d.so expects */
WidgetClass multiSinkObjectClass = (WidgetClass) &multiSinkClassRec;
WidgetClass multiSrcObjectClass = (WidgetClass) &multiSrcClassRec;
