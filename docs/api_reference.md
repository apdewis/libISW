# ISW Public API Reference

This document describes the public C API of libISW. Headers are located in `include/ISW/`.

## Initialization

```c
#include <ISW/Intrinsic.h>
#include <ISW/Shell.h>
```

### Core Functions

| Function | Description |
|----------|-------------|
| `IswCreateApplicationContext()` | Create an application context |
| `IswOpenDisplay(ac, display_string, app_name, app_class, options, num_options, argc, argv)` | Open a display |
| `IswDisplayInitialize(ac, dpy, app_name, app_class, options, num_options, argc, argv)` | Initialize the display |
| `IswAppInitialize(ac, app_class, options, num_options, argc, argv, fallback_resources, widget_class, num_args)` | Create a top-level shell (obsolete — use `IswOpenApplication`) |
| `IswOpenApplication(ac, app_class, options, num_options, argc, argv, fallback_resources, widget_class, num_args)` | Create a shell and application context |
| `IswAppMainLoop(ac)` | Run the event loop (non-returning) |
| `IswAppProcessEvent(ac, mask)` | Process pending events |
| `IswAppPending(ac)` | Return pending event mask |
| `IswDestroyApplicationContext(ac)` | Destroy application context |

### Shell Functions

| Function | Description |
|----------|-------------|
| `IswAppCreateShell(app_name, app_class, widget_class, dpy, args, num_args)` | Create a shell on a display |
| `IswCreatePopupShell(name, widget_class, parent, args, num_args)` | Create a popup shell |

## Widget Creation and Management

```c
#include <ISW/Intrinsic.h>
#include <ISW/Shell.h>
```

### Widget Creation

| Function | Description |
|----------|-------------|
| `IswCreateWidget(name, widget_class, parent, args, num_args)` | Create a widget |
| `IswCreateManagedWidget(name, widget_class, parent, args, num_args)` | Create and manage a widget |
| `IswVaCreateWidget(name, widget_class, parent, ...)` | Variadic create widget |
| `IswVaCreateManagedWidget(name, widget_class, parent, ...)` | Variadic create managed widget |
| `IswVaCreatePopupShell(name, widget_class, parent, ...)` | Variadic create popup shell |

### Widget Lifecycle

| Function | Description |
|----------|-------------|
| `IswRealizeWidget(widget)` | Realize the widget tree |
| `IswUnrealizeWidget(widget)` | Unrealize a widget |
| `IswDestroyWidget(widget)` | Destroy a widget |
| `IswManageChild(widget)` | Manage a child |
| `IswUnmanageChild(widget)` | Unmanage a child |
| `IswManageChildren(children, num_children)` | Manage multiple children |
| `IswUnmanageChildren(children, num_children)` | Unmanage multiple children |

### Widget Visibility

| Function | Description |
|----------|-------------|
| `IswMapWidget(widget)` | Map a widget |
| `IswUnmapWidget(widget)` | Unmap a widget |
| `IswPopup(shell, grab_kind)` | Popup a shell widget |
| `IswPopdown(shell)` | Popdown a shell |

## Resource Management

```c
#include <ISW/Intrinsic.h>
```

### Setting and Getting Values

| Function | Description |
|----------|-------------|
| `IswSetValues(widget, args, num_args)` | Set widget resources |
| `IswGetValues(widget, args, num_args)` | Get widget resources |
| `IswVaSetValues(widget, ...)` | Variadic set values |
| `IswVaGetValues(widget, ...)` | Variadic get values |
| `IswSetSubvalues(base, resources, num_resources, args, num_args)` | Set subvalues |
| `IswVaSetSubvalues(base, resources, num_resources, ...)` | Variadic set subvalues |
| `IswGetSubvalues(base, resources, num_resources, args, num_args)` | Get subvalues |

### ArgList Macros

