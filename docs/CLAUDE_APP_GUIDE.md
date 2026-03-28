# ISW Application Developer Guide

This is a Claude Code instruction file for downstream consumers of the ISW widget library.

## Build Integration

```bash
pkg-config --cflags --libs isw
```

This gives you `-lISW` plus transitive XCB dependencies (`xcb`, `xcb-xrm`, `xcb-keysyms`, `xcb-xfixes`, `xcb-shape`). ISW embeds its own X Toolkit Intrinsics — do not link a separate `libXt`.

Required headers:

```c
#include <X11/Intrinsic.h>    /* XtAppInitialize, XtCreateManagedWidget, etc. */
#include <X11/StringDefs.h>   /* XtNlabel, XtNcallback, XtNforeground, etc. */
```

Widget-specific headers live under `<ISW/WidgetName.h>`.

## Application Initialization

```c
int main(int argc, char *argv[])
{
    XtAppContext app;
    Widget toplevel = XtAppInitialize(&app, "MyApp",
                                      NULL, 0, &argc, argv, NULL, NULL, 0);

    /* Create your widget tree here */

    XtRealizeWidget(toplevel);
    XtAppMainLoop(app);
    return 0;
}
```

## Widget Creation Pattern

All widgets use the Arg/Cardinal pattern:

```c
#include <ISW/Command.h>

Arg args[10];
Cardinal n = 0;
XtSetArg(args[n], XtNlabel, "Click Me"); n++;
Widget btn = XtCreateManagedWidget("btn", commandWidgetClass, parent, args, n);
XtAddCallback(btn, XtNcallback, my_callback, (XtPointer)"btn");
```

Callback signature:

```c
void my_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
    /* client_data is what you passed to XtAddCallback */
}
```

## Available Widgets

### Containers

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| MainWindow | `mainWindowWidgetClass` | `<ISW/MainWindow.h>` | App shell with menubar + status bar |
| Box | `boxWidgetClass` | `<ISW/Box.h>` | Row/column packing |
| Form | `formWidgetClass` | `<ISW/Form.h>` | Constraint-based layout (`XtNfromVert`, `XtNfromHoriz`) |
| Paned | `panedWidgetClass` | `<ISW/Paned.h>` | Vertically stacked panes with dividers |
| Viewport | `viewportWidgetClass` | `<ISW/Viewport.h>` | Scrollable clipped view |
| Tabs | `tabsWidgetClass` | `<ISW/Tabs.h>` | Tabbed pane switching |
| Porthole | `portholeWidgetClass` | `<ISW/Porthole.h>` | 2D scrollable viewport (with Panner) |

### Controls

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| Command | `commandWidgetClass` | `<ISW/Command.h>` | Push button |
| Toggle | `toggleWidgetClass` | `<ISW/Toggle.h>` | Checkbox / radio button (`XtNradioGroup`) |
| Repeater | `repeaterWidgetClass` | `<ISW/Repeater.h>` | Auto-repeating button |
| Scrollbar | `scrollbarWidgetClass` | `<ISW/Scrollbar.h>` | Scrollbar with arrows |
| Scale | `scaleWidgetClass` | `<ISW/Scale.h>` | Slider with numeric display |
| SpinBox | `spinBoxWidgetClass` | `<ISW/SpinBox.h>` | Numeric input with +/- buttons |
| ComboBox | `comboBoxWidgetClass` | `<ISW/ComboBox.h>` | Dropdown selection |

### Display

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| Label | `labelWidgetClass` | `<ISW/Label.h>` | Static text/bitmap/SVG display |
| Image | `imageWidgetClass` | `<ISW/Image.h>` | Pixmap display |
| StripChart | `stripChartWidgetClass` | `<ISW/StripChart.h>` | Real-time data graph |
| ProgressBar | `progressBarWidgetClass` | `<ISW/ProgressBar.h>` | Progress indicator |
| Tip | `tipWidgetClass` | `<ISW/Tip.h>` | Tooltip popup |
| StatusBar | `statusBarWidgetClass` | `<ISW/StatusBar.h>` | Status display area |

### Text

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| AsciiText | `asciiTextWidgetClass` | `<ISW/AsciiText.h>` | Text editor (ASCII) |
| Text | `textWidgetClass` | `<ISW/Text.h>` | Multi-line text (with TextSrc/TextSink) |
| Dialog | `dialogWidgetClass` | `<ISW/Dialog.h>` | Popup dialog with text input |

### Selection

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| List | `listWidgetClass` | `<ISW/List.h>` | Scrollable item list |
| IconView | `iconViewWidgetClass` | `<ISW/IconView.h>` | Scrollable icon grid with multiselect |
| Tree | `treeWidgetClass` | `<ISW/Tree.h>` | Hierarchical tree view |

### Dialogs

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| ColorPicker | `colorPickerWidgetClass` | `<ISW/ColorPicker.h>` | Color selection dialog |
| FontChooser | `fontChooserWidgetClass` | `<ISW/FontChooser.h>` | Font family/size selection |

## Menus

ISW menus use a three-layer hierarchy:

