/*
 * xaw3d_demo_3d.c - 3D Widget Variant Demonstration
 *
 * This program demonstrates the 3D visual capabilities of Isw3d widgets,
 * showcasing various shadow depths, colors, and 3D effects.
 *
 * Compile: See Makefile
 * Run: ./xaw3d_demo_3d
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
#include <ISW/SmeThreeD.h>

/* Text widgets */
#include <ISW/AsciiText.h>

/* Specialized widgets */
#include <ISW/Scrollbar.h>
#include <ISW/Dialog.h>

/* 3D specific */
#include <ISW/ThreeD.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * FORWARD DECLARATIONS
 * ============================================================ */

Widget create_main_window(Widget parent);
Widget create_title_section(Widget parent);
Widget create_shadow_depth_section(Widget parent);
Widget create_shadow_color_section(Widget parent);
Widget create_relief_section(Widget parent);
Widget create_3d_menu_section(Widget parent);
Widget create_3d_text_section(Widget parent);
Widget create_3d_scrollbar_section(Widget parent);

/* Callbacks */
void button_callback(Widget w, XtPointer client_data, XtPointer call_data);
void menu_callback(Widget w, XtPointer client_data, XtPointer call_data);
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data);

/* ============================================================
 * MAIN FUNCTION
 * ============================================================ */

int main(int argc, char *argv[]) {
    XtAppContext app_context;
    Widget toplevel, main_widget;
    Arg args[10];
    Cardinal n;
    
    /* Initialize X Toolkit */
    toplevel = XtAppInitialize(&app_context, "Isw3dDemo3D",
                               NULL, 0,
                               &argc, argv,
                               NULL, NULL, 0);
    
    /* Set main window size and title */
    n = 0;
    XtSetArg(args[n], XtNwidth, 900); n++;
    XtSetArg(args[n], XtNheight, 800); n++;
    XtSetArg(args[n], XtNtitle, "Isw3d 3D Widget Demonstration"); n++;
    XtSetValues(toplevel, args, n);
    
    /* Create main widget structure */
    main_widget = create_main_window(toplevel);
    
    printf("Isw3d 3D Widget Demo starting...\n");
    printf("This demo showcases 3D visual effects\n");
    printf("---------------------------------------\n\n");
    
    /* Realize and run */
    XtRealizeWidget(toplevel);
    XtAppMainLoop(app_context);
    
    return 0;
}

/* ============================================================
 * MAIN WINDOW CREATION
 * ============================================================ */

Widget create_main_window(Widget parent) {
    Widget paned;
    Arg args[10];
    Cardinal n;
    
    /* Create main Paned container */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNshadowWidth, 2); n++;
    paned = XtCreateManagedWidget("mainPane", panedWidgetClass,
                                  parent, args, n);
    
    /* Create demonstration sections */
    create_title_section(paned);
    create_shadow_depth_section(paned);
    create_shadow_color_section(paned);
    create_relief_section(paned);
    create_3d_menu_section(paned);
    create_3d_text_section(paned);
    create_3d_scrollbar_section(paned);
    
    return paned;
}

/* ============================================================
 * TITLE SECTION
 * ============================================================ */

Widget create_title_section(Widget parent) {
    Widget box, title, subtitle;
    Arg args[10];
    Cardinal n;
    
    /* Container with deep shadow */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    box = XtCreateManagedWidget("titleBox", boxWidgetClass, parent, args, n);
    
    /* Main title with heavy 3D effect */
    n = 0;
    XtSetArg(args[n], XtNlabel, "=== Isw3d 3D Widget Demonstration ==="); n++;
    XtSetArg(args[n], XtNjustify, XtJustifyCenter); n++;
    XtSetArg(args[n], XtNwidth, 880); n++;
    XtSetArg(args[n], XtNheight, 40); n++;
    XtSetArg(args[n], XtNshadowWidth, 5); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    title = XtCreateManagedWidget("mainTitle", labelWidgetClass, box, args, n);
    
    /* Subtitle */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Exploring Shadow Depth, Colors, and 3D Effects"); n++;
    XtSetArg(args[n], XtNjustify, XtJustifyCenter); n++;
    XtSetArg(args[n], XtNwidth, 880); n++;
    XtSetArg(args[n], XtNshadowWidth, 2); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    subtitle = XtCreateManagedWidget("subtitle", labelWidgetClass, box, args, n);
    
    return box;
}

/* ============================================================
 * SHADOW DEPTH SECTION
 * ============================================================ */

