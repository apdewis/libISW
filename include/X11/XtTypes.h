/* include/X11/XtTypes.h
 * Local definitions for types and constants not provided by XCB headers.
 * Only contains types/constants that have NO XCB equivalent.
 * Everything else should be used directly from xcb/xproto.h or xcb/xcb_icccm.h.
 */
#ifndef _XtTypes_h
#define _XtTypes_h

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xkbcommon/xkbcommon.h>
#include <stdint.h>

/*
 * -----------------------------------------------------------------------
 * Primitive types with no XCB equivalent
 * -----------------------------------------------------------------------
 */

#ifndef Bool
typedef int Bool;
#endif

#ifndef True
#define True  1
#define False 0
#endif

#ifndef XPointer
typedef char *XPointer;
#endif

#ifndef XID
typedef uint32_t XID;
#endif

/* Must match XtValueMask (unsigned long) to avoid pointer type conflicts. */
#ifndef Mask
typedef unsigned long Mask;
#endif

#ifndef Atom
typedef uint32_t Atom;
#endif

#ifndef XContext
typedef int XContext;
#endif

/*
 * -----------------------------------------------------------------------
 * Accessor macros wrapping XCB calls
 * -----------------------------------------------------------------------
 */

#ifndef ConnectionNumber
#define ConnectionNumber(dpy) xcb_get_file_descriptor(dpy)
#endif

#ifndef DefaultRootWindow
#define DefaultRootWindow(dpy) \
    (xcb_setup_roots_iterator(xcb_get_setup(dpy)).data->root)
#endif

#ifndef RootWindowOfScreen
#define RootWindowOfScreen(s) ((s)->root)
#endif

#ifndef XFree
#define XFree(ptr) free(ptr)
#endif

#ifndef BlackPixelOfScreen
#define BlackPixelOfScreen(s) ((s)->black_pixel)
#define WhitePixelOfScreen(s) ((s)->white_pixel)
#endif


/*
 * -----------------------------------------------------------------------
 * Geometry parse flags — no XCB equivalent
 * -----------------------------------------------------------------------
 */
#ifndef XValue
#define XValue      0x0001
#define YValue      0x0002
#define WidthValue  0x0004
#define HeightValue 0x0008
#define AllValues   0x000F
#define XNegative   0x0010
#define YNegative   0x0020
#endif

/*
 * -----------------------------------------------------------------------
 * KeySym — mapped to xcb_keysym_t
 * -----------------------------------------------------------------------
 */
#ifndef KeySym
typedef xcb_keysym_t KeySym;
#endif

#ifndef NoSymbol
#define NoSymbol 0L
#endif

#ifndef XStringToKeysym
#define XStringToKeysym(str) \
    ((KeySym) xkb_keysym_from_name((str), XKB_KEYSYM_NO_FLAGS))
#endif

#ifndef XKeysymToString
#include <string.h>
static inline const char *XKeysymToString(KeySym keysym) {
    static char _xt_keysym_buf[64];
    int n = xkb_keysym_get_name((xkb_keysym_t) keysym,
                                 _xt_keysym_buf, sizeof(_xt_keysym_buf));
    if (n < 0) return NULL;
    return _xt_keysym_buf;
}
#endif


/*
 * -----------------------------------------------------------------------
 * XSetWindowAttributes — no XCB struct equivalent
 * (XCB uses uint32_t value lists instead)
 * -----------------------------------------------------------------------
 */
#ifndef _XSetWindowAttributes_defined
#define _XSetWindowAttributes_defined
typedef struct {
    xcb_pixmap_t  background_pixmap;
    unsigned long background_pixel;
    xcb_pixmap_t  border_pixmap;
    unsigned long border_pixel;
    int           bit_gravity;
    int           win_gravity;
    int           backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool          save_under;
    long          event_mask;
    long          do_not_propagate_mask;
    Bool          override_redirect;
    xcb_colormap_t colormap;
    xcb_cursor_t  cursor;
} XSetWindowAttributes;
#endif

/*
 * -----------------------------------------------------------------------
 * Constants with no XCB equivalent
 * -----------------------------------------------------------------------
 */

#ifndef None
#define None 0L
#endif

#ifndef CopyFromParent
#define CopyFromParent 0L
#endif

/* No XCB equivalent for NoExpose event type */
#ifndef NoExpose
#define NoExpose 14
#endif

/* Sentinel — not an XCB concept */
#ifndef LASTEvent
#define LASTEvent 36
#endif

#ifndef AnyPropertyType
#define AnyPropertyType 0L
#endif

#ifndef AllTemporary
#define AllTemporary 0L
#endif

#ifndef CurrentTime
#define CurrentTime 0L
#endif

#ifndef AnyKey
#define AnyKey   0L
#define AnyButton 0L
#endif

#ifndef AllPlanes
#define AllPlanes ((unsigned long)~0L)
#endif

#ifndef PointerRoot
#define PointerRoot 1L
#endif

#ifndef InputFocus
#define InputFocus 1L
#endif

#ifndef PointerWindow
#define PointerWindow 0L
#endif

/* NotifyHint — not a mode/detail enum, standalone constant */
#ifndef NotifyHint
#define NotifyHint 1
#endif

/*
 * -----------------------------------------------------------------------
 * DoRed/DoGreen/DoBlue — no XCB equivalent
 * -----------------------------------------------------------------------
 */
#ifndef DoRed
#define DoRed   (1<<0)
#define DoGreen (1<<1)
#define DoBlue  (1<<2)
#endif

/*
 * -----------------------------------------------------------------------
 * Struct replacements — no XCB struct equivalents
 * -----------------------------------------------------------------------
 */

typedef struct {
    unsigned long pixel;
    unsigned short red, green, blue;
    char flags;
    char pad;
} XtColor;
typedef XtColor XColor;

typedef struct {
    xcb_visualtype_t *visual;
    xcb_visualid_t    visualid;
    int               screen;
    int               depth;
    int               class;
    unsigned long     red_mask;
    unsigned long     green_mask;
    unsigned long     blue_mask;
    int               colormap_size;
    int               bits_per_rgb;
} XtVisualInfo;
typedef XtVisualInfo XVisualInfo;

typedef struct _XtFontStruct {
    xcb_font_t      fid;
    unsigned        direction;
    unsigned        min_char_or_byte2;
    unsigned        max_char_or_byte2;
    unsigned        min_byte1;
    unsigned        max_byte1;
    int             ascent;
    int             descent;
    char           *font_family;
    int             font_weight;
    int             font_slant;
} XtFontStruct;
typedef XtFontStruct XFontStruct;

typedef void *XtFontSet;
typedef XtFontSet XFontSet;

#ifndef XrmString
typedef char *XrmString;
#endif

#endif /* _XtTypes_h */
