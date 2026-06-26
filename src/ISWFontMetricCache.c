/*
 * ISWFontMetricCache.c - Application-wide font measurement cache
 *
 * Copyright (c) 2026 ISW Project
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWFontMetricCache.h>
#include <stdlib.h>
#include <string.h>

#define METRIC_CACHE_SIZE 4096
#define METRIC_CACHE_MASK (METRIC_CACHE_SIZE - 1)

typedef struct {
    IswFontStruct *font;
    unsigned int  hash;
    int           len;
    int           width;
    Boolean       occupied;
} MetricEntry;

struct _ISWFontMetricCache {
    MetricEntry entries[METRIC_CACHE_SIZE];
};

static ISWFontMetricCache g_metric_cache;
static Boolean g_metric_cache_init = False;

static unsigned int
fnv1a(const char *data, int len)
{
    unsigned int h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= (unsigned char)data[i];
        h *= 16777619u;
    }
    return h;
}

ISWFontMetricCache *
ISWFontMetricCacheGet(void)
{
    if (!g_metric_cache_init) {
        memset(&g_metric_cache, 0, sizeof(g_metric_cache));
        g_metric_cache_init = True;
    }
    return &g_metric_cache;
}

Boolean
ISWFontMetricCacheLookup(ISWFontMetricCache *cache,
                         IswFontStruct *font,
                         const char *text, int len,
                         int *width_out)
{
    if (!cache || !text)
        return False;

    if (len < 0)
        len = (int)strlen(text);

    unsigned int h = fnv1a(text, len);
    unsigned int idx = (h ^ (unsigned int)(uintptr_t)font) & METRIC_CACHE_MASK;

    MetricEntry *e = &cache->entries[idx];
    if (e->occupied && e->font == font && e->hash == h && e->len == len) {
        if (width_out) *width_out = e->width;
        return True;
    }

    return False;
}

void
ISWFontMetricCacheStore(ISWFontMetricCache *cache,
                        IswFontStruct *font,
                        const char *text, int len,
                        int width)
{
    if (!cache || !text)
        return;

    if (len < 0)
        len = (int)strlen(text);

    unsigned int h = fnv1a(text, len);
    unsigned int idx = (h ^ (unsigned int)(uintptr_t)font) & METRIC_CACHE_MASK;

    MetricEntry *e = &cache->entries[idx];
    e->font = font;
    e->hash = h;
    e->len = len;
    e->width = width;
    e->occupied = True;
}

void
ISWFontMetricCacheFlush(ISWFontMetricCache *cache)
{
    if (!cache)
        return;
    memset(cache->entries, 0, sizeof(cache->entries));
}
