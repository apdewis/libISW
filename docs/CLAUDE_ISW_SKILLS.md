# ISW Implementation Skills

Claude Code instruction file for building applications with the ISW widget library.

## Usage

Copy this file into your project's `docs/` directory or reference it in your project's `CLAUDE.md`:

```markdown
See `docs/CLAUDE_ISW_SKILLS.md` for ISW widget usage patterns.
```

Or include it directly:

```markdown
Contents of docs/CLAUDE_ISW_SKILLS.md apply to all ISW widget code in this project.
```

---

## Skill: Application Shell with MainWindow

MainWindow is the correct top-level container for any standard application. It manages a fixed MenuBar at the top and a single scrollable content area below. Never build this layout manually with Form or Box.

### Correct Pattern

```c
#include <ISW/Intrinsic.h>
#include <ISW/StringDefs.h>
#include <ISW/MainWindow.h>
#include <ISW/MenuBar.h>
#include <ISW/MenuButton.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>
#include <ISW/StatusBar.h>
#include <ISW/Viewport.h>
#include <ISW/Box.h>

int main(int argc, char *argv[])
{
    IswAppContext app;
    Widget toplevel = IswAppInitialize(&app, "MyApp",
                                      NULL, 0, &argc, argv, NULL, NULL, 0);

    /* MainWindow is the DIRECT child of the toplevel shell */
    Widget main_win = IswCreateManagedWidget("mainWindow", mainWindowWidgetClass,
                                            toplevel, NULL, 0);

    /* The menubar is built-in — retrieve it, don't create one */
    Widget menubar = IswMainWindowGetMenuBar(main_win);

    /* Populate menubar with MenuButtons (see Menu skill below) */
    create_menus(menubar);

    /* Optional: StatusBar as a child of MainWindow */
    Widget statusbar = IswCreateManagedWidget("statusbar", statusBarWidgetClass,
                                             main_win, NULL, 0);

    /* Content area: typically a Viewport for scrolling */
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgAllowVert(&ab, True);
    IswArgAllowHoriz(&ab, True);
    Widget viewport = IswCreateManagedWidget("viewport", viewportWidgetClass,
                                            main_win, ab.args, ab.count);

    /* Application content goes inside the viewport */
    Widget content = IswCreateManagedWidget("content", boxWidgetClass,
                                           viewport, NULL, 0);

    IswRealizeWidget(toplevel);
    IswAppMainLoop(app);
    return 0;
}
```

### Rules

- MainWindow must be the **direct child** of the toplevel ApplicationShell.
- Do **not** create a MenuBar widget manually — MainWindow creates one internally. Use `IswMainWindowGetMenuBar()` to access it.
- The StatusBar (if used) must be a child of MainWindow — it is placed at the bottom automatically.
- Only **one** content child is supported (besides the internal menubar and statusbar). Use a Viewport or other container to hold complex layouts.

### Common Mistakes

| Wrong | Right |
|---|---|
| Creating a `menuBarWidgetClass` child of MainWindow | `IswMainWindowGetMenuBar(main_win)` |
| Putting MainWindow inside a Form or Box | MainWindow is the direct shell child |
| Multiple content children in MainWindow | Wrap in a single Viewport or Box |
| Using Box+Scrollbar for app layout | MainWindow + Viewport handles this |

---

## Skill: Popup Shells — Avoiding Stray Windows

ISW transient/popup shells can leave orphan X windows if not managed correctly. The WM_DELETE_WINDOW handler for non-ApplicationShell shells calls `IswDestroyWidget` — but programmatic popdowns do **not** destroy the shell. You must handle lifecycle explicitly.

### How WM_DELETE_WINDOW Works

- **ApplicationShell**: sets the app exit flag → `IswAppMainLoop` returns.
- **All other shells** (TransientShell, TopLevelShell): calls `IswDestroyWidget(shell)`.

This means closing a transient popup via the window manager's X button destroys it. But `IswPopdown()` only **unmaps** the window — the shell widget and its X window remain alive.

### Pattern 1: Reusable Popup (Create Once, Show Many Times)

Use when the popup content is static or expensive to rebuild.

