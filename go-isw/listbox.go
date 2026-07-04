package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/ListBox.h>
*/
import "C"

import "unsafe"

// ListBoxCallbackData is the Go representation of ListBox select and
// activate callback data.
type ListBoxCallbackData struct {
	Child Widget
}

// ParseListBoxCallbackData converts C call_data from a ListBox select or
// activate callback to Go.
func ParseListBoxCallbackData(callData unsafe.Pointer) *ListBoxCallbackData {
	cd := (*C.IswListBoxCallbackData)(callData)
	return &ListBoxCallbackData{Child: Widget{cd.child}}
}

// ListBoxPivotCallbackData is the Go representation of ListBox pivot
// callback data.
type ListBoxPivotCallbackData struct {
	Child Widget
	Open  bool
}

// ParseListBoxPivotCallbackData converts C call_data from a ListBox pivot
// callback to Go.
func ParseListBoxPivotCallbackData(callData unsafe.Pointer) *ListBoxPivotCallbackData {
	cd := (*C.IswListBoxPivotCallbackData)(callData)
	return &ListBoxPivotCallbackData{Child: Widget{cd.child}, Open: cd.open != 0}
}
