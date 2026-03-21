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

/* Basic display widgets */
#include <ISW/Label.h>
#include <ISW/Command.h>
#include <ISW/Toggle.h>

/* Menu widgets */
#include <ISW/MenuButton.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>

/* Rendering backend info */
#include <ISW/ISWRender.h>

/* Selection widgets */
#include <ISW/List.h>
#include <ISW/Tree.h>

/* Text widgets */
#include <ISW/AsciiText.h>
#include <ISW/Text.h>

/* Specialized widgets */
#include <ISW/StripChart.h>
#include <ISW/Scrollbar.h>
#include <ISW/Dialog.h>
#include <ISW/Repeater.h>
#include <ISW/Grip.h>

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
Widget create_title_label(Widget parent);

/* Section creation functions */
Widget create_containers_section(Widget parent);
Widget create_basic_widgets_section(Widget parent);
Widget create_selection_section(Widget parent);
Widget create_specialized_section(Widget parent);

/* Widget demo functions */
Widget create_box_demo(Widget parent);
Widget create_form_demo(Widget parent);
Widget create_viewport_demo(Widget parent);

Widget create_command_demo(Widget parent);
Widget create_toggle_demo(Widget parent);
Widget create_menu_demo(Widget parent);
Widget create_repeater_demo(Widget parent);

Widget create_list_demo(Widget parent);
Widget create_text_demo(Widget parent);

Widget create_stripchart_demo(Widget parent);
Widget create_scrollbar_demo(Widget parent);
Widget create_dialog_demo(Widget parent);

/* Callback functions */
void button_callback(Widget w, XtPointer client_data, XtPointer call_data);
void toggle_callback(Widget w, XtPointer client_data, XtPointer call_data);
void menu_callback(Widget w, XtPointer client_data, XtPointer call_data);
void list_callback(Widget w, XtPointer client_data, XtPointer call_data);
void repeater_callback(Widget w, XtPointer client_data, XtPointer call_data);
void dialog_ok_callback(Widget w, XtPointer client_data, XtPointer call_data);
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data);

/* Timer callbacks */
void update_stripchart(XtPointer client_data, XtIntervalId *id);

/* Global for stripchart */
static double chart_value = 50.0;

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
    
    /* Set main window size */
    n = 0;
    XtSetArg(args[n], XtNwidth, 850); n++;
    XtSetArg(args[n], XtNheight, 700); n++;
    XtSetArg(args[n], XtNtitle, "Isw3d Widget Demonstration"); n++;
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
    Widget paned, title;
    Arg args[10];
    Cardinal n;
    
    /* Create main Paned container for resizable sections */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    paned = XtCreateManagedWidget("mainPane", panedWidgetClass,
                                  parent, args, n);
    
    /* Title section */
    title = create_title_label(paned);
    
    /* Widget demonstration sections */
    create_containers_section(paned);
    create_basic_widgets_section(paned);
    create_selection_section(paned);
    create_specialized_section(paned);
    
    return paned;
}

Widget create_title_label(Widget parent) {
    Widget title;
    Arg args[10];
    Cardinal n;
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "=== Isw3d Widget Demonstration (XCB Backend) ==="); n++;
    XtSetArg(args[n], XtNjustify, XtJustifyCenter); n++;
    XtSetArg(args[n], XtNwidth, 830); n++;
    XtSetArg(args[n], XtNheight, 35); n++;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    title = XtCreateManagedWidget("titleLabel", labelWidgetClass,
                                  parent, args, n);
    
    return title;
}

/* ============================================================
 * CONTAINER WIDGETS SECTION
 * ============================================================ */

