package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Toggle.h>
#include "wrappers.h"
*/
import "C"

import "unsafe"

// ToggleGetCurrent returns the radio data of the currently set toggle in
// the radio group, or 0 if none is set.
func ToggleGetCurrent(radioGroup Widget) uintptr {
	return uintptr(unsafe.Pointer(C.IswToggleGetCurrent(radioGroup.c)))
}

// ToggleSetCurrent sets the toggle associated with the given radio data.
func ToggleSetCurrent(radioGroup Widget, radioData uintptr) {
	C.IswToggleSetCurrent(radioGroup.c,
		C._isw_handle_to_ptr(C.uintptr_t(radioData)))
}

// ToggleUnsetCurrent unsets all toggles in the radio group.
func ToggleUnsetCurrent(radioGroup Widget) {
	C.IswToggleUnsetCurrent(radioGroup.c)
}

// ToggleChangeRadioGroup moves the toggle into another radio group.
func ToggleChangeRadioGroup(w Widget, radioGroup Widget) {
	C.IswToggleChangeRadioGroup(w.c, radioGroup.c)
}
