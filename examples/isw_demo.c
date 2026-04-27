/*
 * xaw3d_demo.c - Comprehensive Isw3d Widget Demonstration
 *
 * This program demonstrates all major Isw3d widgets working with
 * the XCB-ported library and modified libXt.
 *
 * Compile: See Makefile
 * Run: ./xaw3d_demo
 */

#include <ISW/Intrinsic.h>
#include <ISW/StringDefs.h>
#include <ISW/Shell.h>
#include <ISW/IswArgMacros.h>

/* Container widgets */
#include <ISW/Paned.h>
#include <ISW/Box.h>
#include <ISW/Form.h>
#include <ISW/Viewport.h>
#include <ISW/MainWindow.h>

/* Toolbar / StatusBar */
#include <ISW/Toolbar.h>
#include <ISW/StatusBar.h>

/* Basic display widgets */
#include <ISW/Label.h>
#include <ISW/Command.h>
#include <ISW/Toggle.h>

/* Menu widgets */
#include <ISW/MenuBar.h>
#include <ISW/MenuButton.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>

/* Rendering backend info */
#include <ISW/ISWRender.h>

/* Selection widgets */
#include <ISW/IconView.h>
#include <ISW/ListView.h>
#include <ISW/List.h>
#include <ISW/ListBox.h>
#include <ISW/ListBoxRow.h>
#include <ISW/ComboBox.h>
#include <ISW/Tree.h>

/* Layout widgets */
#include <ISW/Layout.h>
#include <ISW/Panner.h>
#include <ISW/Porthole.h>
#include <ISW/Reports.h>

/* Text widgets */
#include <ISW/Text.h>
#include <ISW/Text.h>

/* Specialized widgets */
#include <ISW/ColorPicker.h>
#include <ISW/FontChooser.h>
#include <ISW/SpinBox.h>
#include <ISW/Slider.h>
#include <ISW/Tip.h>
#include <ISW/Scrollbar.h>
#include <ISW/Dialog.h>
#include <ISW/Repeater.h>
#include <ISW/Grip.h>
#include <ISW/ProgressBar.h>
#include <ISW/DrawingArea.h>
#include <ISW/Tabs.h>
#include <ISW/ISWXdnd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * FORWARD DECLARATIONS
 * ============================================================ */

/* Main window creation */
Widget create_main_window(Widget parent);
void populate_menubar(Widget menubar);
Widget create_title_label(Widget parent);

/* Section creation functions */
Widget create_containers_section(Widget parent);
Widget create_basic_widgets_section(Widget parent);
Widget create_selection_section(Widget parent);
Widget create_specialized_section(Widget parent);
Widget create_navigation_section(Widget parent);

/* Widget demo functions */
Widget create_box_demo(Widget parent);
Widget create_form_demo(Widget parent);
Widget create_viewport_demo(Widget parent);
Widget create_layout_demo(Widget parent);
Widget create_paned_grip_demo(Widget parent);
Widget create_toolbar_demo(Widget parent);

Widget create_command_demo(Widget parent);
Widget create_toggle_demo(Widget parent);
Widget create_checkbox_demo(Widget parent);
Widget create_menu_demo(Widget parent);
Widget create_repeater_demo(Widget parent);

Widget create_iconview_demo(Widget parent);
Widget create_listview_demo(Widget parent);
Widget create_list_demo(Widget parent);
Widget create_listbox_demo(Widget parent);
Widget create_combobox_demo(Widget parent);
Widget create_text_demo(Widget parent);
Widget create_tree_demo(Widget parent);

Widget create_panner_demo(Widget parent);

Widget create_fontchooser_demo(Widget parent);
Widget create_colorpicker_demo(Widget parent);
Widget create_spinbox_demo(Widget parent);
Widget create_slider_demo(Widget parent);
Widget create_scrollbar_demo(Widget parent);
Widget create_progressbar_demo(Widget parent);
Widget create_dialog_demo(Widget parent);
void open_modal_dialog_cb(Widget w, IswPointer client_data, IswPointer call_data);
Widget create_drawingarea_demo(Widget parent);
Widget create_tabs_demo(Widget parent);
void tabs_callback(Widget w, IswPointer client_data, IswPointer call_data);

/* Callback functions */
void button_callback(Widget w, IswPointer client_data, IswPointer call_data);
void toggle_callback(Widget w, IswPointer client_data, IswPointer call_data);
void checkbox_callback(Widget w, IswPointer client_data, IswPointer call_data);
void menu_callback(Widget w, IswPointer client_data, IswPointer call_data);
void list_callback(Widget w, IswPointer client_data, IswPointer call_data);
void listbox_select_callback(Widget w, IswPointer client_data, IswPointer call_data);
void listbox_activate_callback(Widget w, IswPointer client_data, IswPointer call_data);
void combobox_callback(Widget w, IswPointer client_data, IswPointer call_data);
void iconview_callback(Widget w, IswPointer client_data, IswPointer call_data);
void repeater_callback(Widget w, IswPointer client_data, IswPointer call_data);
void slider_callback(Widget w, IswPointer client_data, IswPointer call_data);
void spinbox_callback(Widget w, IswPointer client_data, IswPointer call_data);
void colorpicker_callback(Widget w, IswPointer client_data, IswPointer call_data);
void fontchooser_callback(Widget w, IswPointer client_data, IswPointer call_data);
void dialog_ok_callback(Widget w, IswPointer client_data, IswPointer call_data);
void drawingarea_expose(Widget w, IswPointer client_data, IswPointer call_data);
void quit_callback(Widget w, IswPointer client_data, IswPointer call_data);
void drop_callback(Widget w, IswPointer client_data, IswPointer call_data);
void drag_enter_callback(Widget w, IswPointer client_data, IswPointer call_data);
void drag_leave_callback(Widget w, IswPointer client_data, IswPointer call_data);
void drag_start_action(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params);

/* Menu bar callbacks */
void file_menu_callback(Widget w, IswPointer client_data, IswPointer call_data);
void edit_menu_callback(Widget w, IswPointer client_data, IswPointer call_data);
void about_menu_callback(Widget w, IswPointer client_data, IswPointer call_data);

/* Timer callbacks */
/* Tooltip helper */
void attach_tooltip(Widget widget, const char *tip_text);



/* ============================================================
 * MAIN FUNCTION
 * ============================================================ */

int main(int argc, char *argv[]) {
    IswAppContext app_context;
    Widget toplevel, main_widget;

    /* Initialize X Toolkit with XCB backend */
    toplevel = IswAppInitialize(&app_context, "Isw3dDemo",
                               NULL, 0,
                               &argc, argv,
                               NULL, NULL, 0);


    /* Set main window size — not scaled, so it fits the screen at any DPI */
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgWidth(&ab, 1200);
    IswArgHeight(&ab, 900);
    IswArgTitle(&ab, "Isw3d Widget Demonstration - Comprehensive Widget Showcase");
    IswArgAllowShellResize(&ab, True);
    IswSetValues(toplevel, ab.args, ab.count);
    
    /* Create main widget structure */
    main_widget = create_main_window(toplevel);
    
    printf("Isw3d Widget Demo starting...\n");
    printf("This demo showcases widgets with XCB backend\n");
    printf("---------------------------------------------\n");
    
    /* Print rendering backend information */
    ISWRenderPrintBackendInfo();
    printf("\n");
    
    /* Realize all widgets */
    IswRealizeWidget(toplevel);

    /* Enter event loop */
    IswAppMainLoop(app_context);

    return 0;
}

/* ============================================================
 * MAIN WINDOW CREATION
 * ============================================================ */

Widget create_main_window(Widget parent) {
    Widget main_win, viewport, content_box, title;
    IswArgBuilder ab = IswArgBuilderInit();

    /* MainWindow as direct shell child — menubar fixed at top */
    IswArgWidth(&ab, 1200);
    IswArgHeight(&ab, 900);
    main_win = IswCreateManagedWidget("mainWindow", mainWindowWidgetClass,
                                      parent, ab.args, ab.count);

    /* Populate the built-in menubar */
    populate_menubar(IswMainWindowGetMenuBar(main_win));

    /* Status bar at bottom — MainWindow auto-detects StatusBar children */
    {
        Widget statusbar, sb_label;
        IswArgBuilder sb = IswArgBuilderInit();

        statusbar = IswCreateManagedWidget("statusbar", statusBarWidgetClass,
                                           main_win, sb.args, sb.count);

        IswArgLabel(&sb, "Ready");
        IswArgStatusStretch(&sb, True);
        sb_label = IswCreateManagedWidget("statusText", labelWidgetClass,
                                          statusbar, sb.args, sb.count);

        IswArgBuilderReset(&sb);
        IswArgLabel(&sb, "Ln 1, Col 1");
        IswCreateManagedWidget("statusPos", labelWidgetClass,
                               statusbar, sb.args, sb.count);
    }

    /* Viewport as content child — scrolls independently of menubar */
    IswArgBuilder vb = IswArgBuilderInit();
    IswArgAllowVert(&vb, True);
    IswArgAllowHoriz(&vb, True);
    IswArgForceBars(&vb, True);
    IswArgUseRight(&vb, True);
    IswArgUseBottom(&vb, True);
    IswArgBorderWidth(&vb, 0);
    viewport = IswCreateManagedWidget("viewport", viewportWidgetClass,
                                      main_win, vb.args, vb.count);

    /* Content box inside viewport — holds all demo sections */
    IswArgBuilderReset(&ab);
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 0);
    content_box = IswCreateManagedWidget("contentBox", boxWidgetClass,
                                         viewport, ab.args, ab.count);

    /* Title section */
    title = create_title_label(content_box);

    /* Widget demonstration sections */
    create_containers_section(content_box);
    create_basic_widgets_section(content_box);
    create_selection_section(content_box);

    /* Advanced widgets in a horizontal box */
    Widget advanced_box;
    IswArgBuilderReset(&ab);
    IswArgOrientation(&ab, IswOrientHorizontal);
    IswArgBorderWidth(&ab, 0);
    advanced_box = IswCreateManagedWidget("advancedBox", boxWidgetClass, content_box, ab.args, ab.count);

    create_tree_demo(advanced_box);
    create_layout_demo(advanced_box);
    create_paned_grip_demo(advanced_box);

    /* Panner demo in its own section */
    create_navigation_section(content_box);

    create_specialized_section(content_box);

    create_tabs_demo(content_box);

    return main_win;
}

