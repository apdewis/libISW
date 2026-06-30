package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Shell.h>
#include <ISW/Box.h>
#include <ISW/Form.h>
#include <ISW/Paned.h>
#include <ISW/Layout.h>
#include <ISW/FlexBox.h>
#include <ISW/Simple.h>
#include <ISW/Label.h>
#include <ISW/Command.h>
#include <ISW/Toggle.h>
#include <ISW/Slider.h>
#include <ISW/Scrollbar.h>
#include <ISW/Viewport.h>
#include <ISW/Porthole.h>
#include <ISW/Tabs.h>
#include <ISW/Repeater.h>
#include <ISW/Text.h>
#include <ISW/List.h>
#include <ISW/ListBox.h>
#include <ISW/ListBoxRow.h>
#include <ISW/ListView.h>
#include <ISW/IconView.h>
#include <ISW/Tree.h>
#include <ISW/SimpleMenu.h>
#include <ISW/MenuBar.h>
#include <ISW/MenuButton.h>
#include <ISW/SmeBSB.h>
#include <ISW/SmeLine.h>
#include <ISW/Sme.h>
#include <ISW/ComboBox.h>
#include <ISW/ColorPicker.h>
#include <ISW/SpinBox.h>
#include <ISW/FileChooser.h>
#include <ISW/FontChooser.h>
#include <ISW/ProgressBar.h>
#include <ISW/StatusBar.h>
#include <ISW/Toolbar.h>
#include <ISW/Tip.h>
#include <ISW/DrawingArea.h>
#include <ISW/Image.h>
#include <ISW/Grip.h>
#include <ISW/Panner.h>
#include <ISW/MainWindow.h>
#include <ISW/Dialog.h>
*/
import "C"

// Core widget classes.
var (
	CoreClass       = WidgetClass{C.coreWidgetClass}
	CompositeClass  = WidgetClass{C.compositeWidgetClass}
	ConstraintClass = WidgetClass{C.constraintWidgetClass}
)

// Shell classes.
var (
	ShellClass            = WidgetClass{C.shellWidgetClass}
	OverrideShellClass    = WidgetClass{C.overrideShellWidgetClass}
	WMShellClass          = WidgetClass{C.wmShellWidgetClass}
	TransientShellClass   = WidgetClass{C.transientShellWidgetClass}
	TopLevelShellClass    = WidgetClass{C.topLevelShellWidgetClass}
	ApplicationShellClass = WidgetClass{C.applicationShellWidgetClass}
)

// Layout widgets.
var (
	BoxClass     = WidgetClass{C.boxWidgetClass}
	FormClass    = WidgetClass{C.formWidgetClass}
	PanedClass   = WidgetClass{C.panedWidgetClass}
	LayoutClass  = WidgetClass{C.layoutWidgetClass}
	FlexBoxClass = WidgetClass{C.flexBoxWidgetClass}
)

// Simple and input widgets.
var (
	SimpleClass    = WidgetClass{C.simpleWidgetClass}
	LabelClass     = WidgetClass{C.labelWidgetClass}
	CommandClass   = WidgetClass{C.commandWidgetClass}
	ToggleClass    = WidgetClass{C.toggleWidgetClass}
	SliderClass    = WidgetClass{C.sliderWidgetClass}
	ScrollbarClass = WidgetClass{C.scrollbarWidgetClass}
)

// Container widgets.
var (
	ViewportClass = WidgetClass{C.viewportWidgetClass}
	PortholeClass = WidgetClass{C.portholeWidgetClass}
	TabsClass     = WidgetClass{C.tabsWidgetClass}
	RepeaterClass = WidgetClass{C.repeaterWidgetClass}
)

// Text widgets.
var (
	TextClass = WidgetClass{C.textWidgetClass}
)

// List/Table widgets.
var (
	ListClass       = WidgetClass{C.listWidgetClass}
	ListBoxClass    = WidgetClass{C.listBoxWidgetClass}
	ListBoxRowClass = WidgetClass{C.listBoxRowWidgetClass}
	ListViewClass   = WidgetClass{C.listViewWidgetClass}
	IconViewClass   = WidgetClass{C.iconViewWidgetClass}
	TreeClass       = WidgetClass{C.treeWidgetClass}
)

// Menu widgets.
var (
	SimpleMenuClass = WidgetClass{C.simpleMenuWidgetClass}
	MenuBarClass    = WidgetClass{C.menuBarWidgetClass}
	MenuButtonClass = WidgetClass{C.menuButtonWidgetClass}
	SmeObjectClass  = WidgetClass{C.smeObjectClass}
	SmeBSBClass     = WidgetClass{C.smeBSBObjectClass}
	SmeLineClass    = WidgetClass{C.smeLineObjectClass}
)

// Data entry widgets.
var (
	ComboBoxClass    = WidgetClass{C.comboBoxWidgetClass}
	ColorPickerClass = WidgetClass{C.colorPickerWidgetClass}
	SpinBoxClass     = WidgetClass{C.spinBoxWidgetClass}
	FileChooserClass = WidgetClass{C.fileChooserWidgetClass}
	FontChooserClass = WidgetClass{C.fontChooserWidgetClass}
)

// Display widgets.
var (
	ProgressBarClass = WidgetClass{C.progressBarWidgetClass}
	StatusBarClass   = WidgetClass{C.statusBarWidgetClass}
	ToolbarClass     = WidgetClass{C.toolbarWidgetClass}
	TipClass         = WidgetClass{C.tipWidgetClass}
	DrawingAreaClass = WidgetClass{C.drawingAreaWidgetClass}
	ImageClass       = WidgetClass{C.imageWidgetClass}
	GripClass        = WidgetClass{C.gripWidgetClass}
	PannerClass      = WidgetClass{C.pannerWidgetClass}
)

// Window widgets.
var (
	MainWindowClass = WidgetClass{C.mainWindowWidgetClass}
	DialogClass     = WidgetClass{C.dialogWidgetClass}
)

// Miscellaneous.
// TemplateClass is not exposed — header not installed.
