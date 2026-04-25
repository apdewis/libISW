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
    Arg args[10];
    Cardinal n;
    
    /* Initialize X Toolkit with XCB backend */
    toplevel = IswAppInitialize(&app_context, "Isw3dDemo",
                               NULL, 0,
                               &argc, argv,
                               NULL, NULL, 0);
    

    /* Set main window size — not scaled, so it fits the screen at any DPI */
    n = 0;
    IswSetArg(args[n], IswNwidth, 1200); n++;
    IswSetArg(args[n], IswNheight, 900); n++;
    IswSetArg(args[n], IswNtitle, "Isw3d Widget Demonstration - Comprehensive Widget Showcase"); n++;
    IswSetArg(args[n], IswNallowShellResize, True); n++;
    IswSetValues(toplevel, args, n);
    
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
    Arg args[10];
    Cardinal n;

    /* MainWindow as direct shell child — menubar fixed at top */
    n = 0;
    IswSetArg(args[n], IswNwidth, 1200); n++;
    IswSetArg(args[n], IswNheight, 900); n++;
    main_win = IswCreateManagedWidget("mainWindow", mainWindowWidgetClass,
                                      parent, args, n);

    /* Populate the built-in menubar */
    populate_menubar(IswMainWindowGetMenuBar(main_win));

    /* Status bar at bottom — MainWindow auto-detects StatusBar children */
    {
        Widget statusbar, sb_label;
        Arg sb_args[4];
        Cardinal sn;

        sn = 0;
        statusbar = IswCreateManagedWidget("statusbar", statusBarWidgetClass,
                                           main_win, sb_args, sn);

        sn = 0;
        IswSetArg(sb_args[sn], IswNlabel, "Ready"); sn++;
        IswSetArg(sb_args[sn], IswNstatusStretch, True); sn++;
        sb_label = IswCreateManagedWidget("statusText", labelWidgetClass,
                                          statusbar, sb_args, sn);

        sn = 0;
        IswSetArg(sb_args[sn], IswNlabel, "Ln 1, Col 1"); sn++;
        IswCreateManagedWidget("statusPos", labelWidgetClass,
                               statusbar, sb_args, sn);
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
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    content_box = IswCreateManagedWidget("contentBox", boxWidgetClass,
                                         viewport, args, n);

    /* Title section */
    title = create_title_label(content_box);

    /* Widget demonstration sections */
    create_containers_section(content_box);
    create_basic_widgets_section(content_box);
    create_selection_section(content_box);

    /* Advanced widgets in a horizontal box */
    Widget advanced_box;
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientHorizontal); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    advanced_box = IswCreateManagedWidget("advancedBox", boxWidgetClass, content_box, args, n);

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
    Arg args[10];
    Cardinal n;
    
    n = 0;
    IswSetArg(args[n], IswNlabel, "=== Isw3d Widget Demonstration (XCB Backend) ==="); n++;
    IswSetArg(args[n], IswNjustify, IswJustifyCenter); n++;
    IswSetArg(args[n], IswNwidth, 830); n++;
    IswSetArg(args[n], IswNheight, 35); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    title = IswCreateManagedWidget("titleLabel", labelWidgetClass,
                                  parent, args, n);
    
    return title;
}

/* ============================================================
 * CONTAINER WIDGETS SECTION
 * ============================================================ */

Widget create_containers_section(Widget parent) {
    Widget form, section_label;
    Widget toolbar_demo, box_demo, form_demo, viewport_demo;
    Arg args[10];
    Cardinal n;

    /* Create Form to hold container demos */
    n = 0;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNdefaultDistance, 5); n++;
    form = IswCreateManagedWidget("containersForm", formWidgetClass,
                                 parent, args, n);

    /* Section label */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Container Widgets: Toolbar, Box, Form, Viewport"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    section_label = IswCreateManagedWidget("containerLabel", labelWidgetClass,
                                          form, args, n);

    /* Toolbar demo */
    toolbar_demo = create_toolbar_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(toolbar_demo, args, n);

    /* Create demos */
    box_demo = create_box_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, toolbar_demo); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(box_demo, args, n);

    form_demo = create_form_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, box_demo); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(form_demo, args, n);

    viewport_demo = create_viewport_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, form_demo); n++;
    IswSetArg(args[n], IswNfromVert, box_demo); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(viewport_demo, args, n);

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
    Arg args[6];
    Cardinal n;
    Dimension btn_size = 24;

    n = 0;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    toolbar = IswCreateManagedWidget("toolbar", toolbarWidgetClass, parent, args, n);

    /* Icon buttons with uniform size */
    n = 0;
    IswSetArg(args[n], IswNimage, svg_new); n++;
    IswSetArg(args[n], IswNlabel, ""); n++;
    IswSetArg(args[n], IswNwidth, btn_size); n++;
    IswSetArg(args[n], IswNheight, btn_size); n++;
    IswCreateManagedWidget("tbNew", commandWidgetClass, toolbar, args, n);

    n = 0;
    IswSetArg(args[n], IswNimage, svg_open); n++;
    IswSetArg(args[n], IswNlabel, ""); n++;
    IswSetArg(args[n], IswNwidth, btn_size); n++;
    IswSetArg(args[n], IswNheight, btn_size); n++;
    IswCreateManagedWidget("tbOpen", commandWidgetClass, toolbar, args, n);

    n = 0;
    IswSetArg(args[n], IswNimage, svg_save); n++;
    IswSetArg(args[n], IswNlabel, ""); n++;
    IswSetArg(args[n], IswNwidth, btn_size); n++;
    IswSetArg(args[n], IswNheight, btn_size); n++;
    IswCreateManagedWidget("tbSave", commandWidgetClass, toolbar, args, n);

    /* Separator */
    n = 0;
    IswSetArg(args[n], IswNlabel, ""); n++;
    IswSetArg(args[n], IswNwidth, 2); n++;
    IswSetArg(args[n], IswNheight, btn_size); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswCreateManagedWidget("tbSep", labelWidgetClass, toolbar, args, n);

    n = 0;
    IswSetArg(args[n], IswNimage, svg_cut); n++;
    IswSetArg(args[n], IswNlabel, ""); n++;
    IswSetArg(args[n], IswNwidth, btn_size); n++;
    IswSetArg(args[n], IswNheight, btn_size); n++;
    IswCreateManagedWidget("tbCut", commandWidgetClass, toolbar, args, n);

    n = 0;
    IswSetArg(args[n], IswNimage, svg_copy); n++;
    IswSetArg(args[n], IswNlabel, ""); n++;
    IswSetArg(args[n], IswNwidth, btn_size); n++;
    IswSetArg(args[n], IswNheight, btn_size); n++;
    IswCreateManagedWidget("tbCopy", commandWidgetClass, toolbar, args, n);

    n = 0;
    IswSetArg(args[n], IswNimage, svg_paste); n++;
    IswSetArg(args[n], IswNlabel, ""); n++;
    IswSetArg(args[n], IswNwidth, btn_size); n++;
    IswSetArg(args[n], IswNheight, btn_size); n++;
    IswCreateManagedWidget("tbPaste", commandWidgetClass, toolbar, args, n);

    return toolbar;
}

