/*
 * DrawingAreaP.h - Private definitions for DrawingArea widget
 */

#ifndef _ISW_DrawingAreaP_h
#define _ISW_DrawingAreaP_h

#include <ISW/DrawingArea.h>
#include <ISW/SimpleP.h>
#include <ISW/ISWRender.h>

typedef struct {
    int makes_compiler_happy;
} DrawingAreaClassPart;

typedef struct _DrawingAreaClassRec {
    CoreClassPart          core_class;
    SimpleClassPart        simple_class;
    DrawingAreaClassPart   drawing_area_class;
} DrawingAreaClassRec;

extern DrawingAreaClassRec drawingAreaClassRec;

typedef struct {
    /* resources */
    XtCallbackList expose_callbacks;
    XtCallbackList resize_callbacks;
    XtCallbackList input_callbacks;

    /* private */
    ISWRenderContext *render_ctx;
} DrawingAreaPart;

typedef struct _DrawingAreaRec {
    CorePart          core;
    SimplePart        simple;
    DrawingAreaPart   drawing_area;
} DrawingAreaRec;

#endif /* _ISW_DrawingAreaP_h */
