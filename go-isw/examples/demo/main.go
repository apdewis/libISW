package main

/*
#cgo pkg-config: isw xcb-icccm
#include <ISW/Intrinsic.h>
#include <ISW/DrawingArea.h>
#include <ISW/ISWRender.h>
#include <ISW/Text.h>
#include <ISW/List.h>
#include <ISW/Tabs.h>
#include <ISW/MainWindow.h>
#include <ISW/SimpleMenu.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>
#include <ISW/MenuButton.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    ISWRenderContext *render_ctx;
    IswEvent        *event;
    IswWindow        window;
} ISWDrawingCallbackData_Go;

static ISWRenderContext* _drawing_cb_render(void *cd) {
    return ((ISWDrawingCallbackData_Go*)cd)->render_ctx;
}

static Widget _main_window_menubar(Widget mw) {
    return IswMainWindowGetMenuBar(mw);
}

static Widget _create_sme_bsb(const char *name, Widget parent) {
    return IswCreateManagedWidget((String)name, smeBSBObjectClass, parent, NULL, 0);
}

static Widget _create_sme_line(const char *name, Widget parent) {
    return IswCreateManagedWidget((String)name, smeLineObjectClass, parent, NULL, 0);
}
*/
import "C"

import (
	"fmt"
	"math"
	"unsafe"

	"github.com/libisw/go-isw"
)

var (
	app       isw.AppContext
	statusLbl isw.Widget
	progBar   isw.Widget
	progValue int
	logWidget isw.Widget
)

func main() {
	app, toplevel := isw.AppInitialize("Isw3dDemo", nil)

	toplevel.SetValues(isw.NewArgList().
		Add(isw.Nwidth, 900).
		Add(isw.Nheight, 650).
		AddString(isw.Ntitle, "ISW Go Bindings Demo").
		Add(isw.NallowShellResize, 1))

	mainWin := isw.CreateManagedWidget("mainWindow", isw.MainWindowClass, toplevel, nil)

	populateMenubar(C._main_window_menubar(*(*C.Widget)(unsafe.Pointer(&mainWin))), mainWin)
	createStatusBar(mainWin)

	tabs := isw.CreateManagedWidget("tabs", isw.TabsClass, mainWin,
		isw.NewArgList().Add("tabHeight", 28).Add("tabSizing", 1))

	createBasicWidgetsTab(tabs)
	createSelectionTab(tabs)
	createDrawingTab(tabs)
	createFormTab(tabs)

	isw.PrintBackendInfo()
	fmt.Println("ISW Go demo starting...")

	toplevel.Realize()
	app.MainLoop()
}

func populateMenubar(menubar C.Widget, accelDest isw.Widget) {
	mbw := widgetFromC(menubar)

	fileBtn := isw.CreateManagedWidget("file", isw.MenuButtonClass, mbw,
		isw.NewArgList().AddString(isw.Nlabel, "File").AddString(isw.NmenuName, "fileMenu"))
	_ = fileBtn

	fileMenu := isw.CreatePopupShell("fileMenu", isw.SimpleMenuClass, mbw, nil)
	newItem := createSmeBSB("new", fileMenu, "New")
	_ = createSmeLine("sep1", fileMenu)
	quitItem := createSmeBSB("quit", fileMenu, "Quit")

	newItem.AddCallback(isw.Ncallback, func(w isw.Widget, cd unsafe.Pointer) {
		setStatus("New clicked")
	})
	quitItem.AddCallback(isw.Ncallback, func(w isw.Widget, cd unsafe.Pointer) {
		fmt.Println("Quit selected from menu")
		app.Destroy()
	})

	helpMenu := isw.CreatePopupShell("helpMenu", isw.SimpleMenuClass, mbw, nil)
	isw.CreateManagedWidget("help", isw.MenuButtonClass, mbw,
		isw.NewArgList().AddString(isw.Nlabel, "Help").AddString(isw.NmenuName, "helpMenu"))

	aboutItem := createSmeBSB("about", helpMenu, "About")
	aboutItem.AddCallback(isw.Ncallback, func(w isw.Widget, cd unsafe.Pointer) {
		setStatus("ISW Go Bindings Demo — github.com/libisw/go-isw")
	})
}

