package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Text.h>
#include <stdlib.h>

static int _isw_text_replace(Widget w, long start, long end, char *s, int len) {
	ISWTextBlock block;
	block.firstPos = 0;
	block.length = len;
	block.ptr = s;
	block.format = FMT8BIT;
	return IswTextReplace(w, (ISWTextPosition)start, (ISWTextPosition)end,
	                      &block);
}

static long _isw_text_search(Widget w, int dir, char *s, int len) {
	ISWTextBlock block;
	block.firstPos = 0;
	block.length = len;
	block.ptr = s;
	block.format = FMT8BIT;
	return (long)IswTextSearch(w, (IswTextScanDirection)dir, &block);
}
*/
import "C"

import "unsafe"

// TextScanDirection is a search direction for TextSearch.
type TextScanDirection int

const (
	TextScanLeft  TextScanDirection = C.IswsdLeft
	TextScanRight TextScanDirection = C.IswsdRight
)

// TextSelectType is a selection granularity for multi-click selection.
type TextSelectType int

const (
	SelectNull      TextSelectType = C.IswselectNull
	SelectPosition  TextSelectType = C.IswselectPosition
	SelectChar      TextSelectType = C.IswselectChar
	SelectWord      TextSelectType = C.IswselectWord
	SelectLine      TextSelectType = C.IswselectLine
	SelectParagraph TextSelectType = C.IswselectParagraph
	SelectAll       TextSelectType = C.IswselectAll
)

// TextReplace return codes.
const (
	TextReplaceError  = -1
	TextEditDone      = 0
	TextEditError     = 1
	TextPositionError = 2
)

// TextSearchError is returned by TextSearch when the text is not found.
const TextSearchError = -12345

// TextReplace replaces the range [start, end) with text. Returns
// TextEditDone on success.
func TextReplace(w Widget, start, end int, text string) int {
	cText := C.CString(text)
	defer C.free(unsafe.Pointer(cText))
	return int(C._isw_text_replace(w.c, C.long(start), C.long(end),
		cText, C.int(len(text))))
}

// TextSearch searches from the insertion point in the given direction.
// Returns the match position or TextSearchError.
func TextSearch(w Widget, dir TextScanDirection, text string) int {
	cText := C.CString(text)
	defer C.free(unsafe.Pointer(cText))
	return int(C._isw_text_search(w.c, C.int(dir), cText, C.int(len(text))))
}

// TextGetInsertionPoint returns the insertion point position.
func TextGetInsertionPoint(w Widget) int {
	return int(C.IswTextGetInsertionPoint(w.c))
}

// TextSetInsertionPoint moves the insertion point.
func TextSetInsertionPoint(w Widget, position int) {
	C.IswTextSetInsertionPoint(w.c, C.ISWTextPosition(position))
}

// TextSetSelection selects the range [left, right).
func TextSetSelection(w Widget, left, right int) {
	C.IswTextSetSelection(w.c, C.ISWTextPosition(left), C.ISWTextPosition(right))
}

// TextUnsetSelection removes the selection.
func TextUnsetSelection(w Widget) {
	C.IswTextUnsetSelection(w.c)
}

// TextGetSelectionPos returns the selection range; begin == end means no
// selection.
func TextGetSelectionPos(w Widget) (begin, end int) {
	var cBegin, cEnd C.ISWTextPosition
	C.IswTextGetSelectionPos(w.c, &cBegin, &cEnd)
	return int(cBegin), int(cEnd)
}

// TextTopPosition returns the position of the first visible character.
func TextTopPosition(w Widget) int {
	return int(C.IswTextTopPosition(w.c))
}

// TextDisplay forces a redisplay of the text widget.
func TextDisplay(w Widget) {
	C.IswTextDisplay(w.c)
}

// TextInvalidate marks the range [from, to) as needing redisplay.
func TextInvalidate(w Widget, from, to int) {
	C.IswTextInvalidate(w.c, C.ISWTextPosition(from), C.ISWTextPosition(to))
}

// TextEnableRedisplay re-enables redisplay and flushes pending updates.
func TextEnableRedisplay(w Widget) {
	C.IswTextEnableRedisplay(w.c)
}

// TextDisableRedisplay suspends redisplay during batched edits.
func TextDisableRedisplay(w Widget) {
	C.IswTextDisableRedisplay(w.c)
}

// TextDisplayCaret shows or hides the insertion caret.
func TextDisplayCaret(w Widget, visible bool) {
	v := C.Boolean(0)
	if visible {
		v = 1
	}
	C.IswTextDisplayCaret(w.c, v)
}

// TextSetSelectionArray sets the selection granularity used on
// successive mouse clicks. The slice must end with SelectNull to
// terminate the array; if it does not, SelectNull is appended.
func TextSetSelectionArray(w Widget, types []TextSelectType) {
	if len(types) == 0 {
		C.IswTextSetSelectionArray(w.c, nil)
		return
	}
	terminated := false
	if len(types) > 0 && types[len(types)-1] == SelectNull {
		terminated = true
	}
	n := len(types)
	if !terminated {
		n++
	}
	block := C.malloc(C.size_t(n) * C.size_t(unsafe.Sizeof(C.IswTextSelectType(0))))
	arr := unsafe.Slice((*C.IswTextSelectType)(block), n)
	for i, t := range types {
		arr[i] = C.IswTextSelectType(t)
	}
	if !terminated {
		arr[n-1] = C.IswTextSelectType(SelectNull)
	}
	C.IswTextSetSelectionArray(w.c, &arr[0])
	C.free(block)
}

// TextSetSource installs a text source on the widget, scrolling to the
// given position.
func TextSetSource(w Widget, source Widget, position int) {
	C.IswTextSetSource(w.c, source.c, C.ISWTextPosition(position))
}

// TextGetSource returns the text source widget.
func TextGetSource(w Widget) Widget {
	return Widget{C.IswTextGetSource(w.c)}
}

// TextGetSink returns the text sink widget.
func TextGetSink(w Widget) Widget {
	return Widget{C.IswTextGetSink(w.c)}
}
