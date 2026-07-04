package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/IswDragDrop.h>
#include <stdlib.h>
#include "trampolines.h"
#include "wrappers.h"
*/
import "C"

import "unsafe"

// DndAction is a drag-and-drop action bitmask.
type DndAction int

const (
	DndActionNone    DndAction = C.ISW_DND_ACTION_NONE
	DndActionCopy    DndAction = C.ISW_DND_ACTION_COPY
	DndActionMove    DndAction = C.ISW_DND_ACTION_MOVE
	DndActionLink    DndAction = C.ISW_DND_ACTION_LINK
	DndActionAsk     DndAction = C.ISW_DND_ACTION_ASK
	DndActionPrivate DndAction = C.ISW_DND_ACTION_PRIVATE
)

// DropCallbackData is the Go representation of drop callback data.
type DropCallbackData struct {
	URIs       []string
	X, Y       int
	Data       []byte
	DataType   string
	DataFormat int
	Action     DndAction
}

// DndEnable enables drag-and-drop on a shell.
func DndEnable(shell Widget) {
	C.IswDndEnable(shell.c)
}

// DndWidgetAcceptDrops registers a widget as a drop target.
func DndWidgetAcceptDrops(w Widget) {
	C.IswDndWidgetAcceptDrops(w.c)
}

// DndSetAcceptedTypes sets which MIME types a drop target accepts.
func DndSetAcceptedTypes(w Widget, types []string) {
	if len(types) == 0 {
		C.IswDndSetAcceptedTypes(w.c, nil, 0)
		return
	}
	cTypes := make([]*C.char, len(types))
	for i, t := range types {
		cTypes[i] = C.CString(t)
	}
	C.IswDndSetAcceptedTypes(w.c, (**C.char)(unsafe.Pointer(&cTypes[0])),
		C.int(len(types)))
	for _, ct := range cTypes {
		C.free(unsafe.Pointer(ct))
	}
}

// DndSetAcceptedActions sets which actions a drop target accepts.
func DndSetAcceptedActions(w Widget, actions DndAction) {
	C.IswDndSetAcceptedActions(w.c, C.IswDndAction(actions))
}

// DndSetDropCallback sets a direct drop callback.
func DndSetDropCallback(w Widget, fn CallbackFunc) {
	h := registerCallback(fn)
	C.IswDndSetDropCallback(w.c, C._isw_cb_trampoline(),
		handleToPtr(h))
}

// DndSetDragMotionCallback sets a direct drag-motion callback.
func DndSetDragMotionCallback(w Widget, fn CallbackFunc) {
	h := registerCallback(fn)
	C.IswDndSetDragMotionCallback(w.c, C._isw_cb_trampoline(),
		handleToPtr(h))
}

// DndSetDragLeaveCallback sets a direct drag-leave callback.
func DndSetDragLeaveCallback(w Widget, fn CallbackFunc) {
	h := registerCallback(fn)
	C.IswDndSetDragLeaveCallback(w.c, C._isw_cb_trampoline(),
		handleToPtr(h))
}

// DragConvertFunc provides drag data when the drop target requests it.
// format is the data element size in bits (8 for strings and URI lists).
type DragConvertFunc func(w Widget, targetType string) (data []byte, format int, ok bool)

// DragFinishedFunc is called when a drag completes.
type DragFinishedFunc func(w Widget, performedAction DndAction, accepted bool)

// DragSourceDesc configures a drag started with DndStartDrag.
type DragSourceDesc struct {
	Types    []string
	Actions  DndAction
	Convert  DragConvertFunc
	Finished DragFinishedFunc
}

type dragSource struct {
	convert  DragConvertFunc
	finished DragFinishedFunc
}

var dragSourceRegistry = make(map[callbackHandle]*dragSource)

