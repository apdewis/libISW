package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/FileChooser.h>
#include <stdlib.h>
*/
import "C"

import "unsafe"

// FileChooserMode selects open or save behaviour.
type FileChooserMode int

const (
	FileOpen FileChooserMode = C.IswFileOpen
	FileSave FileChooserMode = C.IswFileSave
)

// FileChooserFilter describes one filter entry shown in the combo box.
type FileChooserFilter struct {
	Label   string
	Pattern string
}

// FileChooserCallbackData is the Go representation of file chooser
// callback data.
type FileChooserCallbackData struct {
	Path string
}

// ParseFileChooserCallbackData converts C call_data from a fileSelected
// callback to Go.
func ParseFileChooserCallbackData(callData CallData) *FileChooserCallbackData {
	cd := (*C.IswFileChooserCallbackData)(callData.ptr)
	return &FileChooserCallbackData{Path: C.GoString(cd.path)}
}

// FileChooserGetPath returns the currently selected path.
func FileChooserGetPath(w Widget) string {
	return C.GoString(C.IswFileChooserGetPath(w.c))
}

// addFileFilters allocates a C IswFileFilter array, fills it from the
// Go slice, and appends both the fileFilters pointer and the matching
// numFileFilters count. The C array and its strings are deliberately
// retained for the widget's lifetime (the widget does not copy them),
// matching the lifetime model of AddStringList.
func (al *ArgList) addFileFilters(filters []FileChooserFilter) *ArgList {
	n := len(filters)
	if n == 0 {
		return al.Add("fileFilters", 0).Add("numFileFilters", 0)
	}
	block := C.malloc(C.size_t(n) * C.size_t(unsafe.Sizeof(C.IswFileFilter{})))
	arr := unsafe.Slice((*C.IswFileFilter)(block), n)
	for i, f := range filters {
		arr[i].label = C.CString(f.Label)
		arr[i].pattern = C.CString(f.Pattern)
	}
	al.addRaw("fileFilters", uintptr(block))
	return al.Add("numFileFilters", n)
}