void populate_menubar(Widget menubar) {
    Widget file_button, edit_button, about_button;
    Widget file_menu, edit_menu, about_menu;
    Widget entry;
    IswArgBuilder ab = IswArgBuilderInit();

    /* === FILE MENU === */
    IswArgLabel(&ab, "File");
    IswArgMenuName(&ab, "fileMenu");
    IswArgMnemonicKey(&ab, 'f');
    file_button = IswCreateManagedWidget("fileButton", menuButtonWidgetClass, menubar, ab.args, ab.count);

    file_menu = IswCreatePopupShell("fileMenu", simpleMenuWidgetClass, file_button, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "New");
    IswArgMnemonicKey(&ab, 'n');
    entry = IswCreateManagedWidget("menuNew", smeBSBObjectClass, file_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, file_menu_callback, (IswPointer)"New");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Open...");
    IswArgMnemonicKey(&ab, 'o');
    entry = IswCreateManagedWidget("menuOpen", smeBSBObjectClass, file_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, file_menu_callback, (IswPointer)"Open");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Save");
    IswArgMnemonicKey(&ab, 's');
    entry = IswCreateManagedWidget("menuSave", smeBSBObjectClass, file_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, file_menu_callback, (IswPointer)"Save");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Save As...");
    IswArgMnemonicKey(&ab, 'a');
    entry = IswCreateManagedWidget("menuSaveAs", smeBSBObjectClass, file_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, file_menu_callback, (IswPointer)"Save As");

    /* 3D separator */
    IswCreateManagedWidget("sep1", smeLineObjectClass, file_menu, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Export");
    IswArgMnemonicKey(&ab, 'e');
    IswArgMenuName(&ab, "exportMenu");
    entry = IswCreateManagedWidget("menuExport", smeBSBObjectClass, file_menu, ab.args, ab.count);

    {
	Widget export_menu = IswCreatePopupShell("exportMenu",
	    simpleMenuWidgetClass, file_menu, NULL, 0);

	IswArgBuilderReset(&ab);
	IswArgLabel(&ab, "PDF");
	entry = IswCreateManagedWidget("exportPdf", smeBSBObjectClass, export_menu, ab.args, ab.count);
	IswAddCallback(entry, IswNcallback, file_menu_callback, (IswPointer)"Export PDF");

	IswArgBuilderReset(&ab);
	IswArgLabel(&ab, "PNG");
	entry = IswCreateManagedWidget("exportPng", smeBSBObjectClass, export_menu, ab.args, ab.count);
	IswAddCallback(entry, IswNcallback, file_menu_callback, (IswPointer)"Export PNG");

	IswArgBuilderReset(&ab);
	IswArgLabel(&ab, "SVG");
	entry = IswCreateManagedWidget("exportSvg", smeBSBObjectClass, export_menu, ab.args, ab.count);
	IswAddCallback(entry, IswNcallback, file_menu_callback, (IswPointer)"Export SVG");
    }

    IswCreateManagedWidget("sep2", smeLineObjectClass, file_menu, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Quit");
    IswArgMnemonicKey(&ab, 'q');
    entry = IswCreateManagedWidget("menuQuit", smeBSBObjectClass, file_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, quit_callback, NULL);

    /* === EDIT MENU === */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Edit");
    IswArgMenuName(&ab, "editMenu");
    IswArgMnemonicKey(&ab, 'e');
    edit_button = IswCreateManagedWidget("editButton", menuButtonWidgetClass, menubar, ab.args, ab.count);

    edit_menu = IswCreatePopupShell("editMenu", simpleMenuWidgetClass, edit_button, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Undo");
    IswArgMnemonicKey(&ab, 'u');
    entry = IswCreateManagedWidget("menuUndo", smeBSBObjectClass, edit_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, edit_menu_callback, (IswPointer)"Undo");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Redo");
    IswArgMnemonicKey(&ab, 'r');
    entry = IswCreateManagedWidget("menuRedo", smeBSBObjectClass, edit_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, edit_menu_callback, (IswPointer)"Redo");

    IswCreateManagedWidget("sep3", smeLineObjectClass, edit_menu, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Cut");
    IswArgMnemonicKey(&ab, 't');
    entry = IswCreateManagedWidget("menuCut", smeBSBObjectClass, edit_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, edit_menu_callback, (IswPointer)"Cut");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Copy");
    IswArgMnemonicKey(&ab, 'c');
    entry = IswCreateManagedWidget("menuCopy", smeBSBObjectClass, edit_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, edit_menu_callback, (IswPointer)"Copy");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Paste");
    IswArgMnemonicKey(&ab, 'p');
    entry = IswCreateManagedWidget("menuPaste", smeBSBObjectClass, edit_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, edit_menu_callback, (IswPointer)"Paste");

    IswCreateManagedWidget("sep4", smeLineObjectClass, edit_menu, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Preferences...");
    entry = IswCreateManagedWidget("menuPrefs", smeBSBObjectClass, edit_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, edit_menu_callback, (IswPointer)"Preferences");

    /* === ABOUT MENU === */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "About");
    IswArgMenuName(&ab, "aboutMenu");
    IswArgMnemonicKey(&ab, 'a');
    about_button = IswCreateManagedWidget("aboutButton", menuButtonWidgetClass, menubar, ab.args, ab.count);

    about_menu = IswCreatePopupShell("aboutMenu", simpleMenuWidgetClass, about_button, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "About ISW Demo");
    entry = IswCreateManagedWidget("menuAbout", smeBSBObjectClass, about_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, about_menu_callback, (IswPointer)"About");

    IswCreateManagedWidget("sep5", smeLineObjectClass, about_menu, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "ISW Documentation");
    entry = IswCreateManagedWidget("menuDocs", smeBSBObjectClass, about_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, about_menu_callback, (IswPointer)"Documentation");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Report Bug");
    entry = IswCreateManagedWidget("menuBug", smeBSBObjectClass, about_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, about_menu_callback, (IswPointer)"Bug");

    IswCreateManagedWidget("sep6", smeLineObjectClass, about_menu, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "License");
    entry = IswCreateManagedWidget("menuLicense", smeBSBObjectClass, about_menu, ab.args, ab.count);
    IswAddCallback(entry, IswNcallback, about_menu_callback, (IswPointer)"License");

}

Widget create_title_label(Widget parent) {
    Widget title;
    IswArgBuilder ab = IswArgBuilderInit();

    IswArgLabel(&ab, "=== Isw3d Widget Demonstration (XCB Backend) ===");
    IswArgJustify(&ab, IswJustifyCenter);
    IswArgWidth(&ab, 830);
    IswArgHeight(&ab, 35);
    IswArgBorderWidth(&ab, 1);
    title = IswCreateManagedWidget("titleLabel", labelWidgetClass,
                                  parent, ab.args, ab.count);

    return title;
}

/* ============================================================
 * CONTAINER WIDGETS SECTION
 * ============================================================ */

Widget create_containers_section(Widget parent) {
    Widget form, section_label;
    Widget toolbar_demo, box_demo, form_demo, viewport_demo;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Create Form to hold container demos */
    IswArgBorderWidth(&ab, 1);
    IswArgDefaultDistance(&ab, 5);
    form = IswCreateManagedWidget("containersForm", formWidgetClass,
                                 parent, ab.args, ab.count);

    /* Section label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Container Widgets: Toolbar, Box, Form, Viewport");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    section_label = IswCreateManagedWidget("containerLabel", labelWidgetClass,
                                          form, ab.args, ab.count);

    /* Toolbar demo */
    toolbar_demo = create_toolbar_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, section_label);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(toolbar_demo, ab.args, ab.count);

    /* Create demos */
    box_demo = create_box_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, toolbar_demo);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(box_demo, ab.args, ab.count);

    form_demo = create_form_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, box_demo);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(form_demo, ab.args, ab.count);

    viewport_demo = create_viewport_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, form_demo);
    IswArgFromVert(&ab, box_demo);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(viewport_demo, ab.args, ab.count);

    return form;
}

/* Simple inline SVG icons for toolbar demo */
static const char *svg_new =
    "<svg viewBox='0 0 16 16'><rect x='3' y='1' width='10' height='14' "
    "fill='none' stroke='black' stroke-width='1.5'/>"
    "<line x1='5' y1='5' x2='11' y2='5' stroke='black' stroke-width='1'/>"
    "<line x1='5' y1='8' x2='11' y2='8' stroke='black' stroke-width='1'/>"
    "<line x1='5' y1='11' x2='9' y2='11' stroke='black' stroke-width='1'/></svg>";

static const char *svg_open =
    "<svg viewBox='0 0 16 16'><path d='M2 4 L2 14 L14 14 L14 6 L8 6 L7 4 Z' "
    "fill='none' stroke='black' stroke-width='1.5'/></svg>";

static const char *svg_save =
    "<svg viewBox='0 0 16 16'><rect x='2' y='2' width='12' height='12' "
    "fill='none' stroke='black' stroke-width='1.5' rx='1'/>"
    "<rect x='5' y='2' width='6' height='5' fill='none' stroke='black' stroke-width='1'/>"
    "<rect x='4' y='9' width='8' height='5' fill='none' stroke='black' stroke-width='1'/></svg>";

static const char *svg_cut =
    "<svg viewBox='0 0 16 16'>"
    "<circle cx='5' cy='12' r='2.5' fill='none' stroke='black' stroke-width='1.2'/>"
    "<circle cx='11' cy='12' r='2.5' fill='none' stroke='black' stroke-width='1.2'/>"
    "<line x1='5' y1='10' x2='11' y2='3' stroke='black' stroke-width='1.5'/>"
    "<line x1='11' y1='10' x2='5' y2='3' stroke='black' stroke-width='1.5'/></svg>";

static const char *svg_copy =
    "<svg viewBox='0 0 16 16'>"
    "<rect x='5' y='4' width='8' height='10' fill='none' stroke='black' stroke-width='1.2' rx='1'/>"
    "<rect x='3' y='2' width='8' height='10' fill='none' stroke='black' stroke-width='1.2' rx='1'/></svg>";

static const char *svg_paste =
    "<svg viewBox='0 0 16 16'>"
    "<rect x='3' y='3' width='10' height='11' fill='none' stroke='black' stroke-width='1.2' rx='1'/>"
    "<rect x='5' y='1' width='6' height='3' fill='none' stroke='black' stroke-width='1.2' rx='1'/>"
    "<line x1='5' y1='8' x2='11' y2='8' stroke='black' stroke-width='1'/>"
    "<line x1='5' y1='10' x2='11' y2='10' stroke='black' stroke-width='1'/></svg>";

