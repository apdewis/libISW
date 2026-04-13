# ISW Application Developer Guide

This is a Claude Code instruction file for downstream consumers of the ISW widget library.

## Build Integration

```bash
pkg-config --cflags --libs isw
```

This gives you `-lISW` plus transitive XCB dependencies (`xcb`, `xcb-xrm`, `xcb-keysyms`, `xcb-xfixes`, `xcb-shape`). ISW embeds its own X Toolkit Intrinsics — do not link a separate `libXt`.

Required headers:

```c
#include <ISW/Intrinsic.h>    /* IswAppInitialize, IswCreateManagedWidget, etc. */
#include <ISW/StringDefs.h>   /* IswNlabel, IswNcallback, IswNforeground, etc. */
```

Widget-specific headers live under `<ISW/WidgetName.h>`.

## Application Initialization

```c
int main(int argc, char *argv[])
{
    IswAppContext app;
    Widget toplevel = IswAppInitialize(&app, "MyApp",
                                      NULL, 0, &argc, argv, NULL, NULL, 0);

    /* Create your widget tree here */

    IswRealizeWidget(toplevel);
    IswAppMainLoop(app);
    return 0;
}
```

## Widget Creation Pattern

All widgets use the Arg/Cardinal pattern:

```c
#include <ISW/Command.h>

Arg args[10];
Cardinal n = 0;
IswSetArg(args[n], IswNlabel, "Click Me"); n++;
Widget btn = IswCreateManagedWidget("btn", commandWidgetClass, parent, args, n);
IswAddCallback(btn, IswNcallback, my_callback, (IswPointer)"btn");
```

Callback signature:

```c
void my_callback(Widget w, IswPointer client_data, IswPointer call_data)
{
    /* client_data is what you passed to IswAddCallback */
}
```

## Available Widgets

### Containers

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| MainWindow | `mainWindowWidgetClass` | `<ISW/MainWindow.h>` | App shell with menubar + status bar |
| Box | `boxWidgetClass` | `<ISW/Box.h>` | Row/column packing |
| Form | `formWidgetClass` | `<ISW/Form.h>` | Constraint-based layout (`IswNfromVert`, `IswNfromHoriz`) |
| Paned | `panedWidgetClass` | `<ISW/Paned.h>` | Vertically stacked panes with dividers |
| Viewport | `viewportWidgetClass` | `<ISW/Viewport.h>` | Scrollable clipped view |
| Tabs | `tabsWidgetClass` | `<ISW/Tabs.h>` | Tabbed pane switching |
| Porthole | `portholeWidgetClass` | `<ISW/Porthole.h>` | 2D scrollable viewport (with Panner) |

### Controls

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| Command | `commandWidgetClass` | `<ISW/Command.h>` | Push button |
| Toggle | `toggleWidgetClass` | `<ISW/Toggle.h>` | Checkbox / radio button (`IswNradioGroup`) |
| Repeater | `repeaterWidgetClass` | `<ISW/Repeater.h>` | Auto-repeating button |
| Scrollbar | `scrollbarWidgetClass` | `<ISW/Scrollbar.h>` | Scrollbar with arrows |
| Slider | `sliderWidgetClass` | `<ISW/Slider.h>` | Slider with numeric display |
| SpinBox | `spinBoxWidgetClass` | `<ISW/SpinBox.h>` | Numeric input with +/- buttons |
| ComboBox | `comboBoxWidgetClass` | `<ISW/ComboBox.h>` | Dropdown selection |

### Display

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| Label | `labelWidgetClass` | `<ISW/Label.h>` | Static text or image display |
| Image | `imageWidgetClass` | `<ISW/Image.h>` | Scalable image display |
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
| ListView | `listViewWidgetClass` | `<ISW/ListView.h>` | Multi-column list with resizable columns, multiselect, rubberband |
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

/* 1. Create a MenuButton. IswNmenuName links it to the SimpleMenu by name. */
n = 0;
IswSetArg(args[n], IswNlabel, "File"); n++;
IswSetArg(args[n], IswNmenuName, "fileMenu"); n++;
Widget file_btn = IswCreateManagedWidget("fileBtn", menuButtonWidgetClass,
                                        menubar, args, n);

