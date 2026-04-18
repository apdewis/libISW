/*
 * IswTrayIcon.c - System tray icon implementation
 *
 * Implements the freedesktop.org System Tray Protocol Specification
 * and the XEmbed Protocol for docking into system tray managers.
 *
 * The tray icon is a raw XCB window — not an Xt widget.  It is
 * registered into the Xt event dispatch table via IswRegisterDrawable
 * so that the normal IswAppMainLoop delivers its events.  Rendering
 * uses Cairo directly (the window is small and simple; routing through
 * ISWRender would require a widget, which we don't have).
 *
 * Copyright (c) 2026 ISW Project
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ISW/IswTrayIcon.h>
#include <ISW/IntrinsicP.h>
#include <ISW/CoreP.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <cairo.h>
#include <cairo-xcb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* System Tray / XEmbed protocol constants                            */
/* ------------------------------------------------------------------ */

#define SYSTEM_TRAY_REQUEST_DOCK   0
#define SYSTEM_TRAY_BEGIN_MESSAGE  1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

#define XEMBED_MAPPED              (1 << 0)

/* Default icon size if the tray doesn't specify one */
#define DEFAULT_ICON_SIZE          24

extern double _IswGetScaleFactor(xcb_connection_t *dpy);

/* Maximum number of click callbacks */
#define MAX_CLICK_CALLBACKS        8

/* ------------------------------------------------------------------ */
/* Internal state                                                     */
/* ------------------------------------------------------------------ */

typedef struct _IswTrayClickCB {
    IswTrayIconClickProc proc;
    IswPointer           closure;
} IswTrayClickCB;

struct _IswTrayIcon {
    /* Xt integration */
    Widget               shell;          /* owner shell (for connection, screen, event loop) */
    xcb_connection_t    *conn;
    xcb_screen_t        *screen;

    /* Tray window */
    xcb_window_t         window;
    unsigned int         width;
    unsigned int         height;

    /* Atoms */
    xcb_atom_t           atom_systray;           /* _NET_SYSTEM_TRAY_S{n} */
    xcb_atom_t           atom_systray_opcode;     /* _NET_SYSTEM_TRAY_OPCODE */
    xcb_atom_t           atom_xembed_info;        /* _XEMBED_INFO */
    xcb_atom_t           atom_xembed;             /* _XEMBED */
    xcb_atom_t           atom_manager;            /* MANAGER */
    xcb_atom_t           atom_visual;            /* _NET_SYSTEM_TRAY_VISUAL */

    /* Tray manager */
    xcb_window_t         manager_window;

    /* Rendering */
    cairo_surface_t     *surface;
    cairo_t             *cr;
    xcb_visualtype_t    *visual;
    uint8_t              depth;

    /* Icon data */
    unsigned char       *rgba_data;       /* owned copy, or NULL */
    unsigned int         rgba_w, rgba_h;
    xcb_pixmap_t         icon_pixmap;     /* XCB_NONE if using RGBA */
    xcb_pixmap_t         icon_mask;
    unsigned int         pixmap_w, pixmap_h;
    unsigned int         pixmap_depth;

    /* Menu */
    Widget               menu;

    /* Callbacks */
    IswTrayClickCB       click_cbs[MAX_CLICK_CALLBACKS];
    int                  num_click_cbs;

