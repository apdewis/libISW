package main

import (
	"fmt"
	"math"

	"github.com/apdewis/libISW/go-isw"
)

var (
	app       isw.AppContext
	statusLbl isw.Widget
	progBar   isw.Widget
)

func main() {
	app, toplevel := isw.AppInitialize("Isw3dDemo", nil)

	toplevel.SetValues(isw.NewArgList().
		Width(900).Height(650).
		Title("ISW Go Bindings Demo").
		AllowShellResize(true))

	mainWin := isw.CreateManagedWidget("mainWindow", isw.MainWindowClass, toplevel, nil)

	populateMenubar(isw.MainWindowMenuBar(mainWin), mainWin)
	createStatusBar(mainWin)

	tabs := isw.CreateManagedWidget("tabs", isw.TabsClass, mainWin,
		isw.NewArgList().TabHeight(28).TabSizing(1))

	createBasicWidgetsTab(tabs)
	createSelectionTab(tabs)
	createDrawingTab(tabs)
	createFormTab(tabs)

	isw.PrintBackendInfo()
	fmt.Println("ISW Go demo starting...")

	toplevel.Realize()
	app.MainLoop()
}

func populateMenubar(menubar isw.Widget, accelDest isw.Widget) {
	isw.CreateManagedWidget("file", isw.MenuButtonClass, menubar,
		isw.NewArgList().Label("File").MenuName("fileMenu"))

	fileMenu := isw.CreateWidget("fileMenu", isw.SimpleMenuClass, menubar, nil)
	newItem := isw.CreateManagedWidget("new", isw.SmeBSBClass, fileMenu, nil)
	newItem.SetValues(isw.NewArgList().Label("New"))
	isw.CreateManagedWidget("sep1", isw.SmeLineClass, fileMenu, nil)
	quitItem := isw.CreateManagedWidget("quit", isw.SmeBSBClass, fileMenu, nil)
	quitItem.SetValues(isw.NewArgList().Label("Quit"))

	newItem.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
		setStatus("New clicked")
	})
	quitItem.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
		fmt.Println("Quit selected from menu")
		app.Destroy()
	})

	helpMenu := isw.CreateWidget("helpMenu", isw.SimpleMenuClass, menubar, nil)
	isw.CreateManagedWidget("help", isw.MenuButtonClass, menubar,
		isw.NewArgList().Label("Help").MenuName("helpMenu"))

	aboutItem := isw.CreateManagedWidget("about", isw.SmeBSBClass, helpMenu, nil)
	aboutItem.SetValues(isw.NewArgList().Label("About"))
	aboutItem.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
		setStatus("ISW Go Bindings Demo — github.com/apdewis/libISW/go-isw")
	})
}

func createStatusBar(parent isw.Widget) {
	statusbar := isw.CreateManagedWidget("statusbar", isw.StatusBarClass, parent, nil)

	statusLbl = isw.CreateManagedWidget("statusText", isw.LabelClass, statusbar,
		isw.NewArgList().Label("Ready").StatusStretch(1))

	isw.CreateManagedWidget("statusRight", isw.LabelClass, statusbar,
		isw.NewArgList().Label("Go bindings"))
}

func setStatus(msg string) {
	statusLbl.SetValues(isw.NewArgList().Label(msg))
}

