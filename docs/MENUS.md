# ISW Menu & Popup Usage Guide

This guide covers correct usage of ISW menus and popup shells. Read this before writing any menu code.

## The Golden Rule: No Spring-Loaded Grabs

**Never call `IswPopupSpringLoaded()` in application code.** It is an internal function used by `MenuButton` — it exists so that menus opened by a button press are dismissed on button release (the "spring-loaded" grab). Application code that calls it directly creates menus that behave erratically: they grab the pointer exclusively, eat events from other widgets, and won't dismiss properly.

If you find yourself reaching for `IswPopupSpringLoaded`, you are doing it wrong. Use one of the patterns below instead.

## Menu Patterns

### Pattern 1: MenuButton (Toolbar / Menu Bar)

This is the standard way to attach a dropdown menu to a button. `MenuButton` handles all grab and popup logic internally.

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

**What happens internally:** When the user presses a mouse button on the `MenuButton`, it calls `IswPopupSpringLoaded(menu)`. This sets up an exclusive grab so the menu will be dismissed on the corresponding button release (if no entry is selected) or when any entry's `notify` action fires. You never call this yourself.

**Dismiss behavior:**
- Click an entry → callback fires, menu pops down
- Click anywhere outside the menu → menu pops down (the grab catches it)
- Release the button without moving into the menu → menu pops down

### Pattern 2: Right-Click Context Menu (Translation Table)

For popup menus triggered by right-click (or any button press), use `IswMenuPopup` in a translation table. **Do not write a C action proc that calls IswPopupSpringLoaded.**

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

/* Install a translation that triggers the menu on right-click.
   IswMenuPopup is a built-in action — it handles spring-loaded grabs
   automatically when triggered by a ButtonPress event. */
IswOverrideTranslations(my_widget,
    IswParseTranslationTable("<Btn3Down>: IswMenuPopup(ctxMenu)"));
```

**Why this works:** `IswMenuPopup` inspects the event type. On `ButtonPress`, it calls `_IswPopup` with `spring_loaded=True` and `IswGrabExclusive` — exactly what `IswPopupSpringLoaded` does. On `KeyPress` or `EnterNotify`, it uses `IswGrabNonexclusive` without spring-loading. The grab ensures the menu dismisses properly on the next click.

**Dismiss behavior:** Identical to MenuButton — click an entry or click outside, the menu goes away.

### Pattern 3: Programmatic Popup (Non-Spring-Loaded)

For menus that should stay up until explicitly dismissed (e.g., opened by a keyboard shortcut or a non-button event), use `IswPopup` with `IswGrabNonexclusive`:

```c
/* Position the menu first */
n = 0;
IswSetArg(args[n], IswNx, x_pos); n++;
IswSetArg(args[n], IswNy, y_pos); n++;
IswSetValues(my_menu, args, n);

/* Pop it up with a non-exclusive grab.
   The menu will dismiss when an entry is selected (notify → popdown). */
IswPopup(my_menu, IswGrabNonexclusive);
```

**Dismiss behavior:** The `SimpleMenu` default translations handle popdown:

```
<BtnDown>: notify() unhighlight() popdown()
```

Clicking an entry fires `notify` (which calls your callback) then `popdown`. Clicking outside the menu fires `popdown` because `BtnDown` events inside the menu trigger it. However, clicking **outside** the menu with `IswGrabNonexclusive` will **not** dismiss it automatically — the event goes to whatever widget is under the pointer.

If you need click-outside-to-dismiss with a programmatic popup, use `IswGrabExclusive`:

```c
IswPopup(my_menu, IswGrabExclusive);
```

This grabs all pointer events. Any click outside the menu will be delivered to the menu (because of the grab), triggering its `<BtnDown>` translation which calls `popdown()`.

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

The submenu pops up automatically when the cursor highlights the cascade entry and pops down when the cursor moves away. Submenus use `IswGrabNonexclusive` internally. They nest to arbitrary depth.

## Grab Kinds Reference

| Function | Grab | Spring-loaded | Use case |
|---|---|---|---|
| `IswPopup(w, IswGrabNone)` | None | No | Popup that doesn't interfere with other widgets |
| `IswPopup(w, IswGrabNonexclusive)` | Non-exclusive | No | Submenus, keyboard-triggered menus |
| `IswPopup(w, IswGrabExclusive)` | Exclusive | No | Modal popup that eats all pointer events |
| `IswPopupSpringLoaded(w)` | Exclusive | Yes | **Internal only** — `MenuButton` uses this |

**Do not use `IswPopupSpringLoaded` in application code.** Use `IswMenuPopup` in a translation table or `IswPopup` with the appropriate grab kind.

## Common Mistakes

### Mistake: Writing a custom action proc that calls IswPopupSpringLoaded

```c
/* WRONG — do not do this */
static void popup_context_menu(Widget w, xcb_generic_event_t *event,
                               String *params, Cardinal *num_params)
{
    IswPopupSpringLoaded(my_menu);  /* BAD */
}
```

The problem: you now have a spring-loaded exclusive grab that expects a specific button-release event to dismiss it. If the grab state doesn't match what `SimpleMenu` expects, the menu can become stuck, or subsequent clicks go to the wrong widget.

**Fix:** Use `IswMenuPopup` in a translation table (Pattern 2 above).

### Mistake: Using IswGrabNone and expecting click-outside-to-dismiss

```c
/* This menu will NOT dismiss when you click outside it */
IswPopup(my_menu, IswGrabNone);
```

With no grab, clicks outside the menu go to whatever widget the pointer is over. The menu stays up until something explicitly pops it down.

**Fix:** Use `IswGrabExclusive` or `IswGrabNonexclusive` depending on whether you need click-outside-to-dismiss.

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
```

`notify()` fires the highlighted entry's `IswNcallback`. `popdown()` dismisses the entire menu chain (including parent menus for submenus).

A button **press** inside the menu triggers selection and dismissal. There is no press-then-release — one click (down) does it. This is by design: spring-loaded menus opened by `MenuButton` are already tracking a button-down event, so the corresponding button-up (or a new button-down) dismisses them.