func widgetFromC(cw C.Widget) isw.Widget {
	return *(*isw.Widget)(unsafe.Pointer(&cw))
}

func createSmeBSB(name string, parent isw.Widget, label string) isw.Widget {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	w := widgetFromC(C._create_sme_bsb(cName, *(*C.Widget)(unsafe.Pointer(&parent))))
	w.SetValues(isw.NewArgList().AddString(isw.Nlabel, label))
	return w
}

func createSmeLine(name string, parent isw.Widget) isw.Widget {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	return widgetFromC(C._create_sme_line(cName, *(*C.Widget)(unsafe.Pointer(&parent))))
}

func createStatusBar(parent isw.Widget) {
	statusbar := isw.CreateManagedWidget("statusbar", isw.StatusBarClass, parent, nil)

	statusLbl = isw.CreateManagedWidget("statusText", isw.LabelClass, statusbar,
		isw.NewArgList().
			AddString(isw.Nlabel, "Ready").
			Add("statusStretch", 1))

	isw.CreateManagedWidget("statusRight", isw.LabelClass, statusbar,
		isw.NewArgList().AddString(isw.Nlabel, "Go bindings"))
}

func setStatus(msg string) {
	statusLbl.SetValues(isw.NewArgList().AddString(isw.Nlabel, msg))
}

func createBasicWidgetsTab(tabs isw.Widget) {
	page := isw.CreateManagedWidget("basicPage", isw.ViewportClass, tabs,
		isw.NewArgList().
			Add(isw.NallowVert, 1).
			Add("useRight", 1).
			Add("useBottom", 1).
			AddString(isw.NtabLabel, "Basic Widgets"))

	vbox := isw.CreateManagedWidget("vbox", isw.BoxClass, page,
		isw.NewArgList().Add(isw.Norientation, uintptr(isw.OrientVertical)))

	isw.CreateManagedWidget("heading", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "Buttons & Toggles"))

	hbox := isw.CreateManagedWidget("btnRow", isw.BoxClass, vbox,
		isw.NewArgList().Add(isw.Norientation, uintptr(isw.OrientHorizontal)))

	for i, label := range []string{"Action", "OK", "Cancel"} {
		idx := i
		lbl := label
		btn := isw.CreateManagedWidget(fmt.Sprintf("btn%d", i), isw.CommandClass, hbox,
			isw.NewArgList().AddString(isw.Nlabel, label).Add(isw.NcornerRadius, 4))
		btn.AddCallback(isw.Ncallback, func(w isw.Widget, cd unsafe.Pointer) {
			setStatus(fmt.Sprintf("Button %d (%s) clicked", idx, lbl))
		})
	}

	tRow := isw.CreateManagedWidget("toggleRow", isw.BoxClass, vbox,
		isw.NewArgList().Add(isw.Norientation, uintptr(isw.OrientHorizontal)))

	for _, label := range []string{"Bold", "Italic", "Underline"} {
		lbl := label
		tog := isw.CreateManagedWidget("tog_"+label, isw.ToggleClass, tRow,
			isw.NewArgList().AddString(isw.Nlabel, label))
		tog.AddCallback(isw.Ncallback, func(w isw.Widget, cd unsafe.Pointer) {
			setStatus(fmt.Sprintf("Toggle '%s' changed", lbl))
		})
	}

	isw.CreateManagedWidget("heading2", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "Slider & Progress"))

	slider := isw.CreateManagedWidget("slider", isw.SliderClass, vbox,
		isw.NewArgList().
			Add(isw.NminimumValue, 0).
			Add(isw.NmaximumValue, 100).
			Add(isw.NsliderValue, 0).
			Add(isw.NshowValue, 1).
			Add(isw.Norientation, uintptr(isw.OrientHorizontal)).
			Add(isw.Nwidth, 300))

	progBar = isw.CreateManagedWidget("progress", isw.ProgressBarClass, vbox,
		isw.NewArgList().
			Add(isw.Nwidth, 300).
			Add(isw.Nvalue, 0))

	slider.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd unsafe.Pointer) {
		val := int(uintptr(cd))
		progBar.SetValues(isw.NewArgList().Add(isw.Nvalue, uintptr(val)))
		setStatus(fmt.Sprintf("Slider: %d%%", val))
	})

	isw.CreateManagedWidget("heading3", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "SpinBox"))

	spin := isw.CreateManagedWidget("spin", isw.SpinBoxClass, vbox,
		isw.NewArgList().
			Add(isw.NspinMinimum, 0).
			Add(isw.NspinMaximum, 100).
			Add(isw.NspinValue, 42).
			Add(isw.NspinIncrement, 5))
	spin.AddCallback(isw.NvalueChanged, func(w isw.Widget, cd unsafe.Pointer) {
		setStatus(fmt.Sprintf("SpinBox value: %d", int(uintptr(cd))))
	})

	isw.CreateManagedWidget("heading4", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "Text Input"))

	isw.CreateManagedWidget("textEdit", isw.TextClass, vbox,
		isw.NewArgList().
			AddString(isw.NeditType, "edit").
			AddString(isw.Nstring, "Type here...").
			Add(isw.Nwidth, 400).
			Add(isw.Nheight, 80).
			Add(isw.NscrollVertical, 2).
			Add("wrap", 2))
}