Widget create_shadow_depth_section(Widget parent) {
    Widget form, section_label;
    Widget btn0, btn1, btn2, btn3, btn4, btn5;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 10); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    form = XtCreateManagedWidget("depthForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Shadow Depth Demonstration (shadowWidth: 0, 1, 2, 3, 4, 5)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    section_label = XtCreateManagedWidget("depthLabel", labelWidgetClass, form, args, n);
    
    /* Button with shadowWidth = 0 */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Flat (0)"); n++;
    XtSetArg(args[n], XtNwidth, 120); n++;
    XtSetArg(args[n], XtNheight, 50); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    btn0 = XtCreateManagedWidget("btn0", commandWidgetClass, form, args, n);
    XtAddCallback(btn0, XtNcallback, button_callback, (XtPointer)"Shadow Width 0");
    
    /* Button with shadowWidth = 1 */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Subtle (1)"); n++;
    XtSetArg(args[n], XtNwidth, 120); n++;
    XtSetArg(args[n], XtNheight, 50); n++;
    XtSetArg(args[n], XtNshadowWidth, 1); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, btn0); n++;
    btn1 = XtCreateManagedWidget("btn1", commandWidgetClass, form, args, n);
    XtAddCallback(btn1, XtNcallback, button_callback, (XtPointer)"Shadow Width 1");
    
    /* Button with shadowWidth = 2 (default) */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Default (2)"); n++;
    XtSetArg(args[n], XtNwidth, 120); n++;
    XtSetArg(args[n], XtNheight, 50); n++;
    XtSetArg(args[n], XtNshadowWidth, 2); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, btn1); n++;
    btn2 = XtCreateManagedWidget("btn2", commandWidgetClass, form, args, n);
    XtAddCallback(btn2, XtNcallback, button_callback, (XtPointer)"Shadow Width 2");
    
    /* Button with shadowWidth = 3 */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Medium (3)"); n++;
    XtSetArg(args[n], XtNwidth, 120); n++;
    XtSetArg(args[n], XtNheight, 50); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, btn2); n++;
    btn3 = XtCreateManagedWidget("btn3", commandWidgetClass, form, args, n);
    XtAddCallback(btn3, XtNcallback, button_callback, (XtPointer)"Shadow Width 3");
    
    /* Button with shadowWidth = 4 */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Deep (4)"); n++;
    XtSetArg(args[n], XtNwidth, 120); n++;
    XtSetArg(args[n], XtNheight, 50); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, btn3); n++;
    btn4 = XtCreateManagedWidget("btn4", commandWidgetClass, form, args, n);
    XtAddCallback(btn4, XtNcallback, button_callback, (XtPointer)"Shadow Width 4");
    
    /* Button with shadowWidth = 5 */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Heavy (5)"); n++;
    XtSetArg(args[n], XtNwidth, 120); n++;
    XtSetArg(args[n], XtNheight, 50); n++;
    XtSetArg(args[n], XtNshadowWidth, 5); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, btn4); n++;
    btn5 = XtCreateManagedWidget("btn5", commandWidgetClass, form, args, n);
    XtAddCallback(btn5, XtNcallback, button_callback, (XtPointer)"Shadow Width 5");
    
    return form;
}

/* ============================================================
 * SHADOW COLOR SECTION
 * ============================================================ */

Widget create_shadow_color_section(Widget parent) {
    Widget form, section_label;
    Widget lbl1, lbl2, lbl3, lbl4;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 10); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    form = XtCreateManagedWidget("colorForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "3D Labels with Different Shadow Effects"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    section_label = XtCreateManagedWidget("colorLabel", labelWidgetClass, form, args, n);
    
    /* Label with light shadow */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Light Shadow Effect"); n++;
    XtSetArg(args[n], XtNwidth, 200); n++;
    XtSetArg(args[n], XtNheight, 60); n++;
    XtSetArg(args[n], XtNshadowWidth, 2); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    lbl1 = XtCreateManagedWidget("lightLabel", labelWidgetClass, form, args, n);
    
    /* Label with medium shadow */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Medium Shadow Effect"); n++;
    XtSetArg(args[n], XtNwidth, 200); n++;
    XtSetArg(args[n], XtNheight, 60); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, lbl1); n++;
    lbl2 = XtCreateManagedWidget("mediumLabel", labelWidgetClass, form, args, n);
    
    /* Label with deep shadow */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Deep Shadow Effect"); n++;
    XtSetArg(args[n], XtNwidth, 200); n++;
    XtSetArg(args[n], XtNheight, 60); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, lbl2); n++;
    lbl3 = XtCreateManagedWidget("deepLabel", labelWidgetClass, form, args, n);
    
    /* Label with very deep shadow */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Very Deep Shadow"); n++;
    XtSetArg(args[n], XtNwidth, 200); n++;
    XtSetArg(args[n], XtNheight, 60); n++;
    XtSetArg(args[n], XtNshadowWidth, 5); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, lbl3); n++;
    lbl4 = XtCreateManagedWidget("veryDeepLabel", labelWidgetClass, form, args, n);
    
    return form;
}

