lib_LTLIBRARIES = libISW.la

AM_YFLAGS = -d -p LayYY

AM_CFLAGS =		\
	$(CWARNFLAGS)	\
	$(X11_CFLAGS)	\
	$(XFT_CFLAGS)	\
	$(XPM_CFLAGS)

# CRITICAL: Custom libXt MUST come FIRST to override system headers
AM_CPPFLAGS = \
	$(CUSTOM_XT_CPPFLAGS) \
	-I${top_srcdir}/include	\
	$(ISW_CPPFLAGS)

#
# This doesn't appear to be used on any
# current systems -- it requires SUNSHLIB and !SHAREDCODE,
# but only sunLib.rules defines SUNSHLIB and that file also
# always defines SHAREDCODE. Go figure
#
# SHAREDLIB_SOURCES = sharedlib.c

if ISW_INTERNATIONALIZATION
I18N_SRCS = MultiSrc.c MultiSink.c ISWIm.c ISWI18n.c ISWI18n.h
endif

libISW_la_SOURCES = \
	AllWidgets.c \
	AsciiSink.c \
	AsciiSrc.c \
	AsciiText.c \
	Box.c \
	Command.c \
	Dialog.c \
	Form.c \
	Grip.c \
	Label.c \
	Layout.c \
	LayYY.h \
	List.c \
	MenuButton.c \
	Paned.c \
	Panner.c \
	Porthole.c \
	Repeater.c \
	Scrollbar.c \
	Simple.c \
	SimpleMenu.c \
	SmeBSB.c \
	Sme.c \
	SmeLine.c \
	SmeThreeD.c \
	StripChart.c \
	TextAction.c \
	Text.c \
	TextPop.c \
	TextSink.c \
	TextSrc.c \
	TextTr.c \
	ThreeD.c \
	Tip.c \
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
	laygram.y \
	laylex.l \
	$(I18N_SRCS)

libISW_la_LIBADD = \
	$(CUSTOM_XT_LDFLAGS) -lXt \
	$(X11_LIBS) $(XFT_LIBS) $(XPM_LIBS)

libISW_la_LDFLAGS = -version-info 1:0:0 -no-undefined $(CUSTOM_XT_LDFLAGS)

BUILT_SOURCES = laygram.h

dist_doc_DATA = README.ISW

EXTRA_DIST = sharedlib.c Template.c
