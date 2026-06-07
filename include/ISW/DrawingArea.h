/*
 * DrawingArea.h - Public definitions for DrawingArea widget
 *
 * A canvas widget that delegates all rendering to application callbacks.
 * The application receives an ISWRenderContext and can use either the
 * ISWRender API or obtain the raw cairo_t* for direct Cairo drawing.
 */

#ifndef _ISW_DrawingArea_h
#define _ISW_DrawingArea_h

#include <ISW/Simple.h>
#include <ISW/ISWRender.h>

/* Resources:

 Name             Class            RepType       Default Value
 ----             -----            -------       -------------
 exposeCallback   Callback         Callback      NULL
 resizeCallback   Callback         Callback      NULL
 inputCallback    Callback         Callback      NULL

*/

#ifndef IswNexposeCallback
#define IswNexposeCallback "exposeCallback"
#endif
#ifndef IswNresizeCallback
#define IswNresizeCallback "resizeCallback"
#endif
#ifndef IswNinputCallback
#define IswNinputCallback "inputCallback"
#endif

/*
 * Callback data passed to all DrawingArea callbacks.
 *
 * For expose callbacks: render_ctx is valid and bracketed by Begin/End.
 * For resize callbacks: render_ctx is NULL (invalidated; recreated on next expose).
 * For input callbacks:  render_ctx is available but not in Begin/End state;
 *                       the app may call ISWRenderBegin/End itself to draw.
 */
typedef struct {
    ISWRenderContext     *render_ctx;   /* rendering context */
    IswEvent            *event;        /* triggering event (NULL for resize) */
    IswWindow         window;       /* widget's window */
} ISWDrawingCallbackData;

extern WidgetClass drawingAreaWidgetClass;

typedef struct _DrawingAreaClassRec *DrawingAreaWidgetClass;
typedef struct _DrawingAreaRec     *DrawingAreaWidget;

#endif /* _ISW_DrawingArea_h */
