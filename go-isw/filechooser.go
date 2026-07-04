package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/FileChooser.h>
*/
import "C"

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
