/*
 * ComboBox.c - ComboBox widget
 *
 * A subclass of List that defaults dropdownMode to True, presenting a
 * collapsed single-item selector that pops up a menu on click.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ComboBoxP.h>

#define offset(field) XtOffset(ComboBoxWidget, field)

static XtResource resources[] = {
    {XtNdropdownMode, XtCDropdownMode, XtRBoolean, sizeof(Boolean),
	offset(list.dropdown), XtRImmediate, (XtPointer) True},
};

#undef offset

ComboBoxClassRec comboBoxClassRec = {
  {
/* core_class fields */
    /* superclass		*/	(WidgetClass) &listClassRec,
    /* class_name		*/	"ComboBox",
    /* widget_size		*/	sizeof(ComboBoxRec),
    /* class_initialize		*/	NULL,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	NULL,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	FALSE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	NULL,
    /* resize			*/	XtInheritResize,
    /* expose			*/	XtInheritExpose,
    /* set_values		*/	NULL,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	XtInheritTranslations,
    /* query_geometry		*/	XtInheritQueryGeometry,
  },
/* Simple class fields initialization */
  {
    /* change_sensitive		*/	XtInheritChangeSensitive
  },
/* List class fields initialization */
  {
    /* not used			*/	0
  },
/* ComboBox class fields initialization */
  {
    /* not used			*/	0
  },
};

WidgetClass comboBoxWidgetClass = (WidgetClass)&comboBoxClassRec;