```c
static Widget dialog_shell = NULL;

void show_dialog(Widget parent)
{
    if (dialog_shell == NULL) {
        dialog_shell = IswCreatePopupShell("myDialog", transientShellWidgetClass,
                                          parent, NULL, 0);
        /* Build contents once */
        Widget form = IswCreateManagedWidget("form", formWidgetClass,
                                            dialog_shell, NULL, 0);
        /* ... add children ... */

        /* Handle WM close: popdown instead of destroy, to allow reuse */
        IswOverrideTranslations(dialog_shell,
            IswParseTranslationTable(
                "<Message>WM_PROTOCOLS: IswCallbackPopdown()"));

        /* Track destruction if parent is destroyed */
        IswAddCallback(dialog_shell, IswNdestroyCallback,
                       dialog_destroyed_cb, NULL);
    }
    IswPopup(dialog_shell, IswGrabExclusive);
}

void hide_dialog(Widget w, IswPointer cd, IswPointer call_data)
{
    IswPopdown(dialog_shell);
}

static void dialog_destroyed_cb(Widget w, IswPointer cd, IswPointer call_data)
{
    dialog_shell = NULL;
}
```

### Pattern 2: One-Shot Popup (Create, Show, Destroy)

Use when the popup content is dynamic or the popup should not persist.

```c
void show_one_shot_dialog(Widget parent)
{
    Widget shell = IswCreatePopupShell("tempDialog", transientShellWidgetClass,
                                      parent, NULL, 0);

    /* Build contents */
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgLabel(&ab, "Are you sure?");
    Widget lbl = IswCreateManagedWidget("label", labelWidgetClass,
                                       shell, ab.args, ab.count);

    /* Dismiss AND destroy on close */
    IswAddCallback(ok_btn, IswNcallback, dismiss_and_destroy, (IswPointer)shell);

    IswPopup(shell, IswGrabExclusive);
}

static void dismiss_and_destroy(Widget w, IswPointer cd, IswPointer call_data)
{
    Widget shell = (Widget)cd;
    IswPopdown(shell);
    IswDestroyWidget(shell);
}
```

### Pattern 3: IswCallbackPopdown with IswPopdownID

The toolkit provides a convenience callback for popdown-and-re-enable:

```c
static IswPopdownIDRec popdown_id;

void setup_popup(Widget parent)
{
    Widget shell = IswCreatePopupShell("popup", transientShellWidgetClass,
                                      parent, NULL, 0);
    /* ... build children ... */

    popdown_id.shell_widget = shell;
    popdown_id.enable_widget = trigger_button;  /* or NULL */

    IswAddCallback(close_btn, IswNcallback, IswCallbackPopdown,
                   (IswPointer)&popdown_id);
}
```

### Rules for Stray Window Prevention

1. **Always pair IswPopup with a way to IswPopdown.** Every popup must have a dismiss path — button callback, WM_PROTOCOLS translation, or grab-induced popdown.

2. **If you override WM_PROTOCOLS on a transient shell**, you take responsibility for its lifecycle. The default handler calls `IswDestroyWidget`; if you replace it with just `IswPopdown`, the shell persists until the app exits (which is fine for reusable dialogs).

3. **Destroy one-shot popups explicitly.** After `IswPopdown(shell)`, call `IswDestroyWidget(shell)` if the popup won't be reused. `IswPopdown` alone leaves the widget tree intact.

4. **Never call IswDestroyWidget on a popped-up shell without popping down first.** Always: `IswPopdown(shell); IswDestroyWidget(shell);` in that order.

5. **Track your popup pointer.** If the shell can be destroyed externally (e.g. parent destroyed, WM close with default handler), use a destroy callback to NULL your reference:

   ```c
   IswAddCallback(shell, IswNdestroyCallback, nullify_ref, &my_shell_ptr);
   ```

6. **TransientShell vs TopLevelShell:** Use `transientShellWidgetClass` for dialogs that belong to the main window (WM may group them). Use `topLevelShellWidgetClass` only for independent top-level windows (e.g. a second document window). Both get WM_DELETE_WINDOW handling.

