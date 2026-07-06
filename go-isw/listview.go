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
	Row      int
	Column   int
	Selected []int
}

// ParseListViewCallbackData converts C call_data from a ListView select
// callback to Go.
func ParseListViewCallbackData(callData CallData) *ListViewCallbackData {
	cd := (*C.IswListViewCallbackData)(callData.ptr)
	out := &ListViewCallbackData{
		Row:    int(cd.row),
		Column: int(cd.column),
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
	a := listViewAllocs[unsafe.Pointer(w.c)]
	if a == nil {
		a = &listViewAlloc{}
		listViewAllocs[unsafe.Pointer(w.c)] = a
		w.AddCallback(NdestroyCallback, func(dw Widget, _ CallData) {
			if dead := listViewAllocs[unsafe.Pointer(dw.c)]; dead != nil {
				dead.freeColumns()
				dead.freeData()
				delete(listViewAllocs, unsafe.Pointer(dw.c))
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

// ListViewSortDirection is a column sort direction.
type ListViewSortDirection int

const (
	ListViewSortNone       ListViewSortDirection = C.IswListViewSortNone
	ListViewSortAscending  ListViewSortDirection = C.IswListViewSortAscending
	ListViewSortDescending ListViewSortDirection = C.IswListViewSortDescending
)

// ListViewReorderCallbackData is the Go representation of ListView
// reorderCallback callback data (header click).
type ListViewReorderCallbackData struct {
	Column    int
	Direction ListViewSortDirection
}

// ParseListViewReorderCallbackData converts C call_data from a ListView
// reorderCallback to Go.
func ParseListViewReorderCallbackData(callData CallData) *ListViewReorderCallbackData {
	cd := (*C.IswListViewReorderCallbackData)(callData.ptr)
	return &ListViewReorderCallbackData{
		Column:    int(cd.column),
		Direction: ListViewSortDirection(cd.direction),
	}
}

// ListViewAddColumn appends a column and returns its index.
func ListViewAddColumn(w Widget, title string, width, minWidth int) int {
	cTitle := C.CString(title)
	defer C.free(unsafe.Pointer(cTitle))
	return int(C.IswListViewAddColumn(w.c, cTitle,
		C.Dimension(width), C.Dimension(minWidth)))
}

// ListViewGetSelected returns the first selected row index, or -1.
func ListViewGetSelected(w Widget) int {
	return int(C.IswListViewGetSelected(w.c))
}

// ListViewGetSelectedRows returns all selected row indices.
func ListViewGetSelectedRows(w Widget) []int {
	var arr *C.int
	n := int(C.IswListViewGetSelectedRows(w.c, &arr))
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

// ListViewSetSort sets the sort indicator on a column.
func ListViewSetSort(w Widget, column int, direction ListViewSortDirection) {
	C.IswListViewSetSort(w.c, C.int(column),
		C.IswListViewSortDirection(direction))
}

// ListViewHitTest returns the row index at widget coordinates, or -1.
func ListViewHitTest(w Widget, x, y int) int {
	return int(C.IswListViewHitTest(w.c, C.int(x), C.int(y)))
}

// ListViewSetDropHighlight highlights a row as a drop target (-1 clears).
func ListViewSetDropHighlight(w Widget, rowIndex int) {
	C.IswListViewSetDropHighlight(w.c, C.int(rowIndex))
}

// ListViewBandActive returns true while a rubber-band selection is active.
func ListViewBandActive(w Widget) bool {
	return C.IswListViewBandActive(w.c) != 0
}
