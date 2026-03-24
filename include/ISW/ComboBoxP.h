/*
 * ComboBoxP.h - Private definitions for ComboBox widget
 *
 * ComboBox is a subclass of List with dropdownMode defaulting to True.
 */

#ifndef _ISW_IswComboBoxP_h
#define _ISW_IswComboBoxP_h

#include <ISW/ListP.h>
#include <ISW/ComboBox.h>

/* New fields for the ComboBox widget class record */
typedef struct {int foo;} ComboBoxClassPart;

/* Full class record declaration */
typedef struct _ComboBoxClassRec {
    CoreClassPart       core_class;
    SimpleClassPart     simple_class;
    ListClassPart       list_class;
    ComboBoxClassPart   comboBox_class;
} ComboBoxClassRec;

extern ComboBoxClassRec comboBoxClassRec;

/* No new instance fields — everything is inherited from List */
typedef struct {int dummy;} ComboBoxPart;

typedef struct _ComboBoxRec {
    CorePart    core;
    SimplePart  simple;
    ListPart    list;
    ComboBoxPart comboBox;
} ComboBoxRec;

#endif /* _ISW_IswComboBoxP_h */
