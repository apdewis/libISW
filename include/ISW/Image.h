/*
 * Image.h - Image widget public header
 *
 * The Image widget extends Label to provide a widget specifically for
 * displaying SVG images. It inherits Label's svgData and svgFile
 * resources, and defaults to no text label.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifndef _ISW_Image_h
#define _ISW_Image_h

#include <ISW/Label.h>

/* Class record constants */
extern WidgetClass imageWidgetClass;

typedef struct _ImageClassRec *ImageWidgetClass;
typedef struct _ImageRec      *ImageWidget;

#endif /* _ISW_Image_h */
