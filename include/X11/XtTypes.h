/* include/X11/XtTypes.h
 * Local definitions for Xlib-derived types used in libXt.
 * These replace the types previously obtained from <X11/Xutil.h>,
 * <X11/Xlib.h>, and <X11/X.h>.  No Xlib headers are included here;
 * all constants and types are defined inline using XCB equivalents
 * or plain C types.
 */
#ifndef _XtTypes_h
#define _XtTypes_h

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xkbcommon/xkbcommon.h>
#include <stdint.h>

/*
 * -----------------------------------------------------------------------
 * Basic Xlib primitive types (from <X11/Xdefs.h> / <X11/X.h>)
 * -----------------------------------------------------------------------
 */

/* Bool: Xlib typedef int Bool */
#ifndef Bool
typedef int Bool;
#endif

/* True/False: Xlib macros */
#ifndef True
#define True  1
#define False 0
#endif

/* XPointer: Xlib typedef char* XPointer */
#ifndef XPointer
typedef char *XPointer;
#endif

/* XID: generic X resource ID.
 * Xlib uses unsigned long; XCB uses uint32_t.
 * We use uint32_t to match XCB. */
#ifndef XID
typedef uint32_t XID;
#endif

/* Mask: event/attribute mask type.
 * Xlib: typedef unsigned long Mask.
 * Must match XtValueMask (unsigned long) to avoid pointer type conflicts. */
#ifndef Mask
typedef unsigned long Mask;
#endif

/* Atom: X11 interned string identifier */
#ifndef Atom
typedef uint32_t Atom;
#endif

/* XContext: Xlib context identifier (typedef int XContext in Xutil.h) */
#ifndef XContext
typedef int XContext;
#endif

/* ConnectionNumber: Xlib macro returning the file descriptor of a display.
 * XCB equivalent: xcb_get_file_descriptor(conn) */
#ifndef ConnectionNumber
#define ConnectionNumber(dpy) xcb_get_file_descriptor(dpy)
#endif

/* NextRequest: Xlib function returning the next request sequence number.
 * XCB does not expose this directly; return 0 as a stub.
 * (Used in Shell.c for _wait_for_response which is marked #FIXME for XCB port.) */
#ifndef NextRequest
#define NextRequest(dpy) 0UL
#endif

/* DefaultRootWindow: Xlib macro returning the root window of the default screen.
 * XCB equivalent: first screen's root window from setup. */
#ifndef DefaultRootWindow
#define DefaultRootWindow(dpy) \
    (xcb_setup_roots_iterator(xcb_get_setup(dpy)).data->root)
#endif

/* RootWindowOfScreen: Xlib macro returning root window of a screen.
 * In XCB, xcb_screen_t has a 'root' field directly. */
#ifndef RootWindowOfScreen
#define RootWindowOfScreen(s) ((s)->root)
#endif

/* XFree: Xlib memory free function.
 * In XCB port, use standard free(). */
#ifndef XFree
#define XFree(ptr) free(ptr)
#endif

/*
 * -----------------------------------------------------------------------
 * WM hint flags (from <X11/Xutil.h>)
 * Mapped to XCB ICCCM equivalents from <xcb/xcb_icccm.h>
 * -----------------------------------------------------------------------
 */
#ifndef InputHint
#define InputHint        XCB_ICCCM_WM_HINT_INPUT
#define StateHint        XCB_ICCCM_WM_HINT_STATE
#define IconPixmapHint   XCB_ICCCM_WM_HINT_ICON_PIXMAP
#define IconWindowHint   XCB_ICCCM_WM_HINT_ICON_WINDOW
#define IconPositionHint XCB_ICCCM_WM_HINT_ICON_POSITION
#define IconMaskHint     XCB_ICCCM_WM_HINT_ICON_MASK
#define WindowGroupHint  XCB_ICCCM_WM_HINT_WINDOW_GROUP
#define XUrgencyHint     XCB_ICCCM_WM_HINT_X_URGENCY
#define AllHints         XCB_ICCCM_WM_ALL_HINTS
#endif

