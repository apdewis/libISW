package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/ISWRender.h>
*/
import "C"

import "unsafe"

// RenderBackend identifies a rendering backend.
type RenderBackend int

const (
	RenderBackendAuto    RenderBackend = C.ISW_RENDER_BACKEND_AUTO
	RenderBackendCairoXCB RenderBackend = C.ISW_RENDER_BACKEND_CAIRO_XCB
	RenderBackendEGL     RenderBackend = C.ISW_RENDER_BACKEND_EGL
)

// RenderCaps is a bitmask of backend capabilities.
type RenderCaps int

const (
	RenderCapBasic       RenderCaps = C.ISW_RENDER_CAP_BASIC
	RenderCapAntialiasing RenderCaps = C.ISW_RENDER_CAP_ANTIALIASING
	RenderCapGradients   RenderCaps = C.ISW_RENDER_CAP_GRADIENTS
	RenderCapAlpha       RenderCaps = C.ISW_RENDER_CAP_ALPHA
	RenderCapTransforms  RenderCaps = C.ISW_RENDER_CAP_TRANSFORMS
	RenderCapTextAdvanced RenderCaps = C.ISW_RENDER_CAP_TEXT_ADVANCED
	RenderCapHWAccel     RenderCaps = C.ISW_RENDER_CAP_HW_ACCEL
)

// FillRule controls path fill behaviour.
type FillRule int

const (
	FillRuleWinding FillRule = C.ISW_FILL_RULE_WINDING
	FillRuleEvenOdd FillRule = C.ISW_FILL_RULE_EVEN_ODD
)

// Operator controls compositing mode.
type Operator int

const (
	OperatorOver       Operator = C.ISW_OPERATOR_OVER
	OperatorDifference Operator = C.ISW_OPERATOR_DIFFERENCE
)

// RenderContext wraps an ISWRenderContext.
type RenderContext struct {
	c *C.ISWRenderContext
}

// NewRenderContext creates a render context for a widget.
func NewRenderContext(w Widget, backend RenderBackend) *RenderContext {
	ctx := C.ISWRenderCreate(w.c, C.ISWRenderBackend(backend))
	if ctx == nil {
		return nil
	}
	return &RenderContext{ctx}
}

// Destroy frees the render context.
func (rc *RenderContext) Destroy() {
	if rc.c != nil {
		C.ISWRenderDestroy(rc.c)
		rc.c = nil
	}
}

// Backend returns the active backend.
func (rc *RenderContext) Backend() RenderBackend {
	return RenderBackend(C.ISWRenderGetBackend(rc.c))
}

// Capabilities returns the backend capability flags.
func (rc *RenderContext) Capabilities() RenderCaps {
	return RenderCaps(C.ISWRenderGetCapabilities(rc.c))
}

// BackendName returns a human-readable backend name.
func (rc *RenderContext) BackendName() string {
	return C.GoString(C.ISWRenderGetBackendName(rc.c))
}

// Begin starts a rendering frame.
func (rc *RenderContext) Begin() { C.ISWRenderBegin(rc.c) }

// End ends a rendering frame and flushes.
func (rc *RenderContext) End() { C.ISWRenderEnd(rc.c) }

// Save saves the graphics state.
func (rc *RenderContext) Save() { C.ISWRenderSave(rc.c) }

// Restore restores the graphics state.
func (rc *RenderContext) Restore() { C.ISWRenderRestore(rc.c) }

// SetColor sets the drawing color.
func (rc *RenderContext) SetColor(pixel Pixel) {
	C.ISWRenderSetColor(rc.c, C.Pixel(pixel))
}

// SetLineWidth sets the stroke width.
func (rc *RenderContext) SetLineWidth(width float64) {
	C.ISWRenderSetLineWidth(rc.c, C.double(width))
}

// StrokeRectangle draws a rectangle outline.
func (rc *RenderContext) StrokeRectangle(x, y, w, h int) {
	C.ISWRenderStrokeRectangle(rc.c, C.int(x), C.int(y), C.int(w), C.int(h))
}

// FillRectangle fills a rectangle.
func (rc *RenderContext) FillRectangle(x, y, w, h int) {
	C.ISWRenderFillRectangle(rc.c, C.int(x), C.int(y), C.int(w), C.int(h))
}

