/*
 * xaw3d_demo.c - Comprehensive Isw3d Widget Demonstration
 *
 * This program demonstrates all major Isw3d widgets working with
 * the XCB-ported library and modified libXt.
 *
 * Compile: See Makefile
 * Run: ./xaw3d_demo
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

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
#include <ISW/List.h>
#include <ISW/ComboBox.h>
#include <ISW/Tree.h>

/* Layout widgets */
#include <ISW/Layout.h>
#include <ISW/Panner.h>
#include <ISW/Porthole.h>
#include <ISW/Reports.h>

/* Text widgets */
#include <ISW/AsciiText.h>
#include <ISW/Text.h>

/* Specialized widgets */
#include <ISW/SpinBox.h>
#include <ISW/Scale.h>
#include <ISW/StripChart.h>
#include <ISW/Tip.h>
#include <ISW/Scrollbar.h>
#include <ISW/Dialog.h>
#include <ISW/Repeater.h>
#include <ISW/Grip.h>
#include <ISW/ProgressBar.h>
#include <ISW/Tabs.h>

/* Optional tooltip support */
#ifdef HAVE_TIP
#include <ISW/Tip.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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
Widget create_list_demo(Widget parent);
Widget create_combobox_demo(Widget parent);
Widget create_text_demo(Widget parent);
Widget create_tree_demo(Widget parent);

Widget create_panner_demo(Widget parent);
Widget create_stripchart_demo(Widget parent);
Widget create_spinbox_demo(Widget parent);
Widget create_scale_demo(Widget parent);
Widget create_scrollbar_demo(Widget parent);
Widget create_progressbar_demo(Widget parent);
Widget create_dialog_demo(Widget parent);
Widget create_tabs_demo(Widget parent);
void tabs_callback(Widget w, XtPointer client_data, XtPointer call_data);

/* Callback functions */
void button_callback(Widget w, XtPointer client_data, XtPointer call_data);
void toggle_callback(Widget w, XtPointer client_data, XtPointer call_data);
void checkbox_callback(Widget w, XtPointer client_data, XtPointer call_data);
void menu_callback(Widget w, XtPointer client_data, XtPointer call_data);
void list_callback(Widget w, XtPointer client_data, XtPointer call_data);
void combobox_callback(Widget w, XtPointer client_data, XtPointer call_data);
void iconview_callback(Widget w, XtPointer client_data, XtPointer call_data);
void repeater_callback(Widget w, XtPointer client_data, XtPointer call_data);
void scale_callback(Widget w, XtPointer client_data, XtPointer call_data);
void spinbox_callback(Widget w, XtPointer client_data, XtPointer call_data);
void dialog_ok_callback(Widget w, XtPointer client_data, XtPointer call_data);
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data);

/* Menu bar callbacks */
void file_menu_callback(Widget w, XtPointer client_data, XtPointer call_data);
void edit_menu_callback(Widget w, XtPointer client_data, XtPointer call_data);
void about_menu_callback(Widget w, XtPointer client_data, XtPointer call_data);

/* Timer callbacks */
void update_stripchart(XtPointer client_data, XtIntervalId *id);

/* Tooltip helper */
void attach_tooltip(Widget widget, const char *tip_text);

/* Global for stripchart */
static double chart_value = 50.0;


/* HiDPI scaling for hardcoded dimensions */
static double demo_scale = 1.0;
#define S(x) ((int)((x) * demo_scale))

/* ============================================================
 * MAIN FUNCTION
 * ============================================================ */