Widget create_toolbar_demo(Widget parent) {
    Widget toolbar;
    IswArgBuilder ab = IswArgBuilderInit();
    Dimension btn_size = 24;

    IswArgBorderWidth(&ab, 1);
    toolbar = IswCreateManagedWidget("toolbar", toolbarWidgetClass, parent, ab.args, ab.count);

    /* Icon buttons with uniform size */
    IswArgBuilderReset(&ab);
    IswArgImage(&ab, svg_new);
    IswArgLabel(&ab, "");
    IswArgWidth(&ab, btn_size);
    IswArgHeight(&ab, btn_size);
    IswCreateManagedWidget("tbNew", commandWidgetClass, toolbar, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgImage(&ab, svg_open);
    IswArgLabel(&ab, "");
    IswArgWidth(&ab, btn_size);
    IswArgHeight(&ab, btn_size);
    IswCreateManagedWidget("tbOpen", commandWidgetClass, toolbar, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgImage(&ab, svg_save);
    IswArgLabel(&ab, "");
    IswArgWidth(&ab, btn_size);
    IswArgHeight(&ab, btn_size);
    IswCreateManagedWidget("tbSave", commandWidgetClass, toolbar, ab.args, ab.count);

    /* Separator */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "");
    IswArgWidth(&ab, 2);
    IswArgHeight(&ab, btn_size);
    IswArgBorderWidth(&ab, 0);
    IswCreateManagedWidget("tbSep", labelWidgetClass, toolbar, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgImage(&ab, svg_cut);
    IswArgLabel(&ab, "");
    IswArgWidth(&ab, btn_size);
    IswArgHeight(&ab, btn_size);
    IswCreateManagedWidget("tbCut", commandWidgetClass, toolbar, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgImage(&ab, svg_copy);
    IswArgLabel(&ab, "");
    IswArgWidth(&ab, btn_size);
    IswArgHeight(&ab, btn_size);
    IswCreateManagedWidget("tbCopy", commandWidgetClass, toolbar, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgImage(&ab, svg_paste);
    IswArgLabel(&ab, "");
    IswArgWidth(&ab, btn_size);
    IswArgHeight(&ab, btn_size);
    IswCreateManagedWidget("tbPaste", commandWidgetClass, toolbar, ab.args, ab.count);

    return toolbar;
}

Widget create_box_demo(Widget parent) {
    Widget box_container, box, label1, label2, label3;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container with label */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box_container = IswCreateManagedWidget("boxContainer", boxWidgetClass,
                                          parent, ab.args, ab.count);

    /* Label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Box Widget (Horizontal)");
    IswArgBorderWidth(&ab, 0);
    IswCreateManagedWidget("boxLabel", labelWidgetClass, box_container, ab.args, ab.count);

    /* Horizontal Box */
    IswArgBuilderReset(&ab);
    IswArgOrientation(&ab, IswOrientHorizontal);
    IswArgHSpace(&ab, 5);
    IswArgVSpace(&ab, 5);
    box = IswCreateManagedWidget("demoBox", boxWidgetClass,
                                box_container, ab.args, ab.count);

    /* Add three labels to box */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Item 1");
    label1 = IswCreateManagedWidget("boxItem1", labelWidgetClass, box, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Item 2");
    label2 = IswCreateManagedWidget("boxItem2", labelWidgetClass, box, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Item 3");
    label3 = IswCreateManagedWidget("boxItem3", labelWidgetClass, box, ab.args, ab.count);

    return box_container;
}

Widget create_form_demo(Widget parent) {
    Widget form, title_label, button1, button2, button3;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Form container */
    IswArgBorderWidth(&ab, 1);
    IswArgWidth(&ab, 350);
    IswArgHeight(&ab, 80);
    form = IswCreateManagedWidget("demoForm", formWidgetClass, parent, ab.args, ab.count);

    /* Title label - top, spans width */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Form Widget (Constraint Layout)");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    title_label = IswCreateManagedWidget("formTitle", labelWidgetClass,
                                        form, ab.args, ab.count);

    /* Button row - relative positioning */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Left");
    IswArgFromVert(&ab, title_label);
    IswArgLeft(&ab, IswChainLeft);
    IswArgTop(&ab, IswChainTop);
    button1 = IswCreateManagedWidget("formBtn1", commandWidgetClass,
                                    form, ab.args, ab.count);
    IswAddCallback(button1, IswNcallback, button_callback, (IswPointer)"Form Left Button");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Center");
    IswArgFromVert(&ab, title_label);
    IswArgFromHoriz(&ab, button1);
    IswArgHorizDistance(&ab, 10);
    button2 = IswCreateManagedWidget("formBtn2", commandWidgetClass,
                                    form, ab.args, ab.count);
    IswAddCallback(button2, IswNcallback, button_callback, (IswPointer)"Form Center Button");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Right");
    IswArgFromVert(&ab, title_label);
    IswArgFromHoriz(&ab, button2);
    IswArgHorizDistance(&ab, 10);
    button3 = IswCreateManagedWidget("formBtn3", commandWidgetClass,
                                    form, ab.args, ab.count);
    IswAddCallback(button3, IswNcallback, button_callback, (IswPointer)"Form Right Button");

    return form;
}

Widget create_viewport_demo(Widget parent) {
    Widget viewport_container, viewport, large_label;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    viewport_container = IswCreateManagedWidget("viewportContainer", boxWidgetClass,
                                               parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Viewport Widget (Scrollable)");
    IswArgBorderWidth(&ab, 0);
    IswCreateManagedWidget("viewportTitle", labelWidgetClass,
                          viewport_container, ab.args, ab.count);

    /* Viewport with scrollbars */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 250);
    IswArgHeight(&ab, 80);
    IswArgAllowHoriz(&ab, True);
    IswArgAllowVert(&ab, True);
    IswArgUseBottom(&ab, True);
    IswArgUseRight(&ab, True);
    viewport = IswCreateManagedWidget("viewport", viewportWidgetClass,
                                     viewport_container, ab.args, ab.count);

    /* Large content inside viewport */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab,
             "This demonstrates scrolling.\n"
             "The content is larger than\n"
             "the viewport, so scrollbars\n"
             "appear automatically.\n"
             "Try scrolling! \n"
             "This demonstrates scrolling.\n"
             "The content is larger than\n"
             "the viewport, so scrollbars\n"
             "appear automatically.\n"
            );
    IswArgJustify(&ab, IswJustifyLeft);
    IswArgWidth(&ab, 400);
    IswArgHeight(&ab, 150);
    large_label = IswCreateManagedWidget("viewportContent", labelWidgetClass,
                                        viewport, ab.args, ab.count);

    return viewport_container;
}

/* ============================================================
 * BASIC WIDGETS SECTION
 * ============================================================ */

Widget create_basic_widgets_section(Widget parent) {
    Widget form, section_label;
    Widget command_demo, toggle_demo, checkbox_demo, menu_demo, repeater_demo;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Section container */
    IswArgBorderWidth(&ab, 1);
    IswArgDefaultDistance(&ab, 5);
    form = IswCreateManagedWidget("basicForm", formWidgetClass, parent, ab.args, ab.count);

    /* Section label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Basic Interactive Widgets: Command, Toggle, Checkbox, Menu, Repeater");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    section_label = IswCreateManagedWidget("basicLabel", labelWidgetClass,
                                          form, ab.args, ab.count);

    /* Create widget demos */
    command_demo = create_command_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, section_label);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(command_demo, ab.args, ab.count);

    toggle_demo = create_toggle_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, command_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(toggle_demo, ab.args, ab.count);

    checkbox_demo = create_checkbox_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, toggle_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(checkbox_demo, ab.args, ab.count);

    menu_demo = create_menu_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, checkbox_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(menu_demo, ab.args, ab.count);

    repeater_demo = create_repeater_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, menu_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(repeater_demo, ab.args, ab.count);

    return form;
}

Widget create_command_demo(Widget parent) {
    Widget box, title, button1, button2, quit_button, svg_button;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("commandBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Command Buttons");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("commandTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Buttons */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Click Me!");
    button1 = IswCreateManagedWidget("cmdBtn1", commandWidgetClass, box, ab.args, ab.count);
    IswAddCallback(button1, IswNcallback, button_callback, (IswPointer)"Button 1");
    attach_tooltip(button1, "This is a clickable command button");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Or Me!");
    IswArgCornerRadius(&ab, 0);
    button2 = IswCreateManagedWidget("cmdBtn2", commandWidgetClass, box, ab.args, ab.count);
    IswAddCallback(button2, IswNcallback, button_callback, (IswPointer)"Button 2");
    attach_tooltip(button2, "Another command button with tooltip");

    /* SVG icon button */
    IswArgBuilderReset(&ab);
    IswArgImage(&ab, "x11.svg");
    IswArgLabel(&ab, "");
    svg_button = IswCreateManagedWidget("svgBtn", commandWidgetClass, box, ab.args, ab.count);
    IswAddCallback(svg_button, IswNcallback, button_callback, (IswPointer)"SVG Button");
    attach_tooltip(svg_button, "Command button with SVG icon");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Quit");
    quit_button = IswCreateManagedWidget("quitBtn", commandWidgetClass, box, ab.args, ab.count);
    IswAddCallback(quit_button, IswNcallback, quit_callback, NULL);
    attach_tooltip(quit_button, "Click to exit the application");

    return box;
}

Widget create_toggle_demo(Widget parent) {
    Widget box, title, toggle1, toggle2, toggle3;
    IswArgBuilder ab = IswArgBuilderInit();
    static Widget radio_group = NULL;

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("toggleBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Toggle (Radio) Buttons");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("toggleTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Radio button group */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Option A");
    IswArgState(&ab, True);
    toggle1 = IswCreateManagedWidget("toggleA", toggleWidgetClass, box, ab.args, ab.count);
    IswAddCallback(toggle1, IswNcallback, toggle_callback, (IswPointer)"Option A");
    radio_group = toggle1;

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Option B");
    IswArgRadioGroup(&ab, radio_group);
    toggle2 = IswCreateManagedWidget("toggleB", toggleWidgetClass, box, ab.args, ab.count);
    IswAddCallback(toggle2, IswNcallback, toggle_callback, (IswPointer)"Option B");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Option C");
    IswArgRadioGroup(&ab, radio_group);
    toggle3 = IswCreateManagedWidget("toggleC", toggleWidgetClass, box, ab.args, ab.count);
    IswAddCallback(toggle3, IswNcallback, toggle_callback, (IswPointer)"Option C");

    return box;
}

Widget create_checkbox_demo(Widget parent) {
    Widget box, title, cb1, cb2, cb3;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("checkboxBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Toggle (Checkbox) Buttons");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("checkboxTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Standalone toggles (no radioGroup) render as checkboxes */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Enable notifications");
    IswArgState(&ab, True);
    cb1 = IswCreateManagedWidget("cb1", toggleWidgetClass, box, ab.args, ab.count);
    IswAddCallback(cb1, IswNcallback, checkbox_callback, (IswPointer)"Enable notifications");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Dark mode");
    cb2 = IswCreateManagedWidget("cb2", toggleWidgetClass, box, ab.args, ab.count);
    IswAddCallback(cb2, IswNcallback, checkbox_callback, (IswPointer)"Dark mode");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Auto-save");
    cb3 = IswCreateManagedWidget("cb3", toggleWidgetClass, box, ab.args, ab.count);
    IswAddCallback(cb3, IswNcallback, checkbox_callback, (IswPointer)"Auto-save");

    return box;
}

Widget create_menu_demo(Widget parent) {
    Widget box, title, menu_button, menu;
    Widget entry1, entry2, line, entry3;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("menuBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Menu Demo");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("menuTitle", labelWidgetClass, box, ab.args, ab.count);

    /* MenuButton */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "File Menu");
    menu_button = IswCreateManagedWidget("menuButton", menuButtonWidgetClass,
                                        box, ab.args, ab.count);

    /* Create SimpleMenu popup */
    menu = IswCreatePopupShell("fileMenu", simpleMenuWidgetClass,
                              menu_button, NULL, 0);

    /* Menu entries */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Open");
    entry1 = IswCreateManagedWidget("menuOpen", smeBSBObjectClass, menu, ab.args, ab.count);
    IswAddCallback(entry1, IswNcallback, menu_callback, (IswPointer)"Open");

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Save");
    entry2 = IswCreateManagedWidget("menuSave", smeBSBObjectClass, menu, ab.args, ab.count);
    IswAddCallback(entry2, IswNcallback, menu_callback, (IswPointer)"Save");

    /* Separator line */
    line = IswCreateManagedWidget("menuLine", smeLineObjectClass, menu, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Exit");
    entry3 = IswCreateManagedWidget("menuExit", smeBSBObjectClass, menu, ab.args, ab.count);
    IswAddCallback(entry3, IswNcallback, menu_callback, (IswPointer)"Exit");

    return box;
}

Widget create_repeater_demo(Widget parent) {
    Widget box, title, repeater;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("repeaterBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Repeater Button");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("repeaterTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Repeater button - auto-repeats while held */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Hold Me");
    IswArgRepeatDelay(&ab, 500);
    repeater = IswCreateManagedWidget("repeater", repeaterWidgetClass, box, ab.args, ab.count);
    IswAddCallback(repeater, IswNcallback, repeater_callback, NULL);

    return box;
}

/* ============================================================
 * SELECTION WIDGETS SECTION
 * ============================================================ */

Widget create_selection_section(Widget parent) {
    Widget form, section_label, iconview_demo, listview_demo, list_demo, listbox_demo, combobox_demo, text_demo;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Section container */
    IswArgBorderWidth(&ab, 1);
    IswArgDefaultDistance(&ab, 5);
    form = IswCreateManagedWidget("selectionForm", formWidgetClass, parent, ab.args, ab.count);

    /* Section label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Selection Widgets: IconView, ListView, List, ListBox, ComboBox, Text");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    section_label = IswCreateManagedWidget("selectionLabel", labelWidgetClass,
                                          form, ab.args, ab.count);

    /* IconView demo */
    iconview_demo = create_iconview_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, section_label);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(iconview_demo, ab.args, ab.count);

    /* ListView demo (multi-column list) */
    listview_demo = create_listview_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, iconview_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(listview_demo, ab.args, ab.count);

    /* List demo (classic multi-item list) */
    list_demo = create_list_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, listview_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(list_demo, ab.args, ab.count);

    /* ListBox demo (composite row container) */
    listbox_demo = create_listbox_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, list_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(listbox_demo, ab.args, ab.count);

    /* ComboBox demo (dropdown selector) */
    combobox_demo = create_combobox_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, listbox_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(combobox_demo, ab.args, ab.count);

    /* Text demo */
    text_demo = create_text_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, combobox_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(text_demo, ab.args, ab.count);

    return form;
}

Widget create_iconview_demo(Widget parent) {
    Widget viewport, iconview;
    IswArgBuilder ab = IswArgBuilderInit();

    static String iv_labels[] = {
        "New", "Open", "Save", "Cut", "Copy", "Paste",
        "Undo", "Redo", "Find", "Print", "Help", "Exit"
    };
    static String iv_icons[] = {
        /* Reuse toolbar SVGs and add a few more */
        "<svg viewBox='0 0 16 16'><rect x='3' y='1' width='10' height='14' fill='none' stroke='black' stroke-width='1.5'/><line x1='5' y1='5' x2='11' y2='5' stroke='black'/><line x1='5' y1='8' x2='11' y2='8' stroke='black'/></svg>",
        "<svg viewBox='0 0 16 16'><path d='M2 4 L2 14 L14 14 L14 6 L8 6 L7 4 Z' fill='none' stroke='black' stroke-width='1.5'/></svg>",
        "<svg viewBox='0 0 16 16'><rect x='2' y='2' width='12' height='12' fill='none' stroke='black' stroke-width='1.5' rx='1'/><rect x='5' y='2' width='6' height='5' fill='none' stroke='black'/></svg>",
        "<svg viewBox='0 0 16 16'><circle cx='5' cy='12' r='2.5' fill='none' stroke='black' stroke-width='1.2'/><circle cx='11' cy='12' r='2.5' fill='none' stroke='black' stroke-width='1.2'/><line x1='5' y1='10' x2='11' y2='3' stroke='black' stroke-width='1.5'/><line x1='11' y1='10' x2='5' y2='3' stroke='black' stroke-width='1.5'/></svg>",
        "<svg viewBox='0 0 16 16'><rect x='5' y='4' width='8' height='10' fill='none' stroke='black' stroke-width='1.2' rx='1'/><rect x='3' y='2' width='8' height='10' fill='none' stroke='black' stroke-width='1.2' rx='1'/></svg>",
        "<svg viewBox='0 0 16 16'><rect x='3' y='3' width='10' height='11' fill='none' stroke='black' stroke-width='1.2' rx='1'/><rect x='5' y='1' width='6' height='3' fill='none' stroke='black' stroke-width='1.2' rx='1'/></svg>",
        "<svg viewBox='0 0 16 16'><path d='M3 10 A5 5 0 0 1 8 5' fill='none' stroke='black' stroke-width='1.5'/><path d='M3 10 L5 8 M3 10 L5 12' fill='none' stroke='black' stroke-width='1.2'/></svg>",
        "<svg viewBox='0 0 16 16'><path d='M13 10 A5 5 0 0 0 8 5' fill='none' stroke='black' stroke-width='1.5'/><path d='M13 10 L11 8 M13 10 L11 12' fill='none' stroke='black' stroke-width='1.2'/></svg>",
        "<svg viewBox='0 0 16 16'><circle cx='7' cy='7' r='5' fill='none' stroke='black' stroke-width='1.5'/><line x1='11' y1='11' x2='14' y2='14' stroke='black' stroke-width='1.5'/></svg>",
        "<svg viewBox='0 0 16 16'><rect x='3' y='2' width='10' height='12' fill='none' stroke='black' stroke-width='1.2' rx='1'/><line x1='5' y1='5' x2='11' y2='5' stroke='black'/><line x1='5' y1='8' x2='11' y2='8' stroke='black'/><line x1='5' y1='11' x2='8' y2='11' stroke='black'/></svg>",
        "<svg viewBox='0 0 16 16'><circle cx='8' cy='8' r='6' fill='none' stroke='black' stroke-width='1.5'/><line x1='8' y1='5' x2='8' y2='9' stroke='black' stroke-width='1.5'/><circle cx='8' cy='11.5' r='0.8' fill='black'/></svg>",
        "<svg viewBox='0 0 16 16'><path d='M4 4 L8 2 L12 4 L12 9 L8 14 L4 9 Z' fill='none' stroke='black' stroke-width='1.2'/></svg>",
    };

    /* Viewport for scrolling */
    IswArgAllowVert(&ab, True);
    IswArgWidth(&ab, 200);
    IswArgHeight(&ab, 180);
    IswArgBorderWidth(&ab, 1);
    viewport = IswCreateManagedWidget("iconViewport", viewportWidgetClass,
                                      parent, ab.args, ab.count);

    /* IconView inside viewport */
    IswArgBuilderReset(&ab);
    IswArgIconLabels(&ab, iv_labels);
    IswArgIconData(&ab, iv_icons);
    IswArgNumIcons(&ab, IswNumber(iv_labels));
    IswArgIconSize(&ab, 32);
    IswArgWidth(&ab, 200);
    IswArgMultiSelect(&ab, True);
    iconview = IswCreateManagedWidget("iconView", iconViewWidgetClass,
                                      viewport, ab.args, ab.count);
    IswAddCallback(iconview, IswNselectCallback, iconview_callback, NULL);

    return viewport;
}

void listview_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswListViewCallbackData *data = (IswListViewCallbackData *)call_data;
    printf("ListView row=%d col=%d, %d selected:",
           data->row, data->column, data->num_selected);
    for (int i = 0; i < data->num_selected; i++)
        printf(" %d", data->selected[i]);
    printf("\n");
}

/* --- ListView sort support --- */

#define LV_DEMO_ROWS 12
#define LV_DEMO_COLS 3

typedef struct {
    String  cells[LV_DEMO_COLS]; /* Name, Type, Size (display) */
    double  size_bytes;          /* Size in bytes for numeric sort */
} LvDemoRow;

static LvDemoRow lv_demo_rows[LV_DEMO_ROWS];
static String    lv_demo_flat[LV_DEMO_ROWS * LV_DEMO_COLS];

/* Sort context passed through qsort_r / global (qsort has no context arg) */
static int lv_sort_col;
static int lv_sort_ascending;

static double parse_size(const char *s) {
    double val;
    char unit[4] = {0};
    if (sscanf(s, "%lf %3s", &val, unit) < 1)
        return 0.0;
    if (strcmp(unit, "KB") == 0)  return val * 1024.0;
    if (strcmp(unit, "MB") == 0)  return val * 1024.0 * 1024.0;
    if (strcmp(unit, "GB") == 0)  return val * 1024.0 * 1024.0 * 1024.0;
    return val; /* "B" or no unit */
}

static int lv_demo_cmp(const void *a, const void *b) {
    const LvDemoRow *ra = (const LvDemoRow *)a;
    const LvDemoRow *rb = (const LvDemoRow *)b;
    int result;

    if (lv_sort_col == 2) {
        /* Size column: numeric comparison */
        if (ra->size_bytes < rb->size_bytes)      result = -1;
        else if (ra->size_bytes > rb->size_bytes)  result = 1;
        else                                       result = 0;
    } else {
        /* Name or Type: alphabetical (case-insensitive) */
        result = strcasecmp(ra->cells[lv_sort_col], rb->cells[lv_sort_col]);
    }

    return lv_sort_ascending ? result : -result;
}

static void lv_demo_rebuild_flat(void) {
    for (int r = 0; r < LV_DEMO_ROWS; r++)
        for (int c = 0; c < LV_DEMO_COLS; c++)
            lv_demo_flat[r * LV_DEMO_COLS + c] = lv_demo_rows[r].cells[c];
}

void listview_reorder_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswListViewReorderCallbackData *data = (IswListViewReorderCallbackData *)call_data;
    (void)client_data;

    lv_sort_col = data->column;
    lv_sort_ascending = (data->direction == IswListViewSortAscending);

    qsort(lv_demo_rows, LV_DEMO_ROWS, sizeof(LvDemoRow), lv_demo_cmp);
    lv_demo_rebuild_flat();
    IswListViewSetData(w, lv_demo_flat, LV_DEMO_ROWS, LV_DEMO_COLS);
}