```c
#define IswSetArg(arg, name, value)  /* Set a resource argument */
#define IswArgBuilderInit()           /* Initialize an arg builder */
#define IswArgBuilderAdd(ab, name, value)  /* Add an argument */
```

## Callbacks and Actions

```c
#include <ISW/Intrinsic.h>
```

### Callback Management

| Function | Description |
|----------|-------------|
| `IswAddCallback(widget, callback_name, proc, closure)` | Add a callback |
| `IswRemoveCallback(widget, callback_name, proc, closure)` | Remove a callback |
| `IswAddCallbacks(widget, callback_name, callbacks)` | Add callbacks |
| `IswRemoveCallbacks(widget, callback_name, callbacks)` | Remove callbacks |
| `IswRemoveAllCallbacks(widget, callback_name)` | Remove all callbacks |
| `IswCallCallbacks(widget, callback_name, call_data)` | Invoke callbacks |
| `IswHasCallbacks(widget, callback_name)` | Check if callbacks exist |

### Action Management

| Function | Description |
|----------|-------------|
| `IswAppAddActions(app, actions, num_actions)` | Register actions |
| `IswAppAddActionHook(app, proc, client_data)` | Add an action hook |
| `IswRemoveActionHook(id)` | Remove an action hook |
| `IswCallActionProc(widget, action, event, params, num_params)` | Dispatch an action |

## Event Management

```c
#include <ISW/Intrinsic.h>
```

### Event Handlers

| Function | Description |
|----------|-------------|
| `IswAddEventHandler(widget, event_mask, nonmaskable, proc, closure)` | Add event handler |
| `IswRemoveEventHandler(widget, event_mask, nonmaskable, proc, closure)` | Remove event handler |
| `IswAddRawEventHandler(widget, event_mask, nonmaskable, proc, closure)` | Add raw event handler |
| `IswRemoveRawEventHandler(widget, event_mask, nonmaskable, proc, closure)` | Remove raw event handler |
| `IswInsertEventTypeHandler(widget, type, select_data, proc, closure, position)` | Insert event type handler |
| `IswRemoveEventTypeHandler(widget, type, select_data, proc, closure)` | Remove event type handler |

### Event Loop

| Function | Description |
|----------|-------------|
| `IswNextEvent(event)` | Get next event (obsolete) |
| `IswAppNextEvent(app, event)` | Get next event |
| `IswDispatchEvent(event, dpy)` | Dispatch an event |
| `IswDispatchEventToWidget(widget, event)` | Dispatch to widget |

### Timers and Input

| Function | Description |
|----------|-------------|
| `IswAppAddTimeOut(app, interval, proc, closure)` | Add timer |
| `IswRemoveTimeOut(timer)` | Remove timer |
| `IswAppAddInput(app, source, condition, proc, closure)` | Add file descriptor callback |
| `IswRemoveInput(id)` | Remove input |
| `IswAppAddSignal(app, proc, closure)` | Add signal handler |
| `IswRemoveSignal(id)` | Remove signal |
| `IswAppAddWorkProc(app, proc, client_data)` | Add work procedure |
| `IswAppAddBlockHook(app, proc, client_data)` | Add block hook |
| `IswRemoveBlockHook(id)` | Remove block hook |

## Selection

```c
#include <ISW/Intrinsic.h>
```

### Selection API

| Function | Description |
|----------|-------------|
| `IswOwnSelection(widget, selection, time, convert, lose, done)` | Own a selection |
| `IswOwnSelectionIncremental(widget, selection, time, convert, lose, done, cancel, client_data)` | Own with INCR |
| `IswSelectionOffer(widget, time, offer, lose)` | Simplified text offer |
| `IswSelectionDisown(widget, time)` | Disown selection |
| `IswSelectionRequestText(widget, time, receive, closure)` | Request text from selection |

## Grab Management

```c
#include <ISW/Intrinsic.h>
```

| Function | Description |
|----------|-------------|
| `IswAddGrab(widget, exclusive)` | Add keyboard/mouse grab |
| `IswRemoveGrab(widget)` | Remove grab |