Widget create_containers_section(Widget parent) {
    Widget form, section_label;
    Widget box_demo, form_demo, viewport_demo;
    Arg args[10];
    Cardinal n;
    
    /* Create Form to hold container demos */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("containersForm", formWidgetClass,
                                 parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Container Widgets: Box, Form, Viewport"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("containerLabel", labelWidgetClass,
                                          form, args, n);
    
    /* Create demos */
    box_demo = create_box_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
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
    XtSetArg(args[n], XtNwidth, 60); n++;
    label1 = XtCreateManagedWidget("boxItem1", labelWidgetClass, box, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Item 2"); n++;
    XtSetArg(args[n], XtNwidth, 60); n++;
    label2 = XtCreateManagedWidget("boxItem2", labelWidgetClass, box, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Item 3"); n++;
    XtSetArg(args[n], XtNwidth, 60); n++;
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
    XtSetArg(args[n], XtNwidth, 350); n++;
    XtSetArg(args[n], XtNheight, 80); n++;
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
    XtSetArg(args[n], XtNwidth, 250); n++;
    XtSetArg(args[n], XtNheight, 80); n++;
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
    XtSetArg(args[n], XtNwidth, 400); n++;
    XtSetArg(args[n], XtNheight, 150); n++;
    large_label = XtCreateManagedWidget("viewportContent", labelWidgetClass,
                                        viewport, args, n);
    
    return viewport_container;
}

/* ============================================================
 * BASIC WIDGETS SECTION
 * ============================================================ */

Widget create_basic_widgets_section(Widget parent) {
    Widget form, section_label;
    Widget command_demo, toggle_demo, menu_demo, repeater_demo;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("basicForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Basic Interactive Widgets: Command, Toggle, Menu, Repeater"); n++;
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
    
    menu_demo = create_menu_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, toggle_demo); n++;
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
    Widget box, title, button1, button2, quit_button;
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
    XtSetArg(args[n], XtNwidth, 100); n++;
    button1 = XtCreateManagedWidget("cmdBtn1", commandWidgetClass, box, args, n);
    XtAddCallback(button1, XtNcallback, button_callback, (XtPointer)"Button 1");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Or Me!"); n++;
    XtSetArg(args[n], XtNwidth, 100); n++;
    button2 = XtCreateManagedWidget("cmdBtn2", commandWidgetClass, box, args, n);
    XtAddCallback(button2, XtNcallback, button_callback, (XtPointer)"Button 2");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Quit"); n++;
    XtSetArg(args[n], XtNwidth, 100); n++;
    quit_button = XtCreateManagedWidget("quitBtn", commandWidgetClass, box, args, n);
    XtAddCallback(quit_button, XtNcallback, quit_callback, NULL);
    
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
    XtSetArg(args[n], XtNwidth, 100); n++;
    toggle1 = XtCreateManagedWidget("toggleA", toggleWidgetClass, box, args, n);
    XtAddCallback(toggle1, XtNcallback, toggle_callback, (XtPointer)"Option A");
    radio_group = toggle1;
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Option B"); n++;
    XtSetArg(args[n], XtNradioGroup, radio_group); n++;
    XtSetArg(args[n], XtNwidth, 100); n++;
    toggle2 = XtCreateManagedWidget("toggleB", toggleWidgetClass, box, args, n);
    XtAddCallback(toggle2, XtNcallback, toggle_callback, (XtPointer)"Option B");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Option C"); n++;
    XtSetArg(args[n], XtNradioGroup, radio_group); n++;
    XtSetArg(args[n], XtNwidth, 100); n++;
    toggle3 = XtCreateManagedWidget("toggleC", toggleWidgetClass, box, args, n);
    XtAddCallback(toggle3, XtNcallback, toggle_callback, (XtPointer)"Option C");
    
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
    XtSetArg(args[n], XtNwidth, 100); n++;
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
    XtSetArg(args[n], XtNwidth, 100); n++;
    XtSetArg(args[n], XtNrepeatDelay, 500); n++;
    repeater = XtCreateManagedWidget("repeater", repeaterWidgetClass, box, args, n);
    XtAddCallback(repeater, XtNcallback, repeater_callback, NULL);
    
    return box;
}

/* ============================================================
 * SELECTION WIDGETS SECTION
 * ============================================================ */

Widget create_selection_section(Widget parent) {
    Widget form, section_label, list_demo, text_demo;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("selectionForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Selection & Text Widgets: List, AsciiText"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("selectionLabel", labelWidgetClass,
                                          form, args, n);
    
    /* List demo */
    list_demo = create_list_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(list_demo, args, n);
    
    /* Text demo */
    text_demo = create_text_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, list_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(text_demo, args, n);
    
    return form;
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
    XtSetArg(args[n], XtNlabel, "List Widget"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("listTitle", labelWidgetClass, box, args, n);
    
    /* List widget */
    n = 0;
    XtSetArg(args[n], XtNlist, items); n++;
    XtSetArg(args[n], XtNnumberStrings, XtNumber(items)); n++;
    XtSetArg(args[n], XtNdefaultColumns, 1); n++;
    XtSetArg(args[n], XtNforceColumns, True); n++;
    XtSetArg(args[n], XtNwidth, 150); n++;
    XtSetArg(args[n], XtNheight, 120); n++;
    list = XtCreateManagedWidget("list", listWidgetClass, box, args, n);
    
    XtAddCallback(list, XtNcallback, list_callback, NULL);
    
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
    XtSetArg(args[n], XtNwidth, 450); n++;
    XtSetArg(args[n], XtNheight, 120); n++;
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
 * SPECIALIZED WIDGETS SECTION
 * ============================================================ */

Widget create_specialized_section(Widget parent) {
    Widget form, section_label;
    Widget stripchart_demo, scrollbar_demo, dialog_demo;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 5); n++;
    form = XtCreateManagedWidget("specializedForm", formWidgetClass,
                                 parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Specialized Widgets: StripChart, Scrollbar, Dialog"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    section_label = XtCreateManagedWidget("specializedLabel", labelWidgetClass,
                                          form, args, n);
    
    /* Create demos */
    stripchart_demo = create_stripchart_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNtop, XtChainTop); n++;
    XtSetArg(args[n], XtNleft, XtChainLeft); n++;
    XtSetValues(stripchart_demo, args, n);
    
    scrollbar_demo = create_scrollbar_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, stripchart_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(scrollbar_demo, args, n);
    
    dialog_demo = create_dialog_demo(form);
    n = 0;
    XtSetArg(args[n], XtNfromHoriz, scrollbar_demo); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNhorizDistance, 10); n++;
    XtSetValues(dialog_demo, args, n);
    
    return form;
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
    XtSetArg(args[n], XtNwidth, 220); n++;
    XtSetArg(args[n], XtNheight, 100); n++;
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
    XtSetArg(args[n], XtNwidth, 20); n++;
    XtSetArg(args[n], XtNheight, 100); n++;
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
