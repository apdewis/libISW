package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/IswDragDrop.h>
#include <stdlib.h>
#include "trampolines.h"
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

// DndIsDragging returns true if a drag operation is active.
func DndIsDragging(w Widget) bool {
	return C.IswDndIsDragging(w.c) != 0
}

// ParseDropCallbackData converts C call_data from a drop callback to Go.
func ParseDropCallbackData(callData unsafe.Pointer) *DropCallbackData {
	cd := (*C.IswDropCallbackData)(callData)
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
