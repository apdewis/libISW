package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Slider.h>
*/
import "C"

// SliderValuePosition controls where the value label is drawn relative
// to the slider.
type SliderValuePosition int

const (
	SliderValueTop    SliderValuePosition = C.IswSliderValueTop
	SliderValueBottom SliderValuePosition = C.IswSliderValueBottom
	SliderValueLeft   SliderValuePosition = C.IswSliderValueLeft
	SliderValueRight  SliderValuePosition = C.IswSliderValueRight
)

// SliderCallbackData is the Go representation of Slider valueChanged
// callback data.
type SliderCallbackData struct {
	Value int
}

// ParseSliderCallbackData converts C call_data from a Slider valueChanged
// callback to Go.
func ParseSliderCallbackData(callData CallData) *SliderCallbackData {
	cd := (*C.IswSliderCallbackData)(callData.ptr)
	return &SliderCallbackData{Value: int(cd.value)}
}

// SliderSetValue sets the slider's value.
func SliderSetValue(w Widget, value int) {
	C.IswSliderSetValue(w.c, C.int(value))
}

// SliderGetValue returns the slider's value.
func SliderGetValue(w Widget) int {
	return int(C.IswSliderGetValue(w.c))
}