Widget create_box_demo(Widget parent) {
    Widget box_container, box, label1, label2, label3;
    Arg args[10];
    Cardinal n;
    
    /* Container with label */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box_container = IswCreateManagedWidget("boxContainer", boxWidgetClass,
                                          parent, args, n);
    
    /* Label */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Box Widget (Horizontal)"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswCreateManagedWidget("boxLabel", labelWidgetClass, box_container, args, n);
    
    /* Horizontal Box */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientHorizontal); n++;
    IswSetArg(args[n], IswNhSpace, 5); n++;
    IswSetArg(args[n], IswNvSpace, 5); n++;
    box = IswCreateManagedWidget("demoBox", boxWidgetClass,
                                box_container, args, n);
    
    /* Add three labels to box */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Item 1"); n++;
    label1 = IswCreateManagedWidget("boxItem1", labelWidgetClass, box, args, n);

    n = 0;
    IswSetArg(args[n], IswNlabel, "Item 2"); n++;
    label2 = IswCreateManagedWidget("boxItem2", labelWidgetClass, box, args, n);

    n = 0;
    IswSetArg(args[n], IswNlabel, "Item 3"); n++;
    label3 = IswCreateManagedWidget("boxItem3", labelWidgetClass, box, args, n);
    
    return box_container;
}

Widget create_form_demo(Widget parent) {
    Widget form, title_label, button1, button2, button3;
    Arg args[10];
    Cardinal n;
    
    /* Form container */
    n = 0;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNwidth, 350); n++;
    IswSetArg(args[n], IswNheight, 80); n++;
    form = IswCreateManagedWidget("demoForm", formWidgetClass, parent, args, n);
    
    /* Title label - top, spans width */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Form Widget (Constraint Layout)"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    title_label = IswCreateManagedWidget("formTitle", labelWidgetClass,
                                        form, args, n);
    
    /* Button row - relative positioning */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Left"); n++;
    IswSetArg(args[n], IswNfromVert, title_label); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    button1 = IswCreateManagedWidget("formBtn1", commandWidgetClass,
                                    form, args, n);
    IswAddCallback(button1, IswNcallback, button_callback, (IswPointer)"Form Left Button");
    
    n = 0;
    IswSetArg(args[n], IswNlabel, "Center"); n++;
    IswSetArg(args[n], IswNfromVert, title_label); n++;
    IswSetArg(args[n], IswNfromHoriz, button1); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    button2 = IswCreateManagedWidget("formBtn2", commandWidgetClass,
                                    form, args, n);
    IswAddCallback(button2, IswNcallback, button_callback, (IswPointer)"Form Center Button");
    
    n = 0;
    IswSetArg(args[n], IswNlabel, "Right"); n++;
    IswSetArg(args[n], IswNfromVert, title_label); n++;
    IswSetArg(args[n], IswNfromHoriz, button2); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    button3 = IswCreateManagedWidget("formBtn3", commandWidgetClass,
                                    form, args, n);
    IswAddCallback(button3, IswNcallback, button_callback, (IswPointer)"Form Right Button");
    
    return form;
}

Widget create_viewport_demo(Widget parent) {
    Widget viewport_container, viewport, large_label;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    viewport_container = IswCreateManagedWidget("viewportContainer", boxWidgetClass,
                                               parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Viewport Widget (Scrollable)"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswCreateManagedWidget("viewportTitle", labelWidgetClass,
                          viewport_container, args, n);
    
    /* Viewport with scrollbars */
    n = 0;
    IswSetArg(args[n], IswNwidth, 250); n++;
    IswSetArg(args[n], IswNheight, 80); n++;
    IswSetArg(args[n], IswNallowHoriz, True); n++;
    IswSetArg(args[n], IswNallowVert, True); n++;
    IswSetArg(args[n], IswNuseBottom, True); n++;
    IswSetArg(args[n], IswNuseRight, True); n++;
    viewport = IswCreateManagedWidget("viewport", viewportWidgetClass,
                                     viewport_container, args, n);
    
    /* Large content inside viewport */
    n = 0;
    IswSetArg(args[n], IswNlabel, 
             "This demonstrates scrolling.\n"
             "The content is larger than\n"
             "the viewport, so scrollbars\n"
             "appear automatically.\n"
             "Try scrolling! \n" 
             "This demonstrates scrolling.\n"
             "The content is larger than\n"
             "the viewport, so scrollbars\n"
             "appear automatically.\n"
            ); n++;
    IswSetArg(args[n], IswNjustify, IswJustifyLeft); n++;
    IswSetArg(args[n], IswNwidth, 400); n++;
    IswSetArg(args[n], IswNheight, 150); n++;
    large_label = IswCreateManagedWidget("viewportContent", labelWidgetClass,
                                        viewport, args, n);
    
    return viewport_container;
}

/* ============================================================
 * BASIC WIDGETS SECTION
 * ============================================================ */

Widget create_basic_widgets_section(Widget parent) {
    Widget form, section_label;
    Widget command_demo, toggle_demo, checkbox_demo, menu_demo, repeater_demo;
    Arg args[10];
    Cardinal n;

    /* Section container */
    n = 0;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNdefaultDistance, 5); n++;
    form = IswCreateManagedWidget("basicForm", formWidgetClass, parent, args, n);

    /* Section label */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Basic Interactive Widgets: Command, Toggle, Checkbox, Menu, Repeater"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    section_label = IswCreateManagedWidget("basicLabel", labelWidgetClass,
                                          form, args, n);

    /* Create widget demos */
    command_demo = create_command_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(command_demo, args, n);

    toggle_demo = create_toggle_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, command_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(toggle_demo, args, n);

    checkbox_demo = create_checkbox_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, toggle_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(checkbox_demo, args, n);

    menu_demo = create_menu_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, checkbox_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(menu_demo, args, n);

    repeater_demo = create_repeater_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, menu_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(repeater_demo, args, n);

    return form;
}

