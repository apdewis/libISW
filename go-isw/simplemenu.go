package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/SimpleMenu.h>
#include <stdlib.h>
#include "wrappers.h"
*/
import "C"

import "unsafe"

// SimpleMenuAddGlobalActions registers the XawPositionSimpleMenu and
// MenuPopup actions with the application context.
func SimpleMenuAddGlobalActions(ac AppContext) {
	C.IswSimpleMenuAddGlobalActions(ac.c)
}

// SimpleMenuGetActiveEntry returns the currently active menu entry, or
// NilWidget if none.
func SimpleMenuGetActiveEntry(w Widget) Widget {
	return Widget{C.IswSimpleMenuGetActiveEntry(w.c)}
}

// SimpleMenuClearActiveEntry unsets the currently active menu entry.
func SimpleMenuClearActiveEntry(w Widget) {
	C.IswSimpleMenuClearActiveEntry(w.c)
}

// SimpleMenuInstallAccelerators builds an accelerator table from the
// menu's SmeBSB children and installs it on the destination widget.
func SimpleMenuInstallAccelerators(destination Widget, menu Widget) {
	C.IswSimpleMenuInstallAccelerators(destination.c, menu.c)
}

// SimpleMenuShow positions and shows a windowless SimpleMenu within the
// window of its nearest windowed ancestor. x and y are relative to that
// ancestor window's surface.
func SimpleMenuShow(menu Widget, x, y int) {
	C.IswSimpleMenuShow(menu.c, C.int(x), C.int(y))
}

// SimpleMenuHide hides a shown SimpleMenu and any open submenu.
func SimpleMenuHide(menu Widget) {
	C.IswSimpleMenuHide(menu.c)
}

// CreateMenuPopupShell creates an override-redirect popup shell hosting a
// SimpleMenu, for applications that need the menu in a separate platform
// window rather than positioned within the trigger's toplevel. It returns
// the SimpleMenu widget.
func CreateMenuPopupShell(name string, parent Widget, args *ArgList) Widget {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	a, n := args.cArgPtr()
	return Widget{C._isw_create_menu_popup_shell(cname, parent.c, a, n)}
}
