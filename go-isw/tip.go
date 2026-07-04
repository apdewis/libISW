package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Tip.h>
#include <stdlib.h>
*/
import "C"

import "unsafe"

// TipEnable enables a tooltip with the given label on the widget.
func TipEnable(w Widget, label string) {
	cLabel := C.CString(label)
	defer C.free(unsafe.Pointer(cLabel))
	C.IswTipEnable(w.c, cLabel)
}

// TipDisable disables the widget's tooltip.
func TipDisable(w Widget) {
	C.IswTipDisable(w.c)
}