Widget create_command_demo(Widget parent) {
    Widget box, title, button1, button2, quit_button, svg_button;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("commandBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Command Buttons"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("commandTitle", labelWidgetClass, box, args, n);

    /* Buttons */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Click Me!"); n++;
    button1 = IswCreateManagedWidget("cmdBtn1", commandWidgetClass, box, args, n);
    IswAddCallback(button1, IswNcallback, button_callback, (IswPointer)"Button 1");
    attach_tooltip(button1, "This is a clickable command button");

    n = 0;
    IswSetArg(args[n], IswNlabel, "Or Me!"); n++;
    IswSetArg(args[n], IswNcornerRadius, 0); n++;
    button2 = IswCreateManagedWidget("cmdBtn2", commandWidgetClass, box, args, n);
    IswAddCallback(button2, IswNcallback, button_callback, (IswPointer)"Button 2");
    attach_tooltip(button2, "Another command button with tooltip");

    /* SVG icon button */
    n = 0;
    IswSetArg(args[n], IswNimage, "x11.svg"); n++;
    IswSetArg(args[n], IswNlabel, ""); n++;
    svg_button = IswCreateManagedWidget("svgBtn", commandWidgetClass, box, args, n);
    IswAddCallback(svg_button, IswNcallback, button_callback, (IswPointer)"SVG Button");
    attach_tooltip(svg_button, "Command button with SVG icon");

    n = 0;
    IswSetArg(args[n], IswNlabel, "Quit"); n++;
    quit_button = IswCreateManagedWidget("quitBtn", commandWidgetClass, box, args, n);
    IswAddCallback(quit_button, IswNcallback, quit_callback, NULL);
    attach_tooltip(quit_button, "Click to exit the application");

    return box;
}

Widget create_toggle_demo(Widget parent) {
    Widget box, title, toggle1, toggle2, toggle3;
    Arg args[10];
    Cardinal n;
    static Widget radio_group = NULL;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("toggleBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Toggle (Radio) Buttons"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("toggleTitle", labelWidgetClass, box, args, n);
    
    /* Radio button group */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Option A"); n++;
    IswSetArg(args[n], IswNstate, True); n++;
    toggle1 = IswCreateManagedWidget("toggleA", toggleWidgetClass, box, args, n);
    IswAddCallback(toggle1, IswNcallback, toggle_callback, (IswPointer)"Option A");
    radio_group = toggle1;

    n = 0;
    IswSetArg(args[n], IswNlabel, "Option B"); n++;
    IswSetArg(args[n], IswNradioGroup, radio_group); n++;
    toggle2 = IswCreateManagedWidget("toggleB", toggleWidgetClass, box, args, n);
    IswAddCallback(toggle2, IswNcallback, toggle_callback, (IswPointer)"Option B");

    n = 0;
    IswSetArg(args[n], IswNlabel, "Option C"); n++;
    IswSetArg(args[n], IswNradioGroup, radio_group); n++;
    toggle3 = IswCreateManagedWidget("toggleC", toggleWidgetClass, box, args, n);
    IswAddCallback(toggle3, IswNcallback, toggle_callback, (IswPointer)"Option C");
    
    return box;
}

Widget create_checkbox_demo(Widget parent) {
    Widget box, title, cb1, cb2, cb3;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("checkboxBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Toggle (Checkbox) Buttons"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("checkboxTitle", labelWidgetClass, box, args, n);

    /* Standalone toggles (no radioGroup) render as checkboxes */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Enable notifications"); n++;
    IswSetArg(args[n], IswNstate, True); n++;
    cb1 = IswCreateManagedWidget("cb1", toggleWidgetClass, box, args, n);
    IswAddCallback(cb1, IswNcallback, checkbox_callback, (IswPointer)"Enable notifications");

    n = 0;
    IswSetArg(args[n], IswNlabel, "Dark mode"); n++;
    cb2 = IswCreateManagedWidget("cb2", toggleWidgetClass, box, args, n);
    IswAddCallback(cb2, IswNcallback, checkbox_callback, (IswPointer)"Dark mode");

    n = 0;
    IswSetArg(args[n], IswNlabel, "Auto-save"); n++;
    cb3 = IswCreateManagedWidget("cb3", toggleWidgetClass, box, args, n);
    IswAddCallback(cb3, IswNcallback, checkbox_callback, (IswPointer)"Auto-save");

    return box;
}

Widget create_menu_demo(Widget parent) {
    Widget box, title, menu_button, menu;
    Widget entry1, entry2, line, entry3;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("menuBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Menu Demo"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("menuTitle", labelWidgetClass, box, args, n);
    
    /* MenuButton */
    n = 0;
    IswSetArg(args[n], IswNlabel, "File Menu"); n++;
    menu_button = IswCreateManagedWidget("menuButton", menuButtonWidgetClass,
                                        box, args, n);
    
    /* Create SimpleMenu popup */
    n = 0;
    menu = IswCreatePopupShell("fileMenu", simpleMenuWidgetClass,
                              menu_button, args, n);
    
    /* Menu entries */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Open"); n++;
    entry1 = IswCreateManagedWidget("menuOpen", smeBSBObjectClass, menu, args, n);
    IswAddCallback(entry1, IswNcallback, menu_callback, (IswPointer)"Open");
    
    n = 0;
    IswSetArg(args[n], IswNlabel, "Save"); n++;
    entry2 = IswCreateManagedWidget("menuSave", smeBSBObjectClass, menu, args, n);
    IswAddCallback(entry2, IswNcallback, menu_callback, (IswPointer)"Save");
    
    /* Separator line */
    n = 0;
    line = IswCreateManagedWidget("menuLine", smeLineObjectClass, menu, args, n);
    
    n = 0;
    IswSetArg(args[n], IswNlabel, "Exit"); n++;
    entry3 = IswCreateManagedWidget("menuExit", smeBSBObjectClass, menu, args, n);
    IswAddCallback(entry3, IswNcallback, menu_callback, (IswPointer)"Exit");
    
    return box;
}

Widget create_repeater_demo(Widget parent) {
    Widget box, title, repeater;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("repeaterBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Repeater Button"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("repeaterTitle", labelWidgetClass, box, args, n);
    
    /* Repeater button - auto-repeats while held */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Hold Me"); n++;
    IswSetArg(args[n], IswNrepeatDelay, 500); n++;
    repeater = IswCreateManagedWidget("repeater", repeaterWidgetClass, box, args, n);
    IswAddCallback(repeater, IswNcallback, repeater_callback, NULL);
    
    return box;
}

/* ============================================================
 * SELECTION WIDGETS SECTION
 * ============================================================ */

Widget create_selection_section(Widget parent) {
    Widget form, section_label, iconview_demo, listview_demo, list_demo, combobox_demo, text_demo;
    Arg args[10];
    Cardinal n;

    /* Section container */
    n = 0;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNdefaultDistance, 5); n++;
    form = IswCreateManagedWidget("selectionForm", formWidgetClass, parent, args, n);

    /* Section label */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Selection Widgets: IconView, ListView, List, ComboBox, Text"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    section_label = IswCreateManagedWidget("selectionLabel", labelWidgetClass,
                                          form, args, n);

    /* IconView demo */
    iconview_demo = create_iconview_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(iconview_demo, args, n);

    /* ListView demo (multi-column list) */
    listview_demo = create_listview_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, iconview_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(listview_demo, args, n);

    /* List demo (classic multi-item list) */
    list_demo = create_list_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, listview_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(list_demo, args, n);

    /* ComboBox demo (dropdown selector) */
    combobox_demo = create_combobox_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, list_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(combobox_demo, args, n);

    /* Text demo */
    text_demo = create_text_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, combobox_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(text_demo, args, n);

    return form;
}

Widget create_iconview_demo(Widget parent) {
    Widget viewport, iconview;
    Arg args[10];
    Cardinal n;

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
    n = 0;
    IswSetArg(args[n], IswNallowVert, True); n++;
    IswSetArg(args[n], IswNwidth, 200); n++;
    IswSetArg(args[n], IswNheight, 180); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    viewport = IswCreateManagedWidget("iconViewport", viewportWidgetClass,
                                      parent, args, n);

    /* IconView inside viewport */
    n = 0;
    IswSetArg(args[n], IswNiconLabels, iv_labels); n++;
    IswSetArg(args[n], IswNiconData, iv_icons); n++;
    IswSetArg(args[n], IswNnumIcons, IswNumber(iv_labels)); n++;
    IswSetArg(args[n], IswNiconSize, 32); n++;
    IswSetArg(args[n], IswNwidth, 200); n++;
    IswSetArg(args[n], IswNmultiSelect, True); n++;
    iconview = IswCreateManagedWidget("iconView", iconViewWidgetClass,
                                      viewport, args, n);
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
    Arg args[12];
    Cardinal n;

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
    n = 0;
    IswSetArg(args[n], IswNallowVert, True); n++;
    IswSetArg(args[n], IswNwidth, 300); n++;
    IswSetArg(args[n], IswNheight, 180); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    viewport = IswCreateManagedWidget("listViewViewport", viewportWidgetClass,
                                      parent, args, n);

    /* ListView inside viewport */
    n = 0;
    IswSetArg(args[n], IswNlistViewColumns, cols); n++;
    IswSetArg(args[n], IswNnumColumns, LV_DEMO_COLS); n++;
    IswSetArg(args[n], IswNlistViewData, lv_demo_flat); n++;
    IswSetArg(args[n], IswNnumRows, LV_DEMO_ROWS); n++;
    IswSetArg(args[n], IswNwidth, 300); n++;
    IswSetArg(args[n], IswNmultiSelect, True); n++;
    IswSetArg(args[n], IswNshowHeader, True); n++;
    listview = IswCreateManagedWidget("listView", listViewWidgetClass,
                                      viewport, args, n);
    IswAddCallback(listview, IswNselectCallback, listview_callback, NULL);
    IswAddCallback(listview, IswNreorderCallback, listview_reorder_callback, NULL);

    return viewport;
}

Widget create_list_demo(Widget parent) {
    Widget box, title, list;
    Arg args[10];
    Cardinal n;
    static String items[] = {
        "Apple", "Banana", "Cherry", "Date",
        "Elderberry", "Fig", "Grape", "Honeydew",
        "Kiwi", "Lemon", "Mango"
    };

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("listBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "List"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("listTitle", labelWidgetClass, box, args, n);

    /* Classic list widget — full multi-item display */
    n = 0;
    IswSetArg(args[n], IswNlist, items); n++;
    IswSetArg(args[n], IswNnumberStrings, IswNumber(items)); n++;
    IswSetArg(args[n], IswNdefaultColumns, 1); n++;
    IswSetArg(args[n], IswNforceColumns, True); n++;
    IswSetArg(args[n], IswNwidth, 150); n++;
    list = IswCreateManagedWidget("list", listWidgetClass, box, args, n);

    IswAddCallback(list, IswNcallback, list_callback, NULL);

    return box;
}

Widget create_combobox_demo(Widget parent) {
    Widget box, title, combo;
    Arg args[10];
    Cardinal n;
    static String items[] = {
        "Red", "Orange", "Yellow", "Green",
        "Blue", "Indigo", "Violet"
    };

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("comboBoxBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "ComboBox"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("comboBoxTitle", labelWidgetClass, box, args, n);

    /* ComboBox — List subclass with dropdown default */
    n = 0;
    IswSetArg(args[n], IswNlist, items); n++;
    IswSetArg(args[n], IswNnumberStrings, IswNumber(items)); n++;
    IswSetArg(args[n], IswNwidth, 150); n++;
    combo = IswCreateManagedWidget("comboBox", comboBoxWidgetClass, box, args, n);

    IswAddCallback(combo, IswNcallback, combobox_callback, NULL);

    return box;
}

Widget create_text_demo(Widget parent) {
    Widget box, title, text;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("textBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Text Widget (Editable)"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("textTitle", labelWidgetClass, box, args, n);
    
    /* Editable text widget with scrollbars */
    n = 0;
    IswSetArg(args[n], IswNeditType, IswtextEdit); n++;
    IswSetArg(args[n], IswNwidth, 450); n++;
    IswSetArg(args[n], IswNheight, 120); n++;
    IswSetArg(args[n], IswNscrollVertical, IswtextScrollAlways); n++;
    IswSetArg(args[n], IswNstring,
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
             "Line 15: End of demo text."); n++;
    text = IswCreateManagedWidget("textEditor", textWidgetClass, box, args, n);
    
    return box;
}

/* ============================================================
 * NAVIGATION WIDGETS SECTION
 * ============================================================ */

Widget create_navigation_section(Widget parent) {
    Widget form, section_label;
    Widget panner_demo;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNdefaultDistance, 5); n++;
    form = IswCreateManagedWidget("navigationForm", formWidgetClass,
                                 parent, args, n);
    
    /* Section label */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Navigation Widgets: Panner/Porthole"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    section_label = IswCreateManagedWidget("navigationLabel", labelWidgetClass,
                                          form, args, n);
    
    /* Create demos */
    panner_demo = create_panner_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(panner_demo, args, n);
    
    return form;
}

/* Panner report callback: user dragged the panner slider, move porthole content */
void panner_report_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswPannerReport *report = (IswPannerReport *)call_data;
    Widget content = (Widget)client_data;
    Arg args[2];
    Cardinal n = 0;

    if (report->changed & IswPRSliderX) {
        IswSetArg(args[n], IswNx, -report->slider_x); n++;
    }
    if (report->changed & IswPRSliderY) {
        IswSetArg(args[n], IswNy, -report->slider_y); n++;
    }
    if (n > 0)
        IswSetValues(content, args, n);
}

/* Porthole report callback: porthole moved its child, update panner slider */
void porthole_report_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    IswPannerReport *report = (IswPannerReport *)call_data;
    Widget panner = (Widget)client_data;
    Arg args[6];
    Cardinal n = 0;

    IswSetArg(args[n], IswNsliderX, report->slider_x); n++;
    IswSetArg(args[n], IswNsliderY, report->slider_y); n++;
    IswSetArg(args[n], IswNsliderWidth, report->slider_width); n++;
    IswSetArg(args[n], IswNsliderHeight, report->slider_height); n++;
    IswSetArg(args[n], IswNcanvasWidth, report->canvas_width); n++;
    IswSetArg(args[n], IswNcanvasHeight, report->canvas_height); n++;
    IswSetValues(panner, args, n);
}

Widget create_panner_demo(Widget parent) {
    Widget box, title, panner, porthole, large_widget;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("pannerBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Panner/Porthole"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("pannerTitle", labelWidgetClass, box, args, n);

    /* Panner widget (miniature navigator) */
    n = 0;
    IswSetArg(args[n], IswNwidth, 200); n++;
    IswSetArg(args[n], IswNheight, 150); n++;
    panner = IswCreateManagedWidget("panner", pannerWidgetClass, box, args, n);

    /* Porthole (viewing area) */
    n = 0;
    IswSetArg(args[n], IswNwidth, 200); n++;
    IswSetArg(args[n], IswNheight, 150); n++;
    porthole = IswCreateManagedWidget("porthole", portholeWidgetClass, box, args, n);

    /* Large widget inside porthole */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Large Content Area\n\n"\
             "This area is larger than the\n"\
             "visible porthole window.\n\n"\
             "Use the panner above to\n"\
             "navigate around this content."); n++;
    IswSetArg(args[n], IswNwidth, 400); n++;
    IswSetArg(args[n], IswNheight, 300); n++;
    large_widget = IswCreateManagedWidget("pannerContent", labelWidgetClass,
                                         porthole, args, n);

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
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("treeBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Tree Widget (Hierarchical Structure)"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("treeTitle", labelWidgetClass, box, args, n);
    
    /* Tree widget */
    n = 0;
    IswSetArg(args[n], IswNwidth, 300); n++;
    IswSetArg(args[n], IswNheight, 200); n++;
    IswSetArg(args[n], IswNautoReconfigure, True); n++;
    IswSetArg(args[n], IswNhSpace, 20); n++;
    IswSetArg(args[n], IswNvSpace, 10); n++;
    tree = IswCreateManagedWidget("tree", treeWidgetClass, box, args, n);
    
    /* Root node */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Root"); n++;
    node1 = IswCreateManagedWidget("node1", commandWidgetClass, tree, args, n);
    
    /* Child nodes of root */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Child 1"); n++;
    IswSetArg(args[n], IswNtreeParent, node1); n++;
    node2 = IswCreateManagedWidget("node2", commandWidgetClass, tree, args, n);
    
    n = 0;
    IswSetArg(args[n], IswNlabel, "Child 2"); n++;
    IswSetArg(args[n], IswNtreeParent, node1); n++;
    node3 = IswCreateManagedWidget("node3", commandWidgetClass, tree, args, n);
    
    /* Grandchild nodes */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Grandchild 1"); n++;
    IswSetArg(args[n], IswNtreeParent, node2); n++;
    node4 = IswCreateManagedWidget("node4", commandWidgetClass, tree, args, n);
    
    n = 0;
    IswSetArg(args[n], IswNlabel, "Grandchild 2"); n++;
    IswSetArg(args[n], IswNtreeParent, node3); n++;
    node5 = IswCreateManagedWidget("node5", commandWidgetClass, tree, args, n);
    
    return box;
}

/* ============================================================
 * LAYOUT DEMO SECTION
 * ============================================================ */

Widget create_layout_demo(Widget parent) {
    Widget box, title, layout, button1, button2, button3;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("layoutBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Layout Widget (Constraint-based)"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("layoutTitle", labelWidgetClass, box, args, n);
    
    /* Use Form widget to demonstrate constraint-based layout.
     * Form positions children by fromHoriz/fromVert and distances;
     * chain constraints control how they move on resize. We place a
     * hidden spacer to push "Top Right" to the right edge, and
     * compute an offset for "Bottom Center". */

    n = 0;
    IswSetArg(args[n], IswNwidth, 300); n++;
    IswSetArg(args[n], IswNheight, 120); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNdefaultDistance, 8); n++;
    layout = IswCreateManagedWidget("layout", formWidgetClass, box, args, n);

    /* Top Left: pinned to top-left */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Top Left"); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNbottom, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetArg(args[n], IswNright, IswChainLeft); n++;
    button1 = IswCreateManagedWidget("layoutBtn1", commandWidgetClass, layout, args, n);

    /* Top Right: pushed to right side via horizDistance */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Top Right"); n++;
    IswSetArg(args[n], IswNfromHoriz, button1); n++;
    IswSetArg(args[n], IswNhorizDistance, 100); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNbottom, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainRight); n++;
    IswSetArg(args[n], IswNright, IswChainRight); n++;
    button2 = IswCreateManagedWidget("layoutBtn2", commandWidgetClass, layout, args, n);

    /* Bottom Center: below button1, centered via horizDistance */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Bottom Center"); n++;
    IswSetArg(args[n], IswNfromVert, button1); n++;
    IswSetArg(args[n], IswNhorizDistance, 80); n++;
    IswSetArg(args[n], IswNtop, IswChainBottom); n++;
    IswSetArg(args[n], IswNbottom, IswChainBottom); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetArg(args[n], IswNright, IswChainLeft); n++;
    button3 = IswCreateManagedWidget("layoutBtn3", commandWidgetClass, layout, args, n);
    
    return box;
}

/* ============================================================
 * GRIP DEMO SECTION (Enhanced Paned)
 * ============================================================ */

Widget create_paned_grip_demo(Widget parent) {
    Widget box, title, paned, grip1, grip2;
    Widget section1, section2, section3;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("gripBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Grip Widget (Pane Resizing)"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("gripTitle", labelWidgetClass, box, args, n);
    
    /* Paned widget with visible grips */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNwidth, 200); n++;
    IswSetArg(args[n], IswNheight, 200); n++;
    paned = IswCreateManagedWidget("gripPaned", panedWidgetClass, box, args, n);
    
    /* First section */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Section 1\n(Drag grip to resize)"); n++;
    IswSetArg(args[n], IswNmin, 30); n++;
    IswSetArg(args[n], IswNmax, 150); n++;
    IswSetArg(args[n], IswNshowGrip, True); n++;
    section1 = IswCreateManagedWidget("section1", labelWidgetClass, paned, args, n);
    
    /* Grip is automatically created between panes */
    
    /* Second section */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Section 2"); n++;
    IswSetArg(args[n], IswNmin, 30); n++;
    IswSetArg(args[n], IswNmax, 150); n++;
    IswSetArg(args[n], IswNshowGrip, True); n++;
    section2 = IswCreateManagedWidget("section2", labelWidgetClass, paned, args, n);
    
    /* Third section */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Section 3"); n++;
    IswSetArg(args[n], IswNmin, 30); n++;
    IswSetArg(args[n], IswNskipAdjust, True); n++;
    section3 = IswCreateManagedWidget("section3", labelWidgetClass, paned, args, n);
    
    return box;
}

/* ============================================================
 * SPECIALIZED WIDGETS SECTION
 * ============================================================ */

Widget create_specialized_section(Widget parent) {
    Widget form, section_label;
    Widget spinbox_demo, slider_demo, scrollbar_demo, progressbar_demo, dialog_demo, colorpicker_demo, fontchooser_demo, drawingarea_demo;
    Arg args[10];
    Cardinal n;

    /* Section container */
    n = 0;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNdefaultDistance, 5); n++;
    form = IswCreateManagedWidget("specializedForm", formWidgetClass,
                                 parent, args, n);

    /* Section label */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Specialized Widgets: SpinBox, Slider, Scrollbar, ProgressBar, Dialog"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    section_label = IswCreateManagedWidget("specializedLabel", labelWidgetClass,
                                          form, args, n);

    /* Create demos */
    spinbox_demo = create_spinbox_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(spinbox_demo, args, n);

    slider_demo = create_slider_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, spinbox_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(slider_demo, args, n);

    scrollbar_demo = create_scrollbar_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, slider_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(scrollbar_demo, args, n);

    progressbar_demo = create_progressbar_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, scrollbar_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(progressbar_demo, args, n);

    dialog_demo = create_dialog_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, progressbar_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(dialog_demo, args, n);

    colorpicker_demo = create_colorpicker_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, dialog_demo); n++;
    IswSetArg(args[n], IswNfromVert, section_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(colorpicker_demo, args, n);

    fontchooser_demo = create_fontchooser_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromVert, slider_demo); n++;
    IswSetArg(args[n], IswNtop, IswChainTop); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    IswSetValues(fontchooser_demo, args, n);

    drawingarea_demo = create_drawingarea_demo(form);
    n = 0;
    IswSetArg(args[n], IswNfromHoriz, fontchooser_demo); n++;
    IswSetArg(args[n], IswNfromVert, slider_demo); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetValues(drawingarea_demo, args, n);

    /* Drop target demo — receives drops from any XDND app */
    Widget drop_label;
    n = 0;
    IswSetArg(args[n], IswNlabel, "Drop files here"); n++;
    IswSetArg(args[n], IswNwidth, 200); n++;
    IswSetArg(args[n], IswNheight, 40); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNresize, False); n++;
    IswSetArg(args[n], IswNfromVert, fontchooser_demo); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    drop_label = IswCreateManagedWidget("dropTarget", labelWidgetClass,
                                        form, args, n);
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
    n = 0;
    IswSetArg(args[n], IswNlabel, "Drag me"); n++;
    IswSetArg(args[n], IswNwidth, 100); n++;
    IswSetArg(args[n], IswNheight, 40); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    IswSetArg(args[n], IswNfromVert, fontchooser_demo); n++;
    IswSetArg(args[n], IswNfromHoriz, drop_label); n++;
    IswSetArg(args[n], IswNhorizDistance, 10); n++;
    IswSetArg(args[n], IswNleft, IswChainLeft); n++;
    drag_label = IswCreateManagedWidget("dragSource", labelWidgetClass,
                                        form, args, n);

    /* Override translations so button press starts a drag */
    IswOverrideTranslations(drag_label,
        IswParseTranslationTable("<BtnDown>: drag-start()"));

    return form;
}