## Geometry Management

```c
#include <ISW/Intrinsic.h>
```

| Function | Description |
|----------|-------------|
| `IswMakeGeometryRequest(widget, request, reply)` | Request geometry |
| `IswQueryGeometry(widget, intended, preferred)` | Query geometry |
| `IswMakeResizeRequest(widget, width, height, width_return, height_return)` | Request resize |
| `IswTranslateCoords(widget, x, y, rootx_return, rooty_return)` | Translate coordinates |

## Translation Management

```c
#include <ISW/Intrinsic.h>
```

| Function | Description |
|----------|-------------|
| `IswParseTranslationTable(table)` | Parse a translation table string |
| `IswParseAcceleratorTable(source)` | Parse an accelerator table |
| `IswOverrideTranslations(widget, translations)` | Override widget translations |
| `IswAugmentTranslations(widget, translations)` | Augment translations (merge) |
| `IswInstallAccelerators(destination, source)` | Install accelerators |
| `IswInstallAllAccelerators(destination, source)` | Install all accelerators |
| `IswUninstallTranslations(widget)` | Remove translations |

## Keyboard

| Function | Description |
|----------|-------------|
| `IswSetKeyboardFocus(widget, descendant)` | Set keyboard focus |
| `IswGetKeyboardFocusWidget(widget)` | Get keyboard focus widget |
| `IswSetMultiClickTime(dpy, milliseconds)` | Set multi-click time |
| `IswGetMultiClickTime(dpy)` | Get multi-click time |

## Key Processing

| Function | Description |
|----------|-------------|
| `IswGetActionKeysym(event, modifiers, dpy)` | Get keysym from action |
| `IswTranslateKeycode(dpy, keycode, modifiers, modifiers_return, keysym_return)` | Translate keycode |
| `IswTranslateKey(dpy, keycode, modifiers, modifiers_return, keysym_return)` | Translate key |
| `IswSetKeyTranslator(dpy, proc)` | Set key translator |
| `IswRegisterCaseConverter(dpy, proc, start, stop)` | Register case converter |
| `IswConvertCase(dpy, keysym, lower_return, upper_return)` | Convert case |
| `IswRegisterCaseConverter(dpy, proc, start, stop)` | Register case converter |

## Widget Information

| Function | Description |
|----------|-------------|
| `IswName(widget)` | Get widget name |
| `IswSuperclass(widget)` | Get superclass |
| `IswClass(widget)` | Get widget class |
| `IswParent(widget)` | Get parent widget |
| `IswDisplayToApplicationContext(dpy)` | Get app context from display |
| `IswWidgetToApplicationContext(widget)` | Get app context from widget |

## Display Management

| Function | Description |
|----------|-------------|
| `IswCloseDisplay(dpy)` | Close a display |
| `IswDatabase(dpy)` | Get resource database |
| `IswScreenDatabase(screen)` | Get screen resource database |
| `IswReloadResources(widget)` | Reload resources |
| `IswReloadScreenDatabase(screen)` | Reload screen resources |
| `IswGetApplicationResources(widget, base, resources, num_resources, args, num_args)` | Get application resources |
| `IswVaGetApplicationResources(widget, base, resources, num_resources, ...)` | Variadic get resources |
| `IswGetSubresources(widget, base, name, class, resources, num_resources, args, num_args)` | Get subresources |

## Type Predicates

