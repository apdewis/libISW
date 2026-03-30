/*
 * Isw3dP.c
 *
 * Global functions that don't really belong anywhere else.
 */

/*********************************************************************
Copyright (C) 1992 Kaleb Keithley
Copyright (C) 2000, 2003 David J. Hawkey Jr.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and
that both that copyright notice and this permission notice appear in
supporting documentation, and that the names of the copyright holders
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>

#ifdef ISW_GRAY_BLKWHT_STIPPLES
/* ARGSUSED */
unsigned long
grayPixel(unsigned long p, xcb_connection_t *dpy, xcb_screen_t *scn)
{
    static XColor Gray =
    {
	0,		/* pixel */
	0, 0, 0,	/* red, green, blue */
        0,		/* flags */
        0		/* pad */
    };

    if (!Gray.pixel)
    {
	XColor exact;

	(void)XAllocNamedColor(dpy, DefaultColormapOfScreen(scn),
			       "gray", &Gray, &exact);  /* Blindflug */
    }

    return Gray.pixel;
}
#endif

