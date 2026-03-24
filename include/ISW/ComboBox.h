/*
 * ComboBox.h - Public header for ComboBox widget
 *
 * The ComboBox is a subclass of List that defaults dropdownMode to True,
 * presenting a collapsed single-item selector that pops up a menu on click.
 */

#ifndef _ISW_IswComboBox_h
#define _ISW_IswComboBox_h

#include <ISW/List.h>

/* Class record constants */

extern WidgetClass comboBoxWidgetClass;

typedef struct _ComboBoxClassRec *ComboBoxWidgetClass;
typedef struct _ComboBoxRec      *ComboBoxWidget;

#endif /* _ISW_IswComboBox_h */