/* 2. Create the SimpleMenu as a popup shell. The widget name must match
      the IswNmenuName string above. The parent is the MenuButton. */
Widget file_menu = IswCreatePopupShell("fileMenu", simpleMenuWidgetClass,
                                      file_btn, NULL, 0);

/* 3. Add entries as children of the SimpleMenu. */
n = 0;
IswSetArg(args[n], IswNlabel, "New"); n++;
Widget item_new = IswCreateManagedWidget("new", smeBSBObjectClass,
                                        file_menu, args, n);
IswAddCallback(item_new, IswNcallback, file_cb, (IswPointer)"new");

/* Separator */
IswCreateManagedWidget("sep", smeLineObjectClass, file_menu, NULL, 0);

n = 0;
IswSetArg(args[n], IswNlabel, "Quit"); n++;
Widget item_quit = IswCreateManagedWidget("quit", smeBSBObjectClass,
                                         file_menu, args, n);
IswAddCallback(item_quit, IswNcallback, file_cb, (IswPointer)"quit");
```

### Cascade / Submenu

Any SmeBSB entry can open a submenu. Set `IswNmenuName` on the SmeBSB entry to the name of another SimpleMenu widget. The submenu pops up automatically on highlight and pops down when the cursor moves away.

```c
/* Parent menu already exists as file_menu (SimpleMenu). */

/* Create the submenu as a popup shell. The parent must be an ancestor of
   the SmeBSB entry — typically the same SimpleMenu or its parent. */
Widget export_menu = IswCreatePopupShell("exportMenu", simpleMenuWidgetClass,
                                        file_menu, NULL, 0);

/* Add entries to the submenu */
n = 0;
IswSetArg(args[n], IswNlabel, "PNG"); n++;
Widget png = IswCreateManagedWidget("png", smeBSBObjectClass,
                                   export_menu, args, n);
IswAddCallback(png, IswNcallback, export_cb, (IswPointer)"png");

n = 0;
IswSetArg(args[n], IswNlabel, "SVG"); n++;
Widget svg = IswCreateManagedWidget("svg", smeBSBObjectClass,
                                   export_menu, args, n);
IswAddCallback(svg, IswNcallback, export_cb, (IswPointer)"svg");

/* Now create the cascade entry in the parent menu.
   IswNmenuName must match the popup shell name ("exportMenu"). */
n = 0;
IswSetArg(args[n], IswNlabel, "Export"); n++;
IswSetArg(args[n], IswNmenuName, "exportMenu"); n++;
Widget export_entry = IswCreateManagedWidget("export", smeBSBObjectClass,
                                            file_menu, args, n);
/* No IswNcallback on a cascade entry — selection happens in the submenu. */
```

**How it works:** When the user highlights a SmeBSB entry that has `IswNmenuName` set, `SimpleMenu` automatically finds the named widget, positions it at the right edge of the current menu (or left edge if it would go off-screen), and pops it up. Moving the cursor into the submenu keeps it open. Moving away pops it down. Submenus can nest arbitrarily deep.

**Widget lookup:** `SimpleMenu` searches for the named menu by walking up the widget tree from itself, calling `IswNameToWidget` at each level. The submenu popup shell just needs to be findable from that search — making it a child of the parent menu or any ancestor works.

### Menu Bar

Use `MenuBar` (or `MainWindow` which includes one) as the container for `MenuButton` widgets:

```c
#include <ISW/MenuBar.h>

Widget menubar = IswCreateManagedWidget("menubar", menuBarWidgetClass,
                                       parent, NULL, 0);
/* Then create MenuButtons as children of menubar (as shown above). */
```

With `MainWindow`:

```c
#include <ISW/MainWindow.h>

Widget main_w = IswCreateManagedWidget("main", mainWindowWidgetClass,
                                      toplevel, NULL, 0);
