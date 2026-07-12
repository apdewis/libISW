package isw

// Event is implemented by all event types.
type Event interface {
	EventKind() EventKind
	IsSynthetic() bool
}

// EventKind identifies the type of event.
type EventKind int

const (
	NoEvent        EventKind = 0
	KeyDown        EventKind = 1
	KeyUp          EventKind = 2
	ButtonDown     EventKind = 3
	ButtonUp       EventKind = 4
	Motion         EventKind = 5
	Enter          EventKind = 6
	Leave          EventKind = 7
	FocusIn        EventKind = 8
	FocusOut       EventKind = 9
	Redraw         EventKind = 10
	Geometry       EventKind = 11
	Reparent       EventKind = 12
	MapEv          EventKind = 13
	UnmapEv        EventKind = 14
	Visibility     EventKind = 15
	DestroyEv      EventKind = 16
	MappingChanged EventKind = 17
	Protocol       EventKind = 18
	WindowClose    EventKind = 19
)

// NotifyMode distinguishes crossing/focus transition causes.
type NotifyMode int

const (
	NotifyNormal NotifyMode = 0
	NotifyGrab   NotifyMode = 1
	NotifyUngrab NotifyMode = 2
)

// FocusSource indicates whether focus moved by keyboard or pointer.
type FocusSource int

const (
	FocusByKeyboard FocusSource = 0
	FocusByPointer  FocusSource = 1
)

// NotifyDetail indicates the relationship of the event to the target.
type NotifyDetail int

const (
	NotifyDetailNone           NotifyDetail = 0
	NotifyAncestor             NotifyDetail = 1
	NotifyVirtual              NotifyDetail = 2
	NotifyInferior             NotifyDetail = 3
	NotifyNonlinear            NotifyDetail = 4
	NotifyNonlinearVirtual     NotifyDetail = 5
	NotifyPointer              NotifyDetail = 6
	NotifyPointerRoot          NotifyDetail = 7
)

// ModMask flags.
const (
	ModShift   uint16 = 1 << 0
	ModLock    uint16 = 1 << 1
	ModControl uint16 = 1 << 2
	ModMod1    uint16 = 1 << 3
	ModMod2    uint16 = 1 << 4
	ModMod3    uint16 = 1 << 5
	ModMod4    uint16 = 1 << 6
	ModMod5    uint16 = 1 << 7
	ModButton1 uint16 = 1 << 8
	ModButton2 uint16 = 1 << 9
	ModButton3 uint16 = 1 << 10
	ModButton4 uint16 = 1 << 11
	ModButton5 uint16 = 1 << 12
	ModAlt     uint16 = ModMod1
	ModMeta    uint16 = ModMod2
	ModSuper   uint16 = ModMod4
	ModHyper   uint16 = ModMod3
)

// Button identities.
const (
	ButtonNone       uint8 = 0
	ButtonLeft       uint8 = 1
	ButtonMiddle     uint8 = 2
	ButtonRight      uint8 = 3
	ButtonWheelUp    uint8 = 4
	ButtonWheelDown  uint8 = 5
	ButtonWheelLeft  uint8 = 6
	ButtonWheelRight uint8 = 7
)

// Key identities for non-printable keys (above Unicode range).
const (
	KeyNone      uint32 = 0
	KeyBackspace uint32 = 0x110000
	KeyTab       uint32 = 0x110001
	KeyReturn    uint32 = 0x110002
	KeyEscape    uint32 = 0x110003
	KeyDelete    uint32 = 0x110004
	KeyHome      uint32 = 0x110005
	KeyEnd       uint32 = 0x110006
	KeyArrowLeft uint32 = 0x110007
	KeyArrowRight uint32 = 0x110008
	KeyArrowUp   uint32 = 0x110009
	KeyArrowDown uint32 = 0x11000A
	KeyPageUp    uint32 = 0x11000B
	KeyPageDown  uint32 = 0x11000C
	KeyInsert    uint32 = 0x11000D
	KeyF1        uint32 = 0x11000E
	KeyF2        uint32 = 0x11000F
	KeyF3        uint32 = 0x110010
	KeyF4        uint32 = 0x110011
	KeyF5        uint32 = 0x110012
	KeyF6        uint32 = 0x110013
	KeyF7        uint32 = 0x110014
	KeyF8        uint32 = 0x110015
	KeyF9        uint32 = 0x110016
	KeyF10       uint32 = 0x110017
	KeyF11       uint32 = 0x110018
	KeyF12       uint32 = 0x110019
)

// EventBase is the common header for all events.
type EventBase struct {
	Kind      EventKind
	Synthetic bool
	Time      uint32
}

func (e *EventBase) EventKind() EventKind { return e.Kind }
func (e *EventBase) IsSynthetic() bool    { return e.Synthetic }

// AnyEvent represents events with no specific payload.
type AnyEvent struct{ EventBase }

// KeyEvent represents a key press or release.
type KeyEvent struct {
	EventBase
	Key       uint32
	Unicode   uint32
	Text      string
	Modifiers uint16
	X, Y      int32
	RootX     int16
	RootY     int16
	ShellX    int16
	ShellY    int16
}

// ButtonEvent represents a button press or release.
type ButtonEvent struct {
	EventBase
	Button    uint8
	Modifiers uint16
	X, Y      int32
	RootX     int16
	RootY     int16
	ShellX    int16
	ShellY    int16
}

// MotionEvent represents pointer motion.
type MotionEvent struct {
	EventBase
	Modifiers uint16
	X, Y      int32
	RootX     int16
	RootY     int16
	ShellX    int16
	ShellY    int16
}

// CrossingEvent represents pointer enter/leave.
type CrossingEvent struct {
	EventBase
	Mode       NotifyMode
	Detail     NotifyDetail
	Modifiers  uint16
	X, Y       int32
	RootX      int16
	RootY      int16
	ShellX     int16
	ShellY     int16
	SameScreen bool
}

// FocusEvent represents focus gain/loss.
type FocusEvent struct {
	EventBase
	Mode   NotifyMode
	Detail NotifyDetail
	Source FocusSource
}

// RedrawEvent represents an expose/damage event.
type RedrawEvent struct {
	EventBase
	X, Y          int16
	Width, Height uint16
	Count         uint16
}

// GeometryEvent represents a configure event.
type GeometryEvent struct {
	EventBase
	X, Y          int16
	Width, Height uint16
	BorderWidth   uint16
}

// ReparentEvent represents a reparent event.
type ReparentEvent struct {
	EventBase
	X, Y   int16
	ToRoot bool
}

// StructureEvent represents map/unmap/destroy/visibility events.
type StructureEvent struct {
	EventBase
	Visibility uint8
}

// ProtocolEvent represents a client message or window-close event.
type ProtocolEvent struct {
	EventBase
	MessageType uint32
	Format      uint8
	Data        [5]uint32
}

// Event mask constants for AddEventHandler.
const (
	NoEventMask              uint = 0
	KeyPressMask             uint = 1 << 0
	KeyReleaseMask           uint = 1 << 1
	ButtonPressMask          uint = 1 << 2
	ButtonReleaseMask        uint = 1 << 3
	EnterWindowMask          uint = 1 << 4
	LeaveWindowMask          uint = 1 << 5
	PointerMotionMask        uint = 1 << 6
	PointerMotionHintMask    uint = 1 << 7
	ExposureMask             uint = 1 << 15
	VisibilityChangeMask     uint = 1 << 16
	StructureNotifyMask      uint = 1 << 17
	FocusChangeMask          uint = 1 << 21
)