| Macro/Function | Description |
|----------------|-------------|
| `IswIsRectObj(object)` | Check if RectObj |
| `IswIsWidget(object)` | Check if Widget |
| `IswIsComposite(widget)` | Check if Composite |
| `IswIsConstraint(widget)` | Check if Constraint |
| `IswIsShell(widget)` | Check if Shell |
| `IswIsRealized(widget)` | Check if realized |
| `IswIsSensitive(widget)` | Check if sensitive |
| `IswIsManaged(widget)` | Check if managed |
| `IswIsOverrideShell(widget)` | Check if override shell |
| `IswIsVendorShell(widget)` | Check if vendor shell |
| `IswIsTransientShell(widget)` | Check if transient shell |
| `IswIsTopLevelShell(widget)` | Check if top-level shell |
| `IswIsApplicationShell(widget)` | Check if application shell |
| `IswIsApplicationShell(widget)` | Check if application shell |
| `IswIsSubclass(object, class)` | Check subclass |
| `IswIsObject(object)` | Check if Object |

## Widget Class Constants

| Constant | Description |
|----------|-------------|
| `simpleWidgetClass` | Simple widget class |
| `commandWidgetClass` | Command widget class |
| `formWidgetClass` | Form widget class |
| `labelWidgetClass` | Label widget class |
| `boxWidgetClass` | Box widget class |
| `listWidgetClass` | List widget class |
| `textWidgetClass` | Text widget class |
| `textSinkWidgetClass` | Text sink widget class |
| `textSrcWidgetClass` | Text source widget class |
| `toggleWidgetClass` | Toggle widget class |
| `treeWidgetClass` | Tree widget class |
| `viewportWidgetClass` | Viewport widget class |
| `paneWidgetClass` | Paned widget class |
| `dialogWidgetClass` | Dialog widget class |
| `menuButtonWidgetClass` | Menu button widget class |
| `menuBarWidgetClass` | Menu bar widget class |
| `drawingAreaWidgetClass` | Drawing area widget class |
| `colorPickerWidgetClass` | Color picker widget class |
| `sliderWidgetClass` | Slider widget class |
| `spinBoxWidgetClass` | Spin box widget class |
| `comboBoxWidgetClass` | Combo box widget class |
| `listBoxWidgetClass` | List box widget class |
| `listViewWidgetClass` | List view widget class |
| `fileChooserWidgetClass` | File chooser widget class |
| `tabsWidgetClass` | Tabs widget class |
| `iconViewWidgetClass` | Icon view widget class |
| `fontChooserWidgetClass` | Font chooser widget class |
| `toolbarWidgetClass` | Toolbar widget class |
| `statusBarWidgetClass` | Status bar widget class |
| `progressBarWidgetClass` | Progress bar widget class |
| `pannedWidgetClass` | Panner widget class |
| `repeaterWidgetClass` | Repeater widget class |
| `scrollBarWidgetClass` | Scroll bar widget class |
| `scrollWheelWidgetClass` | Scroll wheel widget class |
| `gripWidgetClass` | Grip widget class |

## Header Files

All public headers are in the `include/ISW/` directory:

### Core Headers

| Header | Contents |
|--------|----------|
| `ISW/Intrinsic.h` | Core intrinsic functions, types, and constants |
| `ISW/Shell.h` | Shell widget and window management |
| `ISW/IswTypes.h` | Neutral type definitions |
| `ISW/ISWInit.h` | Widget class initialization |

### Widget Headers

| Header | Contents |
|--------|----------|
| `ISW/Box.h` | Box widget (container) |
| `ISW/Command.h` | Command widget |
| `ISW/Form.h` | Form widget with constraint layout |
| `ISW/Label.h` | Label widget |
| `ISW/Text.h` | Text widget |
| `ISW/Toggle.h` | Toggle widget (radio/checkbox) |
| `ISW/List.h` | List widget |
| `ISW/Tree.h` | Tree widget |
| `ISW/Viewport.h` | Viewport widget |
| `ISW/Paned.h` | Split panes |
| `ISW/Dialog.h` | Dialog widget |
| `ISW/MenuButton.h` | Menu button |
| `ISW/MenuBar.h` | Menu bar |
| `ISW/DrawingArea.h` | Drawing area |
| `ISW/ColorPicker.h` | Color picker |
| `ISW/Slider.h` | Slider/slider control |
| `ISW/ComboBox.h` | ComboBox widget |
| `ISW/SpinBox.h` | Spin box |
| `ISW/ProgressBar.h` | Progress bar |
| `ISW/Status.h` | Status bar |
| `ISW/Tabs.h` | Tabbed containers |
| `ISW/Toolbar.h` | Toolbar |
| `ISW/Scroll.h` | Scroll bar |
| `ISW/Grip.h` | Grip widget |
| `ISW/Repeater.h` | Repeater widget |