func createBasicWidgetsTab(tabs isw.Widget) {
	page := isw.CreateManagedWidget("basicPage", isw.ViewportClass, tabs,
		isw.NewArgList().AllowVert(true).UseRight(1).UseBottom(1).TabLabel("Basic Widgets"))

	vbox := isw.CreateManagedWidget("vbox", isw.BoxClass, page,
		isw.NewArgList().Orientation(isw.OrientVertical))

	isw.CreateManagedWidget("heading", isw.LabelClass, vbox,
		isw.NewArgList().Label("Buttons & Toggles"))

	hbox := isw.CreateManagedWidget("btnRow", isw.BoxClass, vbox,
		isw.NewArgList().Orientation(isw.OrientHorizontal))

	for i, label := range []string{"Action", "OK", "Cancel"} {
		idx := i
		lbl := label
		btn := isw.CreateManagedWidget(fmt.Sprintf("btn%d", i), isw.CommandClass, hbox,
			isw.NewArgList().Label(label).CornerRadius(4))
		btn.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
			setStatus(fmt.Sprintf("Button %d (%s) clicked", idx, lbl))
		})
	}

	tRow := isw.CreateManagedWidget("toggleRow", isw.BoxClass, vbox,
		isw.NewArgList().Orientation(isw.OrientHorizontal))

	for _, label := range []string{"Bold", "Italic", "Underline"} {
		lbl := label
		tog := isw.CreateManagedWidget("tog_"+label, isw.ToggleClass, tRow,
			isw.NewArgList().Label(label))
		tog.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
			setStatus(fmt.Sprintf("Toggle '%s' changed", lbl))
		})
	}

	isw.CreateManagedWidget("heading2", isw.LabelClass, vbox,
		isw.NewArgList().Label("Slider & Progress"))

	slider := isw.CreateManagedWidget("slider", isw.SliderClass, vbox,
		isw.NewArgList().
			MinimumValue(0).MaximumValue(100).SliderValue(0).ShowValue(true).
			Orientation(isw.OrientHorizontal).Width(300))

	progBar = isw.CreateManagedWidget("progress", isw.ProgressBarClass, vbox,
		isw.NewArgList().Width(300).Value(0))

	slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
		val := cd.Int()
		progBar.SetValues(isw.NewArgList().Value(val))
		setStatus(fmt.Sprintf("Slider: %d%%", val))
	})

	isw.CreateManagedWidget("heading3", isw.LabelClass, vbox,
		isw.NewArgList().Label("SpinBox"))

	spin := isw.CreateManagedWidget("spin", isw.SpinBoxClass, vbox,
		isw.NewArgList().
			SpinMinimum(0).SpinMaximum(100).SpinValue(42).SpinIncrement(5))
	spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd isw.CallData) {
		setStatus(fmt.Sprintf("SpinBox value: %d", cd.Int()))
	})

	isw.CreateManagedWidget("heading4", isw.LabelClass, vbox,
		isw.NewArgList().Label("Text Input"))

	isw.CreateManagedWidget("textEdit", isw.TextClass, vbox,
		isw.NewArgList().
			EditType(isw.TextEdit).String("Type here...").
			Width(400).Height(80).
			ScrollVertical(isw.TextScrollAlways).
			Wrap(isw.TextWrapWord))
}

func createSelectionTab(tabs isw.Widget) {
	page := isw.CreateManagedWidget("selPage", isw.ViewportClass, tabs,
		isw.NewArgList().AllowVert(true).UseRight(1).TabLabel("Selection"))

	vbox := isw.CreateManagedWidget("selVbox", isw.BoxClass, page,
		isw.NewArgList().Orientation(isw.OrientVertical))

	isw.CreateManagedWidget("listHeading", isw.LabelClass, vbox,
		isw.NewArgList().Label("List Widget"))

	items := []string{
		"Alpine", "Arch", "Debian", "Fedora", "Gentoo",
		"NixOS", "openSUSE", "Slackware", "Ubuntu", "Void",
	}
	list := isw.CreateManagedWidget("distroList", isw.ListClass, vbox,
		isw.NewArgList().
			List(items).
			DefaultColumns(2).ForceColumns(true).VerticalList(true))

	list.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
		ret := isw.ParseListCallbackData(cd)
		setStatus(fmt.Sprintf("Selected: %s (index %d)", ret.String, ret.Index))
	})

	isw.CreateManagedWidget("comboHeading", isw.LabelClass, vbox,
		isw.NewArgList().Label("ComboBox"))

	isw.CreateManagedWidget("combo", isw.ComboBoxClass, vbox,
		isw.NewArgList().List(items))

	isw.CreateManagedWidget("lbHeading", isw.LabelClass, vbox,
		isw.NewArgList().Label("ListBox"))

	lb := isw.CreateManagedWidget("listbox", isw.ListBoxClass, vbox,
		isw.NewArgList().Width(300).Height(150).SelectionMode(0))

	for _, lang := range []string{"C", "Go", "Rust", "Zig", "Hare"} {
		isw.CreateManagedWidget("row_"+lang, isw.ListBoxRowClass, lb,
			isw.NewArgList().Label(lang))
	}

	lb.AddCallback(isw.NselectCallback, func(w isw.Widget, cd isw.CallData) {
		setStatus("ListBox selection changed")
	})
}

func createDrawingTab(tabs isw.Widget) {
	page := isw.CreateManagedWidget("drawPage", isw.ViewportClass, tabs,
		isw.NewArgList().AllowVert(true).UseRight(1).TabLabel("Drawing"))

	vbox := isw.CreateManagedWidget("drawVbox", isw.BoxClass, page,
		isw.NewArgList().Orientation(isw.OrientVertical))

	isw.CreateManagedWidget("drawHeading", isw.LabelClass, vbox,
		isw.NewArgList().Label("DrawingArea — ISWRender API from Go"))

	da := isw.CreateManagedWidget("canvas", isw.DrawingAreaClass, vbox,
		isw.NewArgList().Width(500).Height(300).Background(0xFF1a1a2e))

	da.AddCallback(isw.NexposeCallback, func(w isw.Widget, cd isw.CallData) {
		d := isw.ParseDrawingCallbackData(cd)
		if d == nil || d.Render == nil {
			return
		}
		drawScene(d.Render)
	})
}

