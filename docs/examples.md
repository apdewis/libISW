# ISW Examples

This document provides usage examples for the ISW C API and Go bindings.

## C API Examples

### Minimal C Program

```c
#include <ISW/Intrinsic.h>
#include <ISW/Box.h>
#include <ISW/Command.h>
#include <ISW/Form.h>
#include <ISW/Label.h>
#include <ISW/Text.h>
#include <ISW/Toggle.h>
#include <ISW/List.h>
#include <ISW/Slider.h>

int main(int argc, char **argv)
{
    IswAppContext app;
    Widget toplevel, box, button;

    toplevel = IswAppCreateShell("MyApp", "MyAppClass", applicationShellWidgetClass,
                                  NULL, NULL, 0);
    box = IswCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);
    button = IswCreateManagedWidget("quit", commandWidgetClass, box, NULL, 0);

    IswRealizeWidget(toplevel);
    IswAppMainLoop(app);

    return 0;
}
```

### Using ArgList for Widget Creation

```c
#include <ISW/Form.h>
#include <ISW/Label.h>

Widget CreateFormWithLabel(Widget parent)
{
    Arg args[10];
    Cardinal n = 0;

    /* Create form with horizontal layout */
    XtSetArg(args[n], IswNhSpace, 10, n++);
    XtSetArg(args[n], IswNvSpace, 5, n++);
    Widget form = IswCreateManagedWidget("form", formWidgetClass, parent, args, n);

    /* Create label with text */
    XtSetArg(args[n], IswNlabel, "Hello World", n++);
    XtSetArg(args[n], IswNfont, my_font, n++);
    Widget label = IswCreateManagedWidget("label", labelWidgetClass, form, args, n);

    return form;
}
```

### Using Form Constraints

```c
#include <ISW/Form.h>
#include <ISW/Constraint.h>

/* Set form constraint: widget at top-left of form */
Arg args[10];
Cardinal n = 0;

XtSetArg(args[n], IswNtop, IswChainTop, n++);
XtSetArg(args[n], IswNleft, IswChainLeft, n++);
XtSetArg(args[n], IswNresizable, True, n++);
XtSetValues(child, args, n);
```

### Adding Callbacks

```c
#include <ISW/Intrinsic.h>

void ButtonCallback(Widget w, IswPointer client_data, IswListReturnStruct *list)
{
    printf("Button clicked!\n");
}

/* Attach callback to a button */
Widget button = IswCreateManagedWidget("button", commandWidgetClass, parent, NULL, 0);
IswAddCallback(button, "callback", ButtonCallback, NULL);
```

### Adding Actions

```c
#include <ISW/Intrinsic.h>

void myActionProc(Widget w, IswEvent *event, String *params, Cardinal num_params)
{
    if (strcmp(params[0], "quit") == 0) {
        exit(0);
    }
}

/* Register actions */
IswActionsRec actions[] = {
    {"quit", myActionProc},
    {"save", myActionProc},
};
IswAppAddActions(app, actions, IswNumber(actions));
```

### Event Handling

```c
#include <ISW/Intrinsic.h>

void MyEventHandler(Widget w, IswPointer closure, IswEvent *event,
                    Boolean *continue_to_dispatch)
{
    if (event->type == KeyPress) {
        printf("Key pressed: %d\n", event->xkey.keycode);
    }
    *continue_to_dispatch = True;
}

/* Add event handler to widget */
IswAddEventHandler(widget, IswKeyPressMask | IswKeyReleaseMask, False, MyEventHandler, NULL);
```

### Timers and Input

```c
#include <ISW/Intrinsic.h>

void TimerCallback(IswPointer closure, IswIntervalId *id)
{
    printf("Timer fired!\n");
}

void InputCallback(IswPointer closure, int *source, IswInputId *id)
{
    printf("Input ready on fd %d\n", *source);
}

/* Add timer */
IswAppAddTimeOut(app, 5000, TimerCallback, NULL);  /* 5 second timer */

/* Add input handler for stdin */
IswAppAddInput(app, fileno(stdin), IswInputReadMask, InputCallback, NULL);
```

### Selection Handling