### Utility Headers

| Header | Contents |
|--------|----------|
| `ISW/IswQuark.h` | String interning |
| `ISW/IswValue.h` | Value storage |
| `ISW/IswDatabase.h` | Resource database |
| `ISW/IswOptions.h` | Options |
| `ISW/IswDrag.h` | Drag and drop |
| `ISW/ISWRender.h` | Rendering API |
| `ISW/ISWPNG.h` | PNG support |
| `ISW/ISWSVG.h` | SVG support |

## Constants

### Resource Name Constants

Defined in `ISWStringDefs.h` (included via `Intrinsic.h`) or via string literals when not defined.

### Event Masks

| Constant | Value |
|----------|-------|
| `IswNoEventMask` | 0 |
| `IswKeyPressMask` | (1L<<0) |
| `IswKeyReleaseMask` | (1L<<1) |
| `IswButtonPressMask` | (1L<<2) |
| `IswButtonReleaseMask` | (1L<<3) |
| `IswEnterWindowMask` | (1L<<4) |
| `IswLeaveWindowMask` | (1L<<5) |
| `IswPointerMotionMask` | (1L<<6) |
| `IswPointerMotionHintMask` | (1L<<7) |
| `IswButtonMotionMask` | (1L<<13) |
| `IswExposureMask` | (1L<<15) |
| `IswStructureNotifyMask` | (1L<<17) |
| `IswSubstructureNotifyMask` | (1L<<19) |
| `IswAllEvents` | (-1L) |

### Geometry Request Masks

| Constant | Value |
|----------|-------|
| `IswCWX` | (1<<0) |
| `IswCWY` | (1<<1) |
| `IswCWWidth` | (1<<2) |
| `IswCWHeight` | (1<<3) |
| `IswCWBorderWidth` | (1<<4) |
| `IswCWSibling` | (1<<5) |
| `IswCWStackMode` | (1<<6) |

### Window Creation Masks

| Constant | Value |
|----------|-------|
| `IswCWBackPixmap` | (1u<<0) |
| `IswCWBackPixel` | (1u<<1) |
| `IswCWBorderPixmap` | (1u<<2) |
| `IswCWBorderPixel` | (1u<<3) |
| `IswCWBitGravity` | (1u<<4) |
| `IswCWWinGravity` | (1u<<5) |
| `IswCWBackingStore` | (1u<<6) |
| `IswCWOverrideRedirect` | (1u<<9) |
| `IswCWSaveUnder` | (1u<<10) |
| `IswCWColormap` | (1u<<13) |
| `IswCWCursor` | (1u<<14) |

### Grab Kind

| Constant | Value |
|----------|-------|
| `IswGrabNone` | 0 |
| `IswGrabNonexclusive` | 1 |
| `IswGrabExclusive` | 2 |

### Geometry Result

| Constant | Value |
|----------|-------|
| `IswGeometryYes` | 0 |
| `IswGeometryNo` | 1 |
| `IswGeometryAlmost` | 2 |
| `IswGeometryDone` | 3 |

### Text Widget Constants

| Constant | Value |
|----------|-------|
| `IswtextScrollNever` | 0 |
| `IswtextScrollWhenNeeded` | 1 |
| `IswtextScrollAlways` | 2 |
| `IswtextWrapNever` | 0 |
| `IswtextWrapLine` | 1 |
| `IswtextWrapWord` | 2 |
| `IswtextResizeNever` | 0 |
| `IswtextResizeWidth` | 1 |
| `IswtextResizeHeight` | 2 |
| `IswtextResizeBoth` | 3 |
| `IswsdLeft` | 0 |
| `IswsdRight` | 1 |
| `IswtextRead` | 0 |
| `IswtextAppend` | 1 |
| `IswtextE` | 2 |