Widget create_listview_demo(Widget parent) {
    Widget viewport, listview;
    IswArgBuilder ab = IswArgBuilderInit();

    static IswListViewColumn cols[] = {
        {"Name",    120, 60},
        {"Type",     80, 50},
        {"Size",     70, 40},
    };

    /* Source data — copied into sortable lv_demo_rows[] */
    static String src[][3] = {
        {"report.pdf",  "PDF",      "2.4 MB"},
        {"photo.jpg",   "Image",    "3.1 MB"},
        {"notes.txt",   "Text",     "12 KB"},
        {"backup.tar",  "Archive",  "156 MB"},
        {"slides.pptx", "Slides",   "8.7 MB"},
        {"data.csv",    "CSV",      "45 KB"},
        {"music.mp3",   "Audio",    "4.2 MB"},
        {"video.mp4",   "Video",    "890 MB"},
        {"code.c",      "Source",   "3.5 KB"},
        {"readme.md",   "Markdown", "1.8 KB"},
        {"config.json", "JSON",     "520 B"},
        {"logo.svg",    "SVG",      "6.1 KB"},
    };

    for (int i = 0; i < LV_DEMO_ROWS; i++) {
        for (int j = 0; j < LV_DEMO_COLS; j++)
            lv_demo_rows[i].cells[j] = src[i][j];
        lv_demo_rows[i].size_bytes = parse_size(src[i][2]);
    }
    lv_demo_rebuild_flat();

    /* Viewport for scrolling */
    IswArgAllowVert(&ab, True);
    IswArgWidth(&ab, 300);
    IswArgHeight(&ab, 180);
    IswArgBorderWidth(&ab, 1);
    viewport = IswCreateManagedWidget("listViewViewport", viewportWidgetClass,
                                      parent, ab.args, ab.count);

    /* ListView inside viewport */
    IswArgBuilderReset(&ab);
    IswArgListViewColumns(&ab, cols);
    IswArgNumColumns(&ab, LV_DEMO_COLS);
    IswArgListViewData(&ab, lv_demo_flat);
    IswArgNumRows(&ab, LV_DEMO_ROWS);
    IswArgWidth(&ab, 300);
    IswArgMultiSelect(&ab, True);
    IswArgShowHeader(&ab, True);
    listview = IswCreateManagedWidget("listView", listViewWidgetClass,
                                      viewport, ab.args, ab.count);
    IswAddCallback(listview, IswNselectCallback, listview_callback, NULL);
    IswAddCallback(listview, IswNreorderCallback, listview_reorder_callback, NULL);

    return viewport;
}

