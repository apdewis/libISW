package main

import (
	"github.com/apdewis/libISW/go-isw"
)

func main() {
	app, toplevel := isw.AppInitialize("PanedDemo", nil)

	toplevel.SetValues(isw.NewArgList().
		Width(400).Height(500).
		Title("Paned Demo").
		AllowShellResize(true))

	hbox := isw.CreateManagedWidget("hbox", isw.FlexBoxClass, toplevel,
		isw.NewArgList().Orientation(isw.OrientHorizontal))

	paned := isw.CreateManagedWidget("paned", isw.PanedClass, hbox,
		isw.NewArgList().
			Orientation(isw.OrientHorizontal).
			FlexGrow(1))

	isw.CreateManagedWidget("top", isw.LabelClass, paned,
		isw.NewArgList().
			Label("Top Pane").
			PreferredPaneSize(150).
			Min(50).Max(300).
			ShowGrip(true))

	isw.CreateManagedWidget("middle", isw.LabelClass, paned,
		isw.NewArgList().
			Label("Middle Pane").
			PreferredPaneSize(200).
			Min(80).Max(400).
			ShowGrip(true))

	isw.CreateManagedWidget("bottom", isw.LabelClass, paned,
		isw.NewArgList().
			Label("Bottom Pane").
			PreferredPaneSize(150).
			Min(50).
			ShowGrip(false))

	toplevel.Realize()
	app.MainLoop()
}