```c
#include <ISW/Intrinsic.h>

Boolean MyConvertProc(Widget w, IswSelectionId *selection, IswSelectionId *target,
                       IswSelectionId *type, IswPointer *value, unsigned long *length,
                       int *format)
{
    *value = IswMalloc("Hello World");
    *length = strlen("Hello World") + 1;
    *format = 8;
    return True;
}

/* Own the selection */
IswOwnSelection(widget, my_selection, CurrentTime, MyConvertProc, NULL, NULL);
```

### Drag and Drop

```c
#include <ISW/IswDragDrop.h>

void DropCallback(Widget w, IswPointer client_data, IswDropCallbackData *drop)
{
    printf("Dropped %d URIs\n", drop->num_uris);
    for (int i = 0; i < drop->num_uris; i++) {
        printf("  URI[%d]: %s\n", i, drop->uris[i]);
    }
}

/* Enable drag and drop */
IswDndEnable(shell);
IswDndWidgetAcceptDrops(target);
IswDndSetAcceptedTypes(target, accepted_types, num_types);

/* Start a drag from a widget */
IswDragSourceDesc desc;
desc.types = accepted_types;
desc.num_types = num_types;
desc.actions = ISW_DND_ACTION_COPY | ISW_DND_ACTION_MOVE;
IswDndStartDrag(source, trigger_event, &desc);
```

### Drawing with Render Context

```c
#include <ISW/ISWRender.h>

void DrawCallback(Widget w, IswPointer closure, ISWDrawingCallbackData *data)
{
    ISWRenderContext *ctx = data->render_ctx;

    ISWRenderBegin(ctx);
    ISWRenderSave(ctx);

    /* Draw a filled rectangle */
    ISWRenderSetColor(ctx, PixelBlue);
    ISWRenderFillRectangle(ctx, 10, 10, 100, 100);

    /* Draw text */
    ISWRenderDrawString(ctx, "Hello World", strlen("Hello World"), 10, 120);

    ISWRenderRestore(ctx);
    ISWRenderEnd(ctx);
}
```

---

## Go Bindings Examples

### Minimal Go Program

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    // Initialize the toolkit and create a top-level shell
    ac, shell := isw.AppInitialize("MyApp", nil)

    // Create a box widget (container)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create a button
    button := isw.CreateWidget("quit", isw.CommandWidgetClass, box, nil)

    // Realize the widget tree
    box.Realize()

    // Run the event loop (blocks forever)
    ac.MainLoop()
}
```

### Creating a Form with Widgets

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    form := isw.CreateWidget("form", isw.FormWidgetClass, box, nil)

    // Add a label
    label := isw.CreateWidget("label", isw.LabelWidgetClass, form, nil)

    // Add a text field
    text := isw.CreateWidget("text", isw.TextWidgetClass, form, nil)

    // Add a command button
    button := isw.CreateWidget("submit", isw.CommandWidgetClass, form, nil)

    // Realize the widgets
    box.Realize()

    ac.MainLoop()
}
```

### Adding Callbacks

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    button := isw.CreateWidget("quit", isw.CommandWidgetClass, box, nil)

    // Register an action
    ac.AddActions(map[string]isw.ActionFunc{
        "quit": func(w isw.Widget, e isw.Event, params []string) {
            fmt.Println("Quit action triggered")
            ac.SetExitFlag()
        },
    })

    // Register a translation table
    button.AugmentTranslations("<Key>q:quit()")

    box.Realize()
    ac.MainLoop()
}
```

### Form Layout with Constraints

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    form := isw.CreateWidget("form", isw.FormWidgetClass, box, nil)

    // Create a label at top-left
    label := isw.CreateWidget("label", isw.LabelWidgetClass, form, nil)

    // Set form constraint: anchor to top-left
    args := isw.NewArgList().
        AddString(isw.Ntop, isw.ChainTop).
        AddString(isw.Nleft, isw.ChainLeft)
    label.SetValues(args)

    // Create a text field below
    text := isw.CreateWidget("text", isw.TextWidgetClass, form, nil)
    args2 := isw.NewArgList().
        AddString(isw.Ntop, isw.ChainTop).
        AddString(isw.NfromVert, label).
        AddString(isw.Nleft, IswNleft)
    text.SetValues(args2)

    box.Realize()
    ac.MainLoop()
}
```