/* WM state values */
#ifndef WithdrawnState
#define WithdrawnState XCB_ICCCM_WM_STATE_WITHDRAWN
#define NormalState    XCB_ICCCM_WM_STATE_NORMAL
#define IconicState    XCB_ICCCM_WM_STATE_ICONIC
#endif

/*
 * -----------------------------------------------------------------------
 * Size hint flags (from <X11/Xutil.h>)
 * Mapped to XCB ICCCM equivalents from <xcb/xcb_icccm.h>
 * -----------------------------------------------------------------------
 */
#ifndef USPosition
#define USPosition  XCB_ICCCM_SIZE_HINT_US_POSITION
#define USSize      XCB_ICCCM_SIZE_HINT_US_SIZE
#define PPosition   XCB_ICCCM_SIZE_HINT_P_POSITION
#define PSize       XCB_ICCCM_SIZE_HINT_P_SIZE
#define PMinSize    XCB_ICCCM_SIZE_HINT_P_MIN_SIZE
#define PMaxSize    XCB_ICCCM_SIZE_HINT_P_MAX_SIZE
#define PResizeInc  XCB_ICCCM_SIZE_HINT_P_RESIZE_INC
#define PAspect     XCB_ICCCM_SIZE_HINT_P_ASPECT
#define PBaseSize   XCB_ICCCM_SIZE_HINT_BASE_SIZE
#define PWinGravity XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY
#endif

/*
 * -----------------------------------------------------------------------
 * Geometry parse flags (from <X11/Xutil.h>)
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

/* KeySym: X11 key symbol type.
 * Xlib: typedef XID KeySym (unsigned long).
 * XCB:  xcb_keysym_t (uint32_t).
 * We use xcb_keysym_t to match XCB. */
#ifndef KeySym
typedef xcb_keysym_t KeySym;
#endif

/* NoSymbol: null/invalid KeySym */
#ifndef NoSymbol
#define NoSymbol 0L
#endif

/* XStringToKeysym: Xlib function converting keysym name to KeySym.
 * XCB/xkbcommon equivalent: xkb_keysym_from_name(name, 0).
 * Returns NoSymbol (XKB_KEY_NoSymbol) if not found. */
#ifndef XStringToKeysym
#define XStringToKeysym(str) \
    ((KeySym) xkb_keysym_from_name((str), XKB_KEYSYM_NO_FLAGS))
#endif

/* XKeysymToString: Xlib function converting KeySym to name string.
 * xkbcommon equivalent: xkb_keysym_get_name() writes to a buffer.
 * This wrapper uses a static buffer — not thread-safe, matches Xlib semantics. */
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
 * XID-based resource types (from <X11/X.h>)
 * All are uint32_t XIDs in XCB.
 * -----------------------------------------------------------------------
 */

#ifndef Cursor
typedef xcb_cursor_t    Cursor;
#endif

#ifndef Font
typedef xcb_font_t      Font;
#endif

#ifndef Drawable
typedef xcb_drawable_t  Drawable;
#endif

#ifndef Pixmap
typedef xcb_pixmap_t    Pixmap;
#endif

#ifndef Colormap
typedef xcb_colormap_t  Colormap;
#endif

/*
 * Visual: Xlib's opaque Visual struct; XCB equivalent is xcb_visualtype_t.
 * Used as a pointer (Visual*) in IntrinsicP.h.
 */
#ifndef Visual
typedef xcb_visualtype_t Visual;
#endif

/* XSetWindowAttributes: Xlib struct for XChangeWindowAttributes.
 * In XCB, xcb_change_window_attributes takes a uint32_t value list.
 * This struct provides backward-compatible field access. */
#ifndef _XSetWindowAttributes_defined
#define _XSetWindowAttributes_defined
typedef struct {
    Pixmap        background_pixmap;
    unsigned long background_pixel;
    Pixmap        border_pixmap;
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
    Colormap      colormap;
    Cursor        cursor;
} XSetWindowAttributes;
#endif