Widget create_progressbar_demo(Widget parent) {
    Widget box, title, pb_h1, pb_h2, pb_v;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("progressBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "ProgressBar Widget"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("progressTitle", labelWidgetClass, box, args, n);

    /* Horizontal progress bar at 75% with text */
    n = 0;
    IswSetArg(args[n], IswNvalue, 75); n++;
    IswSetArg(args[n], IswNwidth, 180); n++;
    IswSetArg(args[n], IswNheight, 24); n++;
    IswSetArg(args[n], IswNshowValue, True); n++;
    pb_h1 = IswCreateManagedWidget("progressH1", progressBarWidgetClass, box, args, n);

    /* Horizontal progress bar at 30% without text */
    n = 0;
    IswSetArg(args[n], IswNvalue, 30); n++;
    IswSetArg(args[n], IswNwidth, 180); n++;
    IswSetArg(args[n], IswNheight, 18); n++;
    IswSetArg(args[n], IswNshowValue, False); n++;
    pb_h2 = IswCreateManagedWidget("progressH2", progressBarWidgetClass, box, args, n);

    /* Vertical progress bar at 60% with text */
    n = 0;
    IswSetArg(args[n], IswNvalue, 60); n++;
    IswSetArg(args[n], IswNwidth, 30); n++;
    IswSetArg(args[n], IswNheight, 100); n++;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNshowValue, True); n++;
    pb_v = IswCreateManagedWidget("progressV", progressBarWidgetClass, box, args, n);

    return box;
}