7. **OverrideShell is for menus only.** It bypasses the window manager entirely (no titlebar, no WM_DELETE_WINDOW). SimpleMenu uses it internally. Never use `overrideShellWidgetClass` for dialogs.

### Debugging Stray Windows

If you see orphan windows after dismissing a popup:
- The popup was `IswPopdown`'d but never `IswDestroyWidget`'d (one-shot that should have been destroyed).
- Or the default WM_PROTOCOLS was overridden but the replacement doesn't destroy.
- Use `xdotool search --name "popupName"` to find orphan windows by title.

---

## Skill: Widget Selection Guide

### "I need a button"

| Need | Widget | Class Symbol |
|---|---|---|
| Simple push button | Command | `commandWidgetClass` |
| Checkbox (on/off) | Toggle | `toggleWidgetClass` |
| Radio buttons (one-of-N) | Toggle with `IswNradioGroup` | `toggleWidgetClass` |
| Button that opens a menu | MenuButton | `menuButtonWidgetClass` |
| Button that repeats while held | Repeater | `repeaterWidgetClass` |

### "I need to display text/images"

| Need | Widget | Class Symbol |
|---|---|---|
| Static text | Label | `labelWidgetClass` |
| Static image | Label with `IswNimage` | `labelWidgetClass` |
| Scalable image display | Image | `imageWidgetClass` |
| Progress indicator | ProgressBar | `progressBarWidgetClass` |
| Tooltip | Tip | `tipWidgetClass` |
| Status message | StatusBar | `statusBarWidgetClass` |

### "I need text input"

| Need | Widget | Class Symbol |
|---|---|---|
| Single-line text entry | AsciiText (with `IswNeditType = IswTextEdit`, `IswNconsumeTab = False`) | `asciiTextWidgetClass` |
| Multi-line editor | AsciiText (with `IswNeditType = IswTextEdit`) | `asciiTextWidgetClass` |
| Label + text entry dialog | Dialog | `dialogWidgetClass` |
| Numeric entry with +/- | SpinBox | `spinBoxWidgetClass` |

### "I need a list or selection"

| Need | Widget | Class Symbol |
|---|---|---|
| Simple single-column text list | List | `listWidgetClass` |
| Multi-column table with headers | ListView | `listViewWidgetClass` |
| Icon grid (file browser style) | IconView | `iconViewWidgetClass` |
| Hierarchical tree | Tree | `treeWidgetClass` |
| Dropdown choice | ComboBox | `comboBoxWidgetClass` |
| Selectable rows with rich content | ListBox + ListBoxRow | `listBoxWidgetClass` |

**List vs ListBox vs ListView — when to use which:**

- **List** (`listWidgetClass`): Simple flat list of text strings. You provide a `String[]` array. Selection is single-item. Use for simple pick-one-from-N scenarios like a file list or option list. No custom row content.

- **ListBox** (`listBoxWidgetClass`): A container where each child is a selectable row. Children can be *any* widget — Label, Box, ListBoxRow, Form, etc. The ListBox manages selection state (none/single/multi); children don't know they're selected. Use when rows have rich content: icon + label + badge, multi-widget layouts, heterogeneous rows. Wrap in a Viewport for scrolling.

- **ListView** (`listViewWidgetClass`): A columnar data table with resizable column headers, sort indicators, rubberband selection, and alternating row tint. Data is a flat `String[]` array (row-major). Use for tabular data like file managers, log viewers, or any multi-column table. Has its own built-in scrolling via Viewport parent.

| Scenario | Use |
|---|---|
| Pick a name from a list of 20 strings | List |
| Email sidebar: icon + sender + unread badge per row | ListBox + ListBoxRow |
| File manager: Name, Size, Date, Type columns | ListView |
| Settings list with toggles/buttons per row | ListBox (rows are Forms) |
| Search results with just text | List (simple) or ListBox (if you need selection callbacks with index) |

**ListBox usage pattern:**

