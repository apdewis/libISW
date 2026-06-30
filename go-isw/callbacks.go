package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/IswEvent.h>
#include "trampolines.h"
#include "wrappers.h"

typedef struct {
	int kind;
	int synthetic;
	uintptr_t target;
	uint32_t time;
	uint32_t key;
	uint32_t unicode;
	char text[8];
	uint16_t modifiers;
	int32_t x, y;
	int16_t root_x, root_y;
	uint8_t button;
	int notify_mode;
	int notify_detail;
	int focus_source;
	uint8_t same_screen;
	uint16_t redraw_width, redraw_height;
	uint16_t redraw_count;
	uint16_t geom_width, geom_height;
	uint16_t border_width;
	uint8_t to_root;
	uint8_t visibility;
	uint32_t protocol_type;
	uint8_t protocol_format;
	uint32_t protocol_data[5];
} IswEventFlat;

extern void isw_event_flatten(const IswEvent *e, IswEventFlat *out);

*/
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

// CallbackFunc is the Go type for widget callbacks.
type CallbackFunc func(w Widget, callData unsafe.Pointer)

// EventHandlerFunc is the Go type for event handlers.
type EventHandlerFunc func(w Widget, event Event, continueDispatch *bool)

// TimerFunc is called when a timer fires.
type TimerFunc func()

// InputFunc is called when a file descriptor is ready.
type InputFunc func(fd int)

// SignalFunc is called when a signal fires.
type SignalFunc func()

// WorkProcFunc is a background work procedure. Return true to remove.
type WorkProcFunc func() bool

// ActionFunc is called for translation table actions.
type ActionFunc func(w Widget, event Event, params []string)

type callbackHandle uintptr

var (
	handleCounter atomic.Uint64

	cbMu        sync.RWMutex
	cbRegistry  = make(map[callbackHandle]CallbackFunc)
	ehRegistry  = make(map[callbackHandle]EventHandlerFunc)
	tmrRegistry = make(map[callbackHandle]TimerFunc)
	inpRegistry = make(map[callbackHandle]InputFunc)
	sigRegistry = make(map[callbackHandle]SignalFunc)
	wpRegistry  = make(map[callbackHandle]WorkProcFunc)

	actMu       sync.RWMutex
	actRegistry = make(map[string]ActionFunc)
)

func nextHandle() callbackHandle {
	return callbackHandle(handleCounter.Add(1))
}

func registerCallback(fn CallbackFunc) callbackHandle {
	h := nextHandle()
	cbMu.Lock()
	cbRegistry[h] = fn
	cbMu.Unlock()
	return h
}

func registerEventHandler(fn EventHandlerFunc) callbackHandle {
	h := nextHandle()
	cbMu.Lock()
	ehRegistry[h] = fn
	cbMu.Unlock()
	return h
}

func registerTimer(fn TimerFunc) callbackHandle {
	h := nextHandle()
	cbMu.Lock()
	tmrRegistry[h] = fn
	cbMu.Unlock()
	return h
}

func registerInput(fn InputFunc) callbackHandle {
	h := nextHandle()
	cbMu.Lock()
	inpRegistry[h] = fn
	cbMu.Unlock()
	return h
}

func registerSignal(fn SignalFunc) callbackHandle {
	h := nextHandle()
	cbMu.Lock()
	sigRegistry[h] = fn
	cbMu.Unlock()
	return h
}

func registerWorkProc(fn WorkProcFunc) callbackHandle {
	h := nextHandle()
	cbMu.Lock()
	wpRegistry[h] = fn
	cbMu.Unlock()
	return h
}

func flatToEvent(flat *C.IswEventFlat) Event {
	base := EventBase{
		Kind:      EventKind(flat.kind),
		Synthetic: flat.synthetic != 0,
		Time:      uint32(flat.time),
	}

	switch EventKind(flat.kind) {
	case KeyDown, KeyUp:
		return &KeyEvent{
			EventBase: base,
			Key:       uint32(flat.key),
			Unicode:   uint32(flat.unicode),
			Text:      C.GoString(&flat.text[0]),
			Modifiers: uint16(flat.modifiers),
			X:         int32(flat.x),
			Y:         int32(flat.y),
			RootX:     int16(flat.root_x),
			RootY:     int16(flat.root_y),
		}
	case ButtonDown, ButtonUp:
		return &ButtonEvent{
			EventBase: base,
			Button:    uint8(flat.button),
			Modifiers: uint16(flat.modifiers),
			X:         int32(flat.x),
			Y:         int32(flat.y),
			RootX:     int16(flat.root_x),
			RootY:     int16(flat.root_y),
		}
	case Motion:
		return &MotionEvent{
			EventBase: base,
			Modifiers: uint16(flat.modifiers),
			X:         int32(flat.x),
			Y:         int32(flat.y),
			RootX:     int16(flat.root_x),
			RootY:     int16(flat.root_y),
		}
	case Enter, Leave:
		return &CrossingEvent{
			EventBase:  base,
			Mode:       NotifyMode(flat.notify_mode),
			Detail:     NotifyDetail(flat.notify_detail),
			Modifiers:  uint16(flat.modifiers),
			X:          int32(flat.x),
			Y:          int32(flat.y),
			RootX:      int16(flat.root_x),
			RootY:      int16(flat.root_y),
			SameScreen: flat.same_screen != 0,
		}
	case FocusIn, FocusOut:
		return &FocusEvent{
			EventBase: base,
			Mode:      NotifyMode(flat.notify_mode),
			Detail:    NotifyDetail(flat.notify_detail),
			Source:    FocusSource(flat.focus_source),
		}
	case Redraw:
		return &RedrawEvent{
			EventBase: base,
			X:         int16(flat.x),
			Y:         int16(flat.y),
			Width:     uint16(flat.redraw_width),
			Height:    uint16(flat.redraw_height),
			Count:     uint16(flat.redraw_count),
		}
	case Geometry:
		return &GeometryEvent{
			EventBase:   base,
			X:           int16(flat.x),
			Y:           int16(flat.y),
			Width:       uint16(flat.geom_width),
			Height:      uint16(flat.geom_height),
			BorderWidth: uint16(flat.border_width),
		}
	case Reparent:
		return &ReparentEvent{
			EventBase: base,
			X:         int16(flat.x),
			Y:         int16(flat.y),
			ToRoot:    flat.to_root != 0,
		}
	case MapEv, UnmapEv, DestroyEv, Visibility:
		return &StructureEvent{
			EventBase:  base,
			Visibility: uint8(flat.visibility),
		}
	case Protocol, WindowClose:
		data := [5]uint32{}
		for i := range data {
			data[i] = uint32(flat.protocol_data[i])
		}
		return &ProtocolEvent{
			EventBase:   base,
			MessageType: uint32(flat.protocol_type),
			Format:      uint8(flat.protocol_format),
			Data:        data,
		}
	default:
		return &AnyEvent{EventBase: base}
	}
}

