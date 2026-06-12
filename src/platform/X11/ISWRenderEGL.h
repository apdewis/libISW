/*
 * ISWRenderEGL.h - Pure EGL / OpenGL ES rendering backend (NanoVG-based)
 *
 * Copyright (c) 2026 ISW Project
 *
 * A Cairo-free render backend: EGL context + per-widget GL framebuffers, with
 * NanoVG (vendored in src/nanovg/) as the vector-drawing core for paths, text,
 * gradients, images and built-in anti-aliasing.
 *
 * CRITICAL: Uses EGL, NOT GLX, to avoid any Xlib dependency.  The backend
 * resolves its xcb connection / window / visual through the platform ops, the
 * same way the Cairo-XCB backend does.
 */

#ifndef _ISWRenderEGL_h
#define _ISWRenderEGL_h

#include "ISWRenderOps.h"     /* neutral dispatcher structs (no xcb/cairo/gl) */

#ifdef HAVE_EGL

extern const ISWRenderOps  isw_render_egl_ops;
extern const IswSurfaceOps isw_surface_egl_ops;

/* True if a usable EGL display + GLES config (with stencil) can be obtained. */
Boolean ISWRenderGLAvailable(void);

/* Widget-keyed font measurement via NanoVG/FontStash; published on the platform
   render ops so the neutral ISWScaled* wrappers can forward to them when the EGL
   backend is active. */
int egl_scaled_text_width(Widget widget, IswFontStruct *font,
                          const char *text, int len);
int egl_scaled_font_height(Widget widget, IswFontStruct *font);
int egl_scaled_font_ascent(Widget widget, IswFontStruct *font);
int egl_scaled_font_cap_height(Widget widget, IswFontStruct *font);

#endif /* HAVE_EGL */

#endif /* _ISWRenderEGL_h */