### List Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    list := isw.CreateWidget("list", isw.ListWidgetClass, box, nil)

    // Set list items
    isw.ListChange(list, []string{"Item 1", "Item 2", "Item 3"}, 0, true)

    // Add callback for selection
    list.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s (index %d)\n", data.String, data.Index)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle/Radio Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create radio toggles
    radio1 := isw.CreateWidget("radio1", isw.ToggleWidgetClass, box, nil)
    radio2 := isw.CreateWidget("radio2", isw.ToggleWidgetClass, box, nil)

    // Set radio group - both toggles share the same group
    args := isw.NewArgList().AddString(isw.NradioGroup, radio1)
    radio2.SetValues(args)

    // Add callbacks
    radio1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Radio 1 selected")
    })
    radio2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Radio 2 selected")
    })

    box.Realize()
    ac.MainLoop()
}
```

### Dialog Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    dialog := isw.CreateWidget("dialog", isw.DialogWidgetClass, box, nil)

    // Add buttons to dialog
    isw.DialogAddButton(dialog, "OK", func(w isw.Widget, cd isw.CallData) {
        fmt.Printf("Value: %s\n", isw.DialogGetValueString(dialog))
    })
    isw.DialogAddButton(dialog, "Cancel", nil)

    box.Realize()
    ac.MainLoop()
}
```

### Text Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    text := isw.CreateWidget("text", isw.TextWidgetClass, box, nil)

    // Set text content
    isw.TextReplace(text, 0, 0, "Hello World")

    // Set text mode to read-only
    isw.TextSetSource(text, text, 0)

    // Add callbacks
    text.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseTextCallbackData(cd)
        fmt.Printf("Text modified at position %d\n", data.Position)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tree Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tree := isw.CreateWidget("tree", isw.TreeWidgetClass, box, nil)

    // Force layout recalculation
    isw.TreeForceLayout(tree)

    box.Realize()
    ac.MainLoop()
}
```

### Drawing Area

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    area := isw.CreateWidget("area", isw.DrawingAreaWidgetClass, box, nil)

    // Add expose callback for drawing
    area.AddCallback(isw.NexposeCallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseDrawingCallbackData(cd)
        if data.Render != nil {
            // Draw something on the rendering context
            data.Render.Begin()
            data.Render.Save()
            data.Render.SetColor(isw.PixelARGB(255, 255, 0, 0))
            data.Render.FillRectangle(10, 10, 100, 100)
            data.Render.Restore()
            data.Render.End()
        }
    })

    box.Realize()
    ac.MainLoop()
}
```

### Drag and Drop

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Enable drag and drop
    isw.DndEnable(shell)

    // Set up drop target
    isw.DndWidgetAcceptDrops(box)
    isw.DndSetAcceptedTypes(box, []string{"text/uri-list", "text/plain"})
    isw.DndSetAcceptedActions(box, isw.DndActionCopy)

    // Add drop callback
    box.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseDropCallbackData(cd)
        fmt.Printf("Dropped %d URI(s)\n", len(data.URIs))
    })

    // Start drag from a widget
    isw.DndStartDrag(source, &isw.DragSourceDesc{
        Types:   []string{"text/plain", "text/uri-list"},
        Actions: isw.DndActionCopy,
        Convert: func(w isw.Widget, targetType string) ([]byte, int, bool) {
            return []byte("Hello World"), 8, true
        },
        Finished: func(w isw.Widget, action isw.DndAction, accepted bool) {
            fmt.Printf("Drag finished, accepted=%v\n", accepted)
        },
    })

    box.Realize()
    ac.MainLoop()
}
```

### Selection Handling

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Set clipboard text
    isw.ClipboardSet(box, "Hello World")

    // Request clipboard text from another application
    isw.ClipboardRequestText(box, func(w isw.Widget, text string) {
        fmt.Printf("Got clipboard text: %s\n", text)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Event Handling

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Add event handler for key presses
    box.AddEventHandler(isw.KeyPressMask | isw.KeyReleaseMask, false, func(w isw.Widget, e isw.Event, cont *bool) {
        switch ev := e.(type) {
        case *isw.KeyEvent:
            fmt.Printf("Key: %d\n", ev.Key)
        case *isw.ButtonEvent:
            fmt.Printf("Button: %d\n", ev.Button)
        }
    })

    box.Realize()
    ac.MainLoop()
}
```

