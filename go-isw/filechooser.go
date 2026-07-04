package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/FileChooser.h>
*/
import "C"

import "unsafe"

// FileChooserCallbackData is the Go representation of file chooser
// callback data.
type FileChooserCallbackData struct {
	Path string
}

// ParseFileChooserCallbackData converts C call_data from a fileSelected
// callback to Go.
func ParseFileChooserCallbackData(callData unsafe.Pointer) *FileChooserCallbackData {
	cd := (*C.IswFileChooserCallbackData)(callData)
	return &FileChooserCallbackData{Path: C.GoString(cd.path)}
}
