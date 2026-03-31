/*
 * Image.c - Image widget implementation
 *
 * A widget for displaying SVG images. Extends Label, inheriting its
 * svgData/svgFile resources. Re-rasterizes on resize to maintain
 * crisp rendering at any size.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <ISW/ImageP.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWImage.h>
#include <ISW/ISWInit.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdlib.h>

static void ClassInitialize(void);
static void Resize(Widget);

ImageClassRec imageClassRec = {
  {
    /* core_class fields */
    /* superclass	*/	(WidgetClass) &labelClassRec,
    /* class_name	*/	"Image",
    /* widget_size	*/	sizeof(ImageRec),
    /* class_initialize	*/	ClassInitialize,
    /* class_part_init	*/	NULL,
    /* class_inited	*/	FALSE,
    /* initialize	*/	NULL,
    /* initialize_hook	*/	NULL,
    /* realize		*/	XtInheritRealize,
    /* actions		*/	NULL,
    /* num_actions	*/	0,
    /* resources	*/	NULL,
    /* num_resources	*/	0,
    /* xrm_class	*/	NULLQUARK,
    /* compress_motion	*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest	*/	FALSE,
    /* destroy		*/	NULL,
    /* resize		*/	Resize,
    /* expose		*/	XtInheritExpose,
    /* set_values	*/	NULL,
    /* set_values_hook	*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook	*/	NULL,
    /* accept_focus	*/	NULL,
    /* version		*/	XtVersion,
    /* callback_private	*/	NULL,
    /* tm_table		*/	NULL,
    /* query_geometry	*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension	*/	NULL
  },
  /* Simple class */
  {
    /* change_sensitive	*/	XtInheritChangeSensitive
  },
  /* Label class */
  {
    /* ignore		*/	0
  },
  /* Image class */
  {
    /* empty		*/	0
  }
};

WidgetClass imageWidgetClass = (WidgetClass)&imageClassRec;

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
}

/*
 * On resize, invalidate the cached raster so the SVG is re-rasterized
 * at the new dimensions on the next expose. Then call Label's resize
 * for repositioning.
 */
static void
Resize(Widget w)
{
    LabelWidget lw = (LabelWidget)w;

    if (lw->label.image) {
	/*
	 * Recompute label dimensions to fill the available space,
	 * respecting internal padding.
	 */
	Dimension pad_w = 2 * lw->label.internal_width;
	Dimension pad_h = 2 * lw->label.internal_height;
	if (w->core.width > pad_w)
	    lw->label.label_width = w->core.width - pad_w;
	if (w->core.height > pad_h)
	    lw->label.label_height = w->core.height - pad_h;
    }

    /* Call Label's resize for repositioning */
    labelClassRec.core_class.resize(w);
}