/*
 * -----------------------------------------------------------------------
 * X11 protocol constants (from <X11/X.h>)
 * -----------------------------------------------------------------------
 */

/* Null resource / no value */
#ifndef None
#define None 0L
#endif

/* CopyFromParent: inherit attribute from parent window */
#ifndef CopyFromParent
#define CopyFromParent 0L
#endif

/* Window class constants */
#ifndef InputOutput
#define InputOutput 1
#define InputOnly   2
#endif

/* Event mask bits */
#ifndef NoEventMask
#define NoEventMask             0L
#define KeyPressMask            (1L<<0)
#define KeyReleaseMask          (1L<<1)
#define ButtonPressMask         (1L<<2)
#define ButtonReleaseMask       (1L<<3)
#define EnterWindowMask         (1L<<4)
#define LeaveWindowMask         (1L<<5)
#define PointerMotionMask       (1L<<6)
#define PointerMotionHintMask   (1L<<7)
#define Button1MotionMask       (1L<<8)
#define Button2MotionMask       (1L<<9)
#define Button3MotionMask       (1L<<10)
#define Button4MotionMask       (1L<<11)
#define Button5MotionMask       (1L<<12)
#define ButtonMotionMask        (1L<<13)
#define KeymapStateMask         (1L<<14)
#define ExposureMask            (1L<<15)
#define VisibilityChangeMask    (1L<<16)
#define StructureNotifyMask     (1L<<17)
#define ResizeRedirectMask      (1L<<18)
#define SubstructureNotifyMask  (1L<<19)
#define SubstructureRedirectMask (1L<<20)
#define FocusChangeMask         (1L<<21)
#define PropertyChangeMask      (1L<<22)
#define ColormapChangeMask      (1L<<23)
#define OwnerGrabButtonMask     (1L<<24)
#endif

/*
 * X11 event type constants (from <X11/X.h>)
 * These are the response_type values in XCB events.
 */
#ifndef KeyPress
#define KeyPress         2
#define KeyRelease       3
#define ButtonPress      4
#define ButtonRelease    5
#define MotionNotify     6
#define EnterNotify      7
#define LeaveNotify      8
#define FocusIn          9
#define FocusOut         10
#define KeymapNotify     11
#define Expose           12
#define GraphicsExpose   13
#define NoExpose         14
#define VisibilityNotify 15
#define CreateNotify     16
#define DestroyNotify    17
#define UnmapNotify      18
#define MapNotify        19
#define MapRequest       20
#define ReparentNotify   21
#define ConfigureNotify  22
#define ConfigureRequest 23
#define GravityNotify    24
#define ResizeRequest    25
#define CirculateNotify  26
#define CirculateRequest 27
#define PropertyNotify   28
#define SelectionClear   29
#define SelectionRequest 30
#define SelectionNotify  31
#define ColormapNotify   32
#define ClientMessage    33
#define MappingNotify    34
#define GenericEvent     35
#define LASTEvent        36
#endif