### Timer and Input

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Add a timer
    ac.AddTimeout(5000, func() {
        fmt.Println("Timer fired!")
    })

    // Add file descriptor input handler
    ac.AddInput(int(os.Stdin.Fd()), isw.PollReadMask, func(fd int) {
        fmt.Printf("Input ready on fd %d\n", fd)
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil)
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```

### Slider Widget

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    slider := isw.CreateWidget("slider", isw.SliderWidgetClass, box, nil)

    // Add callback for value changes
    slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSliderCallbackData(cd)
        fmt.Printf("Slider value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Toggle Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Create toggle buttons
    toggle1 := isw.CreateWidget("toggle1", isw.ToggleWidgetClass, box, nil)
    toggle2 := isw.CreateWidget("toggle2", isw.ToggleWidgetClass, box, nil)

    // Set up radio group
    args := isw.NewArgList().AddString(isw.NradioGroup, toggle1)
    toggle2.SetValues(args)

    // Add callbacks
    toggle1.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 1 activated")
    })
    toggle2.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        fmt.Println("Toggle 2 activated")
    })

    box.Realize()
    ac.MainLoop()
}
```

### SpinBox Widget

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    spin := isw.CreateWidget("spin", isw.SpinBoxWidgetClass, box, nil)

    // Add callback for value changes
    spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseSpinBoxCallbackData(cd)
        fmt.Printf("SpinBox value: %d\n", data.Value)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Color Picker

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    picker := isw.CreateWidget("color", isw.ColorPickerWidgetClass, box, nil)

    // Add callback for color changes
    picker.AddCallback(isw.NcolorChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseColorPickerCallbackData(cd)
        fmt.Printf("Color: RGB(%d, %d, %d)\n", data.Red, data.Green, data.Blue)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Combo Box

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    combo := isw.CreateWidget("combo", isw.ComboBoxWidgetClass, box, nil)

    // Set items
    isw.ListChange(combo, []string{"Option 1", "Option 2", "Option 3"}, 0, true)

    // Add callback for selection
    combo.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseListCallbackData(cd)
        fmt.Printf("Selected: %s\n", data.String)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Font Chooser

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    chooser := isw.CreateWidget("font", isw.FontChooserWidgetClass, box, nil)

    // Add callback for font changes
    chooser.AddCallback(isw.NfontChanged, func(w isw.Widget, cd isw.CallData) {
        data := isw.ParseFontChooserCallbackData(cd)
        fmt.Printf("Font family: %s, size: %d\n", data.Family, data.Size)
    })

    box.Realize()
    ac.MainLoop()
}
```

### Tabbed Container

```go
package main

import (
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    tabs := isw.CreateWidget("tabs", isw.TabsWidgetClass, box, nil)
    tab1 := isw.CreateWidget("tab1", isw.TabWidgetClass, tabs, nil)
    tab2 := isw.CreateWidget("tab2", isw.TabWidgetClass, tabs, nil)

    box.Realize()
    ac.MainLoop()
}
```

### Image Loading

```go
package main

import (
    "fmt"
    "github.com/yourpkg/isw"
)

func main() {
    ac, shell := isw.AppInitialize("MyApp", nil")
    box := isw.CreateWidget("box", isw.BoxWidgetClass, shell, nil)

    // Load an image from file
    img := isw.LoadImage("image.png", 96, "")
    if img == nil {
        // Load from SVG data
        img = isw.LoadSVGData("<svg>...</svg>", "px", 96, "")
    }

    // Get image dimensions
    width := img.Width()
    height := img.Height()

    // Rasterize to PNG
    png := isw.LoadPNGFile("test.png")

    box.Realize()
    ac.MainLoop()
}
```