1. **MenuButton** — a button that opens a popup menu on click
2. **SimpleMenu** — the popup menu container (an OverrideShell)
3. **SmeBSB / SmeLine** — individual menu entries / separators

### Basic Menu

```c
#include <ISW/MenuButton.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>

/* 1. Create a MenuButton. XtNmenuName links it to the SimpleMenu by name. */
n = 0;
XtSetArg(args[n], XtNlabel, "File"); n++;
XtSetArg(args[n], XtNmenuName, "fileMenu"); n++;
Widget file_btn = XtCreateManagedWidget("fileBtn", menuButtonWidgetClass,
                                        menubar, args, n);

/* 2. Create the SimpleMenu as a popup shell. The widget name must match
      the XtNmenuName string above. The parent is the MenuButton. */
Widget file_menu = XtCreatePopupShell("fileMenu", simpleMenuWidgetClass,
                                      file_btn, NULL, 0);

/* 3. Add entries as children of the SimpleMenu. */
n = 0;
XtSetArg(args[n], XtNlabel, "New"); n++;
Widget item_new = XtCreateManagedWidget("new", smeBSBObjectClass,
                                        file_menu, args, n);
XtAddCallback(item_new, XtNcallback, file_cb, (XtPointer)"new");

/* Separator */
XtCreateManagedWidget("sep", smeLineObjectClass, file_menu, NULL, 0);

n = 0;
XtSetArg(args[n], XtNlabel, "Quit"); n++;
Widget item_quit = XtCreateManagedWidget("quit", smeBSBObjectClass,
                                         file_menu, args, n);
XtAddCallback(item_quit, XtNcallback, file_cb, (XtPointer)"quit");
```

### Cascade / Submenu

Any SmeBSB entry can open a submenu. Set `XtNmenuName` on the SmeBSB entry to the name of another SimpleMenu widget. The submenu pops up automatically on highlight and pops down when the cursor moves away.

```c
/* Parent menu already exists as file_menu (SimpleMenu). */

/* Create the submenu as a popup shell. The parent must be an ancestor of
   the SmeBSB entry — typically the same SimpleMenu or its parent. */
Widget export_menu = XtCreatePopupShell("exportMenu", simpleMenuWidgetClass,
                                        file_menu, NULL, 0);

/* Add entries to the submenu */
n = 0;
XtSetArg(args[n], XtNlabel, "PNG"); n++;
Widget png = XtCreateManagedWidget("png", smeBSBObjectClass,
                                   export_menu, args, n);
XtAddCallback(png, XtNcallback, export_cb, (XtPointer)"png");

n = 0;
XtSetArg(args[n], XtNlabel, "SVG"); n++;
Widget svg = XtCreateManagedWidget("svg", smeBSBObjectClass,
                                   export_menu, args, n);
XtAddCallback(svg, XtNcallback, export_cb, (XtPointer)"svg");

/* Now create the cascade entry in the parent menu.
   XtNmenuName must match the popup shell name ("exportMenu"). */
n = 0;
XtSetArg(args[n], XtNlabel, "Export"); n++;
XtSetArg(args[n], XtNmenuName, "exportMenu"); n++;
Widget export_entry = XtCreateManagedWidget("export", smeBSBObjectClass,
                                            file_menu, args, n);
/* No XtNcallback on a cascade entry — selection happens in the submenu. */
```

**How it works:** When the user highlights a SmeBSB entry that has `XtNmenuName` set, `SimpleMenu` automatically finds the named widget, positions it at the right edge of the current menu (or left edge if it would go off-screen), and pops it up. Moving the cursor into the submenu keeps it open. Moving away pops it down. Submenus can nest arbitrarily deep.

**Widget lookup:** `SimpleMenu` searches for the named menu by walking up the widget tree from itself, calling `XtNameToWidget` at each level. The submenu popup shell just needs to be findable from that search — making it a child of the parent menu or any ancestor works.

### Menu Bar

Use `MenuBar` (or `MainWindow` which includes one) as the container for `MenuButton` widgets:

```c
#include <ISW/MenuBar.h>

Widget menubar = XtCreateManagedWidget("menubar", menuBarWidgetClass,
                                       parent, NULL, 0);
/* Then create MenuButtons as children of menubar (as shown above). */
```

With `MainWindow`:

```c
#include <ISW/MainWindow.h>

Widget main_w = XtCreateManagedWidget("main", mainWindowWidgetClass,
                                      toplevel, NULL, 0);
Widget menubar = IswMainWindowGetMenuBar(main_w);
/* Create MenuButtons as children of menubar. */
```

### SmeBSB Resources

