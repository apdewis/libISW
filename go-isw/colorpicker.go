package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/ColorPicker.h>
*/
import "C"

// ColorPickerCallbackData is the Go representation of ColorPicker
// colorChanged callback data.
type ColorPickerCallbackData struct {
	Red   int
	Green int
	Blue  int
}

// ParseColorPickerCallbackData converts C call_data from a colorChanged
// callback to Go.
func ParseColorPickerCallbackData(callData CallData) *ColorPickerCallbackData {
	cd := (*C.IswColorPickerCallbackData)(callData.ptr)
	return &ColorPickerCallbackData{
		Red:   int(cd.red),
		Green: int(cd.green),
		Blue:  int(cd.blue),
	}
}

// ColorPickerGetColor returns the current red, green and blue components
// (each 0-255).
func ColorPickerGetColor(w Widget) (red, green, blue int) {
	var r, g, b C.int
	C.IswColorPickerGetColor(w.c, &r, &g, &b)
	return int(r), int(g), int(b)
}

// ColorPickerSetColor sets the red, green and blue components (0-255).
func ColorPickerSetColor(w Widget, red, green, blue int) {
	C.IswColorPickerSetColor(w.c, C.int(red), C.int(green), C.int(blue))
}
