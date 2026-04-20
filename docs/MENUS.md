# ISW Menu & Popup Usage Guide

This guide covers correct usage of ISW menus and popup shells. Read this before writing any menu code.

## Interaction Model: Click to Post

ISW menus follow the modern click-to-post model used by GTK, Qt, Windows, and macOS:

- A **button press** on a `MenuButton` (or a right-click on a widget with an `IswMenuPopup` translation) **opens** the menu.
- A subsequent **button press** on an entry activates it and dismisses the menu.
- A button press outside the menu dismisses it.
- `Escape` dismisses the menu.
- Releasing the opening button does **not** dismiss the menu.

There is no press-and-hold "spring-loaded" behavior. Opening and closing are independent events.

## Menu Patterns

### Pattern 1: MenuButton (Toolbar / Menu Bar)

This is the standard way to attach a dropdown menu to a button. `MenuButton` handles all popup logic internally.

```c
#include <ISW/MenuButton.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>

Arg args[10];
Cardinal n;

/* The button. IswNmenuName links it to a SimpleMenu by widget name. */
n = 0;
IswSetArg(args[n], IswNlabel, "File"); n++;
IswSetArg(args[n], IswNmenuName, "fileMenu"); n++;
Widget btn = IswCreateManagedWidget("fileBtn", menuButtonWidgetClass,
                                   parent, args, n);

/* The menu. Widget name MUST match IswNmenuName above.
   Parent MUST be the MenuButton. */
Widget menu = IswCreatePopupShell("fileMenu", simpleMenuWidgetClass,
                                 btn, NULL, 0);

/* Entries */
n = 0;
IswSetArg(args[n], IswNlabel, "New"); n++;
Widget item = IswCreateManagedWidget("new", smeBSBObjectClass, menu, args, n);
IswAddCallback(item, IswNcallback, my_cb, (IswPointer)"new");

/* Separator */
IswCreateManagedWidget("sep", smeLineObjectClass, menu, NULL, 0);

n = 0;
IswSetArg(args[n], IswNlabel, "Quit"); n++;
item = IswCreateManagedWidget("quit", smeBSBObjectClass, menu, args, n);
IswAddCallback(item, IswNcallback, my_cb, (IswPointer)"quit");
```

