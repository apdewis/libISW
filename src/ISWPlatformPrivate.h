/*
 * ISWPlatformPrivate.h - Internal declarations for ISWPlatform backends
 *
 * Copyright (c) 2026 ISW Project
 *
 * Internal counterpart to ISW/ISWPlatform.h, mirroring the public/private
 * split used by ISWRender.h / ISWRenderPrivate.h.  Concrete backends and the
 * platform dispatcher include this; widget code includes only the public
 * header.  This is where each backend's native handle structs, its exported
 * ISWPlatformOps instance, and the active-backend selection live.
 *
 * SCAFFOLDING ONLY (Phase 0).  Declares the backend-extern hook and the
 * accessor for the active vtable; the XCB backend's concrete handle structs
 * and operation implementations are added per-phase (see
 * docs/ISWPLATFORM_PLAN.md).
 *
 * CRITICAL: XCB types are confined to the XCB backend translation unit.  They
 * MUST NOT leak into ISW/ISWPlatform.h.
 */

#ifndef _ISWPlatformPrivate_h
#define _ISWPlatformPrivate_h

#include "../include/ISW/ISWPlatform.h"

/*
 * =================================================================
 * Active platform backend
 * =================================================================
 *
 * The dispatcher resolves a single ISWPlatformOps for the process at startup
 * (XCB today).  _ISWPlatformGetOps returns it; widget-facing platform wrappers
 * dispatch through it.  Returns NULL before a backend is selected.
 */
const ISWPlatformOps *_ISWPlatformGetOps(void);

/*
 * =================================================================
 * XCB backend
 * =================================================================
 *
 * The pure-XCB platform backend.  Its exported vtable is wired up as
 * categories are abstracted; the sub-vtable members of this struct are filled
 * in their respective phases.
 */
extern const ISWPlatformOps isw_platform_xcb_ops;

#endif /* _ISWPlatformPrivate_h */