Widget create_list_demo(Widget parent) {
    Widget box, title, list;
    IswArgBuilder ab = IswArgBuilderInit();
    static String items[] = {
        "Apple", "Banana", "Cherry", "Date",
        "Elderberry", "Fig", "Grape", "Honeydew",
        "Kiwi", "Lemon", "Mango"
    };

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("listBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "List");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("listTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Classic list widget — full multi-item display */
    IswArgBuilderReset(&ab);
    IswArgList(&ab, items);
    IswArgNumberStrings(&ab, IswNumber(items));
    IswArgDefaultColumns(&ab, 1);
    IswArgForceColumns(&ab, True);
    IswArgWidth(&ab, 150);
    list = IswCreateManagedWidget("list", listWidgetClass, box, ab.args, ab.count);

    IswAddCallback(list, IswNcallback, list_callback, NULL);

    return box;
}

Widget create_listbox_demo(Widget parent) {
    Widget outer_box, title, viewport, listbox;
    IswArgBuilder ab = IswArgBuilderInit();

    static const char *icons[]  = { "\xe2\x9c\x89", "\xe2\x9c\x8f", "\xe2\x9e\xa4",
                                    "\xe2\x9a\xa0", "\xf0\x9f\x97\x91", "\xf0\x9f\x93\xa6" };
    static const char *names[]  = { "Inbox",  "Drafts",  "Sent",
                                    "Spam",   "Trash",   "Archive" };
    static const char *counts[] = { "12", "3", "",  "47", "8", "" };

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    outer_box = IswCreateManagedWidget("listBoxOuterBox", boxWidgetClass,
                                       parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "ListBox");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("listBoxTitle", labelWidgetClass,
                                   outer_box, ab.args, ab.count);

    /* Viewport for scrolling */
    IswArgBuilderReset(&ab);
    IswArgAllowVert(&ab, True);
    IswArgWidth(&ab, 220);
    IswArgHeight(&ab, 160);
    IswArgBorderWidth(&ab, 1);
    viewport = IswCreateManagedWidget("listBoxViewport", viewportWidgetClass,
                                      outer_box, ab.args, ab.count);

    /* ListBox inside viewport */
    IswArgBuilderReset(&ab);
    IswArgSelectionMode(&ab, IswListBoxSelectSingle);
    IswArgRowSpacing(&ab, 1);
    IswArgShowSeparators(&ab, True);
    IswArgBorderWidth(&ab, 0);
    listbox = IswCreateManagedWidget("listBox", listBoxWidgetClass,
                                     viewport, ab.args, ab.count);

    IswAddCallback(listbox, IswNselectCallback,
                   listbox_select_callback, NULL);
    IswAddCallback(listbox, IswNactivateCallback,
                   listbox_activate_callback, NULL);

    /* Each row is a Box with icon + name + count labels */
    for (int i = 0; i < 6; i++) {
        char rname[32];
        snprintf(rname, sizeof(rname), "row%d", i);

        IswArgBuilderReset(&ab);
        IswArgBorderWidth(&ab, 0);
        Widget row = IswCreateManagedWidget(rname, listBoxRowWidgetClass,
                                            listbox, ab.args, ab.count);

        /* Icon */
        IswArgBuilderReset(&ab);
        IswArgLabel(&ab, icons[i]);
        IswArgBorderWidth(&ab, 0);
        IswCreateManagedWidget("icon", labelWidgetClass, row, ab.args, ab.count);

        /* Name */
        IswArgBuilderReset(&ab);
        IswArgLabel(&ab, names[i]);
        IswArgBorderWidth(&ab, 0);
        IswArgJustify(&ab, IswJustifyLeft);
        IswCreateManagedWidget("name", labelWidgetClass, row, ab.args, ab.count);

        /* Count badge (if non-empty) */
        if (counts[i][0] != '\0') {
            IswArgBuilderReset(&ab);
            IswArgLabel(&ab, counts[i]);
            IswArgBorderWidth(&ab, 1);
            IswCreateManagedWidget("count", labelWidgetClass, row, ab.args, ab.count);
        }

        /* "Sent" gets a separator below it; "Spam" is non-selectable */
        if (i == 2) {
            IswArgBuilderReset(&ab);
            IswArgSeparator(&ab, True);
            IswSetValues(row, ab.args, ab.count);
        }
        if (i == 3) {
            IswArgBuilderReset(&ab);
            IswArgSelectable(&ab, False);
            IswSetValues(row, ab.args, ab.count);
        }
    }

    return outer_box;
}

void listbox_select_callback(Widget w, IswPointer client_data,
                             IswPointer call_data)
{
    IswListBoxCallbackData *cb = (IswListBoxCallbackData *)call_data;
    (void)w; (void)client_data;
    printf("ListBox: selected row %d (%d total selected)\n",
           cb->index, cb->num_selected);
}

void listbox_activate_callback(Widget w, IswPointer client_data,
                               IswPointer call_data)
{
    IswListBoxCallbackData *cb = (IswListBoxCallbackData *)call_data;
    (void)w; (void)client_data;
    printf("ListBox: activated row %d\n", cb->index);
}

Widget create_combobox_demo(Widget parent) {
    Widget box, title, combo;
    IswArgBuilder ab = IswArgBuilderInit();
    static String items[] = {
        "Red", "Orange", "Yellow", "Green",
        "Blue", "Indigo", "Violet"
    };

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("comboBoxBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "ComboBox");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("comboBoxTitle", labelWidgetClass, box, ab.args, ab.count);

    /* ComboBox — List subclass with dropdown default */
    IswArgBuilderReset(&ab);
    IswArgList(&ab, items);
    IswArgNumberStrings(&ab, IswNumber(items));
    IswArgWidth(&ab, 150);
    combo = IswCreateManagedWidget("comboBox", comboBoxWidgetClass, box, ab.args, ab.count);

    IswAddCallback(combo, IswNcallback, combobox_callback, NULL);

    return box;
}

Widget create_text_demo(Widget parent) {
    Widget box, title, text;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("textBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Text Widget (Editable)");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("textTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Editable text widget with scrollbars */
    IswArgBuilderReset(&ab);
    IswArgEditType(&ab, IswtextEdit);
    IswArgWidth(&ab, 450);
    IswArgHeight(&ab, 120);
    IswArgScrollVertical(&ab, IswtextScrollAlways);
    IswArgString(&ab,
             "This is an editable text widget with scrollbars.\n"
             "Line 2: You can type, edit, and select text here.\n"
             "Line 3: The scrollbar should now work correctly!\n"
             "Line 4: This text extends beyond the visible area.\n"
             "Line 5: Scroll down to see more lines.\n"
             "Line 6: The thumb position should be accurate.\n"
             "Line 7: Scrolling should be smooth and predictable.\n"
             "Line 8: No more jumping or erratic behavior!\n"
             "Line 9: The fix clamps line table positions.\n"
             "Line 10: Sentinel values are handled correctly.\n"
             "Line 11: XCB port is working great!\n"
             "Line 12: Test the scrollbar by dragging it.\n"
             "Line 13: Or use mouse wheel to scroll.\n"
             "Line 14: The text widget is now fully functional.\n"
             "Line 15: End of demo text.");
    text = IswCreateManagedWidget("textEditor", textWidgetClass, box, ab.args, ab.count);

    return box;
}

/* ============================================================
 * NAVIGATION WIDGETS SECTION
 * ============================================================ */

Widget create_navigation_section(Widget parent) {
    Widget form, section_label;
    Widget panner_demo;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Section container */
    IswArgBorderWidth(&ab, 1);
    IswArgDefaultDistance(&ab, 5);
    form = IswCreateManagedWidget("navigationForm", formWidgetClass,
                                 parent, ab.args, ab.count);

    /* Section label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Navigation Widgets: Panner/Porthole");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    section_label = IswCreateManagedWidget("navigationLabel", labelWidgetClass,
                                          form, ab.args, ab.count);

    /* Create demos */
    panner_demo = create_panner_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, section_label);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(panner_demo, ab.args, ab.count);

    return form;
}

/* Panner report callback: user dragged the panner slider, move porthole content */
void panner_report_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswPannerReport *report = (IswPannerReport *)call_data;
    Widget content = (Widget)client_data;
    IswArgBuilder ab = IswArgBuilderInit();

    if (report->changed & IswPRSliderX) {
        IswArgX(&ab, -report->slider_x);
    }
    if (report->changed & IswPRSliderY) {
        IswArgY(&ab, -report->slider_y);
    }
    if (ab.count > 0)
        IswSetValues(content, ab.args, ab.count);
}

/* Porthole report callback: porthole moved its child, update panner slider */
void porthole_report_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswPannerReport *report = (IswPannerReport *)call_data;
    Widget panner = (Widget)client_data;
    IswArgBuilder ab = IswArgBuilderInit();

    IswArgSliderX(&ab, report->slider_x);
    IswArgSliderY(&ab, report->slider_y);
    IswArgSliderWidth(&ab, report->slider_width);
    IswArgSliderHeight(&ab, report->slider_height);
    IswArgCanvasWidth(&ab, report->canvas_width);
    IswArgCanvasHeight(&ab, report->canvas_height);
    IswSetValues(panner, ab.args, ab.count);
}