int main(int argc, char *argv[]) {
    XtAppContext app_context;
    Widget toplevel, main_widget;
    Arg args[10];
    Cardinal n;
    
    /* Seed random number generator for stripchart */
    srand((unsigned int)time(NULL));
    
    /* Initialize X Toolkit with XCB backend */
    toplevel = XtAppInitialize(&app_context, "Isw3dDemo",
                               NULL, 0,
                               &argc, argv,
                               NULL, NULL, 0);
    
    /* Set HiDPI scale for demo dimensions */
    demo_scale = ISWScaleFactor(toplevel);

    /* Set main window size — not scaled, so it fits the screen at any DPI */
    n = 0;
    XtSetArg(args[n], XtNwidth, 1200); n++;
    XtSetArg(args[n], XtNheight, 900); n++;
    XtSetArg(args[n], XtNtitle, "Isw3d Widget Demonstration - Comprehensive Widget Showcase"); n++;
    XtSetArg(args[n], XtNallowShellResize, True); n++;
    XtSetValues(toplevel, args, n);
    
    /* Create main widget structure */
    main_widget = create_main_window(toplevel);
    
    printf("Isw3d Widget Demo starting...\n");
    printf("This demo showcases widgets with XCB backend\n");
    printf("---------------------------------------------\n");
    
    /* Print rendering backend information */
    ISWRenderPrintBackendInfo();
    printf("\n");
    
    /* Realize all widgets */
    XtRealizeWidget(toplevel);

    /* Enter event loop */
    XtAppMainLoop(app_context);
    
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
    XtSetArg(args[n], XtNwidth, 1200); n++;
    XtSetArg(args[n], XtNheight, 900); n++;
    main_win = XtCreateManagedWidget("mainWindow", mainWindowWidgetClass,
                                      parent, args, n);

    /* Populate the built-in menubar */
    populate_menubar(IswMainWindowGetMenuBar(main_win));

    /* Status bar at bottom — MainWindow auto-detects StatusBar children */
    {
        Widget statusbar, sb_label;
        Arg sb_args[4];
        Cardinal sn;

        sn = 0;
        statusbar = XtCreateManagedWidget("statusbar", statusBarWidgetClass,
                                           main_win, sb_args, sn);

        sn = 0;
        XtSetArg(sb_args[sn], XtNlabel, "Ready"); sn++;
        XtSetArg(sb_args[sn], XtNstatusStretch, True); sn++;
        sb_label = XtCreateManagedWidget("statusText", labelWidgetClass,
                                          statusbar, sb_args, sn);

        sn = 0;
        XtSetArg(sb_args[sn], XtNlabel, "Ln 1, Col 1"); sn++;
        XtCreateManagedWidget("statusPos", labelWidgetClass,
                               statusbar, sb_args, sn);
    }

    /* Viewport as content child — scrolls independently of menubar */
    n = 0;
    XtSetArg(args[n], XtNallowVert, True); n++;
    XtSetArg(args[n], XtNuseRight, True); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    viewport = XtCreateManagedWidget("viewport", viewportWidgetClass,
                                      main_win, args, n);

    /* Content box inside viewport — holds all demo sections */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    content_box = XtCreateManagedWidget("contentBox", boxWidgetClass,
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
    XtSetArg(args[n], XtNorientation, XtorientHorizontal); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    advanced_box = XtCreateManagedWidget("advancedBox", boxWidgetClass, content_box, args, n);

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
    Arg args[10];
    Cardinal n;

    /* === FILE MENU === */
    n = 0;
    XtSetArg(args[n], XtNlabel, "File"); n++;
    XtSetArg(args[n], XtNmenuName, "fileMenu"); n++;
    file_button = XtCreateManagedWidget("fileButton", menuButtonWidgetClass, menubar, args, n);
    
    n = 0;
    file_menu = XtCreatePopupShell("fileMenu", simpleMenuWidgetClass, file_button, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "New"); n++;
    entry = XtCreateManagedWidget("menuNew", smeBSBObjectClass, file_menu, args, n);
    XtAddCallback(entry, XtNcallback, file_menu_callback, (XtPointer)"New");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Open..."); n++;
    entry = XtCreateManagedWidget("menuOpen", smeBSBObjectClass, file_menu, args, n);
    XtAddCallback(entry, XtNcallback, file_menu_callback, (XtPointer)"Open");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Save"); n++;
    entry = XtCreateManagedWidget("menuSave", smeBSBObjectClass, file_menu, args, n);
    XtAddCallback(entry, XtNcallback, file_menu_callback, (XtPointer)"Save");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Save As..."); n++;
    entry = XtCreateManagedWidget("menuSaveAs", smeBSBObjectClass, file_menu, args, n);
    XtAddCallback(entry, XtNcallback, file_menu_callback, (XtPointer)"Save As");
    
    /* 3D separator */
    n = 0;
    XtCreateManagedWidget("sep1", smeLineObjectClass, file_menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Export"); n++;
    entry = XtCreateManagedWidget("menuExport", smeBSBObjectClass, file_menu, args, n);
    XtAddCallback(entry, XtNcallback, file_menu_callback, (XtPointer)"Export");
    
    n = 0;
    XtCreateManagedWidget("sep2", smeLineObjectClass, file_menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Quit"); n++;
    entry = XtCreateManagedWidget("menuQuit", smeBSBObjectClass, file_menu, args, n);
    XtAddCallback(entry, XtNcallback, quit_callback, NULL);
    
    /* === EDIT MENU === */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Edit"); n++;
    XtSetArg(args[n], XtNmenuName, "editMenu"); n++;
    edit_button = XtCreateManagedWidget("editButton", menuButtonWidgetClass, menubar, args, n);
    
    n = 0;
    edit_menu = XtCreatePopupShell("editMenu", simpleMenuWidgetClass, edit_button, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Undo"); n++;
    entry = XtCreateManagedWidget("menuUndo", smeBSBObjectClass, edit_menu, args, n);
    XtAddCallback(entry, XtNcallback, edit_menu_callback, (XtPointer)"Undo");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Redo"); n++;
    entry = XtCreateManagedWidget("menuRedo", smeBSBObjectClass, edit_menu, args, n);
    XtAddCallback(entry, XtNcallback, edit_menu_callback, (XtPointer)"Redo");
    
    n = 0;
    XtCreateManagedWidget("sep3", smeLineObjectClass, edit_menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Cut"); n++;
    entry = XtCreateManagedWidget("menuCut", smeBSBObjectClass, edit_menu, args, n);
    XtAddCallback(entry, XtNcallback, edit_menu_callback, (XtPointer)"Cut");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Copy"); n++;
    entry = XtCreateManagedWidget("menuCopy", smeBSBObjectClass, edit_menu, args, n);
    XtAddCallback(entry, XtNcallback, edit_menu_callback, (XtPointer)"Copy");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Paste"); n++;
    entry = XtCreateManagedWidget("menuPaste", smeBSBObjectClass, edit_menu, args, n);
    XtAddCallback(entry, XtNcallback, edit_menu_callback, (XtPointer)"Paste");
    
    n = 0;
    XtCreateManagedWidget("sep4", smeLineObjectClass, edit_menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Preferences..."); n++;
    entry = XtCreateManagedWidget("menuPrefs", smeBSBObjectClass, edit_menu, args, n);
    XtAddCallback(entry, XtNcallback, edit_menu_callback, (XtPointer)"Preferences");
    
    /* === ABOUT MENU === */
    n = 0;
    XtSetArg(args[n], XtNlabel, "About"); n++;
    XtSetArg(args[n], XtNmenuName, "aboutMenu"); n++;
    about_button = XtCreateManagedWidget("aboutButton", menuButtonWidgetClass, menubar, args, n);
    
    n = 0;
    about_menu = XtCreatePopupShell("aboutMenu", simpleMenuWidgetClass, about_button, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "About ISW Demo"); n++;
    entry = XtCreateManagedWidget("menuAbout", smeBSBObjectClass, about_menu, args, n);
    XtAddCallback(entry, XtNcallback, about_menu_callback, (XtPointer)"About");
    
    n = 0;
    XtCreateManagedWidget("sep5", smeLineObjectClass, about_menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "ISW Documentation"); n++;
    entry = XtCreateManagedWidget("menuDocs", smeBSBObjectClass, about_menu, args, n);
    XtAddCallback(entry, XtNcallback, about_menu_callback, (XtPointer)"Documentation");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Report Bug"); n++;
    entry = XtCreateManagedWidget("menuBug", smeBSBObjectClass, about_menu, args, n);
    XtAddCallback(entry, XtNcallback, about_menu_callback, (XtPointer)"Bug");
    
    n = 0;
    XtCreateManagedWidget("sep6", smeLineObjectClass, about_menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "License"); n++;
    entry = XtCreateManagedWidget("menuLicense", smeBSBObjectClass, about_menu, args, n);
    XtAddCallback(entry, XtNcallback, about_menu_callback, (XtPointer)"License");
    
}

Widget create_title_label(Widget parent) {
    Widget title;
    Arg args[10];
    Cardinal n;
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "=== Isw3d Widget Demonstration (XCB Backend) ==="); n++;
    XtSetArg(args[n], XtNjustify, XtJustifyCenter); n++;
    XtSetArg(args[n], XtNwidth, S(830)); n++;
    XtSetArg(args[n], XtNheight, S(35)); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    title = XtCreateManagedWidget("titleLabel", labelWidgetClass,
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
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("containersForm", formWidgetClass,
                                 parent, args, n);

    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Container Widgets: Toolbar, Box, Form, Viewport"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("containerLabel", labelWidgetClass,
                                          form, args, n);

    /* Toolbar demo */
    toolbar_demo = create_toolbar_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(toolbar_demo, args, n);

    /* Create demos */
    box_demo = create_box_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, toolbar_demo); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(box_demo, args, n);

    form_demo = create_form_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, box_demo); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(form_demo, args, n);

    viewport_demo = create_viewport_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, form_demo); n++;
    XtSetArg(args[n], XtNfromVert, box_demo); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(viewport_demo, args, n);

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
    Dimension btn_size = S(24);

    n = 0;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    toolbar = XtCreateManagedWidget("toolbar", toolbarWidgetClass, parent, args, n);

    /* Icon buttons with uniform size */
    n = 0;
    XtSetArg(args[n], XtNsvgData, svg_new); n++;
    XtSetArg(args[n], XtNlabel, ""); n++;
    XtSetArg(args[n], XtNwidth, btn_size); n++;
    XtSetArg(args[n], XtNheight, btn_size); n++;
    XtCreateManagedWidget("tbNew", commandWidgetClass, toolbar, args, n);

    n = 0;
    XtSetArg(args[n], XtNsvgData, svg_open); n++;
    XtSetArg(args[n], XtNlabel, ""); n++;
    XtSetArg(args[n], XtNwidth, btn_size); n++;
    XtSetArg(args[n], XtNheight, btn_size); n++;
    XtCreateManagedWidget("tbOpen", commandWidgetClass, toolbar, args, n);

    n = 0;
    XtSetArg(args[n], XtNsvgData, svg_save); n++;
    XtSetArg(args[n], XtNlabel, ""); n++;
    XtSetArg(args[n], XtNwidth, btn_size); n++;
    XtSetArg(args[n], XtNheight, btn_size); n++;
    XtCreateManagedWidget("tbSave", commandWidgetClass, toolbar, args, n);

    /* Separator */
    n = 0;
    XtSetArg(args[n], XtNlabel, ""); n++;
    XtSetArg(args[n], XtNwidth, S(2)); n++;
    XtSetArg(args[n], XtNheight, btn_size); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtCreateManagedWidget("tbSep", labelWidgetClass, toolbar, args, n);

    n = 0;
    XtSetArg(args[n], XtNsvgData, svg_cut); n++;
    XtSetArg(args[n], XtNlabel, ""); n++;
    XtSetArg(args[n], XtNwidth, btn_size); n++;
    XtSetArg(args[n], XtNheight, btn_size); n++;
    XtCreateManagedWidget("tbCut", commandWidgetClass, toolbar, args, n);

    n = 0;
    XtSetArg(args[n], XtNsvgData, svg_copy); n++;
    XtSetArg(args[n], XtNlabel, ""); n++;
    XtSetArg(args[n], XtNwidth, btn_size); n++;
    XtSetArg(args[n], XtNheight, btn_size); n++;
    XtCreateManagedWidget("tbCopy", commandWidgetClass, toolbar, args, n);

    n = 0;
    XtSetArg(args[n], XtNsvgData, svg_paste); n++;
    XtSetArg(args[n], XtNlabel, ""); n++;
    XtSetArg(args[n], XtNwidth, btn_size); n++;
    XtSetArg(args[n], XtNheight, btn_size); n++;
    XtCreateManagedWidget("tbPaste", commandWidgetClass, toolbar, args, n);

    return toolbar;
}

Widget create_box_demo(Widget parent) {
    Widget box_container, box, label1, label2, label3;
    Arg args[10];
    Cardinal n;
    
    /* Container with label */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box_container = XtCreateManagedWidget("boxContainer", boxWidgetClass,
                                          parent, args, n);
    
    /* Label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Box Widget (Horizontal)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtCreateManagedWidget("boxLabel", labelWidgetClass, box_container, args, n);
    
    /* Horizontal Box */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientHorizontal); n++;
    XtSetArg(args[n], XtNhSpace, 5); n++;
    XtSetArg(args[n], XtNvSpace, 5); n++;
    box = XtCreateManagedWidget("demoBox", boxWidgetClass,
                                box_container, args, n);
    
    /* Add three labels to box */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Item 1"); n++;
    label1 = XtCreateManagedWidget("boxItem1", labelWidgetClass, box, args, n);

    n = 0;
    XtSetArg(args[n], XtNlabel, "Item 2"); n++;
    label2 = XtCreateManagedWidget("boxItem2", labelWidgetClass, box, args, n);

    n = 0;
    XtSetArg(args[n], XtNlabel, "Item 3"); n++;
    label3 = XtCreateManagedWidget("boxItem3", labelWidgetClass, box, args, n);
    
    return box_container;
}

Widget create_form_demo(Widget parent) {
    Widget form, title_label, button1, button2, button3;
    Arg args[10];
    Cardinal n;
    
    /* Form container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    XtSetArg(args[n], XtNwidth, S(350)); n++;
    XtSetArg(args[n], XtNheight, S(80)); n++;
    form = XtCreateManagedWidget("demoForm", formWidgetClass, parent, args, n);
    
    /* Title label - top, spans width */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Form Widget (Constraint Layout)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    title_label = XtCreateManagedWidget("formTitle", labelWidgetClass,
                                        form, args, n);
    
    /* Button row - relative positioning */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Left"); n++;
    XtSetArg(args[n], XtNfromVert, title_label); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    button1 = XtCreateManagedWidget("formBtn1", commandWidgetClass,
                                    form, args, n);
    XtAddCallback(button1, XtNcallback, button_callback, (XtPointer)"Form Left Button");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Center"); n++;
    XtSetArg(args[n], XtNfromVert, title_label); n++;
    XtSetArg(args[n], XtNfromHoriz, button1); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    button2 = XtCreateManagedWidget("formBtn2", commandWidgetClass,
                                    form, args, n);
    XtAddCallback(button2, XtNcallback, button_callback, (XtPointer)"Form Center Button");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Right"); n++;
    XtSetArg(args[n], XtNfromVert, title_label); n++;
    XtSetArg(args[n], XtNfromHoriz, button2); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    button3 = XtCreateManagedWidget("formBtn3", commandWidgetClass,
                                    form, args, n);
    XtAddCallback(button3, XtNcallback, button_callback, (XtPointer)"Form Right Button");
    
    return form;
}

Widget create_viewport_demo(Widget parent) {
    Widget viewport_container, viewport, large_label;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    viewport_container = XtCreateManagedWidget("viewportContainer", boxWidgetClass,
                                               parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Viewport Widget (Scrollable)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtCreateManagedWidget("viewportTitle", labelWidgetClass,
                          viewport_container, args, n);
    
    /* Viewport with scrollbars */
    n = 0;
    XtSetArg(args[n], XtNwidth, S(250)); n++;
    XtSetArg(args[n], XtNheight, S(80)); n++;
    XtSetArg(args[n], XtNallowHoriz, True); n++;
    XtSetArg(args[n], XtNallowVert, True); n++;
    XtSetArg(args[n], XtNuseBottom, True); n++;
    XtSetArg(args[n], XtNuseRight, True); n++;
    viewport = XtCreateManagedWidget("viewport", viewportWidgetClass,
                                     viewport_container, args, n);
    
    /* Large content inside viewport */
    n = 0;
    XtSetArg(args[n], XtNlabel, 
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
    XtSetArg(args[n], XtNjustify, XtJustifyLeft); n++;
    XtSetArg(args[n], XtNwidth, S(400)); n++;
    XtSetArg(args[n], XtNheight, S(150)); n++;
    large_label = XtCreateManagedWidget("viewportContent", labelWidgetClass,
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
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("basicForm", formWidgetClass, parent, args, n);

    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Basic Interactive Widgets: Command, Toggle, Checkbox, Menu, Repeater"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("basicLabel", labelWidgetClass,
                                          form, args, n);

    /* Create widget demos */
    command_demo = create_command_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(command_demo, args, n);

    toggle_demo = create_toggle_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, command_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(toggle_demo, args, n);

    checkbox_demo = create_checkbox_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, toggle_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(checkbox_demo, args, n);

    menu_demo = create_menu_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, checkbox_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(menu_demo, args, n);

    repeater_demo = create_repeater_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, menu_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(repeater_demo, args, n);

    return form;
}

Widget create_command_demo(Widget parent) {
    Widget box, title, button1, button2, quit_button, svg_button;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("commandBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Command Buttons"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("commandTitle", labelWidgetClass, box, args, n);

    /* Buttons */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Click Me!"); n++;
    button1 = XtCreateManagedWidget("cmdBtn1", commandWidgetClass, box, args, n);
    XtAddCallback(button1, XtNcallback, button_callback, (XtPointer)"Button 1");
    attach_tooltip(button1, "This is a clickable command button");

    n = 0;
    XtSetArg(args[n], XtNlabel, "Or Me!"); n++;
    button2 = XtCreateManagedWidget("cmdBtn2", commandWidgetClass, box, args, n);
    XtAddCallback(button2, XtNcallback, button_callback, (XtPointer)"Button 2");
    attach_tooltip(button2, "Another command button with tooltip");

    /* SVG icon button */
    n = 0;
    XtSetArg(args[n], XtNsvgFile, "x11.svg"); n++;
    XtSetArg(args[n], XtNlabel, ""); n++;
    svg_button = XtCreateManagedWidget("svgBtn", commandWidgetClass, box, args, n);
    XtAddCallback(svg_button, XtNcallback, button_callback, (XtPointer)"SVG Button");
    attach_tooltip(svg_button, "Command button with SVG icon");

    n = 0;
    XtSetArg(args[n], XtNlabel, "Quit"); n++;
    quit_button = XtCreateManagedWidget("quitBtn", commandWidgetClass, box, args, n);
    XtAddCallback(quit_button, XtNcallback, quit_callback, NULL);
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
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("toggleBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Toggle (Radio) Buttons"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("toggleTitle", labelWidgetClass, box, args, n);
    
    /* Radio button group */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Option A"); n++;
    XtSetArg(args[n], XtNstate, True); n++;
    toggle1 = XtCreateManagedWidget("toggleA", toggleWidgetClass, box, args, n);
    XtAddCallback(toggle1, XtNcallback, toggle_callback, (XtPointer)"Option A");
    radio_group = toggle1;

    n = 0;
    XtSetArg(args[n], XtNlabel, "Option B"); n++;
    XtSetArg(args[n], XtNradioGroup, radio_group); n++;
    toggle2 = XtCreateManagedWidget("toggleB", toggleWidgetClass, box, args, n);
    XtAddCallback(toggle2, XtNcallback, toggle_callback, (XtPointer)"Option B");

    n = 0;
    XtSetArg(args[n], XtNlabel, "Option C"); n++;
    XtSetArg(args[n], XtNradioGroup, radio_group); n++;
    toggle3 = XtCreateManagedWidget("toggleC", toggleWidgetClass, box, args, n);
    XtAddCallback(toggle3, XtNcallback, toggle_callback, (XtPointer)"Option C");
    
    return box;
}

Widget create_checkbox_demo(Widget parent) {
    Widget box, title, cb1, cb2, cb3;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("checkboxBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Toggle (Checkbox) Buttons"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("checkboxTitle", labelWidgetClass, box, args, n);

    /* Standalone toggles (no radioGroup) render as checkboxes */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Enable notifications"); n++;
    XtSetArg(args[n], XtNstate, True); n++;
    cb1 = XtCreateManagedWidget("cb1", toggleWidgetClass, box, args, n);
    XtAddCallback(cb1, XtNcallback, checkbox_callback, (XtPointer)"Enable notifications");

    n = 0;
    XtSetArg(args[n], XtNlabel, "Dark mode"); n++;
    cb2 = XtCreateManagedWidget("cb2", toggleWidgetClass, box, args, n);
    XtAddCallback(cb2, XtNcallback, checkbox_callback, (XtPointer)"Dark mode");

    n = 0;
    XtSetArg(args[n], XtNlabel, "Auto-save"); n++;
    cb3 = XtCreateManagedWidget("cb3", toggleWidgetClass, box, args, n);
    XtAddCallback(cb3, XtNcallback, checkbox_callback, (XtPointer)"Auto-save");

    return box;
}

Widget create_menu_demo(Widget parent) {
    Widget box, title, menu_button, menu;
    Widget entry1, entry2, line, entry3;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("menuBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Menu Demo"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("menuTitle", labelWidgetClass, box, args, n);
    
    /* MenuButton */
    n = 0;
    XtSetArg(args[n], XtNlabel, "File Menu"); n++;
    menu_button = XtCreateManagedWidget("menuButton", menuButtonWidgetClass,
                                        box, args, n);
    
    /* Create SimpleMenu popup */
    n = 0;
    menu = XtCreatePopupShell("fileMenu", simpleMenuWidgetClass,
                              menu_button, args, n);
    
    /* Menu entries */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Open"); n++;
    entry1 = XtCreateManagedWidget("menuOpen", smeBSBObjectClass, menu, args, n);
    XtAddCallback(entry1, XtNcallback, menu_callback, (XtPointer)"Open");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Save"); n++;
    entry2 = XtCreateManagedWidget("menuSave", smeBSBObjectClass, menu, args, n);
    XtAddCallback(entry2, XtNcallback, menu_callback, (XtPointer)"Save");
    
    /* Separator line */
    n = 0;
    line = XtCreateManagedWidget("menuLine", smeLineObjectClass, menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Exit"); n++;
    entry3 = XtCreateManagedWidget("menuExit", smeBSBObjectClass, menu, args, n);
    XtAddCallback(entry3, XtNcallback, menu_callback, (XtPointer)"Exit");
    
    return box;
}

Widget create_repeater_demo(Widget parent) {
    Widget box, title, repeater;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("repeaterBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Repeater Button"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("repeaterTitle", labelWidgetClass, box, args, n);
    
    /* Repeater button - auto-repeats while held */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Hold Me"); n++;
    XtSetArg(args[n], XtNrepeatDelay, 500); n++;
    repeater = XtCreateManagedWidget("repeater", repeaterWidgetClass, box, args, n);
    XtAddCallback(repeater, XtNcallback, repeater_callback, NULL);
    
    return box;
}

/* ============================================================
 * SELECTION WIDGETS SECTION
 * ============================================================ */

Widget create_selection_section(Widget parent) {
    Widget form, section_label, iconview_demo, list_demo, combobox_demo, text_demo;
    Arg args[10];
    Cardinal n;

    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("selectionForm", formWidgetClass, parent, args, n);

    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Selection Widgets: IconView, List, ComboBox, AsciiText"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("selectionLabel", labelWidgetClass,
                                          form, args, n);

    /* IconView demo */
    iconview_demo = create_iconview_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(iconview_demo, args, n);

    /* List demo (classic multi-item list) */
    list_demo = create_list_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, iconview_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(list_demo, args, n);

    /* ComboBox demo (dropdown selector) */
    combobox_demo = create_combobox_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, list_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(combobox_demo, args, n);

    /* Text demo */
    text_demo = create_text_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, combobox_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(text_demo, args, n);

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
    XtSetArg(args[n], XtNallowVert, True); n++;
    XtSetArg(args[n], XtNwidth, S(200)); n++;
    XtSetArg(args[n], XtNheight, S(180)); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    viewport = XtCreateManagedWidget("iconViewport", viewportWidgetClass,
                                      parent, args, n);

    /* IconView inside viewport */
    n = 0;
    XtSetArg(args[n], XtNiconLabels, iv_labels); n++;
    XtSetArg(args[n], XtNiconData, iv_icons); n++;
    XtSetArg(args[n], XtNnumIcons, XtNumber(iv_labels)); n++;
    XtSetArg(args[n], XtNiconSize, 32); n++;
    XtSetArg(args[n], XtNwidth, S(200)); n++;
    iconview = XtCreateManagedWidget("iconView", iconViewWidgetClass,
                                      viewport, args, n);
    XtAddCallback(iconview, XtNselectCallback, iconview_callback, NULL);

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
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("listBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "List"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("listTitle", labelWidgetClass, box, args, n);

    /* Classic list widget — full multi-item display */
    n = 0;
    XtSetArg(args[n], XtNlist, items); n++;
    XtSetArg(args[n], XtNnumberStrings, XtNumber(items)); n++;
    XtSetArg(args[n], XtNdefaultColumns, 1); n++;
    XtSetArg(args[n], XtNforceColumns, True); n++;
    XtSetArg(args[n], XtNwidth, S(150)); n++;
    list = XtCreateManagedWidget("list", listWidgetClass, box, args, n);

    XtAddCallback(list, XtNcallback, list_callback, NULL);

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
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("comboBoxBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "ComboBox"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("comboBoxTitle", labelWidgetClass, box, args, n);

    /* ComboBox — List subclass with dropdown default */
    n = 0;
    XtSetArg(args[n], XtNlist, items); n++;
    XtSetArg(args[n], XtNnumberStrings, XtNumber(items)); n++;
    XtSetArg(args[n], XtNwidth, S(150)); n++;
    combo = XtCreateManagedWidget("comboBox", comboBoxWidgetClass, box, args, n);

    XtAddCallback(combo, XtNcallback, combobox_callback, NULL);

    return box;
}

Widget create_text_demo(Widget parent) {
    Widget box, title, text;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("textBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "AsciiText Widget (Editable)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("textTitle", labelWidgetClass, box, args, n);
    
    /* Editable text widget with scrollbars */
    n = 0;
    XtSetArg(args[n], XtNeditType, IswtextEdit); n++;
    XtSetArg(args[n], XtNwidth, S(450)); n++;
    XtSetArg(args[n], XtNheight, S(120)); n++;
    XtSetArg(args[n], XtNscrollVertical, IswtextScrollAlways); n++;
    XtSetArg(args[n], XtNstring,
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
    text = XtCreateManagedWidget("textEditor", asciiTextWidgetClass, box, args, n);
    
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
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("navigationForm", formWidgetClass,
                                 parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Navigation Widgets: Panner/Porthole"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("navigationLabel", labelWidgetClass,
                                          form, args, n);
    
    /* Create demos */
    panner_demo = create_panner_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(panner_demo, args, n);
    
    return form;
}

/* Panner report callback: user dragged the panner slider, move porthole content */
void panner_report_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    IswPannerReport *report = (IswPannerReport *)call_data;
    Widget content = (Widget)client_data;
    Arg args[2];
    Cardinal n = 0;

    if (report->changed & IswPRSliderX) {
        XtSetArg(args[n], XtNx, -report->slider_x); n++;
    }
    if (report->changed & IswPRSliderY) {
        XtSetArg(args[n], XtNy, -report->slider_y); n++;
    }
    if (n > 0)
        XtSetValues(content, args, n);
}

/* Porthole report callback: porthole moved its child, update panner slider */
void porthole_report_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    IswPannerReport *report = (IswPannerReport *)call_data;
    Widget panner = (Widget)client_data;
    Arg args[6];
    Cardinal n = 0;

    XtSetArg(args[n], XtNsliderX, report->slider_x); n++;
    XtSetArg(args[n], XtNsliderY, report->slider_y); n++;
    XtSetArg(args[n], XtNsliderWidth, report->slider_width); n++;
    XtSetArg(args[n], XtNsliderHeight, report->slider_height); n++;
    XtSetArg(args[n], XtNcanvasWidth, report->canvas_width); n++;
    XtSetArg(args[n], XtNcanvasHeight, report->canvas_height); n++;
    XtSetValues(panner, args, n);
}

Widget create_panner_demo(Widget parent) {
    Widget box, title, panner, porthole, large_widget;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("pannerBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Panner/Porthole"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("pannerTitle", labelWidgetClass, box, args, n);

    /* Panner widget (miniature navigator) */
    n = 0;
    XtSetArg(args[n], XtNwidth, S(200)); n++;
    XtSetArg(args[n], XtNheight, S(150)); n++;
    panner = XtCreateManagedWidget("panner", pannerWidgetClass, box, args, n);

    /* Porthole (viewing area) */
    n = 0;
    XtSetArg(args[n], XtNwidth, S(200)); n++;
    XtSetArg(args[n], XtNheight, S(150)); n++;
    porthole = XtCreateManagedWidget("porthole", portholeWidgetClass, box, args, n);

    /* Large widget inside porthole */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Large Content Area\n\n"\
             "This area is larger than the\n"\
             "visible porthole window.\n\n"\
             "Use the panner above to\n"\
             "navigate around this content."); n++;
    XtSetArg(args[n], XtNwidth, S(400)); n++;
    XtSetArg(args[n], XtNheight, S(300)); n++;
    large_widget = XtCreateManagedWidget("pannerContent", labelWidgetClass,
                                         porthole, args, n);

    /* Wire panner and porthole together */
    XtAddCallback(panner, XtNreportCallback, panner_report_callback,
                  (XtPointer)large_widget);
    XtAddCallback(porthole, XtNreportCallback, porthole_report_callback,
                  (XtPointer)panner);

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
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("treeBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Tree Widget (Hierarchical Structure)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("treeTitle", labelWidgetClass, box, args, n);
    
    /* Tree widget */
    n = 0;
    XtSetArg(args[n], XtNwidth, S(300)); n++;
    XtSetArg(args[n], XtNheight, S(200)); n++;
    XtSetArg(args[n], XtNautoReconfigure, True); n++;
    XtSetArg(args[n], XtNhSpace, 20); n++;
    XtSetArg(args[n], XtNvSpace, 10); n++;
    tree = XtCreateManagedWidget("tree", treeWidgetClass, box, args, n);
    
    /* Root node */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Root"); n++;
    node1 = XtCreateManagedWidget("node1", commandWidgetClass, tree, args, n);
    
    /* Child nodes of root */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Child 1"); n++;
    XtSetArg(args[n], XtNtreeParent, node1); n++;
    node2 = XtCreateManagedWidget("node2", commandWidgetClass, tree, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Child 2"); n++;
    XtSetArg(args[n], XtNtreeParent, node1); n++;
    node3 = XtCreateManagedWidget("node3", commandWidgetClass, tree, args, n);
    
    /* Grandchild nodes */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Grandchild 1"); n++;
    XtSetArg(args[n], XtNtreeParent, node2); n++;
    node4 = XtCreateManagedWidget("node4", commandWidgetClass, tree, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Grandchild 2"); n++;
    XtSetArg(args[n], XtNtreeParent, node3); n++;
    node5 = XtCreateManagedWidget("node5", commandWidgetClass, tree, args, n);
    
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
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("layoutBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Layout Widget (Constraint-based)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("layoutTitle", labelWidgetClass, box, args, n);
    
    /* Use Form widget to demonstrate constraint-based layout.
     * Form positions children by fromHoriz/fromVert and distances;
     * chain constraints control how they move on resize. We place a
     * hidden spacer to push "Top Right" to the right edge, and
     * compute an offset for "Bottom Center". */

    n = 0;
    XtSetArg(args[n], XtNwidth, S(300)); n++;
    XtSetArg(args[n], XtNheight, S(120)); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    XtSetArg(args[n], XtNdefaultDistance, S(8)); n++;
    layout = XtCreateManagedWidget("layout", formWidgetClass, box, args, n);

    /* Top Left: pinned to top-left */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Top Left"); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNbottom, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetArg(args[n], XtNright, XtChainLeft); n++;
    button1 = XtCreateManagedWidget("layoutBtn1", commandWidgetClass, layout, args, n);

    /* Top Right: pushed to right side via horizDistance */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Top Right"); n++;
    XtSetArg(args[n], XtNfromHoriz, button1); n++;
    XtSetArg(args[n], XtNhorizDistance, S(100)); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNbottom, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainRight); n++;
    XtSetArg(args[n], XtNright, XtChainRight); n++;
    button2 = XtCreateManagedWidget("layoutBtn2", commandWidgetClass, layout, args, n);

    /* Bottom Center: below button1, centered via horizDistance */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Bottom Center"); n++;
    XtSetArg(args[n], XtNfromVert, button1); n++;
    XtSetArg(args[n], XtNhorizDistance, S(80)); n++;
    XtSetArg(args[n], XtNtop, XtChainBottom); n++;
    XtSetArg(args[n], XtNbottom, XtChainBottom); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetArg(args[n], XtNright, XtChainLeft); n++;
    button3 = XtCreateManagedWidget("layoutBtn3", commandWidgetClass, layout, args, n);
    
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
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("gripBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Grip Widget (Pane Resizing)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("gripTitle", labelWidgetClass, box, args, n);
    
    /* Paned widget with visible grips */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNwidth, S(200)); n++;
    XtSetArg(args[n], XtNheight, S(200)); n++;
    paned = XtCreateManagedWidget("gripPaned", panedWidgetClass, box, args, n);
    
    /* First section */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Section 1\n(Drag grip to resize)"); n++;
    XtSetArg(args[n], XtNmin, 30); n++;
    XtSetArg(args[n], XtNmax, 150); n++;
    XtSetArg(args[n], XtNshowGrip, True); n++;
    section1 = XtCreateManagedWidget("section1", labelWidgetClass, paned, args, n);
    
    /* Grip is automatically created between panes */
    
    /* Second section */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Section 2"); n++;
    XtSetArg(args[n], XtNmin, 30); n++;
    XtSetArg(args[n], XtNmax, 150); n++;
    XtSetArg(args[n], XtNshowGrip, True); n++;
    section2 = XtCreateManagedWidget("section2", labelWidgetClass, paned, args, n);
    
    /* Third section */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Section 3"); n++;
    XtSetArg(args[n], XtNmin, 30); n++;
    XtSetArg(args[n], XtNskipAdjust, True); n++;
    section3 = XtCreateManagedWidget("section3", labelWidgetClass, paned, args, n);
    
    return box;
}

/* ============================================================
 * SPECIALIZED WIDGETS SECTION
 * ============================================================ */

Widget create_specialized_section(Widget parent) {
    Widget form, section_label;
    Widget spinbox_demo, scale_demo, stripchart_demo, scrollbar_demo, progressbar_demo, dialog_demo;
    Arg args[10];
    Cardinal n;

    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("specializedForm", formWidgetClass,
                                 parent, args, n);

    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Specialized Widgets: SpinBox, Scale, StripChart, Scrollbar, ProgressBar, Dialog"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("specializedLabel", labelWidgetClass,
                                          form, args, n);

    /* Create demos */
    spinbox_demo = create_spinbox_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(spinbox_demo, args, n);

    scale_demo = create_scale_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, spinbox_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(scale_demo, args, n);

    stripchart_demo = create_stripchart_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, scale_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(stripchart_demo, args, n);

    scrollbar_demo = create_scrollbar_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, stripchart_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(scrollbar_demo, args, n);

    progressbar_demo = create_progressbar_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, scrollbar_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(progressbar_demo, args, n);

    dialog_demo = create_dialog_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, progressbar_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(dialog_demo, args, n);

    return form;
}

Widget create_progressbar_demo(Widget parent) {
    Widget box, title, pb_h1, pb_h2, pb_v;
    Arg args[10];
    Cardinal n;

    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("progressBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "ProgressBar Widget"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("progressTitle", labelWidgetClass, box, args, n);

    /* Horizontal progress bar at 75% with text */
    n = 0;
    XtSetArg(args[n], XtNvalue, 75); n++;
    XtSetArg(args[n], XtNwidth, S(180)); n++;
    XtSetArg(args[n], XtNheight, S(24)); n++;
    XtSetArg(args[n], XtNshowValue, True); n++;
    pb_h1 = XtCreateManagedWidget("progressH1", progressBarWidgetClass, box, args, n);

    /* Horizontal progress bar at 30% without text */
    n = 0;
    XtSetArg(args[n], XtNvalue, 30); n++;
    XtSetArg(args[n], XtNwidth, S(180)); n++;
    XtSetArg(args[n], XtNheight, S(18)); n++;
    XtSetArg(args[n], XtNshowValue, False); n++;
    pb_h2 = XtCreateManagedWidget("progressH2", progressBarWidgetClass, box, args, n);

    /* Vertical progress bar at 60% with text */
    n = 0;
    XtSetArg(args[n], XtNvalue, 60); n++;
    XtSetArg(args[n], XtNwidth, S(30)); n++;
    XtSetArg(args[n], XtNheight, S(100)); n++;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNshowValue, True); n++;
    pb_v = XtCreateManagedWidget("progressV", progressBarWidgetClass, box, args, n);

    return box;
}

Widget create_spinbox_demo(Widget parent) {
    Widget box, title, spin1, spin2;
    Arg args[12];
    Cardinal n;

    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("spinBoxBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "SpinBox"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("spinBoxTitle", labelWidgetClass, box, args, n);

    /* SpinBox: 0-100, increment 1 */
    n = 0;
    XtSetArg(args[n], XtNspinMinimum, 0); n++;
    XtSetArg(args[n], XtNspinMaximum, 100); n++;
    XtSetArg(args[n], XtNspinValue, 50); n++;
    XtSetArg(args[n], XtNspinIncrement, 1); n++;
    XtSetArg(args[n], XtNwidth, S(120)); n++;
    spin1 = XtCreateManagedWidget("spin1", spinBoxWidgetClass, box, args, n);
    XtAddCallback(spin1, XtNvalueChanged, spinbox_callback, (XtPointer)"Spin1");

    /* SpinBox: wrapping, step 10 */
    n = 0;
    XtSetArg(args[n], XtNspinMinimum, 0); n++;
    XtSetArg(args[n], XtNspinMaximum, 255); n++;
    XtSetArg(args[n], XtNspinValue, 128); n++;
    XtSetArg(args[n], XtNspinIncrement, 10); n++;
    XtSetArg(args[n], XtNspinWrap, True); n++;
    XtSetArg(args[n], XtNwidth, S(120)); n++;
    spin2 = XtCreateManagedWidget("spin2", spinBoxWidgetClass, box, args, n);
    XtAddCallback(spin2, XtNvalueChanged, spinbox_callback, (XtPointer)"Spin2");

    return box;
}

Widget create_scale_demo(Widget parent) {
    Widget box, title, scale_h, scale_v;
    Arg args[12];
    Cardinal n;

    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("scaleBox", boxWidgetClass, parent, args, n);

    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Scale"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("scaleTitle", labelWidgetClass, box, args, n);

    /* Horizontal scale with ticks */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientHorizontal); n++;
    XtSetArg(args[n], XtNminimumValue, 0); n++;
    XtSetArg(args[n], XtNmaximumValue, 100); n++;
    XtSetArg(args[n], XtNscaleValue, 50); n++;
    XtSetArg(args[n], XtNtickInterval, 25); n++;
    XtSetArg(args[n], XtNshowValue, True); n++;
    XtSetArg(args[n], XtNwidth, S(200)); n++;
    XtSetArg(args[n], XtNheight, S(50)); n++;
    scale_h = XtCreateManagedWidget("scaleH", scaleWidgetClass, box, args, n);
    XtAddCallback(scale_h, XtNvalueChanged, scale_callback, (XtPointer)"Horizontal");

    /* Vertical scale */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNminimumValue, 0); n++;
    XtSetArg(args[n], XtNmaximumValue, 255); n++;
    XtSetArg(args[n], XtNscaleValue, 128); n++;
    XtSetArg(args[n], XtNshowValue, True); n++;
    XtSetArg(args[n], XtNwidth, S(70)); n++;
    XtSetArg(args[n], XtNheight, S(120)); n++;
    scale_v = XtCreateManagedWidget("scaleV", scaleWidgetClass, box, args, n);
    XtAddCallback(scale_v, XtNvalueChanged, scale_callback, (XtPointer)"Vertical");

    return box;
}

Widget create_stripchart_demo(Widget parent) {
    Widget box, title, chart;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("stripchartBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "StripChart (Live Update)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("chartTitle", labelWidgetClass, box, args, n);
    
    /* StripChart widget */
    n = 0;
    XtSetArg(args[n], XtNwidth, S(220)); n++;
    XtSetArg(args[n], XtNheight, S(100)); n++;
    XtSetArg(args[n], XtNupdate, 1); n++;
    XtSetArg(args[n], XtNminScale, 10); n++;
    chart = XtCreateManagedWidget("stripChart", stripChartWidgetClass,
                                  box, args, n);
    
    /* Start periodic updates */
    XtAppAddTimeOut(XtWidgetToApplicationContext(chart),
                    1000, update_stripchart, (XtPointer)chart);
    
    return box;
}

Widget create_scrollbar_demo(Widget parent) {
    Widget box, title, scrollbar;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("scrollbarBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Scrollbar Widget"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("scrollbarTitle", labelWidgetClass, box, args, n);
    
    /* Vertical scrollbar */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNwidth, S(20)); n++;
    XtSetArg(args[n], XtNheight, S(100)); n++;
    XtSetArg(args[n], XtNshown, 30); n++;
    scrollbar = XtCreateManagedWidget("scrollbar", scrollbarWidgetClass,
                                      box, args, n);
    
    return box;
}

Widget create_dialog_demo(Widget parent) {
    Widget box, title, dialog;
    Arg args[10];
    Cardinal n;
    
    /* Container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    box = XtCreateManagedWidget("dialogBox", boxWidgetClass, parent, args, n);
    
    /* Title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Dialog Widget"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("dialogTitle", labelWidgetClass, box, args, n);
    
    /* Dialog widget */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Enter name:"); n++;
    XtSetArg(args[n], XtNvalue, "John Doe"); n++;
    dialog = XtCreateManagedWidget("dialog", dialogWidgetClass, box, args, n);
    
    /* Add buttons */
    IswDialogAddButton(dialog, "OK", dialog_ok_callback, (XtPointer)dialog);
    IswDialogAddButton(dialog, "Cancel", NULL, NULL);
    
    return box;
}

/* ============================================================
 * CALLBACK FUNCTIONS
 * ============================================================ */

void button_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *button_name = (char *)client_data;
    printf("Button activated: %s\n", button_name);
}

void toggle_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *toggle_name = (char *)client_data;
    Boolean state = (Boolean)(intptr_t)call_data;
    printf("Toggle %s: %s\n", toggle_name, state ? "ON" : "OFF");
}

void checkbox_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *label = (char *)client_data;
    Boolean state = (Boolean)(intptr_t)call_data;
    printf("Checkbox '%s': %s\n", label, state ? "checked" : "unchecked");
}

void menu_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *item = (char *)client_data;
    printf("Menu item selected: %s\n", item);
    
    if (strcmp(item, "Exit") == 0) {
        printf("Exiting via menu...\n");
        exit(0);
    }
}

void list_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    IswListReturnStruct *item = (IswListReturnStruct *)call_data;
    printf("List item selected: %s (index %d)\n",
           item->string, item->list_index);
}

void scale_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    IswScaleCallbackData *data = (IswScaleCallbackData *)call_data;
    printf("Scale (%s) value: %d\n", (char *)client_data, data->value);
}

void spinbox_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    IswSpinBoxCallbackData *data = (IswSpinBoxCallbackData *)call_data;
    printf("SpinBox (%s) value: %d\n", (char *)client_data, data->value);
}

void iconview_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    IswIconViewCallbackData *data = (IswIconViewCallbackData *)call_data;
    printf("IconView selected: %s (index %d)\n",
           data->label ? data->label : "(null)", data->index);
}

void combobox_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    IswListReturnStruct *item = (IswListReturnStruct *)call_data;
    printf("ComboBox selected: %s (index %d)\n",
           item->string, item->list_index);
}


void repeater_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    static int count = 0;
    printf("Repeater activated: count = %d\n", ++count);
}

void dialog_ok_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    Widget dialog = (Widget)client_data;
    String value;
    Arg args[1];
    
    XtSetArg(args[0], XtNvalue, &value);
    XtGetValues(dialog, args, 1);
    
    printf("Dialog OK pressed. Value: %s\n", value);
}

void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Quit button pressed. Exiting...\n");
    exit(0);
}

/* ============================================================
 * MENU BAR CALLBACKS
 * ============================================================ */

void file_menu_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *item = (char *)client_data;
    printf("File menu item selected: %s\n", item);
    
    if (strcmp(item, "Quit") == 0) {
        quit_callback(w, NULL, NULL);
    }
}

