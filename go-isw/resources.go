//go:generate go run ./cmd/genargs

package isw

/*
#include <ISW/Intrinsic.h>
#include <stdlib.h>
#include "trampolines.h"
#include "wrappers.h"

static inline Arg* _isw_alloc_args(int n) {
    return (Arg*)calloc(n, sizeof(Arg));
}

static inline void _isw_set_arg(Arg *args, int idx,
                                const char *name, IswArgVal value) {
    args[idx].name = (String)name;
    args[idx].value = value;
}
*/
import "C"

import "unsafe"

// ArgList accumulates resource arguments for widget creation and SetValues.
type ArgList struct {
	cArr *C.Arg
	cap  int
	len  int
	strs []*C.char
}

// NewArgList creates an empty ArgList.
func NewArgList() *ArgList {
	const initCap = 16
	return &ArgList{
		cArr: C._isw_alloc_args(C.int(initCap)),
		cap:  initCap,
	}
}

func (al *ArgList) grow() {
	newCap := al.cap * 2
	newArr := C._isw_alloc_args(C.int(newCap))
	C.memcpy(unsafe.Pointer(newArr), unsafe.Pointer(al.cArr),
		C.size_t(al.len)*C.size_t(unsafe.Sizeof(C.Arg{})))
	C.free(unsafe.Pointer(al.cArr))
	al.cArr = newArr
	al.cap = newCap
}

// Add appends a resource by name with an integer/pointer-sized value.
func (al *ArgList) Add(name string, value uintptr) *ArgList {
	if al.len >= al.cap {
		al.grow()
	}
	cName := C.CString(name)
	al.strs = append(al.strs, cName)
	C._isw_set_arg(al.cArr, C.int(al.len), cName, C.IswArgVal(value))
	al.len++
	return al
}

// AddString appends a string-valued resource.
func (al *ArgList) AddString(name, value string) *ArgList {
	if al.len >= al.cap {
		al.grow()
	}
	cName := C.CString(name)
	cVal := C.CString(value)
	al.strs = append(al.strs, cName, cVal)
	C._isw_set_arg(al.cArr, C.int(al.len), cName,
		C.IswArgVal(uintptr(unsafe.Pointer(cVal))))
	al.len++
	return al
}

// AddWidget appends a widget-valued resource.
func (al *ArgList) AddWidget(name string, w Widget) *ArgList {
	return al.Add(name, uintptr(unsafe.Pointer(w.c)))
}

// AddStringList appends a string-array resource (e.g. IswNlist).
// The strings are copied into C memory and remain valid for the widget's
// lifetime. Pass the slice length separately via Add(NnumberStrings, …).
func (al *ArgList) AddStringList(name string, values []string) *ArgList {
	ptr := CStringArray(values)
	return al.Add(name, ptr)
}

// AddCallback appends a callback-list resource.
func (al *ArgList) AddCallback(name string, fn CallbackFunc) *ArgList {
	if al.len >= al.cap {
		al.grow()
	}
	h := registerCallback(fn)
	cName := C.CString(name)
	al.strs = append(al.strs, cName)

	cbRec := (*C.IswCallbackRec)(C.calloc(2, C.size_t(unsafe.Sizeof(C.IswCallbackRec{}))))
	cbRec.callback = C._isw_cb_trampoline()
	cbRec.closure = handleToPtr(h)

	C._isw_set_arg(al.cArr, C.int(al.len), cName,
		C.IswArgVal(uintptr(unsafe.Pointer(cbRec))))
	al.len++
	return al
}

// Free releases C memory allocated by the ArgList.
func (al *ArgList) Free() {
	for _, s := range al.strs {
		C.free(unsafe.Pointer(s))
	}
	al.strs = nil
	if al.cArr != nil {
		C.free(unsafe.Pointer(al.cArr))
		al.cArr = nil
	}
	al.len = 0
	al.cap = 0
}

func (al *ArgList) cArgPtr() (*C.Arg, C.Cardinal) {
	if al == nil || al.len == 0 {
		return nil, 0
	}
	return al.cArr, C.Cardinal(al.len)
}