Widget create_panner_demo(Widget parent) {
    Widget box, title, panner, porthole, large_widget;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("pannerBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Panner/Porthole");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("pannerTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Panner widget (miniature navigator) */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 200);
    IswArgHeight(&ab, 150);
    panner = IswCreateManagedWidget("panner", pannerWidgetClass, box, ab.args, ab.count);

    /* Porthole (viewing area) */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 200);
    IswArgHeight(&ab, 150);
    porthole = IswCreateManagedWidget("porthole", portholeWidgetClass, box, ab.args, ab.count);

    /* Large widget inside porthole */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Large Content Area\n\n"\
             "This area is larger than the\n"\
             "visible porthole window.\n\n"\
             "Use the panner above to\n"\
             "navigate around this content.");
    IswArgWidth(&ab, 400);
    IswArgHeight(&ab, 300);
    large_widget = IswCreateManagedWidget("pannerContent", labelWidgetClass,
                                         porthole, ab.args, ab.count);

    /* Wire panner and porthole together */
    IswAddCallback(panner, IswNreportCallback, panner_report_callback,
                  (IswPointer)large_widget);
    IswAddCallback(porthole, IswNreportCallback, porthole_report_callback,
                  (IswPointer)panner);

    return box;
}

/* ============================================================
 * TREE DEMO SECTION
 * ============================================================ */

Widget create_tree_demo(Widget parent) {
    Widget box, title, tree;
    Widget node1, node2, node3, node4, node5;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("treeBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Tree Widget (Hierarchical Structure)");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("treeTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Tree widget */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 300);
    IswArgHeight(&ab, 200);
    IswArgAutoReconfigure(&ab, True);
    IswArgHSpace(&ab, 20);
    IswArgVSpace(&ab, 10);
    tree = IswCreateManagedWidget("tree", treeWidgetClass, box, ab.args, ab.count);

    /* Root node */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Root");
    node1 = IswCreateManagedWidget("node1", commandWidgetClass, tree, ab.args, ab.count);

    /* Child nodes of root */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Child 1");
    IswArgTreeParent(&ab, node1);
    node2 = IswCreateManagedWidget("node2", commandWidgetClass, tree, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Child 2");
    IswArgTreeParent(&ab, node1);
    node3 = IswCreateManagedWidget("node3", commandWidgetClass, tree, ab.args, ab.count);

    /* Grandchild nodes */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Grandchild 1");
    IswArgTreeParent(&ab, node2);
    node4 = IswCreateManagedWidget("node4", commandWidgetClass, tree, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Grandchild 2");
    IswArgTreeParent(&ab, node3);
    node5 = IswCreateManagedWidget("node5", commandWidgetClass, tree, ab.args, ab.count);

    return box;
}

/* ============================================================
 * LAYOUT DEMO SECTION
 * ============================================================ */

Widget create_layout_demo(Widget parent) {
    Widget box, title, layout, button1, button2, button3;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("layoutBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Layout Widget (Constraint-based)");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("layoutTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Use Form widget to demonstrate constraint-based layout.
     * Form positions children by fromHoriz/fromVert and distances;
     * chain constraints control how they move on resize. We place a
     * hidden spacer to push "Top Right" to the right edge, and
     * compute an offset for "Bottom Center". */

    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 300);
    IswArgHeight(&ab, 120);
    IswArgBorderWidth(&ab, 1);
    IswArgDefaultDistance(&ab, 8);
    layout = IswCreateManagedWidget("layout", formWidgetClass, box, ab.args, ab.count);

    /* Top Left: pinned to top-left */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Top Left");
    IswArgTop(&ab, IswChainTop);
    IswArgBottom(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainLeft);
    button1 = IswCreateManagedWidget("layoutBtn1", commandWidgetClass, layout, ab.args, ab.count);

    /* Top Right: pushed to right side via horizDistance */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Top Right");
    IswArgFromHoriz(&ab, button1);
    IswArgHorizDistance(&ab, 100);
    IswArgTop(&ab, IswChainTop);
    IswArgBottom(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainRight);
    IswArgRight(&ab, IswChainRight);
    button2 = IswCreateManagedWidget("layoutBtn2", commandWidgetClass, layout, ab.args, ab.count);

    /* Bottom Center: below button1, centered via horizDistance */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Bottom Center");
    IswArgFromVert(&ab, button1);
    IswArgHorizDistance(&ab, 80);
    IswArgTop(&ab, IswChainBottom);
    IswArgBottom(&ab, IswChainBottom);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainLeft);
    button3 = IswCreateManagedWidget("layoutBtn3", commandWidgetClass, layout, ab.args, ab.count);

    return box;
}

/* ============================================================
 * GRIP DEMO SECTION (Enhanced Paned)
 * ============================================================ */

Widget create_paned_grip_demo(Widget parent) {
    Widget box, title, paned, grip1, grip2;
    Widget section1, section2, section3;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("gripBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Grip Widget (Pane Resizing)");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("gripTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Paned widget with visible grips */
    IswArgBuilderReset(&ab);
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgWidth(&ab, 200);
    IswArgHeight(&ab, 200);
    paned = IswCreateManagedWidget("gripPaned", panedWidgetClass, box, ab.args, ab.count);

    /* First section */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Section 1\n(Drag grip to resize)");
    IswArgMin(&ab, 30);
    IswArgMax(&ab, 150);
    IswArgShowGrip(&ab, True);
    section1 = IswCreateManagedWidget("section1", labelWidgetClass, paned, ab.args, ab.count);

    /* Grip is automatically created between panes */

    /* Second section */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Section 2");
    IswArgMin(&ab, 30);
    IswArgMax(&ab, 150);
    IswArgShowGrip(&ab, True);
    section2 = IswCreateManagedWidget("section2", labelWidgetClass, paned, ab.args, ab.count);

    /* Third section */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Section 3");
    IswArgMin(&ab, 30);
    IswArgSkipAdjust(&ab, True);
    section3 = IswCreateManagedWidget("section3", labelWidgetClass, paned, ab.args, ab.count);

    return box;
}

/* ============================================================
 * SPECIALIZED WIDGETS SECTION
 * ============================================================ */

Widget create_specialized_section(Widget parent) {
    Widget form, section_label;
    Widget spinbox_demo, slider_demo, scrollbar_demo, progressbar_demo, dialog_demo, colorpicker_demo, fontchooser_demo, drawingarea_demo;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Section container */
    IswArgBorderWidth(&ab, 1);
    IswArgDefaultDistance(&ab, 5);
    form = IswCreateManagedWidget("specializedForm", formWidgetClass,
                                 parent, ab.args, ab.count);

    /* Section label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Specialized Widgets: SpinBox, Slider, Scrollbar, ProgressBar, Dialog");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    section_label = IswCreateManagedWidget("specializedLabel", labelWidgetClass,
                                          form, ab.args, ab.count);

    /* Create demos */
    spinbox_demo = create_spinbox_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, section_label);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(spinbox_demo, ab.args, ab.count);

    slider_demo = create_slider_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, spinbox_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(slider_demo, ab.args, ab.count);

    scrollbar_demo = create_scrollbar_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, slider_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(scrollbar_demo, ab.args, ab.count);

    progressbar_demo = create_progressbar_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, scrollbar_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(progressbar_demo, ab.args, ab.count);

    dialog_demo = create_dialog_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, progressbar_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(dialog_demo, ab.args, ab.count);

    colorpicker_demo = create_colorpicker_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, dialog_demo);
    IswArgFromVert(&ab, section_label);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(colorpicker_demo, ab.args, ab.count);

    fontchooser_demo = create_fontchooser_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, slider_demo);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswSetValues(fontchooser_demo, ab.args, ab.count);

    drawingarea_demo = create_drawingarea_demo(form);
    IswArgBuilderReset(&ab);
    IswArgFromHoriz(&ab, fontchooser_demo);
    IswArgFromVert(&ab, slider_demo);
    IswArgHorizDistance(&ab, 10);
    IswSetValues(drawingarea_demo, ab.args, ab.count);

    /* Drop target demo — receives drops from any XDND app */
    Widget drop_label;
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Drop files here");
    IswArgWidth(&ab, 200);
    IswArgHeight(&ab, 40);
    IswArgBorderWidth(&ab, 1);
    IswArgResize(&ab, False);
    IswArgFromVert(&ab, fontchooser_demo);
    IswArgLeft(&ab, IswChainLeft);
    drop_label = IswCreateManagedWidget("dropTarget", labelWidgetClass,
                                        form, ab.args, ab.count);
    IswAddCallback(drop_label, IswNdropCallback, drop_callback, NULL);
    IswAddCallback(drop_label, IswNdragEnterCallback, drag_enter_callback, NULL);
    IswAddCallback(drop_label, IswNdragLeaveCallback, drag_leave_callback, NULL);
    ISWXdndWidgetAcceptDrops(drop_label);

    /* Drag source demo — drag text to any XDND app */
    static IswActionsRec drag_actions[] = {
        {"drag-start", drag_start_action}
    };
    IswAppAddActions(IswWidgetToApplicationContext(form),
                    drag_actions, IswNumber(drag_actions));

    Widget drag_label;
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Drag me");
    IswArgWidth(&ab, 100);
    IswArgHeight(&ab, 40);
    IswArgBorderWidth(&ab, 1);
    IswArgFromVert(&ab, fontchooser_demo);
    IswArgFromHoriz(&ab, drop_label);
    IswArgHorizDistance(&ab, 10);
    IswArgLeft(&ab, IswChainLeft);
    drag_label = IswCreateManagedWidget("dragSource", labelWidgetClass,
                                        form, ab.args, ab.count);

    /* Override translations so button press starts a drag */
    IswOverrideTranslations(drag_label,
        IswParseTranslationTable("<BtnDown>: drag-start()"));

    return form;
}