/* ============================================================
 * RELIEF SECTION
 * ============================================================ */

Widget create_relief_section(Widget parent) {
    Widget form, section_label;
    Widget box1, box2, box3;
    Widget content1, content2, content3;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 10); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    form = XtCreateManagedWidget("reliefForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "3D Box Containers with Different Shadow Depths"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    section_label = XtCreateManagedWidget("reliefLabel", labelWidgetClass, form, args, n);
    
    /* Box with raised relief */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNwidth, 250); n++;
    XtSetArg(args[n], XtNheight, 80); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    box1 = XtCreateManagedWidget("raisedBox", boxWidgetClass, form, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Raised Box (shadowWidth=3)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    content1 = XtCreateManagedWidget("raisedContent", labelWidgetClass, box1, args, n);
    
    /* Box with medium shadow */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNwidth, 250); n++;
    XtSetArg(args[n], XtNheight, 80); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, box1); n++;
    box2 = XtCreateManagedWidget("mediumBox", boxWidgetClass, form, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Medium Box (shadowWidth=4)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    content2 = XtCreateManagedWidget("mediumContent", labelWidgetClass, box2, args, n);
    
    /* Box with deep shadow */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNshadowWidth, 6); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNwidth, 250); n++;
    XtSetArg(args[n], XtNheight, 80); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, box2); n++;
    box3 = XtCreateManagedWidget("deepBox", boxWidgetClass, form, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Deep Box (shadowWidth=6)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    content3 = XtCreateManagedWidget("deepContent", labelWidgetClass, box3, args, n);
    
    return form;
}

/* ============================================================
 * 3D MENU SECTION
 * ============================================================ */

Widget create_3d_menu_section(Widget parent) {
    Widget form, section_label;
    Widget menu_button, menu;
    Widget entry1, entry2, entry3, line;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 10); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    form = XtCreateManagedWidget("menuForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "3D Menu with ThreeD Menu Entries"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    section_label = XtCreateManagedWidget("menuSectionLabel", labelWidgetClass, form, args, n);
    
    /* MenuButton with heavy 3D effect */
    n = 0;
    XtSetArg(args[n], XtNlabel, "3D Effects Menu"); n++;
    XtSetArg(args[n], XtNwidth, 200); n++;
    XtSetArg(args[n], XtNheight, 40); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    menu_button = XtCreateManagedWidget("3dMenuButton", menuButtonWidgetClass,
                                        form, args, n);
    
    /* Create popup menu */
    n = 0;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    menu = XtCreatePopupShell("3dMenu", simpleMenuWidgetClass,
                              menu_button, args, n);
    
    /* Menu entries with 3D effects */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Light 3D Effect"); n++;
    XtSetArg(args[n], XtNshadowWidth, 2); n++;
    entry1 = XtCreateManagedWidget("menu3dLight", smeBSBObjectClass, menu, args, n);
    XtAddCallback(entry1, XtNcallback, menu_callback, (XtPointer)"Light 3D");
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Medium 3D Effect"); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    entry2 = XtCreateManagedWidget("menu3dMedium", smeBSBObjectClass, menu, args, n);
    XtAddCallback(entry2, XtNcallback, menu_callback, (XtPointer)"Medium 3D");
    
    /* 3D separator line */
    n = 0;
    XtSetArg(args[n], XtNshadowWidth, 2); n++;
    line = XtCreateManagedWidget("menu3dLine", smeLineObjectClass, menu, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Deep 3D Effect"); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    entry3 = XtCreateManagedWidget("menu3dDeep", smeBSBObjectClass, menu, args, n);
    XtAddCallback(entry3, XtNcallback, menu_callback, (XtPointer)"Deep 3D");
    
    return form;
}

/* ============================================================
 * 3D TEXT SECTION
 * ============================================================ */

Widget create_3d_text_section(Widget parent) {
    Widget form, section_label, text;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 10); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    form = XtCreateManagedWidget("textForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "3D Text Widget with Deep Shadow"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    section_label = XtCreateManagedWidget("textSectionLabel", labelWidgetClass, form, args, n);
    
    /* Text widget with prominent 3D effect */
    n = 0;
    XtSetArg(args[n], XtNeditType, IswtextEdit); n++;
    XtSetArg(args[n], XtNwidth, 850); n++;
    XtSetArg(args[n], XtNheight, 100); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    XtSetArg(args[n], XtNscrollVertical, IswtextScrollAlways); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNstring,
             "This text widget demonstrates the 3D shadow effect.\n"
             "Notice the deep shadow border (shadowWidth=4) around this widget.\n"
             "The 3D effect gives a raised, tactile appearance to the interface.\n"
             "You can edit this text and see how the 3D effect enhances usability.\n"
             "The scrollbar also has 3D shadowing for a consistent look.\n"); n++;
    text = XtCreateManagedWidget("3dText", asciiTextWidgetClass, form, args, n);
    
    return form;
}