//export goCallbackBridge
func goCallbackBridge(widget, closure, callData C.uintptr_t) {
	h := callbackHandle(closure)
	cbMu.RLock()
	fn, ok := cbRegistry[h]
	cbMu.RUnlock()
	if ok {
		fn(Widget{C._isw_handle_to_widget(widget)},
			C._isw_uintptr_to_voidptr(callData))
	}
}

//export goEventHandlerBridge
func goEventHandlerBridge(widget, closure C.uintptr_t, event *C.IswEvent, cont *C.Boolean) {
	h := callbackHandle(closure)
	cbMu.RLock()
	fn, ok := ehRegistry[h]
	cbMu.RUnlock()
	if ok {
		var flat C.IswEventFlat
		C.isw_event_flatten(event, &flat)
		ev := flatToEvent(&flat)
		c := *cont != 0
		fn(Widget{C._isw_handle_to_widget(widget)}, ev, &c)
		if c {
			*cont = 1
		} else {
			*cont = 0
		}
	}
}

//export goTimerBridge
func goTimerBridge(closure, id C.uintptr_t) {
	h := callbackHandle(closure)
	cbMu.Lock()
	fn, ok := tmrRegistry[h]
	delete(tmrRegistry, h)
	cbMu.Unlock()
	if ok {
		fn()
	}
}

//export goInputBridge
func goInputBridge(closure C.uintptr_t, source C.int, id C.uintptr_t) {
	h := callbackHandle(closure)
	cbMu.RLock()
	fn, ok := inpRegistry[h]
	cbMu.RUnlock()
	if ok {
		fn(int(source))
	}
}

//export goSignalBridge
func goSignalBridge(closure, id C.uintptr_t) {
	h := callbackHandle(closure)
	cbMu.RLock()
	fn, ok := sigRegistry[h]
	cbMu.RUnlock()
	if ok {
		fn()
	}
}

//export goWorkProcBridge
func goWorkProcBridge(closure C.uintptr_t) C.Boolean {
	h := callbackHandle(closure)
	cbMu.RLock()
	fn, ok := wpRegistry[h]
	cbMu.RUnlock()
	if !ok {
		return 1
	}
	done := fn()
	if done {
		cbMu.Lock()
		delete(wpRegistry, h)
		cbMu.Unlock()
		return 1
	}
	return 0
}

//export goActionBridge
func goActionBridge(widget C.uintptr_t, event *C.IswEvent, params *C.String, numParams C.Cardinal) {
	_ = params
	_ = numParams
}

// AddEventHandler registers an event handler on a widget.
func (w Widget) AddEventHandler(mask uint, nonMaskable bool, fn EventHandlerFunc) {
	h := registerEventHandler(fn)
	nm := C.Boolean(0)
	if nonMaskable {
		nm = 1
	}
	C.IswAddEventHandler(w.c, C.EventMask(mask), nm,
		C._isw_eh_trampoline(), handleToPtr(h))
}

// AddTimeout adds a one-shot timer.
func (ac AppContext) AddTimeout(ms uint, fn TimerFunc) {
	h := registerTimer(fn)
	C.IswAppAddTimeOut(ac.c, C.ulong(ms), C._isw_timer_trampoline(),
		handleToPtr(h))
}

// AddInput registers a file-descriptor callback.
func (ac AppContext) AddInput(fd int, mask uint, fn InputFunc) {
	h := registerInput(fn)
	C.IswAppAddInput(ac.c, C.int(fd), C._isw_handle_to_ptr(C.uintptr_t(mask)),
		C._isw_input_trampoline(), handleToPtr(h))
}

// AddWorkProc registers a background work procedure.
func (ac AppContext) AddWorkProc(fn WorkProcFunc) {
	h := registerWorkProc(fn)
	C.IswAppAddWorkProc(ac.c, C._isw_workproc_trampoline(),
		handleToPtr(h))
}
