package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/ListView.h>
#include <stdlib.h>
*/
import "C"

import "unsafe"

// ListViewColumn describes one ListView column.
type ListViewColumn struct {
	Title    string
	Width    int // pixels, 0 = default
	MinWidth int // minimum width for resize, 0 = default
}

// ListViewCallbackData is the Go representation of ListView select
// callback data.
type ListViewCallbackData struct {
	Row    int
	Column int
}

// ParseListViewCallbackData converts C call_data from a ListView select
// callback to Go.
func ParseListViewCallbackData(callData unsafe.Pointer) *ListViewCallbackData {
	cd := (*C.IswListViewCallbackData)(callData)
	return &ListViewCallbackData{Row: int(cd.row), Column: int(cd.column)}
}

// The ListView widget borrows the column and data pointers handed to it,
// so the bindings own the C copies: each setter frees the previous
// allocation and a destroy callback releases whatever is left.
type listViewAlloc struct {
	colBlock  unsafe.Pointer
	titleStrs []*C.char
	dataBlock unsafe.Pointer
	dataStrs  []*C.char
}

var listViewAllocs = make(map[unsafe.Pointer]*listViewAlloc)

func listViewAllocFor(w Widget) *listViewAlloc {
	a := listViewAllocs[w.Raw()]
	if a == nil {
		a = &listViewAlloc{}
		listViewAllocs[w.Raw()] = a
		w.AddCallback(NdestroyCallback, func(dw Widget, _ unsafe.Pointer) {
			if dead := listViewAllocs[dw.Raw()]; dead != nil {
				dead.freeColumns()
				dead.freeData()
				delete(listViewAllocs, dw.Raw())
			}
		})
	}
	return a
}

func (a *listViewAlloc) freeColumns() {
	for _, cs := range a.titleStrs {
		C.free(unsafe.Pointer(cs))
	}
	a.titleStrs = nil
	if a.colBlock != nil {
		C.free(a.colBlock)
		a.colBlock = nil
	}
}

func (a *listViewAlloc) freeData() {
	for _, cs := range a.dataStrs {
		C.free(unsafe.Pointer(cs))
	}
	a.dataStrs = nil
	if a.dataBlock != nil {
		C.free(a.dataBlock)
		a.dataBlock = nil
	}
}

// ListViewSetColumns sets a ListView's columns.
func ListViewSetColumns(w Widget, cols []ListViewColumn) {
	a := listViewAllocFor(w)
	a.freeColumns()

	n := len(cols)
	if n == 0 {
		C.IswListViewSetColumns(w.c, nil, 0)
		return
	}

	a.colBlock = C.malloc(C.size_t(n) * C.size_t(unsafe.Sizeof(C.IswListViewColumn{})))
	arr := unsafe.Slice((*C.IswListViewColumn)(a.colBlock), n)
	a.titleStrs = make([]*C.char, n)
	for i, col := range cols {
		a.titleStrs[i] = C.CString(col.Title)
		arr[i].title = (C.String)(a.titleStrs[i])
		arr[i].width = C.Dimension(col.Width)
		arr[i].min_width = C.Dimension(col.MinWidth)
	}
	C.IswListViewSetColumns(w.c, (*C.IswListViewColumn)(a.colBlock), C.int(n))
}

// ListViewSetData replaces a ListView's cell data. rows must be
// rectangular; the first row sets the column count and short rows are
// padded with empty cells.
func ListViewSetData(w Widget, rows [][]string) {
	a := listViewAllocFor(w)
	a.freeData()

	nrows := len(rows)
	if nrows == 0 || len(rows[0]) == 0 {
		C.IswListViewSetData(w.c, nil, 0, 0)
		return
	}

	ncols := len(rows[0])
	total := nrows * ncols
	a.dataBlock = C.malloc(C.size_t(total) * C.size_t(unsafe.Sizeof((*C.char)(nil))))
	ptrs := unsafe.Slice((**C.char)(a.dataBlock), total)
	a.dataStrs = make([]*C.char, total)
	for r, row := range rows {
		for col := 0; col < ncols; col++ {
			cell := ""
			if col < len(row) {
				cell = row[col]
			}
			cs := C.CString(cell)
			a.dataStrs[r*ncols+col] = cs
			ptrs[r*ncols+col] = cs
		}
	}
	C.IswListViewSetData(w.c, (*C.String)(a.dataBlock), C.int(nrows), C.int(ncols))
}
