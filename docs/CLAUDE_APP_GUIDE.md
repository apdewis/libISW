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

Use the `IswArgBuilder` convenience API:

```c
#include <ISW/Command.h>
#include <ISW/IswArgMacros.h>

IswArgBuilder ab = IswArgBuilderInit();
IswArgLabel(&ab, "Click Me");
Widget btn = IswCreateManagedWidget("btn", commandWidgetClass, parent,
                                   ab.args, ab.count);
IswAddCallback(btn, IswNcallback, my_callback, (IswPointer)"btn");

IswArgBuilderReset(&ab);  /* Reuse for next widget */
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
| FlexBox | `flexBoxWidgetClass` | `<ISW/FlexBox.h>` | Proportional space distribution (flex grow/align) |
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
| ListBox | `listBoxWidgetClass` | `<ISW/ListBox.h>` | Selectable rows with rich widget content; nest a ListBox for collapsible groups |
| ListBoxRow | `listBoxRowWidgetClass` | `<ISW/ListBoxRow.h>` | Left-to-right row container for ListBox |

### Dialogs

| Widget | Class Symbol | Header | Purpose |
|---|---|---|---|
| ColorPicker | `colorPickerWidgetClass` | `<ISW/ColorPicker.h>` | Color selection dialog |
| FontChooser | `fontChooserWidgetClass` | `<ISW/FontChooser.h>` | Font family/size selection |

## Menus

**Full guide:** See `docs/MENUS.md` for comprehensive popup/grab behavior, all four menu patterns, and common mistakes. This section covers the essential API.

ISW menus use a three-layer hierarchy:

1. **MenuButton** — a button that opens a popup menu on click
2. **SimpleMenu** — the popup menu container (an OverrideShell)
3. **SmeBSB / SmeLine** — individual menu entries / separators

### Critical Rules

- **The popup shell widget name must match `IswNmenuName` exactly.** Mismatches cause "Could not find menu widget" warnings and silent failures.
- **Make the popup shell a child of the widget that triggers it** (the `MenuButton` for dropdowns, the owning widget for context menus, the parent `SimpleMenu` for submenus).

### MenuButton Dropdown

```c
#include <ISW/MenuButton.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>

/* 1. Create a MenuButton. IswNmenuName links it to the SimpleMenu by name. */
IswArgBuilder ab = IswArgBuilderInit();
IswArgLabel(&ab, "File");
IswArgMenuName(&ab, "fileMenu");
Widget file_btn = IswCreateManagedWidget("fileBtn", menuButtonWidgetClass,
                                        menubar, ab.args, ab.count);

/* 2. Create the SimpleMenu as a popup shell. The widget name must match
      the IswNmenuName string above. The parent is the MenuButton. */
Widget file_menu = IswCreatePopupShell("fileMenu", simpleMenuWidgetClass,
                                      file_btn, NULL, 0);

/* 3. Add entries as children of the SimpleMenu. */
IswArgBuilderReset(&ab);
IswArgLabel(&ab, "New");
Widget item_new = IswCreateManagedWidget("new", smeBSBObjectClass,
                                        file_menu, ab.args, ab.count);
IswAddCallback(item_new, IswNcallback, file_cb, (IswPointer)"new");

/* Separator */
IswCreateManagedWidget("sep", smeLineObjectClass, file_menu, NULL, 0);

IswArgBuilderReset(&ab);
IswArgLabel(&ab, "Quit");
Widget item_quit = IswCreateManagedWidget("quit", smeBSBObjectClass,
                                         file_menu, ab.args, ab.count);
IswAddCallback(item_quit, IswNcallback, file_cb, (IswPointer)"quit");
```

The menu opens on click and dismisses on entry click, outside click, or `Escape`. Spring-loaded (press-and-hold) behavior is not supported — opening and closing are independent events.

### Right-Click Context Menu

Use the built-in `IswMenuPopup` action in a translation table.

```c
/* Create the menu as a popup shell — parent is the widget that owns it. */
Widget ctx_menu = IswCreatePopupShell("ctxMenu", simpleMenuWidgetClass,
                                     my_widget, NULL, 0);

/* Add entries (SmeBSB with callbacks, same as above) */
IswArgBuilder ab = IswArgBuilderInit();
IswArgLabel(&ab, "Cut");
Widget cut = IswCreateManagedWidget("cut", smeBSBObjectClass, ctx_menu,
                                   ab.args, ab.count);
