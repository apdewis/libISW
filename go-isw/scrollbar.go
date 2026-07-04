package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Scrollbar.h>
*/
import "C"

// ScrollbarSetThumb sets the thumb position and size as fractions of the
// scrollbar length; pass a negative value to leave a field unchanged.
func ScrollbarSetThumb(w Widget, top, shown float64) {
	C.ISWScrollbarSetThumb(w.c, C.float(top), C.float(shown))
}
