lib_LTLIBRARIES = libISW.la

AM_YFLAGS = -d -p LayYY

AM_CFLAGS = \
	$(CWARNFLAGS) \
	$(XT_DEPS_CFLAGS) \
	$(ISW_DEPS_CFLAGS) \
	$(XPM_CFLAGS) \
	$(XKBCOMMON_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(FREETYPE_CFLAGS) \
	$(EGL_CFLAGS) \
	$(CAIRO_GL_CFLAGS) \
	$(SRCH_DEFINES) \
	$(ERRORDB_DEFINES)

# Header include order: local includes first, then generated
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/X11 \
	-I$(top_builddir)/include/X11 \
	$(ISW_CPPFLAGS) \
	-DLIBXT_COMPILATION

# LibXt source files (from X Toolkit Intrinsics)
LIBXT_SOURCES = \
	ActionHook.c \
	Alloc.c \
	ArgList.c \
	Callback.c \
	ClickTime.c \
	Composite.c \
	Constraint.c \
	Context.c \
	Convert.c \
	Converters.c \
	Core.c \
	Create.c \
	Destroy.c \
	Display.c \
	Error.c \
	Event.c \
	EventUtil.c \
	Functions.c \
	GCManager.c \
	Geometry.c \
	GetActKey.c \
	GetResList.c \
	GetValues.c \
	HookObj.c \
	Hooks.c \
	Initialize.c \
	Intrinsic.c \
	Keyboard.c \
	Manage.c \
	NextEvent.c \
	Object.c \
	PassivGrab.c \
	Pointer.c \
	Popup.c \
	PopupCB.c \
	Quark.c \
	RectObj.c \
	ResConfig.c \
	Resources.c \
	Selection.c \
	SetSens.c \
	SetValues.c \
	SetWMCW.c \
	Shell.c \
	Threads.c \
	TMaction.c \
	TMgrab.c \
	TMkey.c \
	TMparse.c \
	TMprint.c \
	TMstate.c \
	VarCreate.c \
	VarGet.c \
	Varargs.c

# Keep ISW_INTERNATIONALIZATION conditional as-is
if ISW_INTERNATIONALIZATION
I18N_SRCS = MultiSrc.c MultiSink.c ISWIm.c ISWI18n.c ISWI18n.h
endif

# Cairo rendering backend sources
# Cairo-XCB is the mandatory default backend
RENDER_CORE_SRCS = \
	ISWRender.c \
	ISWRenderPrivate.h \
	ISWRenderCairoXCB.c

# Cairo-EGL backend (when EGL and Cairo-GL are available)
if HAVE_CAIRO_EGL
RENDER_EGL_SRCS = ISWRenderEGL.c
endif

RENDER_SRCS = $(RENDER_CORE_SRCS) $(RENDER_EGL_SRCS)

# SVG support (nanosvg + ISW wrapper)
SVG_SRCS = \
	nanosvg_impl.c \
	nanosvg.h \
	nanosvgrast.h \
	ISWSVG.c

# ISW widget source files
ISW_SOURCES = \
	AllWidgets.c \
	AsciiSink.c \
	AsciiSrc.c \
	AsciiText.c \
	Box.c \
	ColorPicker.c \
	ComboBox.c \
	Command.c \
	Dialog.c \
	FontChooser.c \
	Form.c \
	Grip.c \
	IconView.c \
	Label.c \
	Layout.c \
	LayYY.h \
	List.c \
	MainWindow.c \
	MenuBar.c \
	MenuButton.c \
	Paned.c \
	Panner.c \
	Porthole.c \
	ProgressBar.c \
	Repeater.c \
	Scale.c \
	Scrollbar.c \
	ScrollWheel.c \
	SpinBox.c \
	StatusBar.c \
	Simple.c \
	SimpleMenu.c \
	SmeBSB.c \
	Sme.c \
	SmeLine.c \
	StripChart.c \
	Tabs.c \
	TextAction.c \
	Text.c \
	TextPop.c \
	TextSink.c \
	TextSrc.c \
	TextTr.c \
	Tip.c \
	Toolbar.c \
	Toggle.c \
	Tree.c \
	Vendor.c \
	Viewport.c \
	ISWP.c \
	ISWAtoms.c \
	ISWContext.c \
	ISWDrawing.c \
	ISWInit.c \
	ISWXcbDraw.c \
	ISWXcbDraw.h \
	Image.c \
	laygram.y \
	laylex.l \
	$(I18N_SRCS) \
	$(RENDER_SRCS) \
	$(SVG_SRCS)

# Generated source files
nodist_libISW_la_SOURCES = StringDefs.c

# All sources together
libISW_la_SOURCES = \
	$(LIBXT_SOURCES) \
	$(ISW_SOURCES)

libISW_la_LIBADD = \
	$(XT_DEPS_LIBS) \
	$(ISW_DEPS_LIBS) \
	$(XPM_LIBS) \
	$(XKBCOMMON_LIBS) \
	$(CAIRO_LIBS) \
	$(FONTCONFIG_LIBS) \
	$(FREETYPE_LIBS) \
	$(EGL_LIBS) \
	$(CAIRO_GL_LIBS) \
	-lm

libISW_la_LDFLAGS = -version-info 1:0:0 -no-undefined

BUILT_SOURCES = StringDefs.c laygram.h
CLEANFILES = $(BUILT_SOURCES) StringDefs.h Shell.h

STRING_LIST = $(top_srcdir)/util/string.list

# Generate StringDefs.c and headers
StringDefs.c: $(top_builddir)/util/makestrs$(EXEEXT) $(STRING_LIST)
	$(AM_V_GEN)$(top_builddir)/util/makestrs -i $(top_srcdir) $(STRINGSABIOPTIONS) < $(STRING_LIST) > StringDefs.c
	$(AM_V_at)$(MKDIR_P) $(top_builddir)/include/X11
	$(AM_V_at)cp StringDefs.h $(top_builddir)/include/X11/StringDefs.h
	$(AM_V_at)cp Shell.h $(top_builddir)/include/X11/Shell.h
	$(AM_V_at)rm -f StringDefs.h Shell.h

$(top_builddir)/util/makestrs$(EXEEXT):
	$(AM_V_at)cd $(top_builddir)/util && $(MAKE) $(AM_MAKEFLAGS) makestrs$(EXEEXT)

dist_doc_DATA = README.ISW

EXTRA_DIST = sharedlib.c Template.c