/* ============================================================
 * 3D SCROLLBAR SECTION
 * ============================================================ */

Widget create_3d_scrollbar_section(Widget parent) {
    Widget form, section_label;
    Widget box1, box2, box3;
    Widget scroll1, scroll2, scroll3;
    Widget label1, label2, label3;
    Arg args[10];
    Cardinal n;
    
    /* Section container */
    n = 0;
    XtSetArg(args[n], XtNborderWidth, 2); n++;
    XtSetArg(args[n], XtNdefaultDistance, 10); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    form = XtCreateManagedWidget("scrollForm", formWidgetClass, parent, args, n);
    
    /* Section label */
    n = 0;
    XtSetArg(args[n], XtNlabel, "3D Scrollbars with Varying Shadow Depths"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    section_label = XtCreateManagedWidget("scrollSectionLabel", labelWidgetClass, form, args, n);
    
    /* First scrollbar group - subtle shadow */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    box1 = XtCreateManagedWidget("scrollBox1", boxWidgetClass, form, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Subtle (2)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    label1 = XtCreateManagedWidget("scrollLabel1", labelWidgetClass, box1, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNwidth, 25); n++;
    XtSetArg(args[n], XtNheight, 150); n++;
    XtSetArg(args[n], XtNshadowWidth, 2); n++;
    XtSetArg(args[n], XtNshown, 30); n++;
    scroll1 = XtCreateManagedWidget("scrollbar1", scrollbarWidgetClass, box1, args, n);
    
    /* Second scrollbar group - medium shadow */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, box1); n++;
    box2 = XtCreateManagedWidget("scrollBox2", boxWidgetClass, form, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Medium (3)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    label2 = XtCreateManagedWidget("scrollLabel2", labelWidgetClass, box2, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNwidth, 25); n++;
    XtSetArg(args[n], XtNheight, 150); n++;
    XtSetArg(args[n], XtNshadowWidth, 3); n++;
    XtSetArg(args[n], XtNshown, 30); n++;
    scroll2 = XtCreateManagedWidget("scrollbar2", scrollbarWidgetClass, box2, args, n);
    
    /* Third scrollbar group - deep shadow */
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, box2); n++;
    box3 = XtCreateManagedWidget("scrollBox3", boxWidgetClass, form, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNlabel, "Deep (4)"); n++;
    XtSetArg(args[n], XtNborderWidth, 0); n++;
    XtSetArg(args[n], XtNshadowWidth, 0); n++;
    label3 = XtCreateManagedWidget("scrollLabel3", labelWidgetClass, box3, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNorientation, XtorientVertical); n++;
    XtSetArg(args[n], XtNwidth, 25); n++;
    XtSetArg(args[n], XtNheight, 150); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    XtSetArg(args[n], XtNshown, 30); n++;
    scroll3 = XtCreateManagedWidget("scrollbar3", scrollbarWidgetClass, box3, args, n);
    
    /* Add Quit button */
    n = 0;
    XtSetArg(args[n], XtNlabel, "Quit Demo"); n++;
    XtSetArg(args[n], XtNwidth, 150); n++;
    XtSetArg(args[n], XtNheight, 40); n++;
    XtSetArg(args[n], XtNshadowWidth, 4); n++;
    XtSetArg(args[n], XtNfromVert, section_label); n++;
    XtSetArg(args[n], XtNfromHoriz, box3); n++;
    XtSetArg(args[n], XtNhorizDistance, 50); n++;
    Widget quit_btn = XtCreateManagedWidget("quitBtn", commandWidgetClass, form, args, n);
    XtAddCallback(quit_btn, XtNcallback, quit_callback, NULL);
    
    return form;
}

/* ============================================================
 * CALLBACK FUNCTIONS
 * ============================================================ */

void button_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *button_name = (char *)client_data;
    printf("Button clicked: %s\n", button_name);
}

void menu_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char *item = (char *)client_data;
    printf("Menu item selected: %s\n", item);
}

void quit_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    printf("Quitting 3D demo...\n");
    exit(0);
}