IswAddCallback(cut, IswNcallback, ctx_cb, (IswPointer)"cut");

/* ... more entries ... */

/* Install translation — IswMenuPopup opens the menu on ButtonPress,
   KeyPress, or EnterNotify. */
IswOverrideTranslations(my_widget,
    IswParseTranslationTable("<Btn3Down>: IswMenuPopup(ctxMenu)"));
```

The menu dismisses on entry click or click outside, identical to `MenuButton`.

### Programmatic Popup

For menus opened by keyboard shortcut or other non-button events:

```c
/* Position, then pop up with an exclusive grab for click-outside-to-dismiss. */
IswArgBuilder ab = IswArgBuilderInit();
IswArgX(&ab, x_pos);
IswArgY(&ab, y_pos);
IswSetValues(my_menu, ab.args, ab.count);

IswPopup(my_menu, IswGrabExclusive);
```

Use `IswGrabExclusive` when you want click-outside-to-dismiss. Use `IswGrabNonexclusive` when the menu should stay up until an entry is explicitly selected. **Never use `IswGrabNone` for menus** — the menu won't be dismissable by clicking outside it.

### Cascade / Submenu

Set `IswNmenuName` on a `SmeBSB` entry to open a submenu on highlight:

```c
/* Submenu popup shell — child of the parent menu */
Widget export_menu = IswCreatePopupShell("exportMenu", simpleMenuWidgetClass,
                                        file_menu, NULL, 0);

/* Submenu entries */
IswArgBuilder ab = IswArgBuilderInit();
IswArgLabel(&ab, "PNG");
Widget png = IswCreateManagedWidget("png", smeBSBObjectClass,
                                   export_menu, ab.args, ab.count);
IswAddCallback(png, IswNcallback, export_cb, (IswPointer)"png");

/* Cascade entry in the parent menu — no callback, just IswNmenuName. */
IswArgBuilderReset(&ab);
IswArgLabel(&ab, "Export");
IswArgMenuName(&ab, "exportMenu");
IswCreateManagedWidget("export", smeBSBObjectClass, file_menu, ab.args, ab.count);
```

Submenus nest to arbitrary depth. `SimpleMenu` searches for the named widget by walking up the tree calling `IswNameToWidget` — the popup shell just needs to be findable from that search.

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
IswArgBuilder ab = IswArgBuilderInit();

/* SVG file */
IswArgImage(&ab, "icon.svg");

/* PNG file */
IswArgImage(&ab, "photo.png");

/* Inline SVG */
IswArgImage(&ab, "<svg viewBox='0 0 24 24'>...</svg>");

/* Icon beside text (does not replace the label) */
IswArgLeftImage(&ab, "bullet.png");
```

`IswNimage` replaces the text label entirely. `IswNleftImage` draws an icon to the left of the text.

For menu entries (`SmeBSB`), use `IswNleftImage` and `IswNrightImage` the same way.

SVG images support `currentColor` — occurrences are automatically substituted with the widget's `IswNforeground` color and update when the foreground changes.

File paths are resolved through ISW's search path: executable directory, `$ISW_DATA_PATH`, `$XDG_DATA_HOME/isw/`, system data dirs, then cwd.

### Custom drawing: retained image handles

If your code draws icons directly with `ISWRenderDrawImageRGBA` / `ISWRenderDrawImageMasked` (e.g. in a DrawingArea expose proc), do NOT call them on every repaint — each call re-uploads a texture (and, for the masked variant, re-tints the buffer on the CPU), which dominates icon-heavy repaint profiles. Upload once and redraw by handle:

```c
/* once, or when the raster/foreground changes */
int handle = ISWRenderImageUploadMasked(ctx, fg, rgba, w, h);  /* monochrome */
int handle = ISWRenderImageUpload(ctx, rgba, w, h);            /* full color */

/* every repaint */
ISWRenderDrawImageHandle(ctx, handle, x, y, dst_w, dst_h);

/* when the source raster or (for masked) the foreground changes, or on teardown */
ISWRenderImageFree(ctx, handle);
```

Both upload functions return 0 on unsupported backends — fall back to the per-paint draw call in that case. The built-in Label and IconView widgets already use this pattern internally.

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

IswArgBuilder ab = IswArgBuilderInit();
IswArgLabel(&ab, "Name:");
Widget lbl = IswCreateManagedWidget("lbl", labelWidgetClass, form,
                                   ab.args, ab.count);