void edit_menu_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *item = (char *)client_data;
    printf("Edit menu item selected: %s\n", item);
}

void about_menu_callback(Widget w, XtPointer client_data, XtPointer call_data) {
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

void update_stripchart(XtPointer client_data, XtIntervalId *id) {
    Widget chart = (Widget)client_data;
    
    /* Generate pseudo-random value (0-100) */
    chart_value += ((double)(rand() % 21) - 10.0);
    if (chart_value < 0.0) chart_value = 0.0;
    if (chart_value > 100.0) chart_value = 100.0;
    
    /* Update chart */
    /* Note: IswStripChartSetValue may not be available in all versions,
     * so this is a placeholder for the update mechanism */
    
    /* Re-schedule for next update */
    XtAppAddTimeOut(XtWidgetToApplicationContext(chart),
                    1000, update_stripchart, client_data);
}

/* ============================================================
 * TIP (TOOLTIP) HELPER FUNCTIONS
 * ============================================================ */

void attach_tooltip(Widget widget, const char *tip_text) {
    Widget tip;
    Arg args[5];
    Cardinal n;
    
    /* Create Tip widget as popup child */
    n = 0;
    XtSetArg(args[n], XtNlabel, tip_text); n++;
    tip = XtCreatePopupShell("tip", tipWidgetClass, widget, args, n);
}

/* ============================================================
 * TABS WIDGET DEMO
 * ============================================================ */

void tabs_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    TabsCallbackStruct *cbs = (TabsCallbackStruct *)call_data;
    printf("Tab switched to index %d (widget: %s)\n",
           cbs->tab_index, XtName(cbs->child));
}