```c
#include <ISW/ListBox.h>
#include <ISW/ListBoxRow.h>

/* ListBox inside a Viewport for scrolling */
IswArgBuilder ab = IswArgBuilderInit();
IswArgSelectionMode(&ab, IswListBoxSelectSingle);
IswArgShowSeparators(&ab, True);
Widget listbox = IswCreateManagedWidget("listBox", listBoxWidgetClass,
                                       viewport, ab.args, ab.count);

IswAddCallback(listbox, IswNselectCallback, on_select, NULL);
IswAddCallback(listbox, IswNactivateCallback, on_activate, NULL);

/* Each row is a ListBoxRow (lays children left-to-right) */
Widget row = IswCreateManagedWidget("row0", listBoxRowWidgetClass,
                                   listbox, NULL, 0);

/* Put anything inside the row */
IswArgBuilderReset(&ab);
IswArgLabel(&ab, "icon.svg");
IswArgBorderWidth(&ab, 0);
IswCreateManagedWidget("icon", labelWidgetClass, row, ab.args, ab.count);

IswArgBuilderReset(&ab);
IswArgLabel(&ab, "Inbox");
IswArgBorderWidth(&ab, 0);
IswCreateManagedWidget("name", labelWidgetClass, row, ab.args, ab.count);

/* Per-row constraints: separators and non-selectable rows */
IswArgBuilderReset(&ab);
IswArgSeparator(&ab, True);
IswSetValues(row, ab.args, ab.count);  /* Draw separator below this row */

IswArgBuilderReset(&ab);
IswArgSelectable(&ab, False);
IswSetValues(row, ab.args, ab.count);  /* This row can't be selected */

/* Callback data */
void on_select(Widget w, IswPointer cd, IswPointer call_data)
{
    IswListBoxCallbackData *cb = (IswListBoxCallbackData *)call_data;
    /* cb->child: the selected row widget */
    /* cb->index: row index */
    /* cb->selected: array of selected widgets */
    /* cb->num_selected: count */
}
```

### "I need a container/layout"