func drawScene(rc *isw.RenderContext) {
	rc.SetColor(isw.PixelARGB(255, 230, 57, 70))
	rc.FillRoundedRectangle(20, 20, 120, 80, 10)

	rc.SetColor(isw.PixelARGB(255, 69, 123, 157))
	rc.FillRoundedRectangle(160, 20, 120, 80, 10)

	rc.SetColor(isw.PixelARGB(255, 241, 250, 238))
	rc.DrawString("Go + ISW", 40, 70)

	rc.SetColor(isw.PixelARGB(255, 168, 218, 220))
	rc.SetLineWidth(2.0)
	rc.StrokeRectangle(300, 20, 150, 80)

	rc.SetColor(isw.PixelARGB(180, 255, 200, 50))
	rc.PathBegin()
	cx, cy, r := 200.0, 200.0, 60.0
	for i := 0; i < 5; i++ {
		angle := float64(i)*4*math.Pi/5 - math.Pi/2
		px := cx + r*math.Cos(angle)
		py := cy + r*math.Sin(angle)
		if i == 0 {
			rc.PathMoveTo(px, py)
		} else {
			rc.PathLineTo(px, py)
		}
	}
	rc.PathClose()
	rc.FillPreserve()
	rc.SetColor(isw.PixelARGB(255, 255, 255, 255))
	rc.SetLineWidth(2.0)
	rc.Stroke()

	rc.SetColor(isw.PixelARGB(255, 29, 53, 87))
	for i := 0; i < 8; i++ {
		x := 320 + i*20
		h := 30 + (i*17)%60
		rc.FillRectangle(x, 260-h, 14, h)
	}

	rc.SetColor(isw.PixelARGB(128, 255, 255, 255))
	rc.DrawString("Rendered from Go via ISWRender API", 20, 290)
}

func createFormTab(tabs isw.Widget) {
	page := isw.CreateManagedWidget("formPage", isw.ViewportClass, tabs,
		isw.NewArgList().AllowVert(true).UseRight(1).TabLabel("Form Layout"))

	form := isw.CreateManagedWidget("form", isw.FormClass, page,
		isw.NewArgList().DefaultDistance(8))

	nameLbl := isw.CreateManagedWidget("nameLbl", isw.LabelClass, form,
		isw.NewArgList().Label("Name:").Left(isw.ChainLeft).Top(isw.ChainTop))

	nameEntry := isw.CreateManagedWidget("nameEntry", isw.TextClass, form,
		isw.NewArgList().
			EditType(isw.TextEdit).String("").Width(250).
			FromHoriz(nameLbl).Left(isw.ChainLeft).Top(isw.ChainTop))

	emailLbl := isw.CreateManagedWidget("emailLbl", isw.LabelClass, form,
		isw.NewArgList().Label("Email:").FromVert(nameLbl).Left(isw.ChainLeft).Top(isw.ChainTop))

	isw.CreateManagedWidget("emailEntry", isw.TextClass, form,
		isw.NewArgList().
			EditType(isw.TextEdit).String("").Width(250).
			FromHoriz(emailLbl).FromVert(nameEntry).Left(isw.ChainLeft).Top(isw.ChainTop))

	msgLbl := isw.CreateManagedWidget("msgLbl", isw.LabelClass, form,
		isw.NewArgList().Label("Message:").FromVert(emailLbl).Left(isw.ChainLeft).Top(isw.ChainTop))

	isw.CreateManagedWidget("msgEntry", isw.TextClass, form,
		isw.NewArgList().
			EditType(isw.TextEdit).String("").
			Width(400).Height(100).
			ScrollVertical(isw.TextScrollAlways).
			Wrap(isw.TextWrapWord).
			FromVert(msgLbl).Left(isw.ChainLeft).Top(isw.ChainTop))

	submit := isw.CreateManagedWidget("submit", isw.CommandClass, form,
		isw.NewArgList().
			Label("Submit").CornerRadius(4).
			FromVert(msgLbl).VertDistance(120).Left(isw.ChainLeft).Top(isw.ChainTop))

	submit.AddCallback(isw.Ncallback, func(w isw.Widget, cd isw.CallData) {
		setStatus("Form submitted!")
	})
}