    /* Tooltip (stored for future use) */
    char                *tooltip;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Find the xcb_visualtype_t for a given visual ID on a screen */
static xcb_visualtype_t *
find_visual(xcb_screen_t *screen, xcb_visualid_t visual_id)
{
    xcb_depth_iterator_t depth_iter;

    for (depth_iter = xcb_screen_allowed_depths_iterator(screen);
         depth_iter.rem;
         xcb_depth_next(&depth_iter))
    {
        xcb_visualtype_iterator_t vis_iter;
        for (vis_iter = xcb_depth_visuals_iterator(depth_iter.data);
             vis_iter.rem;
             xcb_visualtype_next(&vis_iter))
        {
            if (vis_iter.data->visual_id == visual_id)
                return vis_iter.data;
        }
    }
    return NULL;
}

/* Intern a batch of atoms, pipelining the requests */
static void
intern_atoms(IswTrayIcon icon)
{
    int screen_num;
    char sel_name[64];
    xcb_intern_atom_cookie_t cookies[6];
    xcb_intern_atom_reply_t *reply;

    /* Determine screen number from the screen pointer.
     * Walk the setup's screen list to find the index. */
    screen_num = 0;
    {
        const xcb_setup_t *setup = xcb_get_setup(icon->conn);
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
        while (iter.rem) {
            if (iter.data == icon->screen)
                break;
            screen_num++;
            xcb_screen_next(&iter);
        }
    }

    snprintf(sel_name, sizeof(sel_name), "_NET_SYSTEM_TRAY_S%d", screen_num);

    /* Pipeline: send all requests, then collect replies */
    cookies[0] = xcb_intern_atom(icon->conn, 0, strlen(sel_name), sel_name);
    cookies[1] = xcb_intern_atom(icon->conn, 0, 23, "_NET_SYSTEM_TRAY_OPCODE");
    cookies[2] = xcb_intern_atom(icon->conn, 0, 12, "_XEMBED_INFO");
    cookies[3] = xcb_intern_atom(icon->conn, 0, 7,  "_XEMBED");
    cookies[4] = xcb_intern_atom(icon->conn, 0, 7,  "MANAGER");
    cookies[5] = xcb_intern_atom(icon->conn, 0, 24, "_NET_SYSTEM_TRAY_VISUAL");

    reply = xcb_intern_atom_reply(icon->conn, cookies[0], NULL);
    icon->atom_systray = reply ? reply->atom : XCB_NONE;
    free(reply);

    reply = xcb_intern_atom_reply(icon->conn, cookies[1], NULL);
    icon->atom_systray_opcode = reply ? reply->atom : XCB_NONE;
    free(reply);

    reply = xcb_intern_atom_reply(icon->conn, cookies[2], NULL);
    icon->atom_xembed_info = reply ? reply->atom : XCB_NONE;
    free(reply);

    reply = xcb_intern_atom_reply(icon->conn, cookies[3], NULL);
    icon->atom_xembed = reply ? reply->atom : XCB_NONE;
    free(reply);

    reply = xcb_intern_atom_reply(icon->conn, cookies[4], NULL);
    icon->atom_manager = reply ? reply->atom : XCB_NONE;
    free(reply);

    reply = xcb_intern_atom_reply(icon->conn, cookies[5], NULL);
    icon->atom_visual = reply ? reply->atom : XCB_NONE;
    free(reply);
}

/* Find the depth for a given visual ID on a screen */
static uint8_t
find_visual_depth(xcb_screen_t *screen, xcb_visualid_t visual_id)
{
    xcb_depth_iterator_t depth_iter;

    for (depth_iter = xcb_screen_allowed_depths_iterator(screen);
         depth_iter.rem;
         xcb_depth_next(&depth_iter))
    {
        xcb_visualtype_iterator_t vis_iter;
        for (vis_iter = xcb_depth_visuals_iterator(depth_iter.data);
             vis_iter.rem;
             xcb_visualtype_next(&vis_iter))
        {
            if (vis_iter.data->visual_id == visual_id)
                return depth_iter.data->depth;
        }
    }
    return 0;
}

/* Query _NET_SYSTEM_TRAY_VISUAL from the tray manager window */
static xcb_visualid_t
get_tray_visual(IswTrayIcon icon)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    xcb_visualid_t visual_id = XCB_NONE;

    if (icon->atom_visual == XCB_NONE || icon->manager_window == XCB_NONE)
        return XCB_NONE;

    cookie = xcb_get_property(icon->conn, 0, icon->manager_window,
                              icon->atom_visual, XCB_ATOM_VISUALID,
                              0, 1);
    reply = xcb_get_property_reply(icon->conn, cookie, NULL);
    if (reply) {
        if (reply->type == XCB_ATOM_VISUALID &&
            reply->format == 32 &&
            xcb_get_property_value_length(reply) >= 4)
        {
            visual_id = *(xcb_visualid_t *)xcb_get_property_value(reply);
        }
        free(reply);
    }
    return visual_id;
}

/* Find the current tray manager window */
static xcb_window_t
find_tray_manager(IswTrayIcon icon)
{
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;
    xcb_window_t owner = XCB_NONE;

    if (icon->atom_systray == XCB_NONE)
        return XCB_NONE;

    cookie = xcb_get_selection_owner(icon->conn, icon->atom_systray);
    reply = xcb_get_selection_owner_reply(icon->conn, cookie, NULL);
    if (reply) {
        owner = reply->owner;
        free(reply);
    }
    return owner;
}

