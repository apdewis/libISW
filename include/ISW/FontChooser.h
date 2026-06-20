/*
 * FontChooser.h - Public header for FontChooser widget
 *
 * A font selection widget with family list, size selector, and preview.
 */

#ifndef _ISW_IswFontChooser_h
#define _ISW_IswFontChooser_h

#define IswNfontFamily     "fontFamily"
#define IswCFontFamily     "FontFamily"
#define IswNfontSize       "fontSize"
#define IswCFontSize       "FontSize"
#define IswNfontWeight     "fontWeight"
#define IswCFontWeight     "FontWeight"
#define IswNfontSlant      "fontSlant"
#define IswCFontSlant      "FontSlant"
#define IswNpreviewText    "previewText"
#define IswCPreviewText    "PreviewText"
#define IswNfontChanged    "fontChanged"

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