/* Predefined atoms */
#ifndef XA_PRIMARY
#define XA_PRIMARY              ((Atom) 1)
#define XA_SECONDARY            ((Atom) 2)
#define XA_ARC                  ((Atom) 3)
#define XA_ATOM                 ((Atom) 4)
#define XA_BITMAP               ((Atom) 5)
#define XA_CARDINAL             ((Atom) 6)
#define XA_COLORMAP             ((Atom) 7)
#define XA_CURSOR               ((Atom) 8)
#define XA_DRAWABLE             ((Atom) 17)
#define XA_FONT                 ((Atom) 18)
#define XA_INTEGER              ((Atom) 19)
#define XA_PIXMAP               ((Atom) 20)
#define XA_POINT                ((Atom) 21)
#define XA_RECTANGLE            ((Atom) 22)
#define XA_RESOURCE_MANAGER     ((Atom) 23)
#define XA_RGB_COLOR_MAP        ((Atom) 24)
#define XA_RGB_BEST_MAP         ((Atom) 25)
#define XA_RGB_BLUE_MAP         ((Atom) 26)
#define XA_RGB_DEFAULT_MAP      ((Atom) 27)
#define XA_RGB_GRAY_MAP         ((Atom) 28)
#define XA_RGB_GREEN_MAP        ((Atom) 29)
#define XA_RGB_RED_MAP          ((Atom) 30)
#define XA_STRING               ((Atom) 31)
#define XA_VISUALID             ((Atom) 32)
#define XA_WINDOW               ((Atom) 33)
#define XA_WM_COMMAND           ((Atom) 34)
#define XA_WM_HINTS             ((Atom) 35)
#define XA_WM_CLIENT_MACHINE    ((Atom) 36)
#define XA_WM_ICON_NAME         ((Atom) 37)
#define XA_WM_ICON_SIZE         ((Atom) 38)
#define XA_WM_NAME              ((Atom) 39)
#define XA_WM_NORMAL_HINTS      ((Atom) 40)
#define XA_WM_SIZE_HINTS        ((Atom) 41)
#define XA_WM_ZOOM_HINTS        ((Atom) 42)
#define XA_MIN_SPACE            ((Atom) 43)
#define XA_NORM_SPACE           ((Atom) 44)
#define XA_MAX_SPACE            ((Atom) 45)
#define XA_END_SPACE            ((Atom) 46)
#define XA_SUPERSCRIPT_X        ((Atom) 47)
#define XA_SUPERSCRIPT_Y        ((Atom) 48)
#define XA_SUBSCRIPT_X          ((Atom) 49)
#define XA_SUBSCRIPT_Y          ((Atom) 50)
#define XA_UNDERLINE_POSITION   ((Atom) 51)
#define XA_UNDERLINE_THICKNESS  ((Atom) 52)
#define XA_STRIKEOUT_ASCENT     ((Atom) 53)
#define XA_STRIKEOUT_DESCENT    ((Atom) 54)
#define XA_ITALIC_ANGLE         ((Atom) 55)
#define XA_X_HEIGHT             ((Atom) 56)
#define XA_QUAD_WIDTH           ((Atom) 57)
#define XA_WEIGHT               ((Atom) 58)
#define XA_POINT_SIZE           ((Atom) 59)
#define XA_RESOLUTION           ((Atom) 60)
#define XA_COPYRIGHT            ((Atom) 61)
#define XA_NOTICE               ((Atom) 62)
#define XA_FONT_NAME            ((Atom) 63)
#define XA_FAMILY_NAME          ((Atom) 64)
#define XA_FULL_NAME            ((Atom) 65)
#define XA_CAP_HEIGHT           ((Atom) 66)
#define XA_WM_CLASS             ((Atom) 67)
#define XA_WM_TRANSIENT_FOR     ((Atom) 68)
#define XA_LAST_PREDEFINED      ((Atom) 68)
#endif

/* AnyPropertyType */
#ifndef AnyPropertyType
#define AnyPropertyType 0L
#endif

/* GrabMode constants */
#ifndef GrabModeSync
#define GrabModeSync    0
#define GrabModeAsync   1
#endif

/* GrabStatus constants */
#ifndef GrabSuccess
#define GrabSuccess     0
#define AlreadyGrabbed  1
#define GrabInvalidTime 2
#define GrabNotViewable 3
#define GrabFrozen      4
#endif

/* AllTemporary */
#ifndef AllTemporary
#define AllTemporary 0L
#endif

/* CurrentTime */
#ifndef CurrentTime
#define CurrentTime 0L
#endif

/* AnyKey / AnyButton */
#ifndef AnyKey
#define AnyKey   0L
#define AnyButton 0L
#endif

/* AnyModifier */
#ifndef AnyModifier
#define AnyModifier (1<<15)
#endif

/* Button constants */
#ifndef Button1
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5
#endif