func createSelectionTab(tabs isw.Widget) {
	page := isw.CreateManagedWidget("selPage", isw.ViewportClass, tabs,
		isw.NewArgList().
			Add(isw.NallowVert, 1).
			Add("useRight", 1).
			AddString(isw.NtabLabel, "Selection"))

	vbox := isw.CreateManagedWidget("selVbox", isw.BoxClass, page,
		isw.NewArgList().Add(isw.Norientation, uintptr(isw.OrientVertical)))

	isw.CreateManagedWidget("listHeading", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "List Widget"))

	items := []string{
		"Alpine", "Arch", "Debian", "Fedora", "Gentoo",
		"NixOS", "openSUSE", "Slackware", "Ubuntu", "Void",
	}
	cItems := isw.CStringArray(items)

	list := isw.CreateManagedWidget("distroList", isw.ListClass, vbox,
		isw.NewArgList().
			Add(isw.Nlist, cItems).
			Add(isw.NnumberStrings, uintptr(len(items))).
			Add(isw.NdefaultColumns, 2).
			Add(isw.NforceColumns, 1).
			Add(isw.NverticalList, 1))

	list.AddCallback(isw.Ncallback, func(w isw.Widget, cd unsafe.Pointer) {
		ret := (*C.IswListReturnStruct)(cd)
		name := C.GoString(ret.string)
		setStatus(fmt.Sprintf("Selected: %s (index %d)", name, int(ret.list_index)))
	})

	isw.CreateManagedWidget("comboHeading", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "ComboBox"))

	comboItems := isw.CStringArray(items)
	isw.CreateManagedWidget("combo", isw.ComboBoxClass, vbox,
		isw.NewArgList().
			Add(isw.Nlist, comboItems).
			Add(isw.NnumberStrings, uintptr(len(items))))

	isw.CreateManagedWidget("lbHeading", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "ListBox"))

	lb := isw.CreateManagedWidget("listbox", isw.ListBoxClass, vbox,
		isw.NewArgList().
			Add(isw.Nwidth, 300).
			Add(isw.Nheight, 150).
			Add("selectionMode", 0))

	for _, lang := range []string{"C", "Go", "Rust", "Zig", "Hare"} {
		isw.CreateManagedWidget("row_"+lang, isw.ListBoxRowClass, lb,
			isw.NewArgList().AddString(isw.Nlabel, lang))
	}

	lb.AddCallback(isw.NselectCallback, func(w isw.Widget, cd unsafe.Pointer) {
		setStatus("ListBox selection changed")
	})
}

