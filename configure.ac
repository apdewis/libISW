
# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([libISW], [1.0.0],
        [https://github.com/InfiSystems/libISW], [libISW])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Initialize libtool
AC_PROG_LIBTOOL

# Require xorg-macros minimum of 1.8 for XORG_DEFAULT_OPTIONS with XORG_INSTALL
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

# Set-up Lex and Yacc, generated code is distributed
AC_PROG_YACC
AC_PATH_PROG([YACC_INST], $YACC)
if test ! -f "$srcdir/src/laygram.c"; then
   if test -z "$YACC_INST"; then
      AC_MSG_ERROR([yacc not found - unable to compile laygram.y])
   fi
fi
AC_PROG_LEX

# Checks for header files.
# LibXt dependencies (now built internally)
PKG_CHECK_MODULES(XT_DEPS, [sm ice x11 xproto xcb xcb-xrm xcb-keysyms])
PKG_CHECK_MODULES(ISW_DEPS, [xext xcb xcb-xfixes xcb-render xcb-shape])
PKG_CHECK_MODULES(XFT, [xft])

# Cairo rendering (mandatory)
PKG_CHECK_MODULES([CAIRO], [cairo >= 1.12.0 cairo-xcb >= 1.12.0],
    [AC_DEFINE([HAVE_CAIRO], [1], [Have Cairo rendering support])
     AC_DEFINE([HAVE_CAIRO_XCB], [1], [Have Cairo-XCB backend])],
    [AC_MSG_ERROR([Cairo >= 1.12.0 and cairo-xcb are required for ISW rendering])])
HAVE_CAIRO=yes

# Cairo-EGL backend (optional, for hardware accelerated rendering)
PKG_CHECK_MODULES([EGL], [egl >= 1.4],
    [PKG_CHECK_MODULES([CAIRO_GL], [cairo-gl >= 1.12.0],
        [HAVE_CAIRO_EGL=yes
         AC_DEFINE([HAVE_CAIRO_EGL], [1], [Have Cairo-EGL backend for hardware acceleration])],
        [HAVE_CAIRO_EGL=no
         AC_MSG_WARN([cairo-gl not found; hardware accelerated rendering not available])])],
    [HAVE_CAIRO_EGL=no
     AC_MSG_WARN([EGL not found; hardware accelerated rendering not available])])
AM_CONDITIONAL([HAVE_CAIRO_EGL], [test "x$HAVE_CAIRO_EGL" = "xyes"])

# xkbcommon (optional, for keysym name functions)
PKG_CHECK_MODULES([XKBCOMMON], [xkbcommon],
    [AC_DEFINE([HAVE_XKBCOMMON], [1], [Have libxkbcommon])],
    [AC_MSG_WARN([libxkbcommon not found; keysym name functions will be stubs])])

AC_CHECK_HEADERS([wctype.h wchar.h widec.h])
AC_CHECK_FUNCS([iswspace])
AC_TYPE_INTPTR_T

# Enable const support for X Toolkit (_Xconst macro)
AC_DEFINE(_CONST_X_STRING, 1, [Define to 1 to use standard C const feature.])

# LibXt build configuration (now internal)
SRCH_DEFINES="-DXFILESEARCHPATHDEFAULT=\\\"${datadir}/X11/%L/%T/%N%C%S:${datadir}/X11/%T/%N%C%S\\\""
ERRORDB_DEFINES="-DERRORDB=\\\"${datadir}/X11/XtErrorDB\\\""

# String generation options (for makestrs utility)
STRINGSABIOPTIONS=""
case $host_os in
    solaris*)
        STRINGSABIOPTIONS="-intelabi -solarisabinames"
        ;;
    sco* | svr4*)
        case $host_cpu in
            i*86) STRINGSABIOPTIONS="-intelabi" ;;
               *) STRINGSABIOPTIONS="" ;;
        esac
        ;;
esac

ISW_CPPFLAGS=""

AC_ARG_ENABLE([internationalization],
    AS_HELP_STRING([--enable-internationalization], dnl
    [enable utf8 strings (default: yes)]),
    [ISW_I18N=$enableval], [ISW_I18N=yes])
if test "x$ISW_I18N" = xyes; then
    ISW_CPPFLAGS="${ISW_CPPFLAGS} -DISW_INTERNATIONALIZATION"
fi
AM_CONDITIONAL(ISW_INTERNATIONALIZATION, [test "x$ISW_I18N" = xyes])

AC_ARG_ENABLE([xim],
    AS_HELP_STRING([--enable-xim], dnl
    [enable X Input Method support for CJK input (default: no, requires Xlib XIM APIs not available in XCB)]),
    [ISW_XIM=$enableval], [ISW_XIM=no])
if test "x$ISW_XIM" = xyes; then
    ISW_CPPFLAGS="${ISW_CPPFLAGS} -DISW_HAS_XIM"
fi

AC_ARG_ENABLE([multiplane-bitmaps],
    [AS_HELP_STRING([--enable-multiplane-bitmaps], dnl
     [enable XPM support])],
     [ISW_CPPFLAGS="${ISW_CPPFLAGS} -DISW_MULTIPLANE_PIXMAPS" dnl
      PKG_CHECK_MODULES(XPM, xpm)])

AC_ARG_ENABLE([gray-stipples],
    [AS_HELP_STRING([--enable-gray-stipples], dnl
     [enable gray stipples])], [ISW_CPPFLAGS="${ISW_CPPFLAGS} -DISW_GRAY_BLKWHT_STIPPLES"])

dnl Arrow scrollbars are always enabled
ISW_CPPFLAGS="${ISW_CPPFLAGS} -DISW_ARROW_SCROLLBARS"

AC_SUBST(ISW_CPPFLAGS)
AC_SUBST(SRCH_DEFINES)
AC_SUBST(ERRORDB_DEFINES)
AC_SUBST(STRINGSABIOPTIONS)

AC_CONFIG_FILES([Makefile
		util/Makefile
		include/Makefile
		src/Makefile
		isw.pc])

AC_OUTPUT

# Configuration summary
echo ""
echo "=========================================="
echo "libISW Configuration Summary"
echo "=========================================="
echo "Internationalization:    $ISW_I18N"
echo "XIM support:             $ISW_XIM"
echo ""
echo "Rendering Backends:"
echo "  Cairo-XCB:             yes"
if test "x$HAVE_CAIRO_EGL" = "xyes"; then
    echo "  Cairo-EGL:             yes (hardware accelerated)"
else
    echo "  Cairo-EGL:             no"
fi
echo ""
echo "=========================================="
echo ""
