package main

import (
	"fmt"
	"unsafe"

	"github.com/apdewis/libISW/go-isw"
)

func main() {
	app, toplevel := isw.AppInitialize("GoHello", nil)

	box := isw.CreateManagedWidget("box", isw.BoxClass, toplevel,
		isw.NewArgList().Orientation(uintptr(isw.OrientVertical)))

	isw.CreateManagedWidget("label", isw.LabelClass, box,
		isw.NewArgList().Label("Hello from Go!"))

	btn := isw.CreateManagedWidget("quit", isw.CommandClass, box,
		isw.NewArgList().Label("Quit"))

	btn.AddCallback(isw.Ncallback, func(w isw.Widget, callData unsafe.Pointer) {
		fmt.Println("Quit button pressed")
		app.Destroy()
	})

	toplevel.Realize()
	app.MainLoop()
}