func createDrawingTab(tabs isw.Widget) {
	page := isw.CreateManagedWidget("drawPage", isw.ViewportClass, tabs,
		isw.NewArgList().
			Add(isw.NallowVert, 1).
			Add("useRight", 1).
			AddString(isw.NtabLabel, "Drawing"))

	vbox := isw.CreateManagedWidget("drawVbox", isw.BoxClass, page,
		isw.NewArgList().Add(isw.Norientation, uintptr(isw.OrientVertical)))

	isw.CreateManagedWidget("drawHeading", isw.LabelClass, vbox,
		isw.NewArgList().AddString(isw.Nlabel, "DrawingArea — ISWRender API from Go"))

	da := isw.CreateManagedWidget("canvas", isw.DrawingAreaClass, vbox,
		isw.NewArgList().
			Add(isw.Nwidth, 500).
			Add(isw.Nheight, 300).
			AddString(isw.Nbackground, "#1a1a2e"))

	da.AddCallback(isw.NexposeCallback, func(w isw.Widget, cd unsafe.Pointer) {
		renderCtx := C._drawing_cb_render(cd)
		if renderCtx == nil {
			return
		}
		rc := (*isw.RenderContext)(unsafe.Pointer(&renderCtx))
		drawScene(rc)
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
		isw.NewArgList().
			Add(isw.NallowVert, 1).
			Add("useRight", 1).
			AddString(isw.NtabLabel, "Form Layout"))

	form := isw.CreateManagedWidget("form", isw.FormClass, page,
		isw.NewArgList().Add(isw.NdefaultDistance, 8))

	nameLbl := isw.CreateManagedWidget("nameLbl", isw.LabelClass, form,
		isw.NewArgList().
			AddString(isw.Nlabel, "Name:").
			Add("left", 3).
			Add("top", 3))

	nameEntry := isw.CreateManagedWidget("nameEntry", isw.TextClass, form,
		isw.NewArgList().
			AddString(isw.NeditType, "edit").
			AddString(isw.Nstring, "").
			Add(isw.Nwidth, 250).
			AddWidget(isw.NfromHoriz, nameLbl).
			Add("left", 3).
			Add("top", 3))

	emailLbl := isw.CreateManagedWidget("emailLbl", isw.LabelClass, form,
		isw.NewArgList().
			AddString(isw.Nlabel, "Email:").
			AddWidget(isw.NfromVert, nameLbl).
			Add("left", 3).
			Add("top", 3))

	isw.CreateManagedWidget("emailEntry", isw.TextClass, form,
		isw.NewArgList().
			AddString(isw.NeditType, "edit").
			AddString(isw.Nstring, "").
			Add(isw.Nwidth, 250).
			AddWidget(isw.NfromHoriz, emailLbl).
			AddWidget(isw.NfromVert, nameEntry).
			Add("left", 3).
			Add("top", 3))

	msgLbl := isw.CreateManagedWidget("msgLbl", isw.LabelClass, form,
		isw.NewArgList().
			AddString(isw.Nlabel, "Message:").
			AddWidget(isw.NfromVert, emailLbl).
			Add("left", 3).
			Add("top", 3))

	isw.CreateManagedWidget("msgEntry", isw.TextClass, form,
		isw.NewArgList().
			AddString(isw.NeditType, "edit").
			AddString(isw.Nstring, "").
			Add(isw.Nwidth, 400).
			Add(isw.Nheight, 100).
			Add(isw.NscrollVertical, 2).
			Add("wrap", 2).
			AddWidget(isw.NfromVert, msgLbl).
			Add("left", 3).
			Add("top", 3))

	submit := isw.CreateManagedWidget("submit", isw.CommandClass, form,
		isw.NewArgList().
			AddString(isw.Nlabel, "Submit").
			Add(isw.NcornerRadius, 4).
			AddWidget(isw.NfromVert, msgLbl).
			Add(isw.NvertDistance, 120).
			Add("left", 3).
			Add("top", 3))

	submit.AddCallback(isw.Ncallback, func(w isw.Widget, cd unsafe.Pointer) {
		setStatus("Form submitted!")
	})
}