Widget create_fontchooser_demo(Widget parent) {
    Widget box, title, chooser;
    Arg args[4];
    Cardinal n;

    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("fontChooserBox", boxWidgetClass, parent, args, n);

    n = 0;
    IswSetArg(args[n], IswNlabel, "Font Chooser"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("fontChooserTitle", labelWidgetClass, box, args, n);

    n = 0;
    chooser = IswCreateManagedWidget("fontChooser", fontChooserWidgetClass, box, args, n);
    IswAddCallback(chooser, IswNfontChanged, fontchooser_callback, NULL);

    return box;
}

Widget create_colorpicker_demo(Widget parent) {
    Widget box, title, picker;
    Arg args[6];
    Cardinal n;

    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("colorPickerBox", boxWidgetClass, parent, args, n);

    n = 0;
    IswSetArg(args[n], IswNlabel, "Color Picker"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("colorPickerTitle", labelWidgetClass, box, args, n);

    n = 0;
    IswSetArg(args[n], IswNcolorRed, 128); n++;
    IswSetArg(args[n], IswNcolorGreen, 64); n++;
    IswSetArg(args[n], IswNcolorBlue, 192); n++;
    picker = IswCreateManagedWidget("colorPicker", colorPickerWidgetClass, box, args, n);
    IswAddCallback(picker, IswNcolorChanged, colorpicker_callback, NULL);

    return box;
}

Widget create_spinbox_demo(Widget parent) {
    Widget box, title, spin1, spin2;
    Arg args[12];
    Cardinal n;

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("spinBoxBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "SpinBox"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("spinBoxTitle", labelWidgetClass, box, args, n);

    /* SpinBox: 0-100, increment 1 */
    n = 0;
    IswSetArg(args[n], IswNspinMinimum, 0); n++;
    IswSetArg(args[n], IswNspinMaximum, 100); n++;
    IswSetArg(args[n], IswNspinValue, 50); n++;
    IswSetArg(args[n], IswNspinIncrement, 1); n++;
    IswSetArg(args[n], IswNwidth, 120); n++;
    spin1 = IswCreateManagedWidget("spin1", spinBoxWidgetClass, box, args, n);
    IswAddCallback(spin1, IswNvalueChanged, spinbox_callback, (IswPointer)"Spin1");

    /* SpinBox: wrapping, step 10 */
    n = 0;
    IswSetArg(args[n], IswNspinMinimum, 0); n++;
    IswSetArg(args[n], IswNspinMaximum, 255); n++;
    IswSetArg(args[n], IswNspinValue, 128); n++;
    IswSetArg(args[n], IswNspinIncrement, 10); n++;
    IswSetArg(args[n], IswNspinWrap, True); n++;
    IswSetArg(args[n], IswNwidth, 120); n++;
    spin2 = IswCreateManagedWidget("spin2", spinBoxWidgetClass, box, args, n);
    IswAddCallback(spin2, IswNvalueChanged, spinbox_callback, (IswPointer)"Spin2");

    return box;
}

Widget create_slider_demo(Widget parent) {
    Widget box, title, slider_h, slider_v;
    Arg args[12];
    Cardinal n;

    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("sliderBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Slider"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("sliderTitle", labelWidgetClass, box, args, n);

    /* Horizontal slider with ticks */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientHorizontal); n++;
    IswSetArg(args[n], IswNminimumValue, 0); n++;
    IswSetArg(args[n], IswNmaximumValue, 100); n++;
    IswSetArg(args[n], IswNsliderValue, 50); n++;
    IswSetArg(args[n], IswNtickInterval, 25); n++;
    IswSetArg(args[n], IswNshowValue, True); n++;
    IswSetArg(args[n], IswNwidth, 200); n++;
    IswSetArg(args[n], IswNheight, 50); n++;
    slider_h = IswCreateManagedWidget("sliderH", sliderWidgetClass, box, args, n);
    IswAddCallback(slider_h, IswNvalueChanged, slider_callback, (IswPointer)"Horizontal");

    /* Vertical slider */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNminimumValue, 0); n++;
    IswSetArg(args[n], IswNmaximumValue, 255); n++;
    IswSetArg(args[n], IswNsliderValue, 128); n++;
    IswSetArg(args[n], IswNshowValue, True); n++;
    IswSetArg(args[n], IswNwidth, 70); n++;
    IswSetArg(args[n], IswNheight, 120); n++;
    slider_v = IswCreateManagedWidget("sliderV", sliderWidgetClass, box, args, n);
    IswAddCallback(slider_v, IswNvalueChanged, slider_callback, (IswPointer)"Vertical");

    return box;
}

Widget create_scrollbar_demo(Widget parent) {
    Widget box, title, scrollbar;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("scrollbarBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Scrollbar Widget"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("scrollbarTitle", labelWidgetClass, box, args, n);
    
    /* Vertical scrollbar */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNwidth, 20); n++;
    IswSetArg(args[n], IswNheight, 100); n++;
    IswSetArg(args[n], IswNshown, 30); n++;
    scrollbar = IswCreateManagedWidget("scrollbar", scrollbarWidgetClass,
                                      box, args, n);
    
    return box;
}

Widget create_dialog_demo(Widget parent) {
    Widget box, title, dialog;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("dialogBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Dialog Widget"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("dialogTitle", labelWidgetClass, box, args, n);
    
    /* Dialog widget */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Enter name:"); n++;
    IswSetArg(args[n], IswNvalue, "John Doe"); n++;
    dialog = IswCreateManagedWidget("dialog", dialogWidgetClass, box, args, n);
    
    /* Add buttons */
    IswDialogAddButton(dialog, "OK", dialog_ok_callback, (IswPointer)dialog);
    IswDialogAddButton(dialog, "Cancel", NULL, NULL);

    /* A button that launches a real modal transient popup — for testing
     * that the focus manager runs Tab traversal inside the popup shell
     * and doesn't leak to the parent shell. */
    {
        Widget open_btn;
        Arg a[2];
        Cardinal m = 0;
        IswSetArg(a[m], IswNlabel, "Open Modal Dialog..."); m++;
        open_btn = IswCreateManagedWidget("openModal", commandWidgetClass,
                                          box, a, m);
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
    Arg args[10];
    Cardinal n;
    (void)w; (void)call_data;

    /* TransientShell so it gets WM decorations + stays above parent. */
    n = 0;
    IswSetArg(args[n], IswNtitle, "Modal Dialog"); n++;
    IswSetArg(args[n], IswNwidth, 360); n++;
    IswSetArg(args[n], IswNheight, 140); n++;
    shell = IswCreatePopupShell("modalTest", transientShellWidgetClass,
                                parent, args, n);

    form = IswCreateManagedWidget("form", formWidgetClass, shell, NULL, 0);

    n = 0;
    IswSetArg(args[n], IswNlabel, "Type here, then Tab to cycle:"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    label = IswCreateManagedWidget("lbl", labelWidgetClass, form, args, n);

    n = 0;
    IswSetArg(args[n], IswNfromVert, label); n++;
    IswSetArg(args[n], IswNeditType, IswtextEdit); n++;
    IswSetArg(args[n], IswNwidth, 300); n++;
    IswSetArg(args[n], IswNstring, ""); n++;
    IswSetArg(args[n], IswNconsumeTab, False); n++;
    text = IswCreateManagedWidget("entry", textWidgetClass, form, args, n);

    n = 0;
    IswSetArg(args[n], IswNfromVert, text); n++;
    IswSetArg(args[n], IswNlabel, "OK"); n++;
    ok = IswCreateManagedWidget("ok", commandWidgetClass, form, args, n);
    IswAddCallback(ok, IswNcallback, modal_close_cb, (IswPointer)shell);

    n = 0;
    IswSetArg(args[n], IswNfromVert, text); n++;
    IswSetArg(args[n], IswNfromHoriz, ok); n++;
    IswSetArg(args[n], IswNlabel, "Cancel"); n++;
    cancel = IswCreateManagedWidget("cancel", commandWidgetClass, form, args, n);
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
        Arg a[1];
        IswSetArg(a[0], IswNlabel, data->uris[0]);
        IswSetValues(w, a, 1);
    } else if (data->data && data->data_length > 0) {
        /* Non-URI drop — show raw text data */
        printf("Drop received: %lu bytes at (%d, %d)\n",
               data->data_length, data->x, data->y);
        char *text = IswMalloc(data->data_length + 1);
        memcpy(text, data->data, data->data_length);
        text[data->data_length] = '\0';
        printf("  data: %s\n", text);

        Arg a[1];
        IswSetArg(a[0], IswNlabel, text);
        IswSetValues(w, a, 1);
        IswFree(text);
    }
}

void drag_enter_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    (void) client_data;
    (void) call_data;
    Arg a[1];
    Pixel highlight;
    /* Use a simple visual cue — swap border width */
    IswSetArg(a[0], IswNborderWidth, 3);
    IswSetValues(w, a, 1);
    (void) highlight;
}

void drag_leave_callback(Widget w, IswPointer client_data, IswPointer call_data) {
    (void) client_data;
    (void) call_data;
    Arg a[1];
    IswSetArg(a[0], IswNborderWidth, 1);
    IswSetValues(w, a, 1);
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
    Arg args[1];
    
    IswSetArg(args[0], IswNvalue, &value);
    IswGetValues(dialog, args, 1);
    
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
    Arg args[10];
    Cardinal n;

    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    box = IswCreateManagedWidget("drawingAreaBox", boxWidgetClass, parent, args, n);

    n = 0;
    IswSetArg(args[n], IswNlabel, "DrawingArea Widget"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("drawingAreaTitle", labelWidgetClass, box, args, n);

    n = 0;
    IswSetArg(args[n], IswNwidth, 160); n++;
    IswSetArg(args[n], IswNheight, 160); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    canvas = IswCreateManagedWidget("canvas", drawingAreaWidgetClass, box, args, n);

    IswAddCallback(canvas, IswNexposeCallback, drawingarea_expose, NULL);

    return box;
}

Widget create_tabs_demo(Widget parent) {
    Widget section_box, title, tabs_widget;
    Widget tab1_content, tab2_content;
    Arg args[10];
    Cardinal n;

    /* Section container */
    n = 0;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 1); n++;
    section_box = IswCreateManagedWidget("tabsSection", boxWidgetClass,
                                         parent, args, n);

    /* Section title */
    n = 0;
    IswSetArg(args[n], IswNlabel, "Tabs Widget"); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    title = IswCreateManagedWidget("tabsTitle", labelWidgetClass,
                                   section_box, args, n);

    /* The Tabs widget */
    n = 0;
    IswSetArg(args[n], IswNwidth, 400); n++;
    IswSetArg(args[n], IswNheight, 180); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    tabs_widget = IswCreateManagedWidget("tabs", tabsWidgetClass,
                                         section_box, args, n);
    IswAddCallback(tabs_widget, IswNtabCallback, tabs_callback, NULL);

    /* Tab 1: a label */
    n = 0;
    IswSetArg(args[n], IswNtabLabel, "General"); n++;
    IswSetArg(args[n], IswNlabel, "This is the General tab.\n\n"
             "The Tabs widget is a Constraint\n"
             "container that shows one child at\n"
             "a time, with a clickable tab bar."); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    tab1_content = IswCreateManagedWidget("tab1", labelWidgetClass,
                                          tabs_widget, args, n);

    /* Tab 2: a box with buttons */
    n = 0;
    IswSetArg(args[n], IswNtabLabel, "Controls"); n++;
    IswSetArg(args[n], IswNorientation, XtorientVertical); n++;
    IswSetArg(args[n], IswNborderWidth, 0); n++;
    tab2_content = IswCreateManagedWidget("tab2", boxWidgetClass,
                                          tabs_widget, args, n);

    n = 0;
    IswSetArg(args[n], IswNlabel, "Button A"); n++;
    IswCreateManagedWidget("tab2BtnA", commandWidgetClass,
                           tab2_content, args, n);

    n = 0;
    IswSetArg(args[n], IswNlabel, "Button B"); n++;
    IswCreateManagedWidget("tab2BtnB", commandWidgetClass,
                           tab2_content, args, n);

    return section_box;
}
