package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Form.h>
*/
import "C"

// FormDoLayout enables or disables the form's layout processing;
// re-enabling triggers a relayout.
func FormDoLayout(w Widget, doLayout bool) {
	d := C.Boolean(0)
	if doLayout {
		d = 1
	}
	C.IswFormDoLayout(w.c, d)
}
