package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Shell.h>
#include <ISW/StringDefs.h>
#include <stdlib.h>
#include "trampolines.h"
#include "wrappers.h"
*/
import "C"

import "unsafe"

// CreateWidget creates a new widget.
func CreateWidget(name string, class WidgetClass, parent Widget, args *ArgList) Widget {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	a, n := args.cArgPtr()
	return Widget{C._isw_create_widget(cName, class.c, parent.c, a, n)}
}

// CreateManagedWidget creates and manages a new widget.
func CreateManagedWidget(name string, class WidgetClass, parent Widget, args *ArgList) Widget {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	a, n := args.cArgPtr()
	return Widget{C._isw_create_managed_widget(cName, class.c, parent.c, a, n)}
}

// CreatePopupShell creates a popup shell widget.
func CreatePopupShell(name string, class WidgetClass, parent Widget, args *ArgList) Widget {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	a, n := args.cArgPtr()
	return Widget{C._isw_create_popup_shell(cName, class.c, parent.c, a, n)}
}

// AppCreateShell creates a shell on the given display.
func AppCreateShell(appName, appClass string, class WidgetClass, dpy Display, args *ArgList) Widget {
	cName := C.CString(appName)
	cClass := C.CString(appClass)
	defer C.free(unsafe.Pointer(cName))
	defer C.free(unsafe.Pointer(cClass))
	a, n := args.cArgPtr()
	return Widget{C._isw_app_create_shell(cName, cClass, class.c, dpy.c, a, n)}
}

// Realize realizes the widget tree.
func (w Widget) Realize() { C.IswRealizeWidget(w.c) }

// Unrealize unrealizes the widget.
func (w Widget) Unrealize() { C.IswUnrealizeWidget(w.c) }

// Destroy destroys the widget.
func (w Widget) Destroy() { C.IswDestroyWidget(w.c) }

// Manage manages the widget.
func (w Widget) Manage() { C.IswManageChild(w.c) }

// Unmanage unmanages the widget.
func (w Widget) Unmanage() { C.IswUnmanageChild(w.c) }

// Map maps the widget.
func (w Widget) Map() { C.IswMapWidget(w.c) }

// Unmap unmaps the widget.
func (w Widget) Unmap() { C.IswUnmapWidget(w.c) }

// ManageChildren manages multiple children at once.
func ManageChildren(children []Widget) {
	if len(children) == 0 {
		return
	}
	C.IswManageChildren((*C.Widget)(unsafe.Pointer(&children[0])),
		C.Cardinal(len(children)))
}

// UnmanageChildren unmanages multiple children at once.
func UnmanageChildren(children []Widget) {
	if len(children) == 0 {
		return
	}
	C.IswUnmanageChildren((*C.Widget)(unsafe.Pointer(&children[0])),
		C.Cardinal(len(children)))
}

// SetValues sets resources on a widget.
func (w Widget) SetValues(args *ArgList) {
	a, n := args.cArgPtr()
	C._isw_set_values(w.c, a, n)
}

// GetValues reads resources from a widget.
func (w Widget) GetValues(args *ArgList) {
	a, n := args.cArgPtr()
	C._isw_get_values(w.c, a, n)
}

// Parent returns the widget's parent.
func (w Widget) Parent() Widget {
	return Widget{C.IswParent(w.c)}
}

// Class returns the widget's class.
func (w Widget) Class() WidgetClass {
	return WidgetClass{C.IswClass(w.c)}
}

// Popup pops up a shell widget.
func (w Widget) Popup(grab GrabKind) {
	C.IswPopup(w.c, C.IswGrabKind(grab))
}

// Popdown pops down a shell widget.
func (w Widget) Popdown() {
	C.IswPopdown(w.c)
}

// Sensitive returns the widget's sensitivity state.
func (w Widget) Sensitive() bool {
	return C.IswIsSensitive(w.c) != 0
}

// SetSensitive sets widget sensitivity.
func (w Widget) SetSensitive(sensitive bool) {
	args := NewArgList()
	if sensitive {
		args.Add(NsensitiveStr, uintptr(1))
	} else {
		args.Add(NsensitiveStr, 0)
	}
	w.SetValues(args)
}

// GrabKind enumerates grab modes for Popup.
type GrabKind int

const (
	GrabNone         GrabKind = C.IswGrabNone
	GrabNonexclusive GrabKind = C.IswGrabNonexclusive
	GrabExclusive    GrabKind = C.IswGrabExclusive
)

// GeometryResult enumerates geometry request results.
type GeometryResult int

const (
	GeometryYes    GeometryResult = C.IswGeometryYes
	GeometryNo     GeometryResult = C.IswGeometryNo
	GeometryAlmost GeometryResult = C.IswGeometryAlmost
	GeometryDone   GeometryResult = C.IswGeometryDone
)

// AddCallback adds a callback to a widget resource.
func (w Widget) AddCallback(name string, fn CallbackFunc) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	h := registerCallback(fn)
	C.IswAddCallback(w.c, cName, C._isw_cb_trampoline(),
		handleToPtr(h))
}

// RemoveAllCallbacks removes all callbacks from a named resource.
func (w Widget) RemoveAllCallbacks(name string) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	C.IswRemoveAllCallbacks(w.c, cName)
}
