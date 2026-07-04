package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Dialog.h>
#include <stdlib.h>
#include "trampolines.h"
#include "wrappers.h"
*/
import "C"

import "unsafe"

// DialogAddButton creates a Command button in the dialog and attaches
// the callback to it.
func DialogAddButton(dialog Widget, name string, fn CallbackFunc) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	h := registerCallback(fn)
	C.IswDialogAddButton(dialog.c, cName, C._isw_cb_trampoline(),
		handleToPtr(h))
}

// DialogGetValueString returns the dialog's value text.
func DialogGetValueString(w Widget) string {
	return C.GoString(C.IswDialogGetValueString(w.c))
}