IswArgBuilderReset(&ab);
IswArgFromHoriz(&ab, lbl);
Widget txt = IswCreateManagedWidget("txt", asciiTextWidgetClass, form,
                                   ab.args, ab.count);

IswArgBuilderReset(&ab);
IswArgFromVert(&ab, lbl);
Widget btn = IswCreateManagedWidget("ok", commandWidgetClass, form,
                                   ab.args, ab.count);
```

## Toggle / Radio Groups

```c
#include <ISW/Toggle.h>

IswArgBuilder ab = IswArgBuilderInit();
IswArgLabel(&ab, "Option A");
Widget a = IswCreateManagedWidget("a", toggleWidgetClass, parent,
                                 ab.args, ab.count);

IswArgBuilderReset(&ab);
IswArgLabel(&ab, "Option B");
IswArgRadioGroup(&ab, a);
Widget b = IswCreateManagedWidget("b", toggleWidgetClass, parent,
                                 ab.args, ab.count);
```

## Keyboard Navigation

ISW has Tab / Shift+Tab focus traversal across interactive widgets, plus per-widget keyboard activation.

### Tab traversal

All interactive widgets (Command, Toggle, MenuButton, List, ComboBox, IconView, Slider, Text, AsciiText) opt into the Tab cycle by default. Pressing **Tab** advances focus forward in widget-tree order; **Shift+Tab** goes back. The focused widget draws a dashed focus ring. Each popup / transient shell has its own isolated focus cycle — Tab in a modal dialog stays within the dialog.

Scrollbars are deliberately *not* Tab stops: users drive them indirectly via the focused widget they scroll.

Two resources, on any Simple subclass, control participation:

| Resource | Class | Type | Default | Meaning |
|---|---|---|---|---|
| `IswNtraversalOn` | `IswCTraversalOn` | `IswRBoolean` | Widget-specific | Include in Tab cycle |
| `IswNtabIndex` | `IswCTabIndex` | `IswRInt` | 0 | Explicit order (lower = earlier); 0 = tree order |

To exclude a widget from Tab order:

```c
IswArgTraversalOn(&ab, False);
```

To force a specific order:

```c
IswArgTabIndex(&ab, 10);  /* earlier */
IswArgTabIndex(&ab, 20);  /* later */
```

### Activation keys

Once focused, widgets respond to the usual keys:

| Widget | Keys |
|---|---|
| Command | Space, Return → activate |
| Toggle | Space, Return → toggle + notify |
| MenuButton | Space, Return → open menu |
| List / ComboBox | Up/Down/Home/End/Page_Up/Page_Down → move cursor; Return/Space → select |
| ComboBox (open) | Up/Down nav entries, Return selects, Escape closes |
| IconView | Arrow keys (grid), Home/End, Space toggles, Return activates, Ctrl+A select all |
| Slider | Left/Down decrement, Right/Up increment, Page_Up/Page_Down large step, Home/End min/max |
| SpinBox | Up/Down step the value (translations installed on the internal text field) |

SimpleMenu (popup / dropdown) navigation while open:

- Up/Down move between entries (skipping separators/insensitive entries)
- Home/End jump to first / last
- Return or Space select; Escape cancels

### Text widget: Tab behavior

By default Text/AsciiText **consume Tab** as literal input (inserts `\t`). For single-line entries where Tab should traverse out (e.g. form fields, SpinBox's internal text), set:

```c
IswArgConsumeTab(&ab, False);
```

Shift+Tab always traverses out regardless of `IswNconsumeTab`.

### Initial focus

A newly realized shell seeds focus onto its first managed child. Applications can override with `IswSetKeyboardFocus(shell, widget)` at any time.

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

IswArgBuilder ab = IswArgBuilderInit();
IswArgListViewColumns(&ab, cols);
IswArgNumColumns(&ab, 2);
IswArgListViewData(&ab, cell_data);
IswArgNumRows(&ab, 2);
IswArgMultiSelect(&ab, True);
Widget lv = IswCreateManagedWidget("lv", listViewWidgetClass, viewport,
                                  ab.args, ab.count);

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

## ListBox

A Constraint container of selectable rows.  Children are arbitrary widgets
(typically `ListBoxRow` with labels inside); the container tracks selection
and draws the highlight.  Wrap in a Viewport for scrolling.

```c
#include <ISW/ListBox.h>
#include <ISW/ListBoxRow.h>