Widget menubar = IswMainWindowGetMenuBar(main_w);
/* Create MenuButtons as children of menubar. */
```

### SmeBSB Resources

| Resource | Type | Default | Purpose |
|---|---|---|---|
| `IswNlabel` | String | widget name | Entry text |
| `IswNcallback` | Callback | NULL | Selection callback |
| `IswNmenuName` | String | NULL | Submenu name (cascade) |
| `IswNleftImage` | String | NULL | Left icon (file path or inline SVG) |
| `IswNrightImage` | String | NULL | Right icon (file path or inline SVG) |
| `IswNleftMargin` | Dimension | 4 | Left margin |
| `IswNrightMargin` | Dimension | 4 | Right margin |
| `IswNforeground` | Pixel | IswDefaultForeground | Text color |
| `IswNfont` | IswFontStruct* | IswDefaultFont | Font |
| `IswNfontSet` | ISWFontSet* | IswDefaultFontSet | Font (internationalized) |
| `IswNunderline` | int | -1 | Keyboard mnemonic underline index |
| `IswNvertSpace` | int | 25 | Extra vertical space (% of font height) |

### Menu Callback Data

SmeBSB `IswNcallback` provides `NULL` as `call_data`. Use `client_data` to identify which entry was selected.

## Images

Label (and its subclasses Command, MenuButton, Toggle) display images via the unified `IswNimage` and `IswNleftImage` resources. Format is auto-detected from the source string:

- File path ending in `.svg` → SVG (vector, scales to any size)
- File path ending in `.png` → PNG (raster, displayed at native resolution)
- String starting with `<` → inline SVG XML data

```c
/* SVG file */
IswSetArg(args[n], IswNimage, "icon.svg"); n++;

/* PNG file */
IswSetArg(args[n], IswNimage, "photo.png"); n++;

/* Inline SVG */
IswSetArg(args[n], IswNimage, "<svg viewBox='0 0 24 24'>...</svg>"); n++;

/* Icon beside text (does not replace the label) */
IswSetArg(args[n], IswNleftImage, "bullet.png"); n++;
```

`IswNimage` replaces the text label entirely. `IswNleftImage` draws an icon to the left of the text.

For menu entries (`SmeBSB`), use `IswNleftImage` and `IswNrightImage` the same way.

SVG images support `currentColor` — occurrences are automatically substituted with the widget's `IswNforeground` color and update when the foreground changes.

File paths are resolved through ISW's search path: executable directory, `$ISW_DATA_PATH`, `$XDG_DATA_HOME/isw/`, system data dirs, then cwd.

## Drag and Drop (XDND)

```c
#include <ISW/ISWXdnd.h>

/* Enable XDND on the toplevel shell */
ISWXdndEnable(toplevel);

/* Mark a widget as a drop target */
ISWXdndWidgetAcceptDrops(my_widget);
IswAddCallback(my_widget, IswNdropCallback, drop_cb, NULL);

void drop_cb(Widget w, IswPointer client_data, IswPointer call_data)
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
IswSetArg(args[n], IswNlabel, "Name:"); n++;
Widget lbl = IswCreateManagedWidget("lbl", labelWidgetClass, form, args, n);

n = 0;
IswSetArg(args[n], IswNfromHoriz, lbl); n++;
Widget txt = IswCreateManagedWidget("txt", asciiTextWidgetClass, form, args, n);

n = 0;
IswSetArg(args[n], IswNfromVert, lbl); n++;
Widget btn = IswCreateManagedWidget("ok", commandWidgetClass, form, args, n);
```

## Toggle / Radio Groups

```c
#include <ISW/Toggle.h>

n = 0;
IswSetArg(args[n], IswNlabel, "Option A"); n++;
Widget a = IswCreateManagedWidget("a", toggleWidgetClass, parent, args, n);

n = 0;
IswSetArg(args[n], IswNlabel, "Option B"); n++;
IswSetArg(args[n], IswNradioGroup, a); n++;
Widget b = IswCreateManagedWidget("b", toggleWidgetClass, parent, args, n);
```

## IconView

```c
#include <ISW/IconView.h>

String labels[] = { "File 1", "File 2", "File 3" };
String icons[]  = { svg_data_1, svg_data_2, svg_data_3 };  /* SVG strings or NULL */
IswIconViewSetItems(iconview, labels, icons, 3);

/* call_data is IswIconViewCallbackData* */
void iconview_cb(Widget w, IswPointer cd, IswPointer call_data)
{
    IswIconViewCallbackData *d = (IswIconViewCallbackData *)call_data;
    printf("Selected: %s (index %d)\n", d->label, d->index);
}
```

## ListView

```c
#include <ISW/ListView.h>

