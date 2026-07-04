package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/SpinBox.h>
*/
import "C"

// SpinBoxSetValue sets the spin box's value.
func SpinBoxSetValue(w Widget, value int) {
	C.IswSpinBoxSetValue(w.c, C.int(value))
}

// SpinBoxGetValue returns the spin box's value.
func SpinBoxGetValue(w Widget) int {
	return int(C.IswSpinBoxGetValue(w.c))
}