// Pixel is an ARGB pixel value.
type Pixel = uint

// CStringArray allocates a NULL-terminated char** array in C memory.
// Each string and the array itself are C-allocated and must live as long as
// the widget that references them (typically the program lifetime for List
// resources). Call FreeStringArray to release.
func CStringArray(strs []string) uintptr {
	arr := C._isw_alloc_string_array(C.int(len(strs)))
	for i, s := range strs {
		C._isw_string_array_set(arr, C.int(i), C.CString(s))
	}
	return uintptr(C._isw_charpp_to_uintptr(arr))
}

// FreeStringArray frees a char** array created by CStringArray.
func FreeStringArray(ptr uintptr, count int) {
	if ptr == 0 {
		return
	}
	arr := C._isw_uintptr_to_charpp(C.uintptr_t(ptr))
	for i := 0; i < count; i++ {
		s := C._isw_string_array_get(arr, C.int(i))
		if s != nil {
			C.free(unsafe.Pointer(s))
		}
	}
	C.free(unsafe.Pointer(arr))
}

// PixelARGB constructs a Pixel from ARGB components.
func PixelARGB(a, r, g, b uint8) Pixel {
	return Pixel(uint(a)<<24 | uint(r)<<16 | uint(g)<<8 | uint(b))
}

// PixelAlpha extracts the alpha component.
func PixelAlpha(p Pixel) uint8 { return uint8((p >> 24) & 0xFF) }

// PixelRed extracts the red component.
func PixelRed(p Pixel) uint8 { return uint8((p >> 16) & 0xFF) }

// PixelGreen extracts the green component.
func PixelGreen(p Pixel) uint8 { return uint8((p >> 8) & 0xFF) }

// PixelBlue extracts the blue component.
func PixelBlue(p Pixel) uint8 { return uint8(p & 0xFF) }

// Resource name constants.
const (
	Naccelerators            = "accelerators"
	NallowHoriz              = "allowHoriz"
	NallowVert               = "allowVert"
	Nbackground              = "background"
	NbackgroundImage         = "backgroundImage"
	NborderColor             = "borderColor"
	NborderWidth             = "borderWidth"
	Ncallback                = "callback"
	Ncolormap                = "colormap"
	NdestroyCallback         = "destroyCallback"
	Nfont                    = "font"
	Nforeground              = "foreground"
	Nheight                  = "height"
	Nlabel                   = "label"
	NmappedWhenManaged       = "mappedWhenManaged"
	Norientation             = "orientation"
	Nresize                  = "resize"
	NsensitiveStr            = "sensitive"
	Ntranslations            = "translations"
	Nwidth                   = "width"
	Nx                       = "x"
	Ny                       = "y"

	NdefaultDistance = "defaultDistance"
	Ntop            = "top"
	Nbottom         = "bottom"
	Nleft           = "left"
	Nright          = "right"
	NfromHoriz      = "fromHoriz"
	NfromVert       = "fromVert"
	NhorizDistance   = "horizDistance"
	NvertDistance    = "vertDistance"

	Ntitle           = "title"
	NallowShellResize = "allowShellResize"
	Ngeometry        = "geometry"
	NiconName        = "iconName"
	NminWidth        = "minWidth"
	NminHeight       = "minHeight"
	NmaxWidth        = "maxWidth"
	NmaxHeight       = "maxHeight"

	NeditType        = "editType"
	Nstring          = "string"
	NscrollVertical  = "scrollVertical"
	NscrollHorizontal = "scrollHorizontal"

	Nlist           = "list"
	NnumberStrings  = "numberStrings"
	NdefaultColumns = "defaultColumns"
	NforceColumns   = "forceColumns"
	NverticalList   = "verticalList"

	NminimumThumb         = "minimumThumb"
	NtopOfThumb           = "topOfThumb"
	NscrollWheelIncrement = "scrollWheelIncrement"

	NminimumValue = "minimumValue"
	NmaximumValue = "maximumValue"
	NsliderValue  = "sliderValue"
	NshowValue    = "showValue"
	NvalueChanged = "valueChanged"

	Nstate      = "state"
	NradioGroup = "radioGroup"
	NradioData  = "radioData"

	NmenuName = "menuName"

	Nellipsize = "ellipsize"
	Nimage     = "image"

	NcornerRadius = "cornerRadius"

	NallowResize       = "allowResize"
	NshowGrip          = "showGrip"
	NpreferredPaneSize = "preferredPaneSize"

	NtabCallback = "tabCallback"
	NtopWidget   = "topWidget"
	NtabLabel    = "tabLabel"

	Nspace = "space"

	Ntip     = "tip"
	Ntimeout = "timeout"

	NexposeCallback = "exposeCallback"
	NresizeCallback = "resizeCallback"
	NinputCallback  = "inputCallback"

	Nvalue = "value"

	NactiveColor = "activeColor"
	NcursorName  = "cursorName"
	NtraversalOn = "traversalOn"
	NtabIndex    = "tabIndex"

	NdropCallback       = "dropCallback"
	NdragEnterCallback  = "dragEnterCallback"
	NdragMotionCallback = "dragMotionCallback"
	NdragLeaveCallback  = "dragLeaveCallback"

	NselectCallback = "selectCallback"
	NmultiSelect    = "multiSelect"

	NfileMode        = "fileMode"
	NinitialDirectory = "initialDirectory"
	NfileSelected     = "fileSelected"
	NfileCancelled    = "fileCancelled"

	NspinMinimum   = "spinMinimum"
	NspinMaximum   = "spinMaximum"
	NspinValue     = "spinValue"
	NspinIncrement = "spinIncrement"

	NcolorRed     = "colorRed"
	NcolorGreen   = "colorGreen"
	NcolorBlue    = "colorBlue"
	NcolorChanged = "colorChanged"
)

