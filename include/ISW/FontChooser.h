/*
 * FontChooser.h - Public header for FontChooser widget
 *
 * A font selection widget with family list, size selector, and preview.
 */

#ifndef _ISW_IswFontChooser_h
#define _ISW_IswFontChooser_h

#include <X11/Xfuncproto.h>

#define XtNfontFamily     "fontFamily"
#define XtCFontFamily     "FontFamily"
#define XtNfontSize       "fontSize"
#define XtCFontSize       "FontSize"
#define XtNfontWeight     "fontWeight"
#define XtCFontWeight     "FontWeight"
#define XtNfontSlant      "fontSlant"
#define XtCFontSlant      "FontSlant"
#define XtNpreviewText    "previewText"
#define XtCPreviewText    "PreviewText"
#define XtNfontChanged    "fontChanged"

extern WidgetClass fontChooserWidgetClass;

typedef struct _FontChooserClassRec *FontChooserWidgetClass;
typedef struct _FontChooserRec      *FontChooserWidget;

typedef struct {
    String family;
    int    size;
    int    weight;  /* FC_WEIGHT value (e.g. FC_WEIGHT_NORMAL, FC_WEIGHT_BOLD) */
    int    slant;   /* FC_SLANT value (e.g. FC_SLANT_ROMAN, FC_SLANT_ITALIC) */
} IswFontChooserCallbackData;

_XFUNCPROTOBEGIN

extern String IswFontChooserGetFamily(Widget w);
extern int    IswFontChooserGetSize(Widget w);
extern int    IswFontChooserGetWeight(Widget w);
extern int    IswFontChooserGetSlant(Widget w);

_XFUNCPROTOEND

#endif /* _ISW_IswFontChooser_h */
