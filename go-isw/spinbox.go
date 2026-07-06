package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/SpinBox.h>
*/
import "C"

// SpinBoxCallbackData is the Go representation of SpinBox valueChanged
// callback data.
type SpinBoxCallbackData struct {
	Value int
}

// ParseSpinBoxCallbackData converts C call_data from a SpinBox
// valueChanged callback to Go.
func ParseSpinBoxCallbackData(callData CallData) *SpinBoxCallbackData {
	cd := (*C.IswSpinBoxCallbackData)(callData.ptr)
	return &SpinBoxCallbackData{Value: int(cd.value)}
}

// SpinBoxSetValue sets the spin box's value.
func SpinBoxSetValue(w Widget, value int) {
	C.IswSpinBoxSetValue(w.c, C.int(value))
}

// SpinBoxGetValue returns the spin box's value.
func SpinBoxGetValue(w Widget) int {
	return int(C.IswSpinBoxGetValue(w.c))
}
