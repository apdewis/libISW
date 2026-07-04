package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/List.h>
#include <stdlib.h>
*/
import "C"

import "unsafe"

// ListCallbackData is the Go representation of List select callback data.
type ListCallbackData struct {
	String string
	Index  int
}

// ParseListCallbackData converts call data from a List callback to Go.
func ParseListCallbackData(callData CallData) *ListCallbackData {
	cd := (*C.IswListReturnStruct)(callData.ptr)
	return &ListCallbackData{
		String: C.GoString(cd.string),
		Index:  int(cd.list_index),
	}
}

// The List widget borrows the item array handed to IswListChange, so the
// bindings own the C copies: each change frees the previous allocation and
// a destroy callback releases whatever is left.
type listAlloc struct {
	block unsafe.Pointer
	strs  []*C.char
}

var listAllocs = make(map[unsafe.Pointer]*listAlloc)

func listAllocFor(w Widget) *listAlloc {
	a := listAllocs[unsafe.Pointer(w.c)]
	if a == nil {
		a = &listAlloc{}
		listAllocs[unsafe.Pointer(w.c)] = a
		w.AddCallback(NdestroyCallback, func(dw Widget, _ CallData) {
			if dead := listAllocs[unsafe.Pointer(dw.c)]; dead != nil {
				dead.free()
				delete(listAllocs, unsafe.Pointer(dw.c))
			}
		})
	}
	return a
}

func (a *listAlloc) free() {
	for _, cs := range a.strs {
		C.free(unsafe.Pointer(cs))
	}
	a.strs = nil
	if a.block != nil {
		C.free(a.block)
		a.block = nil
	}
}

// ListChange replaces the List widget's items. longest is the width in
// pixels of the longest item (0 = calculate), resize allows the widget to
// try to resize itself.
func ListChange(w Widget, items []string, longest int, resize bool) {
	a := listAllocFor(w)
	a.free()

	n := len(items)
	var arr *C.String
	if n > 0 {
		a.block = C.malloc(C.size_t(n) * C.size_t(unsafe.Sizeof((*C.char)(nil))))
		ptrs := unsafe.Slice((**C.char)(a.block), n)
		a.strs = make([]*C.char, n)
		for i, s := range items {
			a.strs[i] = C.CString(s)
			ptrs[i] = a.strs[i]
		}
		arr = (*C.String)(a.block)
	}
	r := C.Boolean(0)
	if resize {
		r = 1
	}
	C.IswListChange(w.c, arr, C.int(n), C.int(longest), r)
}

// ListHighlight highlights the item at the given index.
func ListHighlight(w Widget, item int) {
	C.IswListHighlight(w.c, C.int(item))
}

// ListUnhighlight unhighlights the currently highlighted item.
func ListUnhighlight(w Widget) {
	C.IswListUnhighlight(w.c)
}

// ListShowCurrent returns the currently highlighted item.
func ListShowCurrent(w Widget) *ListCallbackData {
	ret := C.IswListShowCurrent(w.c)
	if ret == nil {
		return nil
	}
	data := &ListCallbackData{
		String: C.GoString(ret.string),
		Index:  int(ret.list_index),
	}
	C.IswFree(unsafe.Pointer(ret))
	return data
}
