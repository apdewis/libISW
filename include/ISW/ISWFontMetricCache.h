/*
 * ISWFontMetricCache.h - Application-wide font measurement cache
 *
 * Caches text_width results keyed by (font, text content, length) so
 * repeated measurements of the same string at the same font are O(1).
 * Shared across all widgets.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifndef _ISW_ISWFontMetricCache_h
#define _ISW_ISWFontMetricCache_h

#include <ISW/ISWRender.h>

typedef struct _ISWFontMetricCache ISWFontMetricCache;

/*
 * ISWFontMetricCacheGet - Return the process-wide metric cache singleton.
 *   Lazily initialised on first call.  Never returns NULL.
 */
ISWFontMetricCache *ISWFontMetricCacheGet(void);

/*
 * ISWFontMetricCacheLookup - Look up a cached text width.
 *
 * Returns True and sets *width_out on cache hit, False on miss.
 *
 * Parameters:
 *   cache     - Cache instance
 *   font      - Font the text is measured with
 *   text      - Text string (content is hashed, not pointer)
 *   len       - Byte length of text (-1 = strlen)
 *   width_out - Receives the cached width on hit
 */
Boolean ISWFontMetricCacheLookup(ISWFontMetricCache *cache,
                                 IswFontStruct *font,
                                 const char *text, int len,
                                 int *width_out);

/*
 * ISWFontMetricCacheStore - Insert a measurement into the cache.
 *
 * Overwrites any existing entry for the same (font, text, len) key.
 */
void ISWFontMetricCacheStore(ISWFontMetricCache *cache,
                             IswFontStruct *font,
                             const char *text, int len,
                             int width);

/*
 * ISWFontMetricCacheFlush - Invalidate all entries.
 *
 * Call when a global font change makes cached measurements stale.
 */
void ISWFontMetricCacheFlush(ISWFontMetricCache *cache);

#endif /* _ISW_ISWFontMetricCache_h */