/* Set _XEMBED_INFO property on the tray window */
static void
set_xembed_info(IswTrayIcon icon, int mapped)
{
    uint32_t info[2];
    info[0] = 0;                          /* XEMBED version */
    info[1] = mapped ? XEMBED_MAPPED : 0; /* flags */

    xcb_change_property(icon->conn, XCB_PROP_MODE_REPLACE,
                        icon->window, icon->atom_xembed_info,
                        icon->atom_xembed_info,
                        32, 2, info);
}

/* Send SYSTEM_TRAY_REQUEST_DOCK client message to the tray manager */
static void
send_dock_request(IswTrayIcon icon)
{
    xcb_client_message_event_t event;

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = icon->manager_window;
    event.type = icon->atom_systray_opcode;
    event.data.data32[0] = XCB_CURRENT_TIME;
    event.data.data32[1] = SYSTEM_TRAY_REQUEST_DOCK;
    event.data.data32[2] = icon->window;

    xcb_send_event(icon->conn, 0, icon->manager_window,
                   XCB_EVENT_MASK_NO_EVENT,
                   (const char *)&event);
    xcb_flush(icon->conn);
}

/* Create (or recreate) the Cairo surface for the tray window */
static void
create_surface(IswTrayIcon icon)
{
    if (icon->cr) {
        cairo_destroy(icon->cr);
        icon->cr = NULL;
    }
    if (icon->surface) {
        cairo_surface_destroy(icon->surface);
        icon->surface = NULL;
    }

    if (!icon->visual || icon->width == 0 || icon->height == 0)
        return;

    icon->surface = cairo_xcb_surface_create(
        icon->conn, icon->window, icon->visual,
        icon->width, icon->height);

    if (cairo_surface_status(icon->surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(icon->surface);
        icon->surface = NULL;
        return;
    }

    icon->cr = cairo_create(icon->surface);
    cairo_set_antialias(icon->cr, CAIRO_ANTIALIAS_GOOD);
    cairo_set_operator(icon->cr, CAIRO_OPERATOR_OVER);
}

/* Render the icon content into the tray window */
static void
paint_icon(IswTrayIcon icon)
{
    if (!icon->cr)
        return;

    if (icon->depth == 32) {
        /* 32-bit visual has alpha — clear to fully transparent */
        cairo_save(icon->cr);
        cairo_set_operator(icon->cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(icon->cr);
        cairo_restore(icon->cr);
    } else {
        /* Force the server to repaint the ParentRelative background
         * so we have a clean slate when called outside of expose. */
        xcb_clear_area(icon->conn, 0, icon->window, 0, 0,
                       icon->width, icon->height);
        cairo_surface_flush(cairo_get_target(icon->cr));
        xcb_flush(icon->conn);
    }

    if (icon->rgba_data) {
        /* Render RGBA image data */
        cairo_surface_t *img;
        int stride;

        stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, icon->rgba_w);

        /* Convert RGBA to Cairo's ARGB32 (premultiplied, native endian).
         * Cairo ARGB32 on little-endian is stored as BGRA in memory. */
        unsigned char *argb = (unsigned char *)malloc(stride * icon->rgba_h);
        if (!argb)
            return;

        for (unsigned int y = 0; y < icon->rgba_h; y++) {
            for (unsigned int x = 0; x < icon->rgba_w; x++) {
                unsigned int si = (y * icon->rgba_w + x) * 4;
                unsigned int di = y * stride + x * 4;
                unsigned char r = icon->rgba_data[si + 0];
                unsigned char g = icon->rgba_data[si + 1];
                unsigned char b = icon->rgba_data[si + 2];
                unsigned char a = icon->rgba_data[si + 3];

                /* Premultiply alpha */
                r = (r * a + 127) / 255;
                g = (g * a + 127) / 255;
                b = (b * a + 127) / 255;

                /* Cairo ARGB32 native endian: pixel = (a << 24) | (r << 16) | (g << 8) | b */
                uint32_t pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                                 ((uint32_t)g << 8) | (uint32_t)b;
                memcpy(argb + di, &pixel, 4);
            }
        }

        img = cairo_image_surface_create_for_data(
            argb, CAIRO_FORMAT_ARGB32,
            icon->rgba_w, icon->rgba_h, stride);

        if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
            cairo_save(icon->cr);
            cairo_scale(icon->cr,
                        (double)icon->width / icon->rgba_w,
                        (double)icon->height / icon->rgba_h);
            cairo_set_source_surface(icon->cr, img, 0, 0);
            cairo_paint(icon->cr);
            cairo_restore(icon->cr);
        }

        cairo_surface_destroy(img);
        free(argb);
    } else if (icon->icon_pixmap != XCB_NONE) {
        /* Render from X pixmap via a temporary Cairo XCB surface */
        cairo_surface_t *pix_surf;
        pix_surf = cairo_xcb_surface_create(
            icon->conn, icon->icon_pixmap, icon->visual,
            icon->pixmap_w, icon->pixmap_h);

        if (cairo_surface_status(pix_surf) == CAIRO_STATUS_SUCCESS) {
            cairo_save(icon->cr);
            cairo_scale(icon->cr,
                        (double)icon->width / icon->pixmap_w,
                        (double)icon->height / icon->pixmap_h);
            cairo_set_source_surface(icon->cr, pix_surf, 0, 0);
            cairo_paint(icon->cr);
            cairo_restore(icon->cr);
        }
        cairo_surface_destroy(pix_surf);
    }

    cairo_surface_flush(icon->surface);
    xcb_flush(icon->conn);
}

