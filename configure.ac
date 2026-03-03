
# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([libXaw3d], [1.6.2],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], [libXaw3d])
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
# Use custom XCB-based libXt instead of system libXt
PKG_CHECK_MODULES(X11, [x11 xext xcb xcb-xfixes xcb-render xcb-shape])
PKG_CHECK_MODULES(XFT, [xft])

# Custom libXt paths - MUST come before system headers
CUSTOM_XT_CPPFLAGS="-I/home/adam/libxt/include"
CUSTOM_XT_LDFLAGS="-L/home/adam/libxt/src/.libs"

AC_CHECK_HEADERS([wctype.h wchar.h widec.h])
AC_CHECK_FUNCS([iswspace])
AC_TYPE_INTPTR_T

XAW3D_CPPFLAGS="${CUSTOM_XT_CPPFLAGS}"

AC_ARG_ENABLE([internationalization],
    AS_HELP_STRING([--enable-internationalization], dnl
    [enable utf8 strings (default: yes)]),
    [XAW_I18N=$enableval], [XAW_I18N=yes])
if test "x$XAW_I18N" = xyes; then
    XAW3D_CPPFLAGS="${XAW3D_CPPFLAGS} -DXAW_INTERNATIONALIZATION"
fi
AM_CONDITIONAL(XAW_INTERNATIONALIZATION, [test "x$XAW_I18N" = xyes])

AC_ARG_ENABLE([xim],
    AS_HELP_STRING([--enable-xim], dnl
    [enable X Input Method support for CJK input (default: no, requires Xlib XIM APIs not available in XCB)]),
    [XAW_XIM=$enableval], [XAW_XIM=no])
if test "x$XAW_XIM" = xyes; then
    XAW3D_CPPFLAGS="${XAW3D_CPPFLAGS} -DXAW_HAS_XIM"
fi

AC_ARG_ENABLE([multiplane-bitmaps],
    [AS_HELP_STRING([--enable-multiplane-bitmaps], dnl
     [enable XPM support])],
     [XAW3D_CPPFLAGS="${XAW3D_CPPFLAGS} -DXAW_MULTIPLANE_PIXMAPS" dnl
      PKG_CHECK_MODULES(XPM, xpm)])

AC_ARG_ENABLE([gray-stipples],
    [AS_HELP_STRING([--enable-gray-stipples], dnl
     [enable gray stipples])], [XAW3D_CPPFLAGS="${XAW3D_CPPFLAGS} -DXAW_GRAY_BLKWHT_STIPPLES"])

AC_ARG_ENABLE([arrow-scrollbars],
    [AS_HELP_STRING([--enable-arrow-scrollbars], dnl
     [enable arrow scrollbars])], [XAW3D_CPPFLAGS="${XAW3D_CPPFLAGS} -DXAW_ARROW_SCROLLBARS"])

AC_SUBST(XAW3D_CPPFLAGS)
AC_SUBST(CUSTOM_XT_CPPFLAGS)
AC_SUBST(CUSTOM_XT_LDFLAGS)

AC_CONFIG_FILES([Makefile
		include/Makefile
		src/Makefile
		xaw3d.pc])

AC_OUTPUT