/* Modifier key masks */
#ifndef ShiftMask
#define ShiftMask   (1<<0)
#define LockMask    (1<<1)
#define ControlMask (1<<2)
#define Mod1Mask    (1<<3)
#define Mod2Mask    (1<<4)
#define Mod3Mask    (1<<5)
#define Mod4Mask    (1<<6)
#define Mod5Mask    (1<<7)
#endif

/* Button masks */
#ifndef Button1Mask
#define Button1Mask (1<<8)
#define Button2Mask (1<<9)
#define Button3Mask (1<<10)
#define Button4Mask (1<<11)
#define Button5Mask (1<<12)
#endif

/* Notify modes */
#ifndef NotifyNormal
#define NotifyNormal        0
#define NotifyGrab          1
#define NotifyUngrab        2
#define NotifyWhileGrabbed  3
#define NotifyHint          1
#endif

/* Notify detail */
#ifndef NotifyAncestor
#define NotifyAncestor          0
#define NotifyVirtual           1
#define NotifyInferior          2
#define NotifyNonlinear         3
#define NotifyNonlinearVirtual  4
#define NotifyPointer           5
#define NotifyPointerRoot       6
#define NotifyDetailNone        7
#endif

/* Visibility state */
#ifndef VisibilityUnobscured
#define VisibilityUnobscured        0
#define VisibilityPartiallyObscured 1
#define VisibilityFullyObscured     2
#endif

/* Circulation request */
#ifndef PlaceOnTop
#define PlaceOnTop    0
#define PlaceOnBottom 1
#endif

/* Property notification state */
#ifndef PropertyNewValue
#define PropertyNewValue 0
#define PropertyDelete   1
#endif

/* Colormap notification state */
#ifndef ColormapUninstalled
#define ColormapUninstalled 0
#define ColormapInstalled   1
#endif

/* Mapping request */
#ifndef MappingModifier
#define MappingModifier  0
#define MappingKeyboard  1
#define MappingPointer   2
#endif

/* Focus revert-to */
#ifndef RevertToNone
#define RevertToNone        0
#define RevertToPointerRoot 1
#define RevertToParent      2
#endif

/* PointerRoot */
#ifndef PointerRoot
#define PointerRoot 1L
#endif

/* InputFocus */
#ifndef InputFocus
#define InputFocus 1L
#endif

/* PointerWindow */
#ifndef PointerWindow
#define PointerWindow 0L
#endif

/* PropMode constants */
#ifndef PropModeReplace
#define PropModeReplace  0
#define PropModePrepend  1
#define PropModeAppend   2
#endif

/* Window stacking */
#ifndef Above
#define Above    0
#define Below    1
#define TopIf    2
#define BottomIf 3
#define Opposite 4
#endif

/* Gravity */
#ifndef ForgetGravity
#define ForgetGravity    0
#define NorthWestGravity 1
#define NorthGravity     2
#define NorthEastGravity 3
#define WestGravity      4
#define CenterGravity    5
#define EastGravity      6
#define SouthWestGravity 7
#define SouthGravity     8
#define SouthEastGravity 9
#define StaticGravity    10
#define UnmapGravity     0  /* same as ForgetGravity in X11 protocol */
#endif

/* Map state */
#ifndef IsUnmapped
#define IsUnmapped   0
#define IsUnviewable 1
#define IsViewable   2
#endif

/* SetWindowAttributes value mask bits */
#ifndef CWBackPixmap
#define CWBackPixmap       (1L<<0)
#define CWBackPixel        (1L<<1)
#define CWBorderPixmap     (1L<<2)
#define CWBorderPixel      (1L<<3)
#define CWBitGravity       (1L<<4)
#define CWWinGravity       (1L<<5)
#define CWBackingStore     (1L<<6)
#define CWBackingPlanes    (1L<<7)
#define CWBackingPixel     (1L<<8)
#define CWOverrideRedirect (1L<<9)
#define CWSaveUnder        (1L<<10)
#define CWEventMask        (1L<<11)
#define CWDontPropagate    (1L<<12)
#define CWColormap         (1L<<13)
#define CWCursor           (1L<<14)
#endif

