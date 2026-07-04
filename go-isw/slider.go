package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Slider.h>
*/
import "C"

// SliderSetValue sets the slider's value.
func SliderSetValue(w Widget, value int) {
	C.IswSliderSetValue(w.c, C.int(value))
}

// SliderGetValue returns the slider's value.
func SliderGetValue(w Widget) int {
	return int(C.IswSliderGetValue(w.c))
}
