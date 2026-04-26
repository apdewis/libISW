/*
 * ListBoxP.h - Private header for ListBox widget
 */

#ifndef _ISW_IswListBoxP_h
#define _ISW_IswListBoxP_h

#include <ISW/ListBox.h>
#include <ISW/ConstrainP.h>
#include <ISW/ISWRender.h>

typedef struct {
    int empty;
} ListBoxClassPart;

typedef struct _ListBoxClassRec {
    CoreClassPart        core_class;
    CompositeClassPart   composite_class;
    ConstraintClassPart  constraint_class;
    ListBoxClassPart     listBox_class;
} ListBoxClassRec;

extern ListBoxClassRec listBoxClassRec;

typedef struct {
    /* resources */
    IswListBoxSelectionMode selection_mode;
    Dimension               row_spacing;
    Boolean                 show_separators;
    Pixel                   selected_background;
    IswCallbackList         select_callback;
    IswCallbackList         activate_callback;

    /* private state */
    ISWRenderContext        *render_ctx;
    int                     focused_index;
    xcb_timestamp_t         last_click_time;
    int                     last_click_index;
    Boolean                 has_focus;
} ListBoxPart;

typedef struct _ListBoxRec {
    CorePart       core;
    CompositePart  composite;
    ConstraintPart constraint;
    ListBoxPart    listBox;
} ListBoxRec;

typedef struct {
    /* constraint resources */
    Boolean   selectable;
    Dimension row_height;
    Boolean   separator;

    /* private */
    Boolean   selected;
} ListBoxConstraintsPart;

typedef struct _ListBoxConstraintsRec {
    ListBoxConstraintsPart listBox;
} ListBoxConstraintsRec, *ListBoxConstraints;

#endif /* _ISW_IswListBoxP_h */
