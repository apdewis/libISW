#ifndef _ISW_UTF8_H
#define _ISW_UTF8_H

#include <stdint.h>

/*
 * Byte-oriented UTF-8 helpers. All functions operate on UTF-8 byte ranges
 * and codepoint boundaries; no wchar / mbstate involvement.
 *
 * Invalid bytes are treated as a single-byte codepoint U+FFFD so callers
 * never desynchronise against malformed input.
 */

/* Decode one codepoint starting at s[0]. Returns bytes consumed (>=1).
 * On invalid input or len <= 0, returns 0 and leaves *cp_out untouched
 * when cp_out is NULL, or sets *cp_out = 0xFFFD when non-NULL with len > 0. */
int _IswUtf8Decode(const char *s, int len, uint32_t *cp_out);

/* Byte length of the codepoint starting at s[0]. 1 for invalid leading byte. */
int _IswUtf8CharLen(const char *s, int len);

/* Count codepoints in a UTF-8 byte range. */
int _IswUtf8CodepointCount(const char *s, int byte_len);

/* Step forward one codepoint from byte offset pos in [0, len]. Returns the
 * new offset, clamped to len. */
int _IswUtf8Next(const char *s, int len, int pos);

/* Step backward one codepoint from byte offset pos. Returns the new offset,
 * clamped to 0. Does not need len because it scans backwards for a lead byte. */
int _IswUtf8Prev(const char *s, int pos);

/* True if b is a UTF-8 continuation byte (10xxxxxx). */
static inline int _IswUtf8IsCont(unsigned char b) {
    return (b & 0xC0) == 0x80;
}

#endif /* _ISW_UTF8_H */
