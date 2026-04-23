#include <stddef.h>
#include "../include/ISW/ISWUtf8.h"

int
_IswUtf8Decode(const char *s, int len, uint32_t *cp_out)
{
    if (!s || len <= 0) return 0;

    const unsigned char *p = (const unsigned char *)s;
    unsigned char b0 = p[0];
    uint32_t cp;
    int n;

    if (b0 < 0x80) {
        if (cp_out) *cp_out = b0;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0) { cp = b0 & 0x1F; n = 2; }
    else if ((b0 & 0xF0) == 0xE0) { cp = b0 & 0x0F; n = 3; }
    else if ((b0 & 0xF8) == 0xF0) { cp = b0 & 0x07; n = 4; }
    else {
        if (cp_out) *cp_out = 0xFFFD;
        return 1;
    }

    if (len < n) {
        if (cp_out) *cp_out = 0xFFFD;
        return 1;
    }
    for (int i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            if (cp_out) *cp_out = 0xFFFD;
            return 1;
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }

    /* Overlong / surrogate / out-of-range → replacement, but still consume n. */
    int min_cp = (n == 2) ? 0x80 : (n == 3) ? 0x800 : 0x10000;
    if (cp < (uint32_t)min_cp || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF)
        cp = 0xFFFD;

    if (cp_out) *cp_out = cp;
    return n;
}

int
_IswUtf8CharLen(const char *s, int len)
{
    return _IswUtf8Decode(s, len, NULL);
}

int
_IswUtf8CodepointCount(const char *s, int byte_len)
{
    int count = 0, i = 0;
    while (i < byte_len) {
        int n = _IswUtf8Decode(s + i, byte_len - i, NULL);
        if (n <= 0) break;
        i += n;
        count++;
    }
    return count;
}

int
_IswUtf8Next(const char *s, int len, int pos)
{
    if (pos < 0) return 0;
    if (pos >= len) return len;
    int n = _IswUtf8Decode(s + pos, len - pos, NULL);
    if (n <= 0) return len;
    int next = pos + n;
    return next > len ? len : next;
}

int
_IswUtf8Prev(const char *s, int pos)
{
    if (pos <= 0) return 0;
    int i = pos - 1;
    /* Skip continuation bytes, but stop after at most 3 to avoid runaway
     * scans through malformed data. */
    int max_skip = 3;
    while (i > 0 && max_skip > 0 && _IswUtf8IsCont((unsigned char)s[i])) {
        i--;
        max_skip--;
    }
    return i;
}
