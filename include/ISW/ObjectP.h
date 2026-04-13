/***********************************************************

Copyright 1987, 1988, 1994, 1998  The Open Group

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


Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifndef _Isw_ObjectP_h_
#define _Isw_ObjectP_h_

#include <ISW/Object.h>

_XFUNCPROTOBEGIN

/**********************************************************
 * Object Instance Data Structures
 *
 **********************************************************/
/* these fields match CorePart and can not be changed */

typedef struct _ObjectPart {
    Widget          self;               /* pointer to widget itself          */
    WidgetClass     widget_class;       /* pointer to Widget's ClassRec      */
    Widget          parent;             /* parent widget                     */
    XrmName         xrm_name;           /* widget resource name quarkified   */
    Boolean         being_destroyed;    /* marked for destroy                */
    IswCallbackList  destroy_callbacks;  /* who to call when widget destroyed */
    IswPointer       constraints;        /* constraint record                 */
} ObjectPart;

typedef struct _ObjectRec {
    ObjectPart  object;
} ObjectRec;

/********************************************************
 * Object Class Data Structures
 *
 ********************************************************/
/* these fields match CoreClassPart and can not be changed */
/* ideally these structures would only contain the fields required;
   but because the CoreClassPart cannot be changed at this late date
   extraneous fields are necessary to make the field offsets match */

typedef struct _ObjectClassPart {

    WidgetClass     superclass;         /* pointer to superclass ClassRec   */
    String          class_name;         /* widget resource class name       */
    Cardinal        widget_size;        /* size in bytes of widget record   */
    IswProc          class_initialize;   /* class initialization proc        */
    IswWidgetClassProc class_part_initialize; /* dynamic initialization      */
    IswEnum          class_inited;       /* has class been initialized?      */
    IswInitProc      initialize;         /* initialize subclass fields       */
    IswArgsProc      initialize_hook;    /* notify that initialize called    */
    IswProc          obj1;		/* NULL                             */
    IswPointer       obj2;               /* NULL                             */
    Cardinal        obj3;               /* NULL                             */
    IswResourceList  resources;          /* resources for subclass fields    */
    Cardinal        num_resources;      /* number of entries in resources   */
    XrmClass        xrm_class;          /* resource class quarkified (placeholder to match CoreClassPart layout) */
    Boolean         obj4;               /* NULL                             */
    IswEnum          obj5;               /* NULL                             */
    Boolean         obj6;               /* NULL				    */
    Boolean         obj7;               /* NULL                             */
    IswWidgetProc    destroy;            /* free data for subclass pointers  */
    IswProc          obj8;               /* NULL                             */
    IswProc          obj9;               /* NULL			            */
    IswSetValuesFunc set_values;         /* set subclass resource values     */
    IswArgsFunc      set_values_hook;    /* notify that set_values called    */
    IswProc          obj10;              /* NULL                             */
    IswArgsProc      get_values_hook;    /* notify that get_values called    */
    IswProc          obj11;              /* NULL                             */
    IswVersionType   version;            /* version of intrinsics used       */
    IswPointer       callback_private;   /* list of callback offsets         */
    String          obj12;              /* NULL                             */
    IswProc          obj13;              /* NULL                             */
    IswProc          obj14;              /* NULL                             */
    IswPointer       extension;          /* pointer to extension record      */
}ObjectClassPart;

typedef struct {
    IswPointer next_extension;	/* 1st 4 required for all extension records */
    XrmQuark record_type;	/* NULLQUARK; when on ObjectClassPart */
    long version;		/* must be IswObjectExtensionVersion */
    Cardinal record_size;	/* sizeof(ObjectClassExtensionRec) */
    IswAllocateProc allocate;
    IswDeallocateProc deallocate;
} ObjectClassExtensionRec, *ObjectClassExtension;

typedef struct _ObjectClassRec {
    ObjectClassPart object_class;
} ObjectClassRec;

externalref ObjectClassRec objectClassRec;

_XFUNCPROTOEND

#define IswObjectExtensionVersion 1L
#define IswInheritAllocate ((IswAllocateProc) _IswInherit)
#define IswInheritDeallocate ((IswDeallocateProc) _IswInherit)

#endif /*_Isw_ObjectP_h_*/