// EdgeType controls Form constraint edge behavior.
type EdgeType = uintptr

const (
	ChainTop    EdgeType = 0
	ChainBottom EdgeType = 1
	ChainLeft   EdgeType = 2
	ChainRight  EdgeType = 3
	Rubber      EdgeType = 4
)

// Justify controls text alignment.
type Justify int

const (
	JustifyLeft   Justify = 0
	JustifyCenter Justify = 1
	JustifyRight  Justify = 2
)

// Orientation controls layout direction.
type Orientation int

const (
	OrientHorizontal Orientation = 0
	OrientVertical   Orientation = 1
)

// Ellipsize controls text truncation.
type Ellipsize int

const (
	EllipsizeNone   Ellipsize = 0
	EllipsizeStart  Ellipsize = 1
	EllipsizeMiddle Ellipsize = 2
	EllipsizeEnd    Ellipsize = 3
)

// Relief controls 3D shadow style.
type Relief int

const (
	ReliefNone   Relief = 0
	ReliefRaised Relief = 1
	ReliefSunken Relief = 2
	ReliefRidge  Relief = 3
	ReliefGroove Relief = 4
)

// TextEditType controls whether a Text widget is editable.
type TextEditType int

const (
	TextRead   TextEditType = 0
	TextAppend TextEditType = 1
	TextEdit   TextEditType = 2
)

// TextScrollMode controls Text widget scrollbar behavior.
type TextScrollMode int

const (
	TextScrollNever      TextScrollMode = 0
	TextScrollWhenNeeded TextScrollMode = 1
	TextScrollAlways     TextScrollMode = 2
)

// TextWrapMode controls Text widget line wrapping.
type TextWrapMode int

const (
	TextWrapNever TextWrapMode = 0
	TextWrapLine  TextWrapMode = 1
	TextWrapWord  TextWrapMode = 2
)

// TextResizeMode controls Text widget auto-resize behavior.
type TextResizeMode int

const (
	TextResizeNever  TextResizeMode = 0
	TextResizeWidth  TextResizeMode = 1
	TextResizeHeight TextResizeMode = 2
	TextResizeBoth   TextResizeMode = 3
)