Widget create_progressbar_demo(Widget parent) {
    Widget box, title, pb_h1, pb_h2, pb_v;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("progressBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "ProgressBar Widget");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("progressTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Horizontal progress bar at 75% with text */
    IswArgBuilderReset(&ab);
    IswArgValue(&ab, 75);
    IswArgWidth(&ab, 180);
    IswArgHeight(&ab, 24);
    IswArgShowValue(&ab, True);
    pb_h1 = IswCreateManagedWidget("progressH1", progressBarWidgetClass, box, ab.args, ab.count);

    /* Horizontal progress bar at 30% without text */
    IswArgBuilderReset(&ab);
    IswArgValue(&ab, 30);
    IswArgWidth(&ab, 180);
    IswArgHeight(&ab, 18);
    IswArgShowValue(&ab, False);
    pb_h2 = IswCreateManagedWidget("progressH2", progressBarWidgetClass, box, ab.args, ab.count);

    /* Vertical progress bar at 60% with text */
    IswArgBuilderReset(&ab);
    IswArgValue(&ab, 60);
    IswArgWidth(&ab, 30);
    IswArgHeight(&ab, 100);
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgShowValue(&ab, True);
    pb_v = IswCreateManagedWidget("progressV", progressBarWidgetClass, box, ab.args, ab.count);

    return box;
}

Widget create_fontchooser_demo(Widget parent) {
    Widget box, title, chooser;
    IswArgBuilder ab = IswArgBuilderInit();

    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("fontChooserBox", boxWidgetClass, parent, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Font Chooser");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("fontChooserTitle", labelWidgetClass, box, ab.args, ab.count);

    chooser = IswCreateManagedWidget("fontChooser", fontChooserWidgetClass, box, NULL, 0);
    IswAddCallback(chooser, IswNfontChanged, fontchooser_callback, NULL);

    return box;
}

Widget create_colorpicker_demo(Widget parent) {
    Widget box, title, picker;
    IswArgBuilder ab = IswArgBuilderInit();

    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("colorPickerBox", boxWidgetClass, parent, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Color Picker");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("colorPickerTitle", labelWidgetClass, box, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgColorRed(&ab, 128);
    IswArgColorGreen(&ab, 64);
    IswArgColorBlue(&ab, 192);
    picker = IswCreateManagedWidget("colorPicker", colorPickerWidgetClass, box, ab.args, ab.count);
    IswAddCallback(picker, IswNcolorChanged, colorpicker_callback, NULL);

    return box;
}

Widget create_spinbox_demo(Widget parent) {
    Widget box, title, spin1, spin2;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("spinBoxBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "SpinBox");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("spinBoxTitle", labelWidgetClass, box, ab.args, ab.count);

    /* SpinBox: 0-100, increment 1 */
    IswArgBuilderReset(&ab);
    IswArgSpinMinimum(&ab, 0);
    IswArgSpinMaximum(&ab, 100);
    IswArgSpinValue(&ab, 50);
    IswArgSpinIncrement(&ab, 1);
    IswArgWidth(&ab, 120);
    spin1 = IswCreateManagedWidget("spin1", spinBoxWidgetClass, box, ab.args, ab.count);
    IswAddCallback(spin1, IswNvalueChanged, spinbox_callback, (IswPointer)"Spin1");

    /* SpinBox: wrapping, step 10 */
    IswArgBuilderReset(&ab);
    IswArgSpinMinimum(&ab, 0);
    IswArgSpinMaximum(&ab, 255);
    IswArgSpinValue(&ab, 128);
    IswArgSpinIncrement(&ab, 10);
    IswArgSpinWrap(&ab, True);
    IswArgWidth(&ab, 120);
    spin2 = IswCreateManagedWidget("spin2", spinBoxWidgetClass, box, ab.args, ab.count);
    IswAddCallback(spin2, IswNvalueChanged, spinbox_callback, (IswPointer)"Spin2");

    return box;
}

Widget create_slider_demo(Widget parent) {
    Widget box, title, slider_h, slider_v;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("sliderBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Slider");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("sliderTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Horizontal slider with ticks */
    IswArgBuilderReset(&ab);
    IswArgOrientation(&ab, IswOrientHorizontal);
    IswArgMinimumValue(&ab, 0);
    IswArgMaximumValue(&ab, 100);
    IswArgSliderValue(&ab, 50);
    IswArgTickInterval(&ab, 25);
    IswArgShowValue(&ab, True);
    IswArgWidth(&ab, 200);
    IswArgHeight(&ab, 50);
    slider_h = IswCreateManagedWidget("sliderH", sliderWidgetClass, box, ab.args, ab.count);
    IswAddCallback(slider_h, IswNvalueChanged, slider_callback, (IswPointer)"Horizontal");

    /* Vertical slider */
    IswArgBuilderReset(&ab);
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgMinimumValue(&ab, 0);
    IswArgMaximumValue(&ab, 255);
    IswArgSliderValue(&ab, 128);
    IswArgShowValue(&ab, True);
    IswArgWidth(&ab, 70);
    IswArgHeight(&ab, 120);
    slider_v = IswCreateManagedWidget("sliderV", sliderWidgetClass, box, ab.args, ab.count);
    IswAddCallback(slider_v, IswNvalueChanged, slider_callback, (IswPointer)"Vertical");

    return box;
}

Widget create_scrollbar_demo(Widget parent) {
    Widget box, title, scrollbar;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("scrollbarBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Scrollbar Widget");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("scrollbarTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Vertical scrollbar */
    IswArgBuilderReset(&ab);
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgWidth(&ab, 20);
    IswArgHeight(&ab, 100);
    IswArgShown(&ab, 30);
    scrollbar = IswCreateManagedWidget("scrollbar", scrollbarWidgetClass,
                                      box, ab.args, ab.count);

    return box;
}

Widget create_dialog_demo(Widget parent) {
    Widget box, title, dialog;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("dialogBox", boxWidgetClass, parent, ab.args, ab.count);

    /* Title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Dialog Widget");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("dialogTitle", labelWidgetClass, box, ab.args, ab.count);

    /* Dialog widget */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Enter name:");
    IswArgValue(&ab, "John Doe");
    dialog = IswCreateManagedWidget("dialog", dialogWidgetClass, box, ab.args, ab.count);
    
    /* Add buttons */
    IswDialogAddButton(dialog, "OK", dialog_ok_callback, (IswPointer)dialog);
    IswDialogAddButton(dialog, "Cancel", NULL, NULL);

    /* A button that launches a real modal transient popup — for testing
     * that the focus manager runs Tab traversal inside the popup shell
     * and doesn't leak to the parent shell. */
    {
        Widget open_btn;
        IswArgBuilderReset(&ab);
        IswArgLabel(&ab, "Open Modal Dialog...");
        open_btn = IswCreateManagedWidget("openModal", commandWidgetClass,
                                          box, ab.args, ab.count);
        IswAddCallback(open_btn, IswNcallback, open_modal_dialog_cb,
                       (IswPointer)parent);
    }

    return box;
}

static void
modal_close_cb(Widget w, IswPointer client_data, IswPointer call_data)
{
    Widget shell = (Widget) client_data;
    (void)w; (void)call_data;
    IswPopdown(shell);
    IswDestroyWidget(shell);
    printf("Modal dialog closed\n");
}

void open_modal_dialog_cb(Widget w, IswPointer client_data, IswPointer call_data)
{
    Widget parent = (Widget) client_data;
    Widget shell, form, label, text, ok, cancel;
    IswArgBuilder ab = IswArgBuilderInit();
    (void)w; (void)call_data;

    /* TransientShell so it gets WM decorations + stays above parent. */
    IswArgTitle(&ab, "Modal Dialog");
    IswArgWidth(&ab, 360);
    IswArgHeight(&ab, 140);
    shell = IswCreatePopupShell("modalTest", transientShellWidgetClass,
                                parent, ab.args, ab.count);

    form = IswCreateManagedWidget("form", formWidgetClass, shell, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Type here, then Tab to cycle:");
    IswArgBorderWidth(&ab, 0);
    label = IswCreateManagedWidget("lbl", labelWidgetClass, form, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, label);
    IswArgEditType(&ab, IswtextEdit);
    IswArgWidth(&ab, 300);
    IswArgString(&ab, "");
    IswArgConsumeTab(&ab, False);
    text = IswCreateManagedWidget("entry", textWidgetClass, form, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, text);
    IswArgLabel(&ab, "OK");
    ok = IswCreateManagedWidget("ok", commandWidgetClass, form, ab.args, ab.count);
    IswAddCallback(ok, IswNcallback, modal_close_cb, (IswPointer)shell);

    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, text);
    IswArgFromHoriz(&ab, ok);
    IswArgLabel(&ab, "Cancel");
    cancel = IswCreateManagedWidget("cancel", commandWidgetClass, form, ab.args, ab.count);
    IswAddCallback(cancel, IswNcallback, modal_close_cb, (IswPointer)shell);

    (void)text;
    IswPopup(shell, IswGrabExclusive);
}

/* ============================================================
 * CALLBACK FUNCTIONS
 * ============================================================ */

void button_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    char *button_name = (char *)client_data;
    printf("Button activated: %s\n", button_name);
}

void toggle_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    char *toggle_name = (char *)client_data;
    Boolean state = (Boolean)(intptr_t)call_data;
    printf("Toggle %s: %s\n", toggle_name, state ? "ON" : "OFF");
}

void checkbox_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    char *label = (char *)client_data;
    Boolean state = (Boolean)(intptr_t)call_data;
    printf("Checkbox '%s': %s\n", label, state ? "checked" : "unchecked");
}

void menu_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    char *item = (char *)client_data;
    printf("Menu item selected: %s\n", item);
    
    if (strcmp(item, "Exit") == 0) {
        printf("Exiting via menu...\n");
        exit(0);
    }
}

void list_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswListReturnStruct *item = (IswListReturnStruct *)call_data;
    printf("List item selected: %s (index %d)\n",
           item->string, item->list_index);
}

void slider_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswSliderCallbackData *data = (IswSliderCallbackData *)call_data;
    printf("Slider (%s) value: %d\n", (char *)client_data, data->value);
}

