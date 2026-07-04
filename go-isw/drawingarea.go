package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/ISWRender.h>
#include <ISW/DrawingArea.h>

static ISWRenderContext* _isw_dcd_render(void *cd) {
	return ((ISWDrawingCallbackData*)cd)->render_ctx;
}

static IswEvent* _isw_dcd_event(void *cd) {
	return ((ISWDrawingCallbackData*)cd)->event;
}

static IswWindow _isw_dcd_window(void *cd) {
	return ((ISWDrawingCallbackData*)cd)->window;
}
*/
import "C"

import "unsafe"

// Window is an opaque handle to a widget's platform window.
type Window struct {
	c C.IswWindow
}

// DrawingCallbackData is the decoded call data for DrawingArea callbacks.
//
// For expose callbacks: Render is valid and bracketed by Begin/End.
// For resize callbacks: Render and Event are nil.
// For input callbacks:  Render is available but not in Begin/End state.
type DrawingCallbackData struct {
	Render *RenderContext
	Event  Event
	Window Window
}

// ParseDrawingCallbackData decodes DrawingArea callback data.
func ParseDrawingCallbackData(cd CallData) *DrawingCallbackData {
	if cd.ptr == nil {
		return nil
	}
	d := &DrawingCallbackData{Window: Window{C._isw_dcd_window(cd.ptr)}}
	if rc := C._isw_dcd_render(cd.ptr); rc != nil {
		d.Render = &RenderContext{rc}
	}
	d.Event = flattenEvent(unsafe.Pointer(C._isw_dcd_event(cd.ptr)))
	return d
}