/* ------------------------------------------------------------------ */
/* Window creation / visual query (factored for reuse on re-dock)     */
/* ------------------------------------------------------------------ */

/* Query the tray manager's preferred visual (or fall back to root)
 * and set icon->visual / icon->depth accordingly.  Returns True on
 * success, False if no usable visual could be found. */
static Boolean
query_tray_visual(IswTrayIcon icon)
{
    xcb_visualid_t tray_vis_id;
    uint8_t        depth;

    if (icon->manager_window != XCB_NONE) {
        tray_vis_id = get_tray_visual(icon);
        if (tray_vis_id != XCB_NONE) {
            icon->visual = find_visual(icon->screen, tray_vis_id);
            depth = find_visual_depth(icon->screen, tray_vis_id);
            if (icon->visual && depth) {
                icon->depth = depth;
                return True;
            }
        }
    }

    /* Fall back to root visual */
    icon->visual = find_visual(icon->screen, icon->screen->root_visual);
    icon->depth  = icon->screen->root_depth;
    return icon->visual != NULL;
}

/* Create (or recreate) the icon's XCB window, register it into Xt's
 * event dispatch, set _XEMBED_INFO, and create the Cairo surface.
 * Assumes icon->visual / icon->depth are already set.
 * Does NOT send a dock request. */
static Boolean
create_icon_window(IswTrayIcon icon)
{
    icon->width  = DEFAULT_ICON_SIZE;
    icon->height = DEFAULT_ICON_SIZE;

    icon->window = xcb_generate_id(icon->conn);

    if (icon->depth == 32) {
        xcb_colormap_t cmap = xcb_generate_id(icon->conn);
        xcb_create_colormap(icon->conn, XCB_COLORMAP_ALLOC_NONE,
                            cmap, icon->screen->root,
                            icon->visual->visual_id);

        uint32_t vals[4];
        uint32_t wmask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
                         XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
        vals[0] = 0;           /* back_pixel — transparent black */
        vals[1] = 0;           /* border_pixel */
        vals[2] = XCB_EVENT_MASK_EXPOSURE |
                  XCB_EVENT_MASK_BUTTON_PRESS |
                  XCB_EVENT_MASK_STRUCTURE_NOTIFY;
        vals[3] = cmap;

        xcb_create_window(icon->conn,
                          icon->depth,
                          icon->window,
                          icon->screen->root,
                          0, 0,
                          icon->width, icon->height,
                          0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          icon->visual->visual_id,
                          wmask, vals);
    } else {
        uint32_t mask = XCB_CW_BACK_PIXMAP | XCB_CW_EVENT_MASK;
        uint32_t values[2];
        values[0] = XCB_BACK_PIXMAP_PARENT_RELATIVE;
        values[1] = XCB_EVENT_MASK_EXPOSURE |
                    XCB_EVENT_MASK_BUTTON_PRESS |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY;

        xcb_create_window(icon->conn,
                          icon->depth,
                          icon->window,
                          icon->screen->root,
                          0, 0,
                          icon->width, icon->height,
                          0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          icon->visual->visual_id,
                          mask, values);
    }

    set_xembed_info(icon, 1);

    IswRegisterDrawable(icon->conn, icon->window, icon->shell);

    create_surface(icon);

    return True;
}

