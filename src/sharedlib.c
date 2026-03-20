/*

Copyright (c) 1991, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

#if defined(SUNSHLIB) && !defined(SHAREDCODE)

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <ISW/AsciiSinkP.h>
#include <ISW/AsciiSrcP.h>
#include <ISW/AsciiTextP.h>
#ifdef ISW_INTERNATIONALIZATION
#include <ISW/MultiSinkP.h>
#include <ISW/MultiSrcP.h>
#endif
#include <ISW/BoxP.h>
#include <ISW/CommandP.h>
#include <ISW/DialogP.h>
#include <ISW/FormP.h>
#include <ISW/GripP.h>
#include <ISW/LabelP.h>
#include <ISW/ListP.h>
#include <ISW/MenuButtoP.h>
#include <ISW/PanedP.h>
#include <ISW/PannerP.h>
#include <ISW/PortholeP.h>
#include <ISW/RepeaterP.h>
#include <ISW/ScrollbarP.h>
#include <ISW/SimpleP.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeP.h>
#include <ISW/SmeThreeDP.h>
#include <ISW/SmeBSBP.h>
#include <ISW/SmeLineP.h>
#include <ISW/StripCharP.h>
#include <ISW/TextP.h>
#include <ISW/TextSinkP.h>
#include <ISW/TextSrcP.h>
#include <ISW/ThreeDP.h>
#include <ISW/TipP.h>
#include <ISW/ToggleP.h>
#include <ISW/TreeP.h>
#include <X11/VendorP.h>
#include <ISW/ViewportP.h>

extern AsciiSinkClassRec asciiSinkClassRec;
WidgetClass asciiSinkObjectClass = (WidgetClass)&asciiSinkClassRec;

extern AsciiSrcClassRec asciiSrcClassRec;
WidgetClass asciiSrcObjectClass = (WidgetClass)&asciiSrcClassRec;

extern AsciiTextClassRec asciiTextClassRec;
WidgetClass asciiTextWidgetClass = (WidgetClass)&asciiTextClassRec;

#ifdef ASCII_STRING
extern AsciiStringClassRec asciiStringClassRec;
WidgetClass asciiStringWidgetClass = (WidgetClass)&asciiStringClassRec;
#endif

#ifdef ASCII_DISK
extern AsciiDiskClassRec asciiDiskClassRec;
WidgetClass asciiDiskWidgetClass = (WidgetClass)&asciiDiskClassRec;
#endif

#ifdef ISW_INTERNATIONALIZATION
extern MultiSinkClassRec multiSinkClassRec;
WidgetClass multiSinkObjectClass = (WidgetClass)&multiSinkClassRec;
#endif

#ifdef ISW_INTERNATIONALIZATION
extern MultiSrcClassRec multiSrcClassRec;
WidgetClass multiSrcObjectClass = (WidgetClass)&multiSrcClassRec;
#endif

extern BoxClassRec boxClassRec;
WidgetClass boxWidgetClass = (WidgetClass)&boxClassRec;

extern CommandClassRec commandClassRec;
WidgetClass commandWidgetClass = (WidgetClass) &commandClassRec;

extern DialogClassRec dialogClassRec;
WidgetClass dialogWidgetClass = (WidgetClass)&dialogClassRec;

extern FormClassRec formClassRec;
WidgetClass formWidgetClass = (WidgetClass)&formClassRec;

extern GripClassRec gripClassRec;
WidgetClass gripWidgetClass = (WidgetClass) &gripClassRec;

extern LabelClassRec labelClassRec;
WidgetClass labelWidgetClass = (WidgetClass)&labelClassRec;

extern ListClassRec listClassRec;
WidgetClass listWidgetClass = (WidgetClass)&listClassRec;

extern MenuButtonClassRec menuButtonClassRec;
WidgetClass menuButtonWidgetClass = (WidgetClass) &menuButtonClassRec;

extern PanedClassRec panedClassRec;
WidgetClass panedWidgetClass = (WidgetClass) &panedClassRec;
WidgetClass vPanedWidgetClass = (WidgetClass) &panedClassRec;

extern PannerClassRec pannerClassRec;
WidgetClass pannerWidgetClass = (WidgetClass) &pannerClassRec;

extern PortholeClassRec portholeClassRec;
WidgetClass portholeWidgetClass = (WidgetClass) &portholeClassRec;

extern RepeaterClassRec repeaterClassRec;
WidgetClass repeaterWidgetClass = (WidgetClass) &repeaterClassRec;

extern ScrollbarClassRec scrollbarClassRec;
WidgetClass scrollbarWidgetClass = (WidgetClass)&scrollbarClassRec;

extern SimpleClassRec simpleClassRec;
WidgetClass simpleWidgetClass = (WidgetClass)&simpleClassRec;

extern SimpleMenuClassRec simpleMenuClassRec;
WidgetClass simpleMenuWidgetClass = (WidgetClass)&simpleMenuClassRec;

extern SmeClassRec smeClassRec;
WidgetClass smeObjectClass = (WidgetClass) &smeClassRec;

extern smeThreeDClassRec smeThreeDClassRec;
WidgetClass smeThreeDObjectClass = (WidgetClass) &smeThreeDClassRec;

WidgetClass smeBSBObjectClass = (WidgetClass) &smeBSBClassRec;

extern SmeLineClassRec smeLineClassRec;
WidgetClass smeLineObjectClass = (WidgetClass) &smeLineClassRec;

extern StripChartClassRec stripChartClassRec;
WidgetClass stripChartWidgetClass = (WidgetClass) &stripChartClassRec;

extern TextClassRec textClassRec;
WidgetClass textWidgetClass = (WidgetClass)&textClassRec;

unsigned long FMT8BIT = 0L;
unsigned long IswFmt8Bit = 0L;
#ifdef ISW_INTERNATIONALIZATION
unsigned long IswFmtWide = 0L;
#endif

extern TextSinkClassRec textSinkClassRec;
WidgetClass textSinkObjectClass = (WidgetClass)&textSinkClassRec;

extern TextSrcClassRec textSrcClassRec;
WidgetClass textSrcObjectClass = (WidgetClass)&textSrcClassRec;

extern ThreeDClassRec threeDClassRec;
WidgetClass threeDClass = (WidgetClass)&threeDClassRec;

extern TipClassRec tipClassRec;
WidgetClass tipWidgetClass = (WidgetClass)&tipClassRec;

extern ToggleClassRec toggleClassRec;
WidgetClass toggleWidgetClass = (WidgetClass) &toggleClassRec;

extern TreeClassRec treeClassRec;
WidgetClass treeWidgetClass = (WidgetClass) &treeClassRec;

extern VendorShellClassRec vendorShellClassRec;
WidgetClass vendorShellWidgetClass = (WidgetClass) &vendorShellClassRec;

extern ViewportClassRec viewportClassRec;
WidgetClass viewportWidgetClass = (WidgetClass)&viewportClassRec;

#endif /* SUNSHLIB */