/* ConfigureWindow value mask bits */
#ifndef CWX
#define CWX            (1<<0)
#define CWY            (1<<1)
#define CWWidth        (1<<2)
#define CWHeight       (1<<3)
#define CWBorderWidth  (1<<4)
#define CWSibling      (1<<5)
#define CWStackMode    (1<<6)
#endif

/* Backing store */
#ifndef NotUseful
#define NotUseful  0
#define WhenMapped 1
#define Always     2
#endif

/* AllPlanes */
#ifndef AllPlanes
#define AllPlanes ((unsigned long)~0L)
#endif

/* GC function constants */
#ifndef GXclear
#define GXclear        0x0
#define GXand          0x1
#define GXandReverse   0x2
#define GXcopy         0x3
#define GXandInverted  0x4
#define GXnoop         0x5
#define GXxor          0x6
#define GXor           0x7
#define GXnor          0x8
#define GXequiv        0x9
#define GXinvert       0xa
#define GXorReverse    0xb
#define GXcopyInverted 0xc
#define GXorInverted   0xd
#define GXnand         0xe
#define GXset          0xf
#endif

/* GC value mask bits */
#ifndef GCFunction
#define GCFunction          (1L<<0)
#define GCPlaneMask         (1L<<1)
#define GCForeground        (1L<<2)
#define GCBackground        (1L<<3)
#define GCLineWidth         (1L<<4)
#define GCLineStyle         (1L<<5)
#define GCCapStyle          (1L<<6)
#define GCJoinStyle         (1L<<7)
#define GCFillStyle         (1L<<8)
#define GCFillRule          (1L<<9)
#define GCTile              (1L<<10)
#define GCStipple           (1L<<11)
#define GCTileStipXOrigin   (1L<<12)
#define GCTileStipYOrigin   (1L<<13)
#define GCFont              (1L<<14)
#define GCSubwindowMode     (1L<<15)
#define GCGraphicsExposures (1L<<16)
#define GCClipXOrigin       (1L<<17)
#define GCClipYOrigin       (1L<<18)
#define GCClipMask          (1L<<19)
#define GCDashOffset        (1L<<20)
#define GCDashList          (1L<<21)
#define GCArcMode           (1L<<22)
#endif

/* Line style */
#ifndef LineSolid
#define LineSolid      0
#define LineOnOffDash  1
#define LineDoubleDash 2
#endif

/* Cap style */
#ifndef CapNotLast
#define CapNotLast   0
#define CapButt      1
#define CapRound     2
#define CapProjecting 3
#endif

/* Join style */
#ifndef JoinMiter
#define JoinMiter 0
#define JoinRound 1
#define JoinBevel 2
#endif

/* Fill style */
#ifndef FillSolid
#define FillSolid          0
#define FillTiled          1
#define FillStippled       2
#define FillOpaqueStippled 3
#endif

/* Fill rule */
#ifndef EvenOddRule
#define EvenOddRule 0
#define WindingRule 1
#endif

/* Subwindow mode */
#ifndef ClipByChildren
#define ClipByChildren   0
#define IncludeInferiors 1
#endif

/* Arc mode */
#ifndef ArcChord
#define ArcChord    0
#define ArcPieSlice 1
#endif

/* Coordinate mode */
#ifndef CoordModeOrigin
#define CoordModeOrigin   0
#define CoordModePrevious 1
#endif

/* Shape */
#ifndef Complex
#define Complex   0
#define Nonconvex 1
#define Convex    2
#endif

/* Image format */
#ifndef XYBitmap
#define XYBitmap 0
#define XYPixmap 1
#define ZPixmap  2
#endif

/* Byte order */
#ifndef LSBFirst
#define LSBFirst 0
#define MSBFirst 1
#endif

