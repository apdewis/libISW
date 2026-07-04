package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/SimpleMenu.h>
*/
import "C"

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