IswArgBuilder ab = IswArgBuilderInit();
IswArgSelectionMode(&ab, IswListBoxSelectSingle);  /* none/single/multi */
IswArgRowSpacing(&ab, 1);
IswArgShowSeparators(&ab, True);
Widget lb = IswCreateManagedWidget("lb", listBoxWidgetClass, viewport,
                                   ab.args, ab.count);

/* A row: any widget works; ListBoxRow lays labels out left-to-right */
Widget row = IswCreateManagedWidget("row", listBoxRowWidgetClass, lb,
                                    NULL, 0);
IswArgBuilderReset(&ab);
IswArgLabel(&ab, "Apple");
IswCreateManagedWidget("name", labelWidgetClass, row, ab.args, ab.count);

/* call_data is IswListBoxCallbackData* */
void select_cb(Widget w, IswPointer cd, IswPointer call_data)
{
    IswListBoxCallbackData *d = (IswListBoxCallbackData *)call_data;
    /* d->child: the entry widget; d->index: index within d->list
       (its immediate ListBox); d->selected/d->num_selected: tree-wide */
}
```

### Collapsible groups (nested ListBoxes)

A ListBox child that is itself a ListBox is a collapsible group.  The
parent draws a fixed header band (chevron + `pivotLabel` text) above the
indented body, toggles it on chevron clicks, and hides the body when
closed.  Group appearance/state is set via constraint resources on the
nested child:

```c
IswArgBuilderReset(&ab);
IswArgPivotLabel(&ab, "Fruits");   /* header text */
IswArgPivotOpen(&ab, True);        /* default False (closed) */
IswArgSelectable(&ab, True);       /* header selectable like any row */
Widget group = IswCreateManagedWidget("fruits", listBoxWidgetClass, lb,
                                      ab.args, ab.count);
/* add rows (or further nested groups) to `group` as usual */

/* Pivot callback fires on the OUTERMOST ListBox; call_data is
   IswListBoxPivotCallbackData* {child, index, open} */
IswAddCallback(lb, IswNpivotCallback, pivot_cb, NULL);
```

Selection rules for a nested tree:

- The **outermost ListBox owns everything**: `selectionMode`,
  `selectCallback`, `activateCallback`, `pivotCallback`, focus, and
  keyboard navigation.  Register callbacks there; nested ListBoxes'
  own selection resources are ignored.
- Exactly one selection domain per tree — selecting anywhere clears
  conflicting selections at every level (single mode).
- Chevron clicks only toggle; clicks on the rest of the header
  select/activate the group entry (its widget appears as `d->child`).
- Collapsing a group clears any selection inside it and moves focus to
  the group header; there is never a hidden selection.
- `IswListBoxGetSelected` etc. called on any ListBox of a tree answer
  for the whole tree.
- Programmatic open/close: `IswSetValues` of `pivotOpen` on the group
  widget (same collapse rules apply, callbacks fire).

**Widget resources:** `selectionMode`, `rowSpacing`, `showSeparators`,
`foreground`, `font` (header text), `selectCallback`, `activateCallback`,
`pivotCallback`.

**Constraint resources (per child):** `selectable`, `listBoxRowHeight`,
`separator`; for group children additionally `pivotLabel`, `pivotOpen`,
`pivotImage`, `pivotImageOpen` (SVG/PNG chevron overrides).

**API:** `IswListBoxGetSelected`, `IswListBoxGetSelectedChildren`,
`IswListBoxClearSelection`, `IswListBoxSelectChild`.

**Interaction:** click select, double-click activate, Ctrl+click toggle
(multi), Shift+click range (within one level), Ctrl+A select all,
Up/Down/Home/End navigation across open groups with Shift-extend,
Space selection toggle, Return activate.

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
- **Label ellipsize** — `IswNellipsize` resource (`"none"`, `"start"`, `"middle"`, `"end"`) truncates text with "…" when the widget is narrower than the label's preferred width. Useful for paths and filenames.
- **Toolbar center shrink** — center-aligned children (`IswToolbarAlignCenter`) are shrunk to fit between left and right groups instead of overflowing. Combined with `IswNellipsize`, center labels truncate cleanly.
- **New widgets** — MainWindow, MenuBar, Toolbar, StatusBar, Tabs, ComboBox, SpinBox, ProgressBar, IconView, ListView, ColorPicker, FontChooser, ScrollWheel.