// FillRoundedRectangle fills a rounded rectangle.
func (rc *RenderContext) FillRoundedRectangle(x, y, w, h int, radius float64) {
	C.ISWRenderFillRoundedRectangle(rc.c, C.int(x), C.int(y),
		C.int(w), C.int(h), C.double(radius))
}

// StrokeRoundedRectangle strokes a rounded rectangle outline.
func (rc *RenderContext) StrokeRoundedRectangle(x, y, w, h int, radius, strokeWidth float64) {
	C.ISWRenderStrokeRoundedRectangle(rc.c, C.int(x), C.int(y),
		C.int(w), C.int(h), C.double(radius), C.double(strokeWidth))
}

// DrawLine draws a line.
func (rc *RenderContext) DrawLine(x1, y1, x2, y2 int) {
	C.ISWRenderDrawLine(rc.c, C.int(x1), C.int(y1), C.int(x2), C.int(y2))
}

// DrawArc draws an arc.
func (rc *RenderContext) DrawArc(x, y, w, h int, angle1, angle2 float64) {
	C.ISWRenderDrawArc(rc.c, C.int(x), C.int(y), C.int(w), C.int(h),
		C.double(angle1), C.double(angle2))
}

// Point is a 2D coordinate.
type Point struct {
	X, Y int
}

// StrokePolygon draws a polygon outline.
func (rc *RenderContext) StrokePolygon(points []Point) {
	if len(points) == 0 {
		return
	}
	cPoints := make([]C.IswPoint, len(points))
	for i, p := range points {
		cPoints[i] = C.IswPoint{x: C.int(p.X), y: C.int(p.Y)}
	}
	C.ISWRenderStrokePolygon(rc.c, &cPoints[0], C.int(len(cPoints)))
}

// FillPolygon fills a polygon.
func (rc *RenderContext) FillPolygon(points []Point) {
	if len(points) == 0 {
		return
	}
	cPoints := make([]C.IswPoint, len(points))
	for i, p := range points {
		cPoints[i] = C.IswPoint{x: C.int(p.X), y: C.int(p.Y)}
	}
	C.ISWRenderFillPolygon(rc.c, &cPoints[0], C.int(len(cPoints)))
}

// DrawString draws a text string.
func (rc *RenderContext) DrawString(text string, x, y int) {
	cText := C.CString(text)
	defer C.free(unsafe.Pointer(cText))
	C.ISWRenderDrawString(rc.c, cText, C.int(len(text)), C.int(x), C.int(y))
}

// TextWidth measures text width in pixels.
func (rc *RenderContext) TextWidth(text string) int {
	cText := C.CString(text)
	defer C.free(unsafe.Pointer(cText))
	return int(C.ISWRenderTextWidth(rc.c, cText, C.int(len(text))))
}

// TextHeight returns the font line height.
func (rc *RenderContext) TextHeight() int {
	return int(C.ISWRenderTextHeight(rc.c))
}

// SetClipRectangle sets the clip region.
func (rc *RenderContext) SetClipRectangle(x, y, w, h int) {
	C.ISWRenderSetClipRectangle(rc.c, C.int(x), C.int(y), C.int(w), C.int(h))
}

// ClearClip removes the clip region.
func (rc *RenderContext) ClearClip() { C.ISWRenderClearClip(rc.c) }

// CopyArea copies pixels within the surface.
func (rc *RenderContext) CopyArea(srcX, srcY, dstX, dstY int, w, h uint) {
	C.ISWRenderCopyArea(rc.c, C.int(srcX), C.int(srcY),
		C.int(dstX), C.int(dstY), C.uint(w), C.uint(h))
}

// DrawImageRGBA draws an RGBA pixel buffer.
func (rc *RenderContext) DrawImageRGBA(rgba []byte, imgW, imgH uint, dstX, dstY int, dstW, dstH uint) {
	if len(rgba) == 0 {
		return
	}
	C.ISWRenderDrawImageRGBA(rc.c, (*C.uchar)(unsafe.Pointer(&rgba[0])),
		C.uint(imgW), C.uint(imgH),
		C.int(dstX), C.int(dstY), C.uint(dstW), C.uint(dstH))
}

// ImageUpload uploads RGBA pixels to a retained texture.
func (rc *RenderContext) ImageUpload(rgba []byte, w, h uint) int {
	if len(rgba) == 0 {
		return 0
	}
	return int(C.ISWRenderImageUpload(rc.c,
		(*C.uchar)(unsafe.Pointer(&rgba[0])), C.uint(w), C.uint(h)))
}

