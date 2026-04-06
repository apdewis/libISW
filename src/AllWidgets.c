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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <X11/IntrinsicP.h>
#include <ISW/AllWidgets.h>

#include <X11/Composite.h>
#include <X11/Constraint.h>
#include <X11/Core.h>
#include <X11/Object.h>
#include <X11/RectObj.h>
#include <X11/Shell.h>
#include <X11/Vendor.h>
#include <ISW/AsciiSink.h>
#include <ISW/AsciiText.h>
#include <ISW/Box.h>
#include <ISW/Dialog.h>
#include <ISW/DrawingArea.h>
#include <ISW/FontChooser.h>
#include <ISW/Form.h>
#include <ISW/Grip.h>
#include <ISW/Layout.h>
#include <ISW/ColorPicker.h>
#include <ISW/ComboBox.h>
#include <ISW/IconView.h>
#include <ISW/ListView.h>
#include <ISW/List.h>
#include <ISW/MenuBar.h>
#include <ISW/MenuButton.h>
#include <ISW/MultiSink.h>
#include <ISW/MultiSrc.h>
#include <ISW/Paned.h>
#include <ISW/Panner.h>
#include <ISW/Porthole.h>
#include <ISW/ProgressBar.h>
#include <ISW/Repeater.h>
#include <ISW/Scale.h>
#include <ISW/Scrollbar.h>
#include <ISW/SpinBox.h>
#include <ISW/SimpleMenu.h>
#include <ISW/Sme.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>
#include <ISW/StatusBar.h>
#include <ISW/Tabs.h>
#include <ISW/Toolbar.h>
#include <ISW/Toggle.h>
#include <ISW/Tree.h>
#include <ISW/Viewport.h>

IswWidgetNode IswWidgetArray[] = {
{ "applicationShell", &applicationShellWidgetClass },
{ "asciiSink", &asciiSinkObjectClass },
{ "asciiSrc", &asciiSrcObjectClass },
{ "asciiText", &asciiTextWidgetClass },
{ "box", &boxWidgetClass },
{ "colorPicker", &colorPickerWidgetClass },
{ "comboBox", &comboBoxWidgetClass },
{ "command", &commandWidgetClass },
{ "composite", &compositeWidgetClass },
{ "constraint", &constraintWidgetClass },
{ "core", &coreWidgetClass },
{ "dialog", &dialogWidgetClass },
{ "drawingArea", &drawingAreaWidgetClass },
{ "fontChooser", &fontChooserWidgetClass },
{ "form", &formWidgetClass },
{ "grip", &gripWidgetClass },
{ "iconView", &iconViewWidgetClass },
{ "label", &labelWidgetClass },
{ "layout", &layoutWidgetClass },
{ "listView", &listViewWidgetClass },
{ "list", &listWidgetClass },
{ "menuBar", &menuBarWidgetClass },
{ "menuButton", &menuButtonWidgetClass },
{ "multiSink", &multiSinkObjectClass },
{ "multiSrc", &multiSrcObjectClass },
{ "object", &objectClass },
{ "overrideShell", &overrideShellWidgetClass },
{ "paned", &panedWidgetClass },
{ "panner", &pannerWidgetClass },
{ "porthole", &portholeWidgetClass },
{ "progressBar", &progressBarWidgetClass },
{ "rect", &rectObjClass },
{ "repeater", &repeaterWidgetClass },
{ "scale", &scaleWidgetClass },
{ "scrollbar", &scrollbarWidgetClass },
{ "shell", &shellWidgetClass },
{ "simpleMenu", &simpleMenuWidgetClass },
{ "spinBox", &spinBoxWidgetClass },
{ "simple", &simpleWidgetClass },
{ "smeBSB", &smeBSBObjectClass },
{ "smeLine", &smeLineObjectClass },
{ "sme", &smeObjectClass },
{ "statusBar", &statusBarWidgetClass },
{ "tabs", &tabsWidgetClass },
{ "textSink", &textSinkObjectClass },
{ "textSrc", &textSrcObjectClass },
{ "text", &textWidgetClass },
{ "toolbar", &toolbarWidgetClass },
{ "toggle", &toggleWidgetClass },
{ "topLevelShell", &topLevelShellWidgetClass },
{ "transientShell", &transientShellWidgetClass },
{ "tree", &treeWidgetClass },
{ "vendorShell", &vendorShellWidgetClass },
{ "viewport", &viewportWidgetClass },
{ "wmShell", &wmShellWidgetClass },
};

int IswWidgetCount = XtNumber(IswWidgetArray);

