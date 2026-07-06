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
	Child    Widget
	Index    int
	List     Widget
	Selected []Widget
}

// ParseListBoxCallbackData converts C call_data from a ListBox select or
// activate callback to Go.
func ParseListBoxCallbackData(callData CallData) *ListBoxCallbackData {
	cd := (*C.IswListBoxCallbackData)(callData.ptr)
	out := &ListBoxCallbackData{
		Child: Widget{cd.child},
		Index: int(cd.index),
		List:  Widget{cd.list},
	}
	if cd.num_selected > 0 && cd.selected != nil {
		cArr := unsafe.Slice(cd.selected, int(cd.num_selected))
		out.Selected = make([]Widget, int(cd.num_selected))
		for i := range out.Selected {
			out.Selected[i] = Widget{cArr[i]}
		}
	}
	return out
}

// ListBoxPivotCallbackData is the Go representation of ListBox pivot
// callback data.
type ListBoxPivotCallbackData struct {
	Child Widget
	Index int
	Open  bool
}

// ParseListBoxPivotCallbackData converts C call_data from a ListBox pivot
// callback to Go.
func ParseListBoxPivotCallbackData(callData CallData) *ListBoxPivotCallbackData {
	cd := (*C.IswListBoxPivotCallbackData)(callData.ptr)
	return &ListBoxPivotCallbackData{
		Child: Widget{cd.child},
		Index: int(cd.index),
		Open:  cd.open != 0,
	}
}

// ListBoxGetSelected returns the selected row child, or NilWidget if no
// row is selected.
func ListBoxGetSelected(w Widget) Widget {
	return Widget{C.IswListBoxGetSelected(w.c)}
}

// ListBoxGetSelectedChildren returns all selected row children.
func ListBoxGetSelectedChildren(w Widget) []Widget {
	var arr *C.Widget
	n := int(C.IswListBoxGetSelectedChildren(w.c, &arr))
	if n == 0 || arr == nil {
		return nil
	}
	cArr := unsafe.Slice(arr, n)
	out := make([]Widget, n)
	for i := range out {
		out[i] = Widget{cArr[i]}
	}
	C.IswFree(unsafe.Pointer(arr))
	return out
}

// ListBoxSelectChild selects the given row child.
func ListBoxSelectChild(w Widget, child Widget) {
	C.IswListBoxSelectChild(w.c, child.c)
}

// ListBoxClearSelection clears the selection.
func ListBoxClearSelection(w Widget) {
	C.IswListBoxClearSelection(w.c)
}
