/*

Copyright 1985, 1986, 1987, 1988, 1989, 1998  The Open Group

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

*/

#ifndef _VarargsI_h_
#define _VarargsI_h_

#include <stdarg.h>

/* private routines */

_XFUNCPROTOBEGIN

extern void _IswCountVaList(
    va_list /*var*/, int* /*total_count*/, int* /*typed_count*/
);

extern void _IswVaToArgList(
   Widget /*widget*/, va_list /*var*/, int /*max_count*/, ArgList* /*args_return*/, Cardinal* /*num_args_return*/
);

extern void _IswVaToTypedArgList(
    va_list /*var*/, int /*count*/, IswTypedArgList* /*args_return*/, Cardinal* /*num_args_return*/
);

extern IswTypedArgList _IswVaCreateTypedArgList(
    va_list /*var*/, int /*count*/
);

extern void _IswFreeArgList(
    ArgList /*args*/, int /*total_count*/, int /*typed_count*/
);

extern void _IswGetApplicationResources(
    Widget /*w*/, IswPointer /*base*/, IswResourceList /*resources*/, Cardinal /*num_resources*/, ArgList /*args*/, Cardinal /*num_args*/, IswTypedArgList /*typed_args*/, Cardinal /*num_typed_args*/
);

extern void _IswGetSubresources(
    Widget /*w*/, IswPointer /*base*/, const char* /*name*/, const char* /*class*/, IswResourceList /*resources*/, Cardinal /*num_resources*/, ArgList /*args*/, Cardinal /*num_args*/, IswTypedArgList /*typed_args*/, Cardinal /*num_typed_args*/
);

_XFUNCPROTOEND

#endif /* _VarargsI_h_ */
