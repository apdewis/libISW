package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/MainWindow.h>
*/
import "C"

// MainWindowMenuBar returns the menu bar widget of a MainWindow.
func MainWindowMenuBar(w Widget) Widget {
	return Widget{C.IswMainWindowGetMenuBar(w.c)}
}

// MainWindowStatusBar returns the status bar widget of a MainWindow.
func MainWindowStatusBar(w Widget) Widget {
	return Widget{C.IswMainWindowGetStatusBar(w.c)}
}