// ImageFree frees a retained image texture.
func (rc *RenderContext) ImageFree(handle int) {
	C.ISWRenderImageFree(rc.c, C.int(handle))
}

// DrawImageHandle draws a previously uploaded image.
func (rc *RenderContext) DrawImageHandle(handle int, dstX, dstY int, dstW, dstH uint) {
	C.ISWRenderDrawImageHandle(rc.c, C.int(handle),
		C.int(dstX), C.int(dstY), C.uint(dstW), C.uint(dstH))
}

// SetGradient sets a linear gradient source.
func (rc *RenderContext) SetGradient(x1, y1, x2, y2 float64, color1, color2 Pixel) bool {
	return C.ISWRenderSetGradient(rc.c,
		C.double(x1), C.double(y1), C.double(x2), C.double(y2),
		C.Pixel(color1), C.Pixel(color2)) != 0
}

// PushGroup begins an offscreen compositing group.
func (rc *RenderContext) PushGroup() { C.ISWRenderPushGroup(rc.c) }

// PopGroupWithAlpha ends a group and composites at the given opacity.
func (rc *RenderContext) PopGroupWithAlpha(alpha float64) {
	C.ISWRenderPopGroupWithAlpha(rc.c, C.double(alpha))
}

// Path construction.

func (rc *RenderContext) PathBegin()                            { C.ISWRenderPathBegin(rc.c) }
func (rc *RenderContext) PathNewSubPath()                       { C.ISWRenderPathNewSubPath(rc.c) }
func (rc *RenderContext) PathMoveTo(x, y float64)              { C.ISWRenderPathMoveTo(rc.c, C.double(x), C.double(y)) }
func (rc *RenderContext) PathLineTo(x, y float64)              { C.ISWRenderPathLineTo(rc.c, C.double(x), C.double(y)) }
func (rc *RenderContext) PathArc(cx, cy, r, a1, a2 float64)   { C.ISWRenderPathArc(rc.c, C.double(cx), C.double(cy), C.double(r), C.double(a1), C.double(a2)) }
func (rc *RenderContext) PathRectangle(x, y, w, h float64)    { C.ISWRenderPathRectangle(rc.c, C.double(x), C.double(y), C.double(w), C.double(h)) }
func (rc *RenderContext) PathClose()                           { C.ISWRenderPathClose(rc.c) }

func (rc *RenderContext) Fill()            { C.ISWRenderFill(rc.c) }
func (rc *RenderContext) FillPreserve()    { C.ISWRenderFillPreserve(rc.c) }
func (rc *RenderContext) Stroke()          { C.ISWRenderStroke(rc.c) }
func (rc *RenderContext) StrokePreserve()  { C.ISWRenderStrokePreserve(rc.c) }
func (rc *RenderContext) Clip()            { C.ISWRenderClip(rc.c) }
func (rc *RenderContext) Paint()           { C.ISWRenderPaint(rc.c) }

func (rc *RenderContext) SetFillRule(rule FillRule) {
	C.ISWRenderSetFillRule(rc.c, C.ISWFillRule(rule))
}

func (rc *RenderContext) SetOperator(op Operator) {
	C.ISWRenderSetOperator(rc.c, C.ISWOperator(op))
}

func (rc *RenderContext) Translate(tx, ty float64) { C.ISWRenderTranslate(rc.c, C.double(tx), C.double(ty)) }
func (rc *RenderContext) Scale(sx, sy float64)     { C.ISWRenderScale(rc.c, C.double(sx), C.double(sy)) }
func (rc *RenderContext) Rotate(radians float64)   { C.ISWRenderRotate(rc.c, C.double(radians)) }

// HiDPI scaling.

// ScaleFactor returns the display's HiDPI scale factor.
func ScaleFactor(w Widget) float64 {
	return float64(C.ISWScaleFactor(w.c))
}

// ScaleDim scales a logical dimension to physical pixels.
func ScaleDim(w Widget, value int) int {
	return int(C.ISWScaleDim(w.c, C.int(value)))
}

// UnscaleDim converts physical pixels to logical.
func UnscaleDim(w Widget, value int) int {
	return int(C.ISWUnscaleDim(w.c, C.int(value)))
}

// PrintBackendInfo prints rendering backend info to stdout.
func PrintBackendInfo() { C.ISWRenderPrintBackendInfo() }