### Justify Constants

| Constant | Value |
|----------|-------|
| `IswJustifyLeft` | 0 |
| `IswJustifyCenter` | 1 |
| `IswJustifyRight` | 2 |

### Toggle Shape Constants

| Constant | Value |
|----------|-------|
| `IswToggleShapeAuto` | 0 |
| `IswToggleShapeCheckbox` | 1 |
| `IswToggleShapeRadio` | 2 |
| `IswToggleShapeSlide` | 3 |

### Edge Constants

| Constant | Value |
|----------|-------|
| `IswChainTop` | 0 |
| `IswChainBottom` | 1 |
| `IswChainLeft` | 2 |
| `IswChainRight` | 3 |
| `IswRubber` | 4 |

### Text Selection Constants

| Constant | Value |
|----------|-------|
| `IswselectNull` | 0 |
| `IswselectPosition` | 1 |
| `IswselectChar` | 2 |
| `IswselectWord` | 3 |
| `IswselectLine` | 4 |
| `IswselectParagraph` | 5 |
| `IswselectAll` | 6 |

### Text Replace Constants

| Constant | Value |
|----------|-------|
| `IswReplaceError` | -1 |
| `IswEditDone` | 0 |
| `IswEditError` | 1 |
| `IswPositionError` | 2 |

### Text Search Constants

| Constant | Value |
|----------|-------|
| `IswTextSearchError` | -12345 |

### Selection Constants

| Constant | Value |
|----------|-------|
| `ISW_SELECTION_NONE` | 0 |
| `ISW_SEL_STDTYPE_ID_LIST` | 0 |
| `ISW_SEL_STDTYPE_STRING` | 1 |
| `ISW_SEL_STDTYPE_INTEGER` | 2 |
| `ISW_SEL_EVENT_OTHER` | 0 |
| `ISW_SEL_EVENT_CLEAR` | 1 |
| `ISW_SEL_EVENT_REQUEST` | 2 |
| `ISW_SEL_EVENT_NOTIFY` | 3 |
| `ISW_SEL_EVENT_PROP_NEW` | 4 |
| `ISW_SEL_EVENT_PROP_DELETE` | 5 |

### Ellipsize Constants

| Constant | Value |
|----------|-------|
| `IswEllipsizeNone` | 0 |
| `IswEllipsizeStart` | 1 |
| `IswEllipsizeMiddle` | 2 |
| `IswEllipsizeEnd` | 3 |

### Render Constants

| Constant | Value |
|----------|-------|
| `ISW_RENDER_BACKEND_AUTO` | (C constant) |
| `ISW_RENDER_BACKEND_CAIRO_XCB` | (C constant) |
| `ISW_RENDER_BACKEND_EGL` | (C constant) |
| `ISW_RENDER_CAP_BASIC` | (C constant) |
| `ISW_RENDER_CAP_ANTIALIASING` | (C constant) |
| `ISW_RENDER_CAP_GRADIENTS` | (C constant) |
| `ISW_RENDER_CAP_ALPHA` | (C constant) |
| `ISW_RENDER_CAP_TRANSFORMS` | (C constant) |
| `ISW_RENDER_CAP_TEXT_ADVANCED` | (C constant) |
| `ISW_RENDER_CAP_HW_ACCEL` | (C constant) |
| `ISW_FILL_RULE_WINDING` | (C constant) |
| `ISW_FILL_RULE_EVEN_ODD` | (C constant) |
| `ISW_OPERATOR_OVER` | (C constant) |
| `ISW_OPERATOR_DIFFERENCE` | (C constant) |
