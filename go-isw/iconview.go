package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/IconView.h>
#include <stdlib.h>
*/
import "C"

import "unsafe"

// IconViewCallbackData is the Go representation of IconView
// selectCallback callback data.
type IconViewCallbackData struct {
	Index    int
	Label    string
	Selected []int
}

// ParseIconViewCallbackData converts C call_data from an IconView
// selectCallback to Go.
func ParseIconViewCallbackData(callData CallData) *IconViewCallbackData {
	cd := (*C.IswIconViewCallbackData)(callData.ptr)
	out := &IconViewCallbackData{
		Index: int(cd.index),
		Label: C.GoString(cd.label),
	}
	if cd.num_selected > 0 && cd.selected != nil {
		cArr := unsafe.Slice(cd.selected, int(cd.num_selected))
		out.Selected = make([]int, int(cd.num_selected))
		for i := range out.Selected {
			out.Selected[i] = int(cArr[i])
		}
	}
	return out
}

// The IconView widget borrows the label and icon-data arrays handed to
// it, so the bindings own the C copies: each setter frees the previous
// allocation and a destroy callback releases whatever is left.
type iconViewAlloc struct {
	labelBlock unsafe.Pointer
	labelStrs  []*C.char
	iconBlock  unsafe.Pointer
	iconStrs   []*C.char
}

var iconViewAllocs = make(map[unsafe.Pointer]*iconViewAlloc)

func iconViewAllocFor(w Widget) *iconViewAlloc {
	a := iconViewAllocs[unsafe.Pointer(w.c)]
	if a == nil {
		a = &iconViewAlloc{}
		iconViewAllocs[unsafe.Pointer(w.c)] = a
		w.AddCallback(NdestroyCallback, func(dw Widget, _ CallData) {
			if dead := iconViewAllocs[unsafe.Pointer(dw.c)]; dead != nil {
				dead.free()
				delete(iconViewAllocs, unsafe.Pointer(dw.c))
			}
		})
	}
	return a
}

func (a *iconViewAlloc) free() {
	for _, cs := range a.labelStrs {
		C.free(unsafe.Pointer(cs))
	}
	a.labelStrs = nil
	if a.labelBlock != nil {
		C.free(a.labelBlock)
		a.labelBlock = nil
	}
	for _, cs := range a.iconStrs {
		C.free(unsafe.Pointer(cs))
	}
	a.iconStrs = nil
	if a.iconBlock != nil {
		C.free(a.iconBlock)
		a.iconBlock = nil
	}
}

func (a *iconViewAlloc) allocStrings(items []string) (unsafe.Pointer, []*C.char) {
	block := C.malloc(C.size_t(len(items)) * C.size_t(unsafe.Sizeof((*C.char)(nil))))
	ptrs := unsafe.Slice((**C.char)(block), len(items))
	strs := make([]*C.char, len(items))
	for i, s := range items {
		strs[i] = C.CString(s)
		ptrs[i] = strs[i]
	}
	return block, strs
}

// IconViewSetItems replaces the icon view's items. iconData holds each
// item's image source (file path or inline SVG); nil for no icons.
func IconViewSetItems(w Widget, labels []string, iconData []string) {
	a := iconViewAllocFor(w)
	a.free()

	n := len(labels)
	var cLabels, cIcons *C.String
	if n > 0 {
		a.labelBlock, a.labelStrs = a.allocStrings(labels)
		cLabels = (*C.String)(a.labelBlock)
	}
	if len(iconData) > 0 {
		a.iconBlock, a.iconStrs = a.allocStrings(iconData)
		cIcons = (*C.String)(a.iconBlock)
	}
	C.IswIconViewSetItems(w.c, cLabels, cIcons, C.int(n))
}

// IconViewGetSelected returns the first selected item index, or -1.
func IconViewGetSelected(w Widget) int {
	return int(C.IswIconViewGetSelected(w.c))
}

// IconViewGetSelectedItems returns all selected item indices.
func IconViewGetSelectedItems(w Widget) []int {
	var arr *C.int
	n := int(C.IswIconViewGetSelectedItems(w.c, &arr))
	if n == 0 || arr == nil {
		return nil
	}
	cArr := unsafe.Slice(arr, n)
	out := make([]int, n)
	for i := range out {
		out[i] = int(cArr[i])
	}
	C.IswFree(unsafe.Pointer(arr))
	return out
}

// IconViewHitTest returns the item index at widget coordinates, or -1.
func IconViewHitTest(w Widget, x, y int) int {
	return int(C.IswIconViewHitTest(w.c, C.int(x), C.int(y)))
}

// IconViewSetDropHighlight highlights an item as a drop target (-1
// clears).
func IconViewSetDropHighlight(w Widget, itemIndex int) {
	C.IswIconViewSetDropHighlight(w.c, C.int(itemIndex))
}

// IconViewBandActive returns true while a rubber-band selection is
// active.
func IconViewBandActive(w Widget) bool {
	return C.IswIconViewBandActive(w.c) != 0
}

// IconViewGetItemRaster returns a copy of an item's icon RGBA raster and
// its dimensions, or nil if the item has no icon.
func IconViewGetItemRaster(w Widget, index int) ([]byte, uint, uint) {
	var cw, ch C.uint
	ptr := C.IswIconViewGetItemRaster(w.c, C.int(index), &cw, &ch)
	if ptr == nil {
		return nil, 0, 0
	}
	n := uint(cw) * uint(ch) * 4
	return C.GoBytes(unsafe.Pointer(ptr), C.int(n)), uint(cw), uint(ch)
}