// DndStartDrag initiates a drag from a widget. Call it from the event
// handler or action proc of the button press that triggers the drag; that
// event becomes the drag's trigger event. The drag runs asynchronously —
// this function returns immediately.
func DndStartDrag(source Widget, desc *DragSourceDesc) {
	if currentCEvent == nil {
		return
	}

	h := nextHandle()
	cbMu.Lock()
	dragSourceRegistry[h] = &dragSource{
		convert:  desc.Convert,
		finished: desc.Finished,
	}
	cbMu.Unlock()

	var cDesc C.IswDragSourceDesc
	var cTypes []*C.char
	if len(desc.Types) > 0 {
		cTypes = make([]*C.char, len(desc.Types))
		for i, t := range desc.Types {
			cTypes[i] = C.CString(t)
		}
		cDesc.types = (**C.char)(unsafe.Pointer(&cTypes[0]))
		cDesc.num_types = C.int(len(desc.Types))
	}
	cDesc.actions = C.IswDndAction(desc.Actions)
	cDesc.convert = C._isw_drag_convert_trampoline()
	cDesc.finished = C._isw_drag_finished_trampoline()
	cDesc.client_data = handleToPtr(h)

	// The library copies the descriptor and duplicates the type strings,
	// so the C allocations can be released as soon as the call returns.
	C.IswDndStartDrag(source.c, (*C.IswEvent)(currentCEvent), &cDesc)
	for _, ct := range cTypes {
		C.free(unsafe.Pointer(ct))
	}
}

//export goDragConvertBridge
func goDragConvertBridge(widget C.uintptr_t, targetType *C.char, dataReturn *C.IswPointer, lengthReturn *C.ulong, formatReturn *C.int, closure C.uintptr_t) C.Boolean {
	h := callbackHandle(closure)
	cbMu.RLock()
	src := dragSourceRegistry[h]
	cbMu.RUnlock()
	if src == nil || src.convert == nil {
		return 0
	}
	data, format, ok := src.convert(Widget{C._isw_handle_to_widget(widget)},
		C.GoString(targetType))
	if !ok {
		return 0
	}
	// The library takes ownership of the buffer and releases it with
	// IswFree (plain free), which matches C.CBytes' malloc.
	*dataReturn = C.IswPointer(C.CBytes(data))
	*lengthReturn = C.ulong(len(data))
	*formatReturn = C.int(format)
	return 1
}

//export goDragFinishedBridge
func goDragFinishedBridge(widget C.uintptr_t, action C.int, accepted C.Boolean, closure C.uintptr_t) {
	h := callbackHandle(closure)
	cbMu.Lock()
	src := dragSourceRegistry[h]
	delete(dragSourceRegistry, h)
	cbMu.Unlock()
	if src != nil && src.finished != nil {
		src.finished(Widget{C._isw_handle_to_widget(widget)},
			DndAction(action), accepted != 0)
	}
}

// DndIsDragging returns true if a drag operation is active.
func DndIsDragging(w Widget) bool {
	return C.IswDndIsDragging(w.c) != 0
}

// ParseDropCallbackData converts C call_data from a drop callback to Go.
func ParseDropCallbackData(callData CallData) *DropCallbackData {
	cd := (*C.IswDropCallbackData)(callData.ptr)
	result := &DropCallbackData{
		X:          int(cd.x),
		Y:          int(cd.y),
		DataType:   C.GoString(cd.data_type),
		DataFormat: int(cd.data_format),
		Action:     DndAction(cd.action),
	}

	if cd.data != nil && cd.data_length > 0 {
		result.Data = C.GoBytes(unsafe.Pointer(cd.data), C.int(cd.data_length))
	}

	if cd.num_uris > 0 && cd.uris != nil {
		n := int(cd.num_uris)
		cArr := unsafe.Slice((**C.char)(unsafe.Pointer(cd.uris)), n)
		result.URIs = make([]string, n)
		for i := 0; i < n; i++ {
			result.URIs[i] = C.GoString(cArr[i])
		}
	}

	return result
}