**What happens internally:** On a button press on the `MenuButton`, it calls `IswPopup(menu, IswGrabNonexclusive)`. The menu is dismissed when an entry is clicked (the entry's `notify` action fires the callback, then `popdown` runs), when any click lands outside the menu, or when `Escape` is pressed.

### Pattern 2: Right-Click Context Menu (Translation Table)

For popup menus triggered by right-click (or any button press), use `IswMenuPopup` in a translation table.

```c
/* Create the menu as a popup shell.
   Parent should be the widget that owns the context menu. */
Widget ctx_menu = IswCreatePopupShell("ctxMenu", simpleMenuWidgetClass,
                                     my_widget, NULL, 0);

/* Add entries */
n = 0;
IswSetArg(args[n], IswNlabel, "Cut"); n++;
Widget cut = IswCreateManagedWidget("cut", smeBSBObjectClass, ctx_menu, args, n);
IswAddCallback(cut, IswNcallback, ctx_cb, (IswPointer)"cut");

n = 0;
IswSetArg(args[n], IswNlabel, "Copy"); n++;
Widget copy = IswCreateManagedWidget("copy", smeBSBObjectClass, ctx_menu, args, n);
IswAddCallback(copy, IswNcallback, ctx_cb, (IswPointer)"copy");

n = 0;
IswSetArg(args[n], IswNlabel, "Paste"); n++;
Widget paste = IswCreateManagedWidget("paste", smeBSBObjectClass, ctx_menu, args, n);
IswAddCallback(paste, IswNcallback, ctx_cb, (IswPointer)"paste");

/* Install a translation that triggers the menu on right-click. */
IswOverrideTranslations(my_widget,
    IswParseTranslationTable("<Btn3Down>: IswMenuPopup(ctxMenu)"));
```

`IswMenuPopup` accepts `ButtonPress`, `KeyPress`, and `EnterNotify` events. All open the menu with `IswGrabNonexclusive`.

**Dismiss behavior:** Identical to MenuButton — click an entry, click outside, or press Escape.

### Pattern 3: Programmatic Popup

To open a menu from C code (e.g. from a keyboard shortcut handler), use `IswPopup`:

```c
/* Position the menu first */
n = 0;
IswSetArg(args[n], IswNx, x_pos); n++;
IswSetArg(args[n], IswNy, y_pos); n++;
IswSetValues(my_menu, args, n);

IswPopup(my_menu, IswGrabNonexclusive);
```

**Grab-kind choice:**

- `IswGrabNonexclusive` — menu stays up until an entry is picked. Clicks outside the menu are delivered to whatever widget they land on and do *not* dismiss the menu. Submenus and keyboard-triggered menus use this.
- `IswGrabExclusive` — all pointer events go to the menu. A click outside hits the menu's `<BtnDown>` translation, which calls `popdown()`. Use this when you want a modal-style popup that dismisses on any outside click.
- `IswGrabNone` — no grab, no redirection. The menu stays up until you explicitly pop it down.

### Pattern 4: Cascade / Submenu

Any `SmeBSB` entry can open a submenu. Set `IswNmenuName` on the entry:

```c
/* Submenu popup shell — child of the parent menu */
Widget sub = IswCreatePopupShell("exportMenu", simpleMenuWidgetClass,
                                parent_menu, NULL, 0);

/* Entries in the submenu */
n = 0;
IswSetArg(args[n], IswNlabel, "PNG"); n++;
Widget png = IswCreateManagedWidget("png", smeBSBObjectClass, sub, args, n);
IswAddCallback(png, IswNcallback, export_cb, (IswPointer)"png");

/* Cascade entry in the parent menu */
n = 0;
IswSetArg(args[n], IswNlabel, "Export"); n++;
IswSetArg(args[n], IswNmenuName, "exportMenu"); n++;
IswCreateManagedWidget("export", smeBSBObjectClass, parent_menu, args, n);
/* No callback on the cascade entry — selection happens in the submenu. */
```

The submenu pops up when the cursor highlights the cascade entry and pops down when the cursor moves away. Submenus use `IswGrabNonexclusive`. They nest to arbitrary depth.

## Grab Kinds Reference

| Call | Grab | Use case |
|---|---|---|
| `IswPopup(w, IswGrabNone)` | None | Popup that doesn't interfere with other widgets |
| `IswPopup(w, IswGrabNonexclusive)` | Non-exclusive | Menus, submenus (the usual choice) |
| `IswPopup(w, IswGrabExclusive)` | Exclusive | Modal popup; any outside click is delivered to the menu |

## Common Mistakes

### Mistake: Using IswGrabNone and expecting click-outside-to-dismiss

```c
/* This menu will NOT dismiss when you click outside it */
IswPopup(my_menu, IswGrabNone);
```

With no grab, clicks outside the menu go to whatever widget the pointer is over. The menu stays up until something explicitly pops it down.

**Fix:** Use `IswGrabExclusive` (for click-outside-to-dismiss) or `IswGrabNonexclusive` (the default menu behavior).

### Mistake: Mismatched widget name and IswNmenuName

```c
IswSetArg(args[n], IswNmenuName, "fileMenu"); n++;
/* ... */
IswCreatePopupShell("file_menu", ...);  /* WRONG — name mismatch */
```

`MenuButton` and cascade entries look up menus by walking the widget tree and calling `IswNameToWidget`. If the popup shell's widget name doesn't match `IswNmenuName`, the menu won't be found and you'll get a warning: `"Could not find menu widget named fileMenu"`.

### Mistake: Wrong parent for the popup shell

The popup shell must be findable by `IswNameToWidget` from the `MenuButton` or cascade entry. The simplest rule: **make the popup shell a child of the widget that triggers it** (the `MenuButton` or the parent `SimpleMenu`).

## SimpleMenu Default Translations

These are the built-in translations on every `SimpleMenu`:

```
<EnterWindow>:  highlight()
<LeaveWindow>:  unhighlight()
<Motion>:       highlight()
<BtnDown>:      notify() unhighlight() popdown()
<Key>Down:      next-entry()
<Key>Up:        prev-entry()
<Key>Home:      first-entry()
<Key>End:       last-entry()
<Key>Return:    notify() unhighlight() popdown()
<Key>space:     notify() unhighlight() popdown()
<Key>Escape:    unhighlight() popdown()
```

`notify()` fires the highlighted entry's `IswNcallback`. `popdown()` dismisses the entire menu chain (including parent menus for submenus).

A button **press** on an entry activates it. The opening press (on the `MenuButton` or the widget holding the `IswMenuPopup` translation) and the selecting press (on an entry) are independent events.
