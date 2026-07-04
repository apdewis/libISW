package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/FontChooser.h>
*/
import "C"

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
