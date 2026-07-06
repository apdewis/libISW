package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/FontChooser.h>
*/
import "C"

// FontChooserCallbackData is the Go representation of FontChooser
// fontChanged callback data.
type FontChooserCallbackData struct {
	Family string
	Size   int
	Weight int
	Slant  int
}

// ParseFontChooserCallbackData converts C call_data from a FontChooser
// fontChanged callback to Go.
func ParseFontChooserCallbackData(callData CallData) *FontChooserCallbackData {
	cd := (*C.IswFontChooserCallbackData)(callData.ptr)
	return &FontChooserCallbackData{
		Family: C.GoString(cd.family),
		Size:   int(cd.size),
		Weight: int(cd.weight),
		Slant:  int(cd.slant),
	}
}

// FontChooserGetFamily returns the selected font family.
func FontChooserGetFamily(w Widget) string {
	return C.GoString(C.IswFontChooserGetFamily(w.c))
}

// FontChooserGetSize returns the selected font size.
func FontChooserGetSize(w Widget) int {
	return int(C.IswFontChooserGetSize(w.c))
}

// FontChooserGetWeight returns the selected font weight.
func FontChooserGetWeight(w Widget) int {
	return int(C.IswFontChooserGetWeight(w.c))
}

// FontChooserGetSlant returns the selected font slant.
func FontChooserGetSlant(w Widget) int {
	return int(C.IswFontChooserGetSlant(w.c))
}
