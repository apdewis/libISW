/*
 * IswTrayIcon.h - System tray icon API for ISW
 *
 * Implements the freedesktop.org System Tray Protocol Specification.
 * Creates a raw XCB window outside the Xt widget tree, docks it into
 * the system tray manager, and renders via ISWRender.  The Xt toolkit
 * is used only for the popup menu and event loop integration.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifndef _IswTrayIcon_h
#define _IswTrayIcon_h

#include <ISW/Intrinsic.h>
#include <xcb/xcb.h>

typedef struct _IswTrayIcon *IswTrayIcon;

/*
 * Click callback — invoked on button press events on the tray icon.
 *
 * Parameters:
 *   icon    - The tray icon handle
 *   button  - X button number (1=left, 2=middle, 3=right)
 *   closure - Client data passed to IswTrayIconAddClickCallback
 */
typedef void (*IswTrayIconClickProc)(
    IswTrayIcon     icon,
    int             button,
    IswPointer      closure
);

/*
 * IswTrayIconCreate - Create and dock a system tray icon
 *
 * Parameters:
 *   shell   - Any realized shell widget (provides connection and screen)
 *   tooltip - Tooltip text (may be NULL; stored but not yet displayed
 *             by this implementation)
 *
 * Returns: Tray icon handle, or NULL on allocation/visual failure.
 *          If no tray manager is currently running, the icon is still
 *          created and will auto-dock when a manager appears.
 *
 * Notes:
 *   - The shell must be realized before calling this function
 *   - Creates a small XCB window and sends SYSTEM_TRAY_REQUEST_DOCK
 *   - Sets _XEMBED_INFO for tray manager communication
 *   - Hooks into the Xt event loop via IswRegisterDrawable
 *   - Monitors root window for MANAGER announcements; automatically
 *     re-docks when a tray manager (re)starts
 */
IswTrayIcon IswTrayIconCreate(Widget shell, const char *tooltip);

/*
 * IswTrayIconDestroy - Destroy a tray icon and free resources
 *
 * Parameters:
 *   icon - Tray icon handle (safe to call with NULL)
 */
void IswTrayIconDestroy(IswTrayIcon icon);

/*
 * IswTrayIconSetPixmap - Set the icon from a pixmap
 *
 * Parameters:
 *   icon   - Tray icon handle
 *   pixmap - Source pixmap (copied; caller retains ownership)
 *   mask   - Clip mask pixmap (XCB_NONE for no mask)
 *   width  - Pixmap width
 *   height - Pixmap height
 *   depth  - Pixmap depth
 */
void IswTrayIconSetPixmap(IswTrayIcon icon, xcb_pixmap_t pixmap,
                          xcb_pixmap_t mask,
                          unsigned int width, unsigned int height,
                          unsigned int depth);

/*
 * IswTrayIconSetRGBA - Set the icon from an RGBA pixel buffer
 *
 * Parameters:
 *   icon   - Tray icon handle
 *   rgba   - RGBA pixel data (4 bytes per pixel, row-major)
 *   width  - Image width in pixels
 *   height - Image height in pixels
 *
 * Notes:
 *   - Data is copied internally; caller retains ownership
 *   - Alpha channel is respected when rendering with Cairo
 */
void IswTrayIconSetRGBA(IswTrayIcon icon, const unsigned char *rgba,
                        unsigned int width, unsigned int height);

/*
 * IswTrayIconSetMenu - Attach a popup menu to the tray icon
 *
 * Parameters:
 *   icon - Tray icon handle
 *   menu - A SimpleMenu widget (shown on right-click)
 *
 * Notes:
 *   - The menu widget must be a child of the same shell hierarchy
 *   - Pass NULL to detach any existing menu
 */
void IswTrayIconSetMenu(IswTrayIcon icon, Widget menu);

/*
 * IswTrayIconAddClickCallback - Register a click callback
 *
 * Parameters:
 *   icon    - Tray icon handle
 *   proc    - Callback function
 *   closure - Client data passed to proc
 */
void IswTrayIconAddClickCallback(IswTrayIcon icon,
                                 IswTrayIconClickProc proc,
                                 IswPointer closure);

/*
 * IswTrayIconGetWindow - Get the underlying XCB window
 *
 * Parameters:
 *   icon - Tray icon handle
 *
 * Returns: The tray icon's xcb_window_t, or XCB_NONE if icon is NULL
 */
xcb_window_t IswTrayIconGetWindow(IswTrayIcon icon);

#endif /* _IswTrayIcon_h */
