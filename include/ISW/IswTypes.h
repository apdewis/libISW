/* include/ISW/IswTypes.h
 * Neutral primitive types and constants for the platform-independent core.
 * No XCB/cairo dependency: handles are opaque value/pointer types the active
 * backend reinterprets; X11-derived constant values are kept only where they
 * are numerically protocol-compatible and carry no native type.
 */
#ifndef _IswTypes_h
#define _IswTypes_h

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

#ifndef XID
typedef uint32_t XID;
#endif

/* Neutral input vocabulary (full ops in ISW/ISWPlatform.h).  Numerically
   X11-keysym/keycode compatible; carry no xcb dependency.  Defined here (the
   earliest-included types header) so KeySym below and the public key APIs can
   use them. */
typedef uint32_t IswKeyCode;
typedef uint32_t IswKeySym;
#ifndef IswNoSymbol
#define IswNoSymbol ((IswKeySym) 0)
#endif

/* Portable integer point — neutral replacement for xcb_point_t in
   platform-neutral signatures (polygon vertex lists, etc.).  Defined here so
   the render and platform headers share one definition with no xcb dependency. */
#ifndef ISW_POINT_DEFINED
#define ISW_POINT_DEFINED
typedef struct {
    int16_t x, y;
} IswPoint;
#endif

/* Neutral color/font/visual handles (full ops in ISW/ISWPlatform.h).  Value
   handles: each IS the native id/pointer reinterpreted by the backend (like
   IswWindow), so seam conversions are plain casts.  Carry no xcb dependency.
   Defined here (the earliest-included types header) so the IswColor /
   IswVisualInfo / IswFontStruct structs below can embed them.  Phase 4. */
typedef uintptr_t IswColormap;   /* colormap id (value handle)            */
typedef uintptr_t IswFontId;     /* core font id (value handle), 0 = none */
typedef void     *IswVisual;     /* a visual (value handle over the native
                                    visual type)                          */
typedef uint32_t  IswVisualId;   /* a visual id                           */

/* Neutral cursor handle + server time (full ops in ISW/ISWPlatform.h).  Value
   handles; numerically X11-compatible; carry no xcb dependency.  Defined here
   (the earliest types header) so IswSetWindowAttributes below and the public
   grab/selection APIs can use them.  Phase 5. */
typedef uintptr_t IswCursor;     /* cursor id (value handle), 0 = none    */
typedef uintptr_t IswPixmap;     /* pixmap id (value handle), 0 = none    */
typedef uint32_t  IswTime;       /* server timestamp                      */

/* Neutral damage/expose region handle.  A client-side, toolkit-owned
   rectangle-set (struct _IswRegion, defined in the XCB draw backend) — NOT an
   X server object.  Replaces the XFixes server-region type in the IswExpose
   contract so no widget's expose proc names an X extension.  0/NULL means "no
   region; repaint everything".  Phase 14. */
typedef struct _IswRegion *IswRegion;
#ifndef ISW_CURRENT_TIME
#define ISW_CURRENT_TIME ((IswTime) 0)
#endif

/* Must match IswValueMask (unsigned long) to avoid pointer type conflicts. */
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
 * Geometry parse flags — no XCB equivalent
 * -----------------------------------------------------------------------
 */
#ifndef XValue
#define XValue      0x0001
#define YValue      0x0002
#define WidthValue  0x0004
#define HeightValue 0x0008
#endif

/*
 * -----------------------------------------------------------------------
 * KeySym — neutral key identity (IswKeySym).  Numerically X11-keysym
 * compatible; the type carries no xcb dependency.  Phase 3.
 * -----------------------------------------------------------------------
 */
#ifndef KeySym
typedef IswKeySym KeySym;
#endif

#ifndef NoSymbol
#define NoSymbol 0L
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

#ifndef CurrentTime
#define CurrentTime 0L
#endif

#ifndef AnyKey
#define AnyKey   0L
#define AnyButton 0L
#endif

#ifndef PointerRoot
#define PointerRoot 1L
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
} IswColor;

typedef struct {
    IswVisual         visual;
    IswVisualId       visualid;
    int               screen;
    int               depth;
    int               class;
    unsigned long     red_mask;
    unsigned long     green_mask;
    unsigned long     blue_mask;
    int               colormap_size;
    int               bits_per_rgb;
} IswVisualInfo;

typedef struct _IswFontStruct {
    IswFontId       fid;
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
    double          pt_size;
} IswFontStruct;

#ifndef XrmString
typedef char *XrmString;
#endif

#endif /* _IswTypes_h */