/* Dock into a (possibly new) tray manager.  If the window was
 * destroyed, recreate it first; otherwise just re-query the visual
 * and send the dock request. */
static void
dock_into_manager(IswTrayIcon icon)
{
    if (icon->window == XCB_NONE) {
        /* Window was destroyed — need to recreate everything */
        if (!query_tray_visual(icon))
            return;
        if (!create_icon_window(icon))
            return;
    } else {
        /* Window still exists (reparented back to root).
         * The new manager might want a different visual; if it changed
         * we have to recreate, otherwise just re-dock the existing window. */
        xcb_visualtype_t *old_visual = icon->visual;
        uint8_t           old_depth  = icon->depth;

        query_tray_visual(icon);

        if (icon->visual != old_visual || icon->depth != old_depth) {
            /* Visual changed — destroy and recreate */
            IswUnregisterDrawable(icon->conn, icon->window);
            xcb_destroy_window(icon->conn, icon->window);
            icon->window = XCB_NONE;

            if (icon->cr) {
                cairo_destroy(icon->cr);
                icon->cr = NULL;
            }
            if (icon->surface) {
                cairo_surface_destroy(icon->surface);
                icon->surface = NULL;
            }

            if (!create_icon_window(icon))
                return;
        }

        set_xembed_info(icon, 1);
    }

    send_dock_request(icon);
}

/* ------------------------------------------------------------------ */
/* Root event handler — watches for MANAGER announcements             */
/* ------------------------------------------------------------------ */

static void
root_event_handler(Widget widget, IswPointer closure,
                   xcb_generic_event_t *event,
                   Boolean *continue_to_dispatch)
{
    IswTrayIcon icon = (IswTrayIcon)closure;
    uint8_t type = event->response_type & 0x7f;

    (void)widget;
    (void)continue_to_dispatch;

    if (type != XCB_CLIENT_MESSAGE)
        return;

    xcb_client_message_event_t *e = (xcb_client_message_event_t *)event;

    /* MANAGER announcement: data32[1] is the selection atom */
    if (e->type != icon->atom_manager || e->format != 32)
        return;
    if (e->data.data32[1] != icon->atom_systray)
        return;

    /* A new tray manager appeared — data32[2] is its window */
    icon->manager_window = e->data.data32[2];
    if (icon->manager_window == XCB_NONE)
        return;

    dock_into_manager(icon);
    xcb_flush(icon->conn);
}

/* ------------------------------------------------------------------ */
/* Icon event handler — registered via IswAddRawEventHandler          */
/* ------------------------------------------------------------------ */