Widget create_tabs_demo(Widget parent) {
    Widget section_box, title, tabs_widget;
    Widget tab1_content, tab2_content;
    Arg args[10];
    Cardinal n;

    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 1); n++;
    section_box = XtCreateManagedWidget("tabsSection", boxWidgetClass,
                                         parent, args, n);

    /* Section title */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Tabs Widget"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("tabsTitle", labelWidgetClass,
                                   section_box, args, n);

    /* The Tabs widget */
    n = 0;
    XtSetArg(args[n], XtNwidth, S(400)); n++;
    XtSetArg(args[n], XtNheight, S(180)); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    tabs_widget = XtCreateManagedWidget("tabs", tabsWidgetClass,
                                         section_box, args, n);
    XtAddCallback(tabs_widget, XtNtabCallback, tabs_callback, NULL);

    /* Tab 1: a label */
    n = 0;
    XtSetArg(args[n], XtNtabLabel, "General"); n++;
    XtSetArg(args[n], XtNlabel, "This is the General tab.\n\n"
             "The Tabs widget is a Constraint\n"
             "container that shows one child at\n"
             "a time, with a clickable tab bar."); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    tab1_content = XtCreateManagedWidget("tab1", labelWidgetClass,
                                          tabs_widget, args, n);

    /* Tab 2: a box with buttons */
    n = 0;
    XtSetArg(args[n], XtNtabLabel, "Controls"); n++;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    tab2_content = XtCreateManagedWidget("tab2", boxWidgetClass,
                                          tabs_widget, args, n);

    n = 0;
    XtSetArg(args[n], XtNlabel, "Button A"); n++;
    XtCreateManagedWidget("tab2BtnA", commandWidgetClass,
                           tab2_content, args, n);

    n = 0;
    XtSetArg(args[n], XtNlabel, "Button B"); n++;
    XtCreateManagedWidget("tab2BtnB", commandWidgetClass,
                           tab2_content, args, n);

    return section_box;
}