void fontchooser_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswFontChooserCallbackData *data = (IswFontChooserCallbackData *)call_data;
    printf("Font: %s %dpt\n", data->family ? data->family : "(null)", data->size);
}

void colorpicker_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswColorPickerCallbackData *data = (IswColorPickerCallbackData *)call_data;
    printf("Color: R=%d G=%d B=%d\n", data->red, data->green, data->blue);
}

void spinbox_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswSpinBoxCallbackData *data = (IswSpinBoxCallbackData *)call_data;
    printf("SpinBox (%s) value: %d\n", (char *)client_data, data->value);
}

void iconview_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswIconViewCallbackData *data = (IswIconViewCallbackData *)call_data;
    (void)w; (void)client_data;
    printf("IconView clicked: %s (index %d), %d selected:",
           data->label ? data->label : "(null)", data->index,
           data->num_selected);
    for (int i = 0; i < data->num_selected; i++)
        printf(" %d", data->selected[i]);
    printf("\n");
}

void combobox_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswListReturnStruct *item = (IswListReturnStruct *)call_data;
    printf("ComboBox selected: %s (index %d)\n",
           item->string, item->list_index);
}

void drop_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswDropCallbackData *data = (IswDropCallbackData *)call_data;
    (void) client_data;

    if (data->num_uris > 0) {
        printf("Drop received: %d file(s) at (%d, %d)\n",
               data->num_uris, data->x, data->y);
        for (int i = 0; i < data->num_uris; i++)
            printf("  [%d] %s\n", i, data->uris[i]);

        /* Update the label to show the first dropped file */
        IswArgBuilder ab = IswArgBuilderInit();
        IswArgLabel(&ab, data->uris[0]);
        IswSetValues(w, ab.args, ab.count);
    } else if (data->data && data->data_length > 0) {
        /* Non-URI drop — show raw text data */
        printf("Drop received: %lu bytes at (%d, %d)\n",
               data->data_length, data->x, data->y);
        char *text = IswMalloc(data->data_length + 1);
        memcpy(text, data->data, data->data_length);
        text[data->data_length] = '\0';
        printf("  data: %s\n", text);

        IswArgBuilder ab = IswArgBuilderInit();
        IswArgLabel(&ab, text);
        IswSetValues(w, ab.args, ab.count);
        IswFree(text);
    }
}

void drag_enter_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    (void) client_data;
    (void) call_data;
    Pixel highlight;
    /* Use a simple visual cue — swap border width */
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgBorderWidth(&ab, 3);
    IswSetValues(w, ab.args, ab.count);
    (void) highlight;
}

void drag_leave_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    (void) client_data;
    (void) call_data;
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgBorderWidth(&ab, 1);
    IswSetValues(w, ab.args, ab.count);
}

/* Drag source convert proc — provides text/plain data */
static Boolean
demo_drag_convert(Widget widget, xcb_atom_t target_type,
                  IswPointer *data_return, unsigned long *length_return,
                  int *format_return, IswPointer client_data)
{
    (void) widget;
    (void) client_data;

    /* Check if the target is text/plain */
    xcb_atom_t text_plain = ISWXdndInternType(widget, "text/plain");
    xcb_atom_t text_uri = ISWXdndInternType(widget, "text/uri-list");

    if (target_type == text_plain) {
        const char *msg = "Hello from ISW drag source!";
        int len = strlen(msg);
        char *copy = IswMalloc(len + 1);
        memcpy(copy, msg, len + 1);
        *data_return = copy;
        *length_return = len;
        *format_return = 8;
        return True;
    }

    if (target_type == text_uri) {
        const char *uri = "file:///tmp/isw-demo-drag\r\n";
        int len = strlen(uri);
        char *copy = IswMalloc(len + 1);
        memcpy(copy, uri, len + 1);
        *data_return = copy;
        *length_return = len;
        *format_return = 8;
        return True;
    }

    return False;
}

static void
demo_drag_finished(Widget widget, IswDndAction performed_action,
                   Boolean accepted, IswPointer client_data)
{
    (void) widget;
    (void) client_data;
    printf("Drag finished: %s (action=%d)\n",
           accepted ? "accepted" : "rejected", performed_action);
}

void drag_start_action(Widget w, xcb_generic_event_t *event,
                       String *params, Cardinal *num_params)
{
    (void) params;
    (void) num_params;

    xcb_atom_t types[2];
    types[0] = ISWXdndInternType(w, "text/plain");
    types[1] = ISWXdndInternType(w, "text/uri-list");

    IswDragSourceDesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.types = types;
    desc.num_types = 2;
    desc.actions = ISW_DND_ACTION_COPY | ISW_DND_ACTION_MOVE;
    desc.convert = demo_drag_convert;
    desc.finished = demo_drag_finished;
    desc.client_data = NULL;

    ISWXdndStartDrag(w, (xcb_button_press_event_t *) event, &desc);
}


void repeater_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    static int count = 0;
    printf("Repeater activated: count = %d\n", ++count);
}

void dialog_ok_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    Widget dialog = (Widget)client_data;
    String value;
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgValue(&ab, (IswArgVal)&value);
    IswGetValues(dialog, ab.args, ab.count);
    
    printf("Dialog OK pressed. Value: %s\n", value);
}

void quit_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    printf("Quit button pressed. Exiting...\n");
    exit(0);
}

/* ============================================================
 * MENU BAR CALLBACKS
 * ============================================================ */

void file_menu_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    char *item = (char *)client_data;
    printf("File menu item selected: %s\n", item);
    
    if (strcmp(item, "Quit") == 0) {
        quit_callback(w, NULL, NULL);
    }
}

void edit_menu_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    char *item = (char *)client_data;
    printf("Edit menu item selected: %s\n", item);
}

void about_menu_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    char *item = (char *)client_data;
    
    if (strcmp(item, "About") == 0) {
        printf("ISW3D Widget Demo\n");
        printf("Version: 1.0\n");
        printf("Backend: Cairo-XCB\n");
        printf("Demonstrating comprehensive ISW widget library\n");
    } else if (strcmp(item, "License") == 0) {
        printf("License: MIT/X Consortium License\n");
    } else {
        printf("About menu item selected: %s\n", item);
    }
}

/* ============================================================
 * TIMER CALLBACKS
 * ============================================================ */


/* ============================================================
 * TIP (TOOLTIP) HELPER FUNCTIONS
 * ============================================================ */

void attach_tooltip(Widget widget, const char *tip_text) {
    IswTipEnable(widget, (String)tip_text);
}

/* ============================================================
 * TABS WIDGET DEMO
 * ============================================================ */

void tabs_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    TabsCallbackStruct *cbs = (TabsCallbackStruct *)call_data;
    printf("Tab switched to index %d (widget: %s)\n",
           cbs->tab_index, IswName(cbs->child));
}

/* ============================================================
 * DRAWING AREA DEMO - Procedural checkerboard
 * ============================================================ */

void drawingarea_expose(Widget w, IswPointer client_data, IswPointer call_data) {
    ISWDrawingCallbackData *cb = (ISWDrawingCallbackData *)call_data;
    ISWRenderContext *ctx = cb->render_ctx;
    Dimension width, height;
    int cell_size = 20;
    int row, col;

    IswVaGetValues(w, IswNwidth, &width, IswNheight, &height, NULL);

    /* Clear to white */
    ISWRenderSetColorRGBA(ctx, 1.0, 1.0, 1.0, 1.0);
    ISWRenderFillRectangle(ctx, 0, 0, width, height);

    /* Draw checkerboard */
    for (row = 0; row * cell_size < (int)height; row++) {
	for (col = 0; col * cell_size < (int)width; col++) {
	    if ((row + col) % 2 == 0) {
		ISWRenderSetColorRGBA(ctx, 0.2, 0.2, 0.6, 1.0);
	    } else {
		ISWRenderSetColorRGBA(ctx, 0.85, 0.85, 0.9, 1.0);
	    }
	    ISWRenderFillRectangle(ctx,
				   col * cell_size, row * cell_size,
				   cell_size, cell_size);
	}
    }
}

Widget create_drawingarea_demo(Widget parent) {
    Widget box, title, canvas;
    IswArgBuilder ab = IswArgBuilderInit();

    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    box = IswCreateManagedWidget("drawingAreaBox", boxWidgetClass, parent, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "DrawingArea Widget");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("drawingAreaTitle", labelWidgetClass, box, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 160);
    IswArgHeight(&ab, 160);
    IswArgBorderWidth(&ab, 1);
    canvas = IswCreateManagedWidget("canvas", drawingAreaWidgetClass, box, ab.args, ab.count);

    IswAddCallback(canvas, IswNexposeCallback, drawingarea_expose, NULL);

    return box;
}

Widget create_tabs_demo(Widget parent) {
    Widget section_box, title, tabs_widget;
    Widget tab1_content, tab2_content;
    IswArgBuilder ab = IswArgBuilderInit();

    /* Section container */
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 1);
    section_box = IswCreateManagedWidget("tabsSection", boxWidgetClass,
                                         parent, ab.args, ab.count);

    /* Section title */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Tabs Widget");
    IswArgBorderWidth(&ab, 0);
    title = IswCreateManagedWidget("tabsTitle", labelWidgetClass,
                                   section_box, ab.args, ab.count);

    /* The Tabs widget */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 400);
    IswArgHeight(&ab, 180);
    IswArgBorderWidth(&ab, 0);
    tabs_widget = IswCreateManagedWidget("tabs", tabsWidgetClass,
                                         section_box, ab.args, ab.count);
    IswAddCallback(tabs_widget, IswNtabCallback, tabs_callback, NULL);

    /* Tab 1: a label */
    IswArgBuilderReset(&ab);
    IswArgTabLabel(&ab, "General");
    IswArgLabel(&ab, "This is the General tab.\n\n"
             "The Tabs widget is a Constraint\n"
             "container that shows one child at\n"
             "a time, with a clickable tab bar.");
    IswArgBorderWidth(&ab, 0);
    tab1_content = IswCreateManagedWidget("tab1", labelWidgetClass,
                                          tabs_widget, ab.args, ab.count);

    /* Tab 2: a box with buttons */
    IswArgBuilderReset(&ab);
    IswArgTabLabel(&ab, "Controls");
    IswArgOrientation(&ab, IswOrientVertical);
    IswArgBorderWidth(&ab, 0);
    tab2_content = IswCreateManagedWidget("tab2", boxWidgetClass,
                                          tabs_widget, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Button A");
    IswCreateManagedWidget("tab2BtnA", commandWidgetClass,
                           tab2_content, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Button B");
    IswCreateManagedWidget("tab2BtnB", commandWidgetClass,
                           tab2_content, ab.args, ab.count);

    return section_box;
}