/* Define columns */
IswListViewColumn cols[] = {
    {"Name", 150, 60},   /* title, width, min_width */
    {"Size",  80, 40},
};

/* Flat row-major data: data[row * ncols + col] */
String cell_data[] = {
    "report.pdf", "2.4 MB",
    "photo.jpg",  "3.1 MB",
};

n = 0;
IswSetArg(args[n], IswNlistViewColumns, cols); n++;
IswSetArg(args[n], IswNnumColumns, 2); n++;
IswSetArg(args[n], IswNlistViewData, cell_data); n++;
IswSetArg(args[n], IswNnumRows, 2); n++;
IswSetArg(args[n], IswNmultiSelect, True); n++;
Widget lv = IswCreateManagedWidget("lv", listViewWidgetClass, viewport, args, n);

/* Dynamic column addition */
IswListViewAddColumn(lv, "Type", 100, 50);

/* call_data is IswListViewCallbackData* */
void listview_cb(Widget w, IswPointer cd, IswPointer call_data)
{
    IswListViewCallbackData *d = (IswListViewCallbackData *)call_data;
    printf("Row %d, Col %d, %d selected\n", d->row, d->column, d->num_selected);
}

/* call_data is IswListViewReorderCallbackData* — fired on header click */
void reorder_cb(Widget w, IswPointer cd, IswPointer call_data)
{
    IswListViewReorderCallbackData *d = (IswListViewReorderCallbackData *)call_data;
    /* d->column: which column was clicked */
    /* d->direction: IswListViewSortAscending or IswListViewSortDescending */
    /* Application should re-sort data and call IswListViewSetData() */
}

/* Programmatic sort indicator (without triggering callback) */
IswListViewSetSort(lv, 0, IswListViewSortAscending);
```

**Resources:** `listViewColumns` (IswListViewColumn*), `numColumns`, `numRows`, `listViewData` (flat String*), `multiSelect`, `showHeader`, `rowHeight`, `headerHeight`, `cursorRow`, `reorderCallback`.

**API:** `IswListViewSetData`, `IswListViewSetColumns`, `IswListViewAddColumn`, `IswListViewGetSelected`, `IswListViewGetSelectedRows`, `IswListViewBandActive`, `IswListViewSetSort`.

**Features:** Resizable column headers (drag separator), column sort indicators (arrow icons toggled on header click with `reorderCallback`), rubberband row selection, Ctrl+click toggle, Shift+click range, Ctrl+A select all, keyboard Up/Down/Home/End navigation with Shift-extend, alternating row tint.

## FontChooser

```c
#include <ISW/FontChooser.h>

/* call_data is IswFontChooserCallbackData* */
void font_cb(Widget w, IswPointer cd, IswPointer call_data)
{
    IswFontChooserCallbackData *d = (IswFontChooserCallbackData *)call_data;
    printf("Font: %s %d\n", d->family, d->size);
}
```

## Key Differences from Xaw/Xaw3d

- **Pure XCB** — no Xlib types. `Display*` → `xcb_connection_t*`, `Window` → `xcb_window_t`, `XEvent` → `xcb_generic_event_t*`.
- **Embedded libXt** — do not link a separate libXt. The Xt API (`IswCreateManagedWidget`, `IswAddCallback`, etc.) is provided by `libISW.so`.
- **Cairo rendering** — anti-aliased text and drawing by default via Cairo-XCB backend. The `ISW_RENDER_BACKEND` environment variable overrides backend selection.
- **HiDPI aware** — widgets auto-scale. Use `ISWScaleDim`/`ISWScaleFactor` for app-level dimensions.
- **Unified image loading** — Label/Command/Toggle display PNG or SVG via `IswNimage`/`IswNleftImage` (format auto-detected). SmeBSB uses `IswNleftImage`/`IswNrightImage`.
- **New widgets** — MainWindow, MenuBar, Toolbar, StatusBar, Tabs, ComboBox, SpinBox, ProgressBar, IconView, ListView, ColorPicker, FontChooser, ScrollWheel.
