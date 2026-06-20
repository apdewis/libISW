/*
 * ComboBox.c - ComboBox widget
 *
 * A subclass of List that defaults dropdownMode to True, presenting a
 * collapsed single-item selector that pops up a menu on click.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ComboBoxP.h>

#define offset(field) IswOffset(ComboBoxWidget, field)

static IswResource resources[] = {
    {IswNdropdownMode, IswCDropdownMode, IswRBoolean, sizeof(Boolean),
	offset(list.dropdown), IswRImmediate, (IswPointer) True},
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
    /* realize			*/	IswInheritRealize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* num_resources		*/	IswNumber(resources),
    /* xrm_class		*/	ISW_NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	FALSE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	NULL,
    /* resize			*/	IswInheritResize,
    /* expose			*/	IswInheritExpose,
    /* set_values		*/	NULL,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	IswInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	IswVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	IswInheritTranslations,
    /* query_geometry		*/	IswInheritQueryGeometry,
  },
/* Simple class fields initialization */
  {
    /* change_sensitive		*/	IswInheritChangeSensitive
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
