/*
 * ListBoxP.h - Private header for ListBox widget
 */

#ifndef _ISW_IswListBoxP_h
#define _ISW_IswListBoxP_h

#include <ISW/ListBox.h>
#include <ISW/ConstrainP.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWImage.h>

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
    Pixel                   foreground;
    IswFontStruct           *font;
    IswCallbackList         select_callback;
    IswCallbackList         activate_callback;
    IswCallbackList         pivot_callback;

    /* private state (selection/focus fields are meaningful on the
       selection root — the outermost ListBox of a nested tree) */
    ISWRenderContext        *render_ctx;
    Widget                  focused_child;
    IswTime                 last_click_time;
    Widget                  last_click_child;
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
    String    pivot_label;        /* group header text (nested-ListBox children) */
    Boolean   pivot_open;
    String    pivot_image;        /* closed-state chevron override */
    String    pivot_image_open;   /* open-state chevron override */

    /* private */
    Boolean   selected;

    /* group header render state (valid only for nested-ListBox children) */
    Position  header_y;           /* top of the header band, set by layout */
    ISWImage  *icon_closed;
    ISWImage  *icon_open;
    int       icon_handle;
    const unsigned char *icon_handle_raster;
    unsigned int icon_handle_w;
    unsigned int icon_handle_h;
    Pixel     icon_handle_fg;
} ListBoxConstraintsPart;

typedef struct _ListBoxConstraintsRec {
    ListBoxConstraintsPart listBox;
} ListBoxConstraintsRec, *ListBoxConstraints;

#endif /* _ISW_IswListBoxP_h */