| Resource | Type | Default | Purpose |
|---|---|---|---|
| `XtNlabel` | String | widget name | Entry text |
| `XtNcallback` | Callback | NULL | Selection callback |
| `XtNmenuName` | String | NULL | Submenu name (cascade) |
| `XtNleftBitmap` | Pixmap | None | Left icon |
| `XtNrightBitmap` | Pixmap | None | Right icon |
| `XtNleftMargin` | Dimension | 4 | Left margin |
| `XtNrightMargin` | Dimension | 4 | Right margin |
| `XtNforeground` | Pixel | XtDefaultForeground | Text color |
| `XtNfont` | XFontStruct* | XtDefaultFont | Font |
| `XtNfontSet` | ISWFontSet* | XtDefaultFontSet | Font (internationalized) |
| `XtNunderline` | int | -1 | Keyboard mnemonic underline index |
| `XtNvertSpace` | int | 25 | Extra vertical space (% of font height) |

### Menu Callback Data

SmeBSB `XtNcallback` provides `NULL` as `call_data`. Use `client_data` to identify which entry was selected.

## SVG Icons

Label (and its subclasses Command, MenuButton, Toggle) support SVG via:

```c
XtSetArg(args[n], XtNsvgFile, "/path/to/icon.svg"); n++;
/* or inline SVG data: */
XtSetArg(args[n], XtNsvgData, svg_string); n++;
```

## Drag and Drop (XDND)

```c
#include <ISW/ISWXdnd.h>

/* Enable XDND on the toplevel shell */
ISWXdndEnable(toplevel);

/* Mark a widget as a drop target */
ISWXdndWidgetAcceptDrops(my_widget);
XtAddCallback(my_widget, XtNdropCallback, drop_cb, NULL);

void drop_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    IswDropCallbackData *data = (IswDropCallbackData *)call_data;
    for (int i = 0; i < data->num_uris; i++)
        printf("Dropped: %s\n", data->uris[i]);
}
```

## HiDPI / Scaling

```c
#include <ISW/ISWRender.h>

double scale = ISWScaleFactor(widget);       /* e.g. 1.0, 1.5, 2.0 */
Dimension d = ISWScaleDim(widget, 16);       /* 16 → 32 at 2x */
```

Widgets scale their own internal dimensions automatically. Use `ISWScaleDim` when you need to scale application-level pixel values (padding, icon sizes, etc.).

## Form Layout

Form is the primary constraint-based layout. Position children relative to each other:

```c
#include <ISW/Form.h>

n = 0;
XtSetArg(args[n], XtNlabel, "Name:"); n++;
Widget lbl = XtCreateManagedWidget("lbl", labelWidgetClass, form, args, n);

n = 0;
XtSetArg(args[n], XtNfromHoriz, lbl); n++;
Widget txt = XtCreateManagedWidget("txt", asciiTextWidgetClass, form, args, n);

n = 0;
XtSetArg(args[n], XtNfromVert, lbl); n++;
Widget btn = XtCreateManagedWidget("ok", commandWidgetClass, form, args, n);
```

## Toggle / Radio Groups

```c
#include <ISW/Toggle.h>

n = 0;
XtSetArg(args[n], XtNlabel, "Option A"); n++;
Widget a = XtCreateManagedWidget("a", toggleWidgetClass, parent, args, n);

n = 0;
XtSetArg(args[n], XtNlabel, "Option B"); n++;
XtSetArg(args[n], XtNradioGroup, a); n++;
Widget b = XtCreateManagedWidget("b", toggleWidgetClass, parent, args, n);
```

## IconView

```c
#include <ISW/IconView.h>

String labels[] = { "File 1", "File 2", "File 3" };
String icons[]  = { svg_data_1, svg_data_2, svg_data_3 };  /* SVG strings or NULL */
IswIconViewSetItems(iconview, labels, icons, 3);

/* call_data is IswIconViewCallbackData* */
void iconview_cb(Widget w, XtPointer cd, XtPointer call_data)
{
    IswIconViewCallbackData *d = (IswIconViewCallbackData *)call_data;
    printf("Selected: %s (index %d)\n", d->label, d->index);
}
```

## FontChooser

```c
#include <ISW/FontChooser.h>

/* call_data is IswFontChooserCallbackData* */
void font_cb(Widget w, XtPointer cd, XtPointer call_data)
{
    IswFontChooserCallbackData *d = (IswFontChooserCallbackData *)call_data;
    printf("Font: %s %d\n", d->family, d->size);
}
```

## Key Differences from Xaw/Xaw3d

- **Pure XCB** — no Xlib types. `Display*` → `xcb_connection_t*`, `Window` → `xcb_window_t`, `XEvent` → `xcb_generic_event_t*`.
- **Embedded libXt** — do not link a separate libXt. The Xt API (`XtCreateManagedWidget`, `XtAddCallback`, etc.) is provided by `libISW.so`.
- **Cairo rendering** — anti-aliased text and drawing by default via Cairo-XCB backend. The `ISW_RENDER_BACKEND` environment variable overrides backend selection.
- **HiDPI aware** — widgets auto-scale. Use `ISWScaleDim`/`ISWScaleFactor` for app-level dimensions.
- **SVG support** — Label/Command/Toggle can display SVG via `XtNsvgFile`/`XtNsvgData`.
- **New widgets** — MainWindow, MenuBar, Toolbar, StatusBar, Tabs, ComboBox, SpinBox, ProgressBar, IconView, ColorPicker, FontChooser, ScrollWheel.
