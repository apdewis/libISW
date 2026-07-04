package isw

/*
#include <ISW/ISWPNG.h>
#include <ISW/ISWSVG.h>
#include <ISW/ISWImage.h>
#include <stdlib.h>
*/
import "C"

import "unsafe"

// Image wraps a unified ISWImage (SVG or PNG, decoded on demand).
type Image struct {
	c *C.ISWImage
}

// LoadImage loads an image from a file path or inline SVG data. source
// starting with '<' is inline SVG XML, ending in ".svg" an SVG file,
// anything else a PNG file. dpi sizes SVGs (96 * scale factor);
// currentColor substitutes the SVG currentColor ("" for none).
func LoadImage(source string, dpi float64, currentColor string) *Image {
	cSource := C.CString(source)
	defer C.free(unsafe.Pointer(cSource))

	var cColor *C.char
	if currentColor != "" {
		cColor = C.CString(currentColor)
		defer C.free(unsafe.Pointer(cColor))
	}

	img := C.ISWImageLoad(cSource, C.double(dpi), cColor)
	if img == nil {
		return nil
	}
	return &Image{img}
}

// Destroy frees the image and its raster cache.
func (i *Image) Destroy() {
	if i.c != nil {
		C.ISWImageDestroy(i.c)
		i.c = nil
	}
}

// Width returns the native image width.
func (i *Image) Width() float32 { return float32(C.ISWImageGetWidth(i.c)) }

// Height returns the native image height.
func (i *Image) Height() float32 { return float32(C.ISWImageGetHeight(i.c)) }

// IsVector returns true for vector (SVG) images.
func (i *Image) IsVector() bool { return C.ISWImageIsVector(i.c) != 0 }

// IsMonochrome returns true for single-color currentColor SVGs, which
// suit alpha-mask recoloring.
func (i *Image) IsMonochrome() bool { return C.ISWImageIsMonochrome(i.c) != 0 }

// Rasterize returns RGBA pixel data and its actual dimensions. SVGs
// rasterize at the hinted size; PNGs return native pixels.
func (i *Image) Rasterize(hintW, hintH uint) ([]byte, uint, uint) {
	var outW, outH C.uint
	ptr := C.ISWImageRasterize(i.c, C.uint(hintW), C.uint(hintH), &outW, &outH)
	if ptr == nil {
		return nil, 0, 0
	}
	n := uint(outW) * uint(outH) * 4
	return C.GoBytes(unsafe.Pointer(ptr), C.int(n)), uint(outW), uint(outH)
}

// Recolor re-parses an SVG with a new currentColor value ("" removes the
// substitution). No-op for PNGs.
func (i *Image) Recolor(hexColor string) {
	var cColor *C.char
	if hexColor != "" {
		cColor = C.CString(hexColor)
		defer C.free(unsafe.Pointer(cColor))
	}
	C.ISWImageRecolor(i.c, cColor)
}

// PNGImage wraps a decoded PNG image.
type PNGImage struct {
	c *C.ISWPNGImage
}

// LoadPNGFile loads a PNG image from a file.
func LoadPNGFile(filename string) *PNGImage {
	cFile := C.CString(filename)
	defer C.free(unsafe.Pointer(cFile))
	img := C.ISWPNGLoadFile(cFile)
	if img == nil {
		return nil
	}
	return &PNGImage{img}
}

// LoadPNGData loads a PNG image from memory.
func LoadPNGData(data []byte) *PNGImage {
	if len(data) == 0 {
		return nil
	}
	img := C.ISWPNGLoadData((*C.uchar)(unsafe.Pointer(&data[0])), C.uint(len(data)))
	if img == nil {
		return nil
	}
	return &PNGImage{img}
}

// Destroy frees the PNG image.
func (p *PNGImage) Destroy() {
	if p.c != nil {
		C.ISWPNGDestroy(p.c)
		p.c = nil
	}
}

// Width returns the image width.
func (p *PNGImage) Width() uint { return uint(C.ISWPNGGetWidth(p.c)) }

// Height returns the image height.
func (p *PNGImage) Height() uint { return uint(C.ISWPNGGetHeight(p.c)) }

// RGBA returns the raw RGBA pixel data. The slice is valid until Destroy.
func (p *PNGImage) RGBA() []byte {
	ptr := C.ISWPNGGetRGBA(p.c)
	if ptr == nil {
		return nil
	}
	n := p.Width() * p.Height() * 4
	return unsafe.Slice((*byte)(unsafe.Pointer(ptr)), n)
}

// SVGImage wraps a parsed SVG image.
type SVGImage struct {
	c *C.ISWSVGImage
}

// LoadSVGFile loads an SVG image from a file.
func LoadSVGFile(filename, units string, dpi float32, currentColor string) *SVGImage {
	cFile := C.CString(filename)
	cUnits := C.CString(units)
	defer C.free(unsafe.Pointer(cFile))
	defer C.free(unsafe.Pointer(cUnits))

	var cColor *C.char
	if currentColor != "" {
		cColor = C.CString(currentColor)
		defer C.free(unsafe.Pointer(cColor))
	}

	img := C.ISWSVGLoadFile(cFile, cUnits, C.float(dpi), cColor)
	if img == nil {
		return nil
	}
	return &SVGImage{img}
}

// LoadSVGData loads an SVG from an in-memory string.
func LoadSVGData(data, units string, dpi float32, currentColor string) *SVGImage {
	cData := C.CString(data)
	cUnits := C.CString(units)
	defer C.free(unsafe.Pointer(cData))
	defer C.free(unsafe.Pointer(cUnits))

	var cColor *C.char
	if currentColor != "" {
		cColor = C.CString(currentColor)
		defer C.free(unsafe.Pointer(cColor))
	}

	img := C.ISWSVGLoadData(cData, cUnits, C.float(dpi), cColor)
	if img == nil {
		return nil
	}
	return &SVGImage{img}
}

// Destroy frees the SVG image.
func (s *SVGImage) Destroy() {
	if s.c != nil {
		C.ISWSVGDestroy(s.c)
		s.c = nil
	}
}

// Width returns the native SVG width.
func (s *SVGImage) Width() float32 { return float32(C.ISWSVGGetWidth(s.c)) }

// Height returns the native SVG height.
func (s *SVGImage) Height() float32 { return float32(C.ISWSVGGetHeight(s.c)) }

// Rasterize rasterizes the SVG to the given dimensions.
// The returned slice must be freed by the caller with C.free (or let GC handle
// a copy). Caller owns the buffer.
func (s *SVGImage) Rasterize(width, height uint) []byte {
	ptr := C.ISWSVGRasterize(s.c, C.uint(width), C.uint(height))
	if ptr == nil {
		return nil
	}
	n := width * height * 4
	buf := C.GoBytes(unsafe.Pointer(ptr), C.int(n))
	C.free(unsafe.Pointer(ptr))
	return buf
}

// RasterizeScale rasterizes at a scale factor, returning the actual dimensions.
func (s *SVGImage) RasterizeScale(scale float32) ([]byte, uint, uint) {
	var outW, outH C.uint
	ptr := C.ISWSVGRasterizeScale(s.c, C.float(scale), &outW, &outH)
	if ptr == nil {
		return nil, 0, 0
	}
	n := uint(outW) * uint(outH) * 4
	buf := C.GoBytes(unsafe.Pointer(ptr), C.int(n))
	C.free(unsafe.Pointer(ptr))
	return buf, uint(outW), uint(outH)
}
