/* include/X11/XtFuncproto.h
 * Local replacement for <X11/Xfuncproto.h>
 * Provides C++ extern "C" guards and function attributes without pulling in Xlib headers.
 */

/***********************************************************

Copyright 1989, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

******************************************************************/

#ifndef _XtFuncproto_h
#define _XtFuncproto_h

/* C++ extern "C" guards */
#ifdef __cplusplus
#define _XFUNCPROTOBEGIN extern "C" {
#define _XFUNCPROTOEND }
#else
#define _XFUNCPROTOBEGIN
#define _XFUNCPROTOEND
#endif

/* _Xconst: const qualifier (can be disabled with -D_CONST_X_STRING) */
#ifndef _XCONST
# ifdef _CONST_X_STRING
#  define _Xconst const
# else
#  define _Xconst
# endif
#endif

/* Function attributes for GCC and compatible compilers */
#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 203)
# define _X_ATTRIBUTE_PRINTF(x,y) __attribute__((__format__(__printf__,x,y)))
#else
# define _X_ATTRIBUTE_PRINTF(x,y)
#endif

#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 205)
# define _X_NORETURN __attribute__((__noreturn__))
#else
# define _X_NORETURN
#endif

#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 203)
# define _X_DEPRECATED __attribute__((__deprecated__))
#else
# define _X_DEPRECATED
#endif

#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 205)
# define _X_UNUSED __attribute__((__unused__))
#else
# define _X_UNUSED
#endif

/* Sentinel attribute for variadic functions (NULL-terminated argument lists) */
#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 400)
# define _X_SENTINEL(x) __attribute__((__sentinel__(x)))
#else
# define _X_SENTINEL(x)
#endif

#endif /* _XtFuncproto_h */