static void
tray_event_handler(Widget widget, IswPointer closure,
                   xcb_generic_event_t *event,
                   Boolean *continue_to_dispatch)
{
    IswTrayIcon icon = (IswTrayIcon)closure;
    uint8_t type = event->response_type & 0x7f;

    (void)widget;

    /* Filter: only handle events for our window */
    {
        xcb_window_t event_window = XCB_NONE;

        switch (type) {
        case XCB_EXPOSE: {
            xcb_expose_event_t *e = (xcb_expose_event_t *)event;
            event_window = e->window;
            break;
        }
        case XCB_BUTTON_PRESS: {
            xcb_button_press_event_t *e = (xcb_button_press_event_t *)event;
            event_window = e->event;
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t *e = (xcb_configure_notify_event_t *)event;
            event_window = e->window;
            break;
        }
        case XCB_DESTROY_NOTIFY: {
            xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)event;
            event_window = e->window;
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t *e = (xcb_client_message_event_t *)event;
            event_window = e->window;
            break;
        }
        case XCB_REPARENT_NOTIFY: {
            xcb_reparent_notify_event_t *e = (xcb_reparent_notify_event_t *)event;
            event_window = e->window;
            break;
        }
        default:
            return;
        }

        if (event_window != icon->window)
            return;
    }

    *continue_to_dispatch = False;

    switch (type) {
    case XCB_EXPOSE: {
        xcb_expose_event_t *e = (xcb_expose_event_t *)event;
        if (e->count == 0)
            paint_icon(icon);
        break;
    }

    case XCB_BUTTON_PRESS: {
        xcb_button_press_event_t *e = (xcb_button_press_event_t *)event;
        int button = e->detail;

        /* Right-click: show menu if attached */
        if (button == 3 && icon->menu) {
            /* Position menu near the tray icon.
             * Use root coordinates from the event. */
            IswPopup(icon->menu, IswGrabNonexclusive);
        }

        /* Invoke click callbacks */
        for (int i = 0; i < icon->num_click_cbs; i++) {
            icon->click_cbs[i].proc(icon, button, icon->click_cbs[i].closure);
        }
        break;
    }

    case XCB_CONFIGURE_NOTIFY: {
        xcb_configure_notify_event_t *e = (xcb_configure_notify_event_t *)event;
        /* The event dispatcher descales coordinates to logical pixels,
         * but our Cairo surface needs physical pixel dimensions. */
        double sf = _IswGetScaleFactor(icon->conn);
        uint16_t phys_w = (uint16_t)(e->width * sf + 0.5);
        uint16_t phys_h = (uint16_t)(e->height * sf + 0.5);
        if (phys_w != icon->width || phys_h != icon->height) {
            icon->width = phys_w;
            icon->height = phys_h;

            /* Recreate the Cairo surface at the new dimensions */
            create_surface(icon);

            paint_icon(icon);
        }
        break;
    }

    case XCB_DESTROY_NOTIFY:
        /* Tray manager destroyed our window — clean up rendering state.
         * The root_event_handler will re-dock when a new manager appears. */
        if (icon->cr) {
            cairo_destroy(icon->cr);
            icon->cr = NULL;
        }
        if (icon->surface) {
            cairo_surface_destroy(icon->surface);
            icon->surface = NULL;
        }
        IswUnregisterDrawable(icon->conn, icon->window);
        icon->window = XCB_NONE;
        icon->manager_window = XCB_NONE;
        break;

    case XCB_REPARENT_NOTIFY: {
        xcb_reparent_notify_event_t *e = (xcb_reparent_notify_event_t *)event;
        if (e->parent == icon->screen->root) {
            /* Reparented back to root — panel is cleaning up.
             * Enter "waiting for new manager" state: window is still
             * alive but we're no longer docked. */
            icon->manager_window = XCB_NONE;

            /* Destroy the Cairo surface — it may be stale after
             * reparent, and we'll recreate on next dock. */
            if (icon->cr) {
                cairo_destroy(icon->cr);
                icon->cr = NULL;
            }
            if (icon->surface) {
                cairo_surface_destroy(icon->surface);
                icon->surface = NULL;
            }
        }
        break;
    }

    case XCB_CLIENT_MESSAGE: {
        /* Could be _XEMBED messages from the tray manager */
        break;
    }

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

IswTrayIcon
IswTrayIconCreate(Widget shell, const char *tooltip)
{
    IswTrayIcon icon;

    if (!shell || !IswIsRealized(shell))
        return NULL;

    icon = (IswTrayIcon)calloc(1, sizeof(struct _IswTrayIcon));
    if (!icon)
        return NULL;

    icon->shell = shell;
    icon->conn = IswDisplay(shell);
    icon->screen = IswScreen(shell);
    icon->icon_pixmap = XCB_NONE;
    icon->icon_mask = XCB_NONE;

    if (tooltip)
        icon->tooltip = strdup(tooltip);

    /* Intern protocol atoms */
    intern_atoms(icon);

    /* Look for an existing tray manager */
    icon->manager_window = find_tray_manager(icon);

    /* Query visual (from manager if present, else root fallback) */
    if (!query_tray_visual(icon)) {
        fprintf(stderr, "IswTrayIcon: No usable visual\n");
        free(icon->tooltip);
        free(icon);
        return NULL;
    }

    /* Create the icon window */
    if (!create_icon_window(icon)) {
        free(icon->tooltip);
        free(icon);
        return NULL;
    }

    /* Add raw event handler on the shell — it receives events for all
     * drawables registered to it, including our tray window. */
    IswAddRawEventHandler(shell,
                          XCB_EVENT_MASK_EXPOSURE |
                          XCB_EVENT_MASK_BUTTON_PRESS |
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                          True,  /* nonmaskable — for client messages */
                          tray_event_handler,
                          (IswPointer)icon);

    /* Monitor root window for MANAGER announcements so we can
     * (re-)dock when a tray manager appears or restarts. */
    {
        uint32_t emask = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
        xcb_change_window_attributes(icon->conn, icon->screen->root,
                                     XCB_CW_EVENT_MASK, &emask);
    }
    IswRegisterDrawable(icon->conn, icon->screen->root, shell);
    IswAddRawEventHandler(shell,
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                          True,  /* nonmaskable — for MANAGER client messages */
                          root_event_handler,
                          (IswPointer)icon);

    /* If a manager is already running, dock now; otherwise wait. */
    if (icon->manager_window != XCB_NONE)
        send_dock_request(icon);

    xcb_flush(icon->conn);

    return icon;
}

void
IswTrayIconDestroy(IswTrayIcon icon)
{
    if (!icon)
        return;

    /* Remove event handlers */
    IswRemoveRawEventHandler(icon->shell,
                             XCB_EVENT_MASK_EXPOSURE |
                             XCB_EVENT_MASK_BUTTON_PRESS |
                             XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                             True,
                             tray_event_handler,
                             (IswPointer)icon);
    IswRemoveRawEventHandler(icon->shell,
                             XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                             True,
                             root_event_handler,
                             (IswPointer)icon);

    /* Clean up rendering */
    if (icon->cr)
        cairo_destroy(icon->cr);
    if (icon->surface)
        cairo_surface_destroy(icon->surface);

    /* Unregister and destroy the window */
    if (icon->window != XCB_NONE) {
        IswUnregisterDrawable(icon->conn, icon->window);
        xcb_destroy_window(icon->conn, icon->window);
        xcb_flush(icon->conn);
    }

    /* Free icon data */
    free(icon->rgba_data);
    free(icon->tooltip);
    free(icon);
}

void
IswTrayIconSetPixmap(IswTrayIcon icon, xcb_pixmap_t pixmap,
                     xcb_pixmap_t mask,
                     unsigned int width, unsigned int height,
                     unsigned int depth)
{
    if (!icon)
        return;

    /* Clear any RGBA data */
    free(icon->rgba_data);
    icon->rgba_data = NULL;

    icon->icon_pixmap = pixmap;
    icon->icon_mask = mask;
    icon->pixmap_w = width;
    icon->pixmap_h = height;
    icon->pixmap_depth = depth;

    paint_icon(icon);
}

void
IswTrayIconSetRGBA(IswTrayIcon icon, const unsigned char *rgba,
                   unsigned int width, unsigned int height)
{
    if (!icon || !rgba || width == 0 || height == 0)
        return;

    /* Replace existing data */
    free(icon->rgba_data);
    icon->rgba_data = (unsigned char *)malloc(width * height * 4);
    if (!icon->rgba_data)
        return;

    memcpy(icon->rgba_data, rgba, width * height * 4);
    icon->rgba_w = width;
    icon->rgba_h = height;

    /* Clear pixmap source */
    icon->icon_pixmap = XCB_NONE;
    icon->icon_mask = XCB_NONE;

    paint_icon(icon);
}

void
IswTrayIconSetMenu(IswTrayIcon icon, Widget menu)
{
    if (!icon)
        return;
    icon->menu = menu;
}

void
IswTrayIconAddClickCallback(IswTrayIcon icon,
                            IswTrayIconClickProc proc,
                            IswPointer closure)
{
    if (!icon || !proc)
        return;
    if (icon->num_click_cbs >= MAX_CLICK_CALLBACKS) {
        fprintf(stderr, "IswTrayIcon: Maximum click callbacks (%d) reached\n",
                MAX_CLICK_CALLBACKS);
        return;
    }

    icon->click_cbs[icon->num_click_cbs].proc = proc;
    icon->click_cbs[icon->num_click_cbs].closure = closure;
    icon->num_click_cbs++;
}

xcb_window_t
IswTrayIconGetWindow(IswTrayIcon icon)
{
    if (!icon)
        return XCB_NONE;
    return icon->window;
}