/*
 * -----------------------------------------------------------------------
 * Visual class constants
 * -----------------------------------------------------------------------
 */
#ifndef StaticGray
#define StaticGray    0
#define GrayScale     1
#define StaticColor   2
#define PseudoColor   3
#define TrueColor     4
#define DirectColor   5
#endif

/*
 * -----------------------------------------------------------------------
 * DoRed/DoGreen/DoBlue flags for XColor
 * -----------------------------------------------------------------------
 */
#ifndef DoRed
#define DoRed   (1<<0)
#define DoGreen (1<<1)
#define DoBlue  (1<<2)
#endif

/*
 * -----------------------------------------------------------------------
 * Replacement for XColor (was in <X11/Xutil.h>)
 * -----------------------------------------------------------------------
 */
typedef struct {
    unsigned long pixel;    /* pixel value */
    unsigned short red;     /* red component (0-65535) */
    unsigned short green;   /* green component (0-65535) */
    unsigned short blue;    /* blue component (0-65535) */
    char flags;             /* DoRed, DoGreen, DoBlue */
    char pad;
} XtColor;

/* Backward compat alias */
typedef XtColor XColor;

/*
 * -----------------------------------------------------------------------
 * Replacement for XVisualInfo (was in <X11/Xutil.h>)
 * -----------------------------------------------------------------------
 */
typedef struct {
    xcb_visualtype_t *visual;   /* pointer into screen's visual list */
    xcb_visualid_t    visualid;
    int               screen;
    int               depth;
    int               class;    /* visual class (StaticGray, TrueColor, etc.) */
    unsigned long     red_mask;
    unsigned long     green_mask;
    unsigned long     blue_mask;
    int               colormap_size;
    int               bits_per_rgb;
} XtVisualInfo;

/* Backward compat alias */
typedef XtVisualInfo XVisualInfo;

/*
 * -----------------------------------------------------------------------
 * Replacement for XFontStruct (was in <X11/Xutil.h>)
 * Minimal — only fields used in libXt.
 * -----------------------------------------------------------------------
 */
typedef struct _XtFontStruct {
    xcb_font_t      fid;        /* font resource ID */
    unsigned        direction;  /* hint about direction the font is painted */
    unsigned        min_char_or_byte2;
    unsigned        max_char_or_byte2;
    unsigned        min_byte1;
    unsigned        max_byte1;
    int             ascent;
    int             descent;
    char           *font_family; /* fontconfig family name for Cairo rendering */
} XtFontStruct;

/* Backward compat alias */
typedef XtFontStruct XFontStruct;

/*
 * -----------------------------------------------------------------------
 * XFontSet - no XCB equivalent; opaque stub
 * -----------------------------------------------------------------------
 */
typedef void *XtFontSet;
typedef XtFontSet XFontSet;

/*
 * -----------------------------------------------------------------------
 * Screen pixel helpers (from <X11/Xlib.h> macros)
 * xcb_screen_t has black_pixel and white_pixel fields directly.
 * -----------------------------------------------------------------------
 */
#ifndef BlackPixelOfScreen
#define BlackPixelOfScreen(s) ((s)->black_pixel)
#define WhitePixelOfScreen(s) ((s)->white_pixel)
#endif

/*
 * DisplayOfScreen: Xlib macro returning Display* from Screen*.
 * In the XCB port, xcb_screen_t has no back-pointer to the connection.
 * This macro is only valid where the caller already has 'dpy' in scope.
 * For FetchDisplayArg in Converters.c, the widget's display is used directly.
 * Define as a stub that returns NULL to satisfy the compiler; the actual
 * call site in FetchDisplayArg is fixed separately.
 */
#ifndef DisplayOfScreen
#define DisplayOfScreen(s) ((xcb_connection_t *) NULL)
#endif

/*
 * XrmString: Xlib typedef for resource string (char*).
 * Used in FetchLocaleArg in Converters.c.
 */
#ifndef XrmString
typedef char *XrmString;
#endif

#endif /* _XtTypes_h */
