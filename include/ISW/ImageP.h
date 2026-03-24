/*
 * ImageP.h - Image widget private header
 *
 * Copyright (c) 2026 ISW Project
 */

#ifndef _ISW_ImageP_h
#define _ISW_ImageP_h

#include <ISW/Image.h>
#include <ISW/LabelP.h>

/* Class part — empty, no new class methods */
typedef struct { int empty; } ImageClassPart;

/* Full class record */
typedef struct _ImageClassRec {
    CoreClassPart	core_class;
    SimpleClassPart	simple_class;
    LabelClassPart	label_class;
    ImageClassPart	image_class;
} ImageClassRec;

extern ImageClassRec imageClassRec;

/* Instance part — empty, all state lives in LabelPart */
typedef struct { int empty; } ImagePart;

/* Full instance record */
typedef struct _ImageRec {
    CorePart	core;
    SimplePart	simple;
    LabelPart	label;
    ImagePart	image;
} ImageRec;

#endif /* _ISW_ImageP_h */