| Need | Widget | Class Symbol |
|---|---|---|
| App shell with menubar + content | MainWindow | `mainWindowWidgetClass` |
| Horizontal/vertical packing (wraps) | Box | `boxWidgetClass` |
| Proportional space distribution | FlexBox | `flexBoxWidgetClass` |
| Relative positioning (form fields) | Form | `formWidgetClass` |
| Resizable panes with dividers | Paned | `panedWidgetClass` |
| Scrollable content area | Viewport | `viewportWidgetClass` |
| Tab pages | Tabs | `tabsWidgetClass` |
| 2D pan (with Panner control) | Porthole | `portholeWidgetClass` |
| Menu bar | MenuBar (or MainWindow's built-in) | `menuBarWidgetClass` |
| Toolbar | Toolbar | `toolbarWidgetClass` |

**Box vs FlexBox vs Form vs Paned — when to use which:**

- **Box** (`boxWidgetClass`): Packs children sequentially (horizontal or vertical via `IswNorientation`). Children get their preferred size, no stretching. Wraps to next row/column when space runs out. Use for simple button bars, icon rows, or any layout where children are all natural-sized and wrapping is acceptable.

- **FlexBox** (`flexBoxWidgetClass`): Distributes space along a primary axis. Children with `IswNflexGrow > 0` expand to fill available space proportionally. Children with `IswNflexGrow = 0` keep their preferred size. Cross-axis alignment is per-child (`IswNflexAlign`: start/end/center/stretch). Use for toolbars, split panels, sidebar+content layouts, or any layout where some children should stretch to fill space. Does not wrap.

- **Form** (`formWidgetClass`): Constraint-based positioning. Children declare relative positions: `IswNfromVert` (below widget X), `IswNfromHoriz` (right of widget X). Use for dialog layouts, form fields (label-input pairs), or any layout where children have specific spatial relationships. Most flexible but most verbose.

- **Paned** (`panedWidgetClass`): Stacks children vertically with user-draggable dividers between them. Use for resizable split views (editor + console, list + detail). Children declare `IswNmin` and `IswNmax` constraints.

| Scenario | Use |
|---|---|
| Row of buttons, all same size | Box (horizontal) |
| Sidebar (fixed 200px) + content (fills rest) | FlexBox (horizontal), sidebar flexGrow=0, content flexGrow=1 |
| Label-input pairs in a dialog | Form |
| Three stacked panels, user-resizable | Paned |
| Toolbar with left group + spacer + right group | FlexBox (horizontal) with spacer flexGrow=1 |
| Icon grid that wraps to fit window width | Box (horizontal, wraps naturally) |
| Header + body + footer (body stretches) | FlexBox (vertical), header/footer flexGrow=0, body flexGrow=1 |

**FlexBox usage pattern:**

```c
#include <ISW/FlexBox.h>

IswArgBuilder ab = IswArgBuilderInit();

/* Horizontal FlexBox: sidebar + content */
IswArgOrientation(&ab, IswOrientHorizontal);
IswArgSpacing(&ab, 0);
Widget hbox = IswCreateManagedWidget("hbox", flexBoxWidgetClass,
                                    parent, ab.args, ab.count);

/* Sidebar: fixed width, stretches vertically */
IswArgBuilderReset(&ab);
IswArgWidth(&ab, 200);
IswArgFlexGrow(&ab, 0);
IswArgFlexAlign(&ab, IswFlexAlignStretch);
Widget sidebar = IswCreateManagedWidget("sidebar", boxWidgetClass,
                                       hbox, ab.args, ab.count);

/* Content: takes all remaining space */
IswArgBuilderReset(&ab);
IswArgFlexGrow(&ab, 1);
IswArgFlexAlign(&ab, IswFlexAlignStretch);
Widget content = IswCreateManagedWidget("content", viewportWidgetClass,
                                       hbox, ab.args, ab.count);

/* Vertical FlexBox: header + body + footer */
IswArgBuilderReset(&ab);
IswArgOrientation(&ab, IswOrientVertical);
Widget vbox = IswCreateManagedWidget("vbox", flexBoxWidgetClass,
                                    parent, ab.args, ab.count);

/* Header: natural height */
IswArgBuilderReset(&ab);
IswArgFlexGrow(&ab, 0);
Widget header = IswCreateManagedWidget("header", labelWidgetClass,
                                      vbox, ab.args, ab.count);

/* Body: fills remaining vertical space */
IswArgBuilderReset(&ab);
IswArgFlexGrow(&ab, 1);
Widget body = IswCreateManagedWidget("body", viewportWidgetClass,
                                    vbox, ab.args, ab.count);

/* Footer: natural height */
IswArgBuilderReset(&ab);
IswArgFlexGrow(&ab, 0);
Widget footer = IswCreateManagedWidget("footer", labelWidgetClass,
                                      vbox, ab.args, ab.count);
```

**FlexBox constraint resources (per-child):**

| Resource | Type | Default | Purpose |
|---|---|---|---|
| `IswNflexGrow` | int | 0 | Grow factor. 0 = fixed at preferred size. >0 = share of remaining space |
| `IswNflexBasis` | Dimension | 0 | Explicit base size along primary axis (0 = use preferred size) |
| `IswNflexAlign` | FlexAlign | `IswFlexAlignStretch` | Cross-axis alignment: `start`, `end`, `center`, `stretch` |

### "I need a menu"

| Need | Widget |
|---|---|
| Dropdown from menu bar | MenuButton + SimpleMenu + SmeBSB |
| Right-click context menu | SimpleMenu + `IswMenuPopup` translation |
| Submenu / cascade | SmeBSB with `IswNmenuName` + nested SimpleMenu |
| Menu separator | SmeLine (`smeLineObjectClass`) |

### "I need a dialog"

| Need | Widget / Pattern |
|---|---|
| Modal confirmation | TransientShell + Form + Label + Command buttons + `IswGrabExclusive` |
| Text input prompt | Dialog widget (has built-in Label + text field) |
| Color picker | ColorPicker (`colorPickerWidgetClass`) |
| Font selector | FontChooser (`fontChooserWidgetClass`) |

### "I need a slider/scroll"

| Need | Widget | Class Symbol |
|---|---|---|
| Value slider with display | Slider | `sliderWidgetClass` |
| Scrollbar (standalone) | Scrollbar | `scrollbarWidgetClass` |
| Scrollable view (auto-scrollbars) | Viewport | `viewportWidgetClass` |
| Numeric value stepper | SpinBox | `spinBoxWidgetClass` |

---

## Skill: Grab Kinds for Popups

| Grab Kind | Use For | Behavior |
|---|---|---|
| `IswGrabExclusive` | Modal dialogs, confirmation popups | All input goes to popup; click outside is blocked |
| `IswGrabNonexclusive` | Menus, tooltips, non-modal popups | Input can go elsewhere; popup stays until explicitly dismissed |
| `IswGrabNone` | Persistent tool windows | No grab at all; popup is just another mapped window |

**Default choice for dialogs:** `IswGrabExclusive`.
**Default choice for menus:** `IswGrabNonexclusive` (SimpleMenu handles this internally via MenuButton).
**Never use `IswGrabNone` for menus** — outside clicks won't dismiss them.

---

## Skill: IswArgBuilder Convenience API

For widgets with many resources, use `IswArgBuilder` instead of manual `Arg`/`n` counting:

```c
IswArgBuilder ab = IswArgBuilderInit();
IswArgLabel(&ab, "Click Me");
IswArgWidth(&ab, 200);
IswArgHeight(&ab, 40);
IswArgBorderWidth(&ab, 1);

Widget btn = IswCreateManagedWidget("btn", commandWidgetClass,
                                   parent, ab.args, ab.count);

IswArgBuilderReset(&ab);  /* Reuse for next widget */
```

Available builders: `IswArgLabel`, `IswArgWidth`, `IswArgHeight`, `IswArgBorderWidth`, `IswArgMenuName`, `IswArgMnemonicKey`, `IswArgOrientation`, `IswArgAllowVert`, `IswArgAllowHoriz`, `IswArgForceBars`, and others matching `IswN*` resources.

---

## Quick Reference: Shell Widget Classes

| Class | Use | WM Decorations | WM_DELETE_WINDOW Default |
|---|---|---|---|
| `applicationShellWidgetClass` | App's main window (one per app) | Full | Sets exit flag |
| `topLevelShellWidgetClass` | Secondary top-level windows | Full | `IswDestroyWidget` |
| `transientShellWidgetClass` | Dialogs, popups belonging to a parent | Reduced (no taskbar) | `IswDestroyWidget` |
| `overrideShellWidgetClass` | Menus only (no WM interaction) | None | N/A |

---

## Anti-Patterns

### Don't: Manually build a menu bar with Box + Command buttons

```c
/* WRONG — loses keyboard nav, focus handling, menu semantics */
Widget bar = IswCreateManagedWidget("bar", boxWidgetClass, parent, NULL, 0);
Widget file = IswCreateManagedWidget("file", commandWidgetClass, bar, ...);
```

**Do:** Use MainWindow's built-in MenuBar with MenuButton widgets.

### Don't: Use IswPopup without ensuring a dismiss path

```c
/* WRONG — no way to close this */
IswPopup(shell, IswGrabNone);
```

**Do:** Always provide a close button callback, WM_PROTOCOLS handler, or use a grab kind that allows outside-click dismissal.

### Don't: Create TransientShell without a parent relationship

```c
/* WRONG — orphan window not associated with main window */
Widget popup = IswCreatePopupShell("dlg", transientShellWidgetClass,
                                  some_random_widget, NULL, 0);
```

**Do:** Parent transient shells to the widget that logically owns them (typically the toplevel shell or the MainWindow). The `IswNtransientFor` resource is set automatically to the nearest shell ancestor.

### Don't: Forget to destroy one-shot popups

```c
/* WRONG — popup unmapped but widget tree leaks */
void close_cb(Widget w, IswPointer cd, IswPointer call_data) {
    IswPopdown((Widget)cd);
    /* Missing: IswDestroyWidget((Widget)cd); */
}
```

### Don't: Call IswDestroyWidget on a still-popped-up shell

```c
/* WRONG — may leave grab state inconsistent */
IswDestroyWidget(shell);  /* shell is still grabbed */
```

**Do:** `IswPopdown(shell); IswDestroyWidget(shell);`
