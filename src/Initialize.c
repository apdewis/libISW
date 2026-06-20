/***********************************************************
Copyright (c) 1993, Oracle and/or its affiliates.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

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

/*

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

*/

/* Make sure all wm properties can make it out of the resource manager */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include <ISW/ISWPlatform.h>
#include "StringDefs.h"
#include "CoreP.h"
#include "ShellP.h"
#include <stdio.h>
#include <X11/Xlocale.h>
#ifndef WIN32
#include <pwd.h>
#endif

#include <stdlib.h>
#include <ISW/IswArgMacros.h>
#include <ISW/ISWPlatform.h>


/*
 Default command-line option table applied before the application's list.
*/

/* *INDENT-OFF* */
static IswOptionDescRec const opTable[] = {
{"+rv",               "*reverseVideo",     IswOptionNoArg,   (IswPointer) "off"},
{"+synchronous",      "*synchronous",      IswOptionNoArg,   (IswPointer) "off"},
{"-background",       "*background",       IswOptionSepArg,  (IswPointer) NULL},
{"-bd",               "*borderColor",      IswOptionSepArg,  (IswPointer) NULL},
{"-bg",               "*background",       IswOptionSepArg,  (IswPointer) NULL},
{"-bordercolor",      "*borderColor",      IswOptionSepArg,  (IswPointer) NULL},
{"-borderwidth",      ".borderWidth",      IswOptionSepArg,  (IswPointer) NULL},
{"-bw",               ".borderWidth",      IswOptionSepArg,  (IswPointer) NULL},
{"-display",          ".display",          IswOptionSepArg,  (IswPointer) NULL},
{"-fg",               "*foreground",       IswOptionSepArg,  (IswPointer) NULL},
{"-fn",               "*font",             IswOptionSepArg,  (IswPointer) NULL},
{"-font",             "*font",             IswOptionSepArg,  (IswPointer) NULL},
{"-foreground",       "*foreground",       IswOptionSepArg,  (IswPointer) NULL},
{"-geometry",         ".geometry",         IswOptionSepArg,  (IswPointer) NULL},
{"-iconic",           ".iconic",           IswOptionNoArg,   (IswPointer) "on"},
{"-name",             ".name",             IswOptionSepArg,  (IswPointer) NULL},
{"-reverse",          "*reverseVideo",     IswOptionNoArg,   (IswPointer) "on"},
{"-rv",               "*reverseVideo",     IswOptionNoArg,   (IswPointer) "on"},
{"-selectionTimeout", ".selectionTimeout", IswOptionSepArg,  (IswPointer) NULL},
{"-synchronous",      "*synchronous",      IswOptionNoArg,   (IswPointer) "on"},
{"-title",            ".title",            IswOptionSepArg,  (IswPointer) NULL},
{"-xnllanguage",      ".xnlLanguage",      IswOptionSepArg,  (IswPointer) NULL},
{"-xrm",              NULL,                IswOptionResArg,  (IswPointer) NULL},
{"-xtsessionID",      ".sessionID",        IswOptionSepArg,  (IswPointer) NULL},
};
/* *INDENT-ON* */



#if defined (WIN32) || defined(__CYGWIN__)
/*
 * The Symbol _IswInherit is used in two different manners.
 * First it could be used as a generic function and second
 * as an absolute address reference, which will be used to
 * check the initialisation process of several other libraries.
 * Because of this the symbol must be accessible by all
 * client dll's and applications.  In unix environments
 * this is no problem, because the used shared libraries
 * format (elf) supports this immediately.  Under Windows
 * this isn't true, because a functions address in a dll
 * is different from the same function in another dll or
 * applications, because the used Portable Executable
 * File adds a code stub to each client to provide the
 * exported symbol name.  This stub uses an indirect
 * pointer to get the original symbol address, which is
 * then jumped to, like in this example:
 *
 * --- client ---                                     --- dll ----
 *  ...
 *  call foo
 *
 * foo: jmp (*_imp_foo)               ---->           foo: ....
 *      nop
 *      nop
 *
 * _imp_foo: .long <index of foo in dll export table, is
 *                  set to the real address by the runtime linker>
 *
 * Now it is clear why the clients symbol foo isn't the same
 * as in the dll and we can think about how to deal which
 * this two above mentioned requirements, to export this
 * symbol to all clients and to allow calling this symbol
 * as a function.  The solution I've used exports the
 * symbol _IswInherit as data symbol, because global data
 * symbols are exported to all clients.  But how to deal
 * with the second requirement, that this symbol should
 * be used as function.  The Trick is to build a little
 * code stub in the data section in the exact manner as
 * above explained.  This is done with the assembler code
 * below.
 *
 * Ralf Habacker
 *
 * References:
 * msdn          http://msdn.microsoft.com/msdnmag/issues/02/02/PE/PE.asp
 * cygwin-xfree: http://www.cygwin.com/ml/cygwin-xfree/2003-10/msg00000.html
 */

#ifdef __x86_64__
asm(".section .trampoline, \"dwx\" \n\
 .globl _IswInherit        \n\
 _IswInherit:              \n\
    jmp *_y(%rip)         \n\
_y: .quad __XtInherit     \n\
    .text                 \n");
#else
asm(".data\n\
 .globl __XtInherit        \n\
 __XtInherit:      jmp *_y \n\
  _y: .long ___XtInherit   \n\
    .text                 \n");
#endif

#define _IswInherit __XtInherit
#endif

void
_IswInherit(void)
{
    IswErrorMsg("invalidProcedure", "inheritanceProc", IswCIswToolkitError,
               "Unresolved inheritance operation", NULL, NULL);
}

void
IswToolkitInitialize(void)
{
    static Boolean initialized = False;

    LOCK_PROCESS;
    if (initialized) {
        UNLOCK_PROCESS;
        return;
    }
    initialized = True;
    UNLOCK_PROCESS;
    _IswResourceListInitialize();

    /* Other intrinsic initialization */

    _IswConvertInitialize();
    _IswEventInitialize();
    _IswTranslateInitialize();

    /* Some apps rely on old (broken) IswAppPeekEvent behavior */
    //if (getenv("XTAPPPEEKEVENT_SKIPTIMER"))
    //    IswAppPeekEvent_SkipTimer = True;
    //else
    //    IswAppPeekEvent_SkipTimer = False;
}

String
_IswGetUserName(_IswString dest, int len)
{
#ifdef WIN32
    String ptr = NULL;

    if ((ptr = getenv("USERNAME"))) {
        (void) strncpy(dest, ptr, len - 1);
        dest[len - 1] = '\0';
    }
    else
        *dest = '\0';
#else
    struct passwd *pw;
    char *ptr;

    if ((ptr = getenv("USER"))) {
        (void) strncpy(dest, ptr, (size_t) (len - 1));
        dest[len - 1] = '\0';
    }
    else {
        if ((pw = getpwuid(getuid())) != NULL) {
            (void) strncpy(dest, pw->pw_name, (size_t) (len - 1));
            dest[len - 1] = '\0';
        }
        else
            *dest = '\0';
    }
#endif
    return dest;
}

static IswDatabaseHandle
CopyDB(IswDatabaseHandle db)
{
    IswDatabaseHandle copy = NULL;
    if (db) {
        char *str = _IswPlatformResourceToString(db);
        if (str) {
            copy = _IswPlatformResourceFromString(str);
            free(str);
        }
    }
    return copy;
}

static String
_IswDefaultLanguageProc(IswDisplay dpy _X_UNUSED,
                       String xnl,
                       IswPointer closure _X_UNUSED)
{
    if (!setlocale(LC_ALL, xnl))
        IswWarning("locale not supported by C library, locale unchanged");

    /* XCB Note: Unlike Xlib, XCB doesn't provide XSupportsLocale() as locale
     * support is handled by the C library, not the X protocol. The setlocale()
     * call above already validates locale support.
     *
     * For XSetLocaleModifiers() functionality: XCB doesn't need X-specific
     * locale modifier setup at the protocol level. Input method (IM) support
     * and locale modifiers are handled by higher-level libraries. However,
     * we check XMODIFIERS for basic compatibility.
     */
    if (getenv("XMODIFIERS") == NULL) {
        /* If XMODIFIERS is not set, input methods may use their defaults */
        static int warned = 0;
        if (!warned) {
            IswWarning("XMODIFIERS environment variable not set, input methods may use defaults");
            warned = 1;
        }
    }

    return setlocale(LC_ALL, NULL);     /* re-query in case overwritten */
}

IswLanguageProc
IswSetLanguageProc(IswAppContext app, IswLanguageProc proc, IswPointer closure)
{
    IswLanguageProc old;

    if (!proc) {
        proc = _IswDefaultLanguageProc;
        closure = NULL;
    }

    if (app) {
        LOCK_APP(app);
        LOCK_PROCESS;
        /* set langProcRec only for this application context */
        old = app->langProcRec.proc;
        app->langProcRec.proc = proc;
        app->langProcRec.closure = closure;
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
    }
    else {
        /* set langProcRec for all application contexts */
        ProcessContext process;

        LOCK_PROCESS;
        process = _IswGetProcessContext();
        old = process->globalLangProcRec.proc;
        process->globalLangProcRec.proc = proc;
        process->globalLangProcRec.closure = closure;
        app = process->appContextList;
        while (app) {
            app->langProcRec.proc = proc;
            app->langProcRec.closure = closure;
            app = app->next;
        }
        UNLOCK_PROCESS;
    }
    return (old ? old : _IswDefaultLanguageProc);
}

IswDatabaseHandle
IswScreenDatabase(IswScreen screen)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    int scrno;
    IswDatabaseHandle db;
    IswPerDisplay pd;
    IswDisplay dpy = NULL;
    int nscreens;

    if (screen == NULL) {
        IswErrorMsg("nullDisplay",
                   "IswScreenDatabase", IswCIswToolkitError,
                   "IswScreenDatabase requires a non-NULL screen",
                   NULL, NULL);
        return NULL;
    }

    /* Find the connection that owns this screen by iterating per-display list */
    PerDisplayTablePtr pdt;
    LOCK_PROCESS;
    for (pdt = _IswperDisplayList; pdt != NULL; pdt = pdt->next) {
        int n_pdt = ops->display->screen_count(pdt->dpy);
        int n;
        for (n = 0; n < n_pdt; n++) {
            if (ops->display->screen(pdt->dpy, n) == screen) {
                dpy = pdt->dpy;
                scrno = n;
                pd = &pdt->perDpy;
                break;
            }
        }
        if (dpy != NULL)
            break;
    }
    UNLOCK_PROCESS;

    if (dpy == NULL) {
        IswErrorMsg("nullDisplay",
                   "IswScreenDatabase", IswCIswToolkitError,
                   "IswScreenDatabase: could not find display for screen",
                   NULL, NULL);
        return NULL;
    }

    DPY_TO_APPCON(dpy);
    LOCK_APP(app);
    LOCK_PROCESS;

    nscreens = ops->display->screen_count(dpy);

    /* Return cached database if available */
    if (pd->per_screen_db && pd->per_screen_db[scrno]) {
        db = pd->per_screen_db[scrno];
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return db;
    }

    /* Start with command-line database */
    if (nscreens == 1) {
        db = pd->cmd_db;
        pd->cmd_db = NULL;
    }
    else {
        db = CopyDB(pd->cmd_db);
    }

    /* Platform-specific user defaults (X11: RESOURCE_MANAGER, ~/.Xdefaults,
       XENVIRONMENT, ~/.Xdefaults-hostname, XUSERFILESEARCHPATH, app-defaults) */
    if (!pd->server_db) {
        IswDatabaseHandle user_db = _IswPlatformResourceBuildUserDb(dpy, screen);
        if (user_db)
            _IswPlatformResourceCombine(user_db, &db, False);
    }
    else {
        _IswPlatformResourceCombine(pd->server_db, &db, False);
        pd->server_db = NULL;
    }

    /* Ensure we have at least an empty database */
    if (!db)
        db = _IswPlatformResourceFromString("");

    /* Cache the database for this screen */
    if (pd->per_screen_db)
        pd->per_screen_db[scrno] = db;

    /* Fallback resources */
    if (pd->appContext->fallback_resources) {
        String *res;
        for (res = pd->appContext->fallback_resources; *res; res++)
            _IswPlatformResourcePutLine(&db, *res);
    }

    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return db;
}

void
IswReloadScreenDatabase(IswScreen screen_handle)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    IswDisplay dpy = NULL;
    int scrno = 0;
    IswPerDisplay pd;

    if (screen_handle == NULL)
        return;

    PerDisplayTablePtr pdt;
    LOCK_PROCESS;
    for (pdt = _IswperDisplayList; pdt != NULL; pdt = pdt->next) {
        int nscreens = ops->display->screen_count(pdt->dpy);
        int n;
        for (n = 0; n < nscreens; n++) {
            if (ops->display->screen(pdt->dpy, n) == screen_handle) {
                dpy = pdt->dpy;
                scrno = n;
                pd = &pdt->perDpy;
                break;
            }
        }
        if (dpy != NULL)
            break;
    }
    UNLOCK_PROCESS;
    
    if (dpy == NULL)
        return;
    
    DPY_TO_APPCON(dpy);
    LOCK_APP(app);
    LOCK_PROCESS;
    if (pd->per_screen_db && pd->per_screen_db[scrno]) {
        _IswPlatformResourceFree(pd->per_screen_db[scrno]);
        pd->per_screen_db[scrno] = NULL;
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

/*
 * Merge two option tables, allowing the second to over-ride the first,
 * so that ambiguous abbreviations can be noticed.  The merge attempts
 * to make the resulting table lexicographically sorted, but succeeds
 * only if the first source table is sorted.  Neither source table is
 * required to be sorted.
 *
 * Caller is responsible for freeing the returned option table.
 */

static void
_MergeOptionTables(const IswOptionDescRec *src1,
                   Cardinal num_src1,
                   const IswOptionDescRec *src2,
                   Cardinal num_src2,
                   IswOptionDescRec **dst,
                   Cardinal *num_dst)
{
    IswOptionDescRec *table, *endP;
    IswOptionDescRec *opt1, *dstP;
    const IswOptionDescRec *opt2;
    int i1;
    Cardinal i2;
    int dst_len, order;
    enum { Check, NotSorted, IsSorted } sort_order = Check;

    *dst = table = IswMallocArray(num_src1 + num_src2,
                                 (Cardinal) sizeof(IswOptionDescRec));

    (void) memcpy(table, src1, sizeof(IswOptionDescRec) * num_src1);
    if (num_src2 == 0) {
        *num_dst = num_src1;
        return;
    }
    endP = &table[dst_len = (int) num_src1];
    for (opt2 = src2, i2 = 0; i2 < num_src2; opt2++, i2++) {
        IswOptionDescRec *whereP;
        Boolean found;

        found = False;
        whereP = endP - 1;      /* assume new option goes at the end */
        for (opt1 = table, i1 = 0; i1 < dst_len; opt1++, i1++) {
            if (sort_order == Check && i1 > 0
                && strcmp(opt1->option, (opt1 - 1)->option) < 0)
                sort_order = NotSorted;
            if ((order = strcmp(opt1->option, opt2->option)) == 0) {
                *opt1 = *opt2;
                found = True;
                break;
            }
            if (sort_order == IsSorted && order > 0) {
                for (dstP = endP++; dstP > opt1; dstP--)
                    *dstP = *(dstP - 1);
                *opt1 = *opt2;
                dst_len++;
                found = True;
                break;
            }
            if (order < 0)
                whereP = opt1;
        }
        if (sort_order == Check && i1 == dst_len)
            sort_order = IsSorted;
        if (!found) {
            whereP++;
            for (dstP = endP++; dstP > whereP; dstP--)
                *dstP = *(dstP - 1);
            *whereP = *opt2;
            dst_len++;
        }
    }
    *num_dst = (Cardinal) dst_len;
}

/*
 * _IswParseCommand - Parse command line options into a resource database.
 */
/* Helper: put a resource into the database, using put_resource_line for
 * entries with wildcards (* or ?) so xcb-util-xrm parses them correctly. */
static void
_IswDbPutResource(IswDatabaseHandle *db, const char *resource, const char *value)
{
    char line_buf[1024];

    if (strchr(resource, '*') || strchr(resource, '?')) {
        snprintf(line_buf, sizeof(line_buf), "%s: %s", resource, value);
        _IswPlatformResourcePutLine(db, line_buf);
    } else {
        _IswPlatformResourcePut(db, resource, value);
    }
}

static void
_IswParseCommand(IswDatabaseHandle *db,
                IswOptionDescRec *options,
                int num_options,
                _Xconst char *prefix,
                int *argc,
                _IswString *argv)
{
    int i, j;
    int remaining = *argc;
    _IswString *src = argv;
    _IswString *dst = argv;
    char resource_buf[512];

    /* Skip argv[0] (program name) */
    if (remaining > 0) {
        *dst++ = *src++;
        remaining--;
    }

    while (remaining > 0) {
        Boolean matched = False;

        for (i = 0; i < num_options; i++) {
            int optlen = (int) strlen(options[i].option);

            if (strncmp(*src, options[i].option, (size_t) optlen) != 0)
                continue;

            /* Check for exact match or sticky arg */
            if ((*src)[optlen] != '\0' &&
                options[i].argKind != IswOptionStickyArg)
                continue;

            matched = True;

            switch (options[i].argKind) {
            case IswOptionNoArg:
                if (options[i].specifier != NULL) {
                    snprintf(resource_buf, sizeof(resource_buf),
                             "%s%s", prefix, options[i].specifier);
                    if (*db == NULL)
                        *db = _IswPlatformResourceFromString("");
                    _IswDbPutResource(db, resource_buf,
                                     (char *) options[i].value);
                }
                src++;
                remaining--;
                break;

            case IswOptionIsArg:
                if (options[i].specifier != NULL) {
                    snprintf(resource_buf, sizeof(resource_buf),
                             "%s%s", prefix, options[i].specifier);
                    if (*db == NULL)
                        *db = _IswPlatformResourceFromString("");
                    _IswDbPutResource(db, resource_buf, *src);
                }
                src++;
                remaining--;
                break;

            case IswOptionStickyArg:
                if (options[i].specifier != NULL) {
                    snprintf(resource_buf, sizeof(resource_buf),
                             "%s%s", prefix, options[i].specifier);
                    if (*db == NULL)
                        *db = _IswPlatformResourceFromString("");
                    _IswDbPutResource(db, resource_buf, *src + optlen);
                }
                src++;
                remaining--;
                break;

            case IswOptionSepArg:
                src++;
                remaining--;
                if (remaining > 0 && options[i].specifier != NULL) {
                    snprintf(resource_buf, sizeof(resource_buf),
                             "%s%s", prefix, options[i].specifier);
                    if (*db == NULL)
                        *db = _IswPlatformResourceFromString("");
                    _IswDbPutResource(db, resource_buf, *src);
                    src++;
                    remaining--;
                }
                break;

            case IswOptionResArg:
                src++;
                remaining--;
                if (remaining > 0) {
                    if (*db == NULL)
                        *db = _IswPlatformResourceFromString("");
                    _IswPlatformResourcePutLine(db, *src);
                    src++;
                    remaining--;
                }
                break;

            case IswOptionSkipArg:
                *dst++ = *src++;
                remaining--;
                if (remaining > 0) {
                    *dst++ = *src++;
                    remaining--;
                }
                break;

            case IswOptionSkipLine:
                while (remaining > 0) {
                    *dst++ = *src++;
                    remaining--;
                }
                break;

            case IswOptionSkipNArgs:
                {
                    int n = (int) (long) options[i].value;
                    *dst++ = *src++;
                    remaining--;
                    for (j = 0; j < n && remaining > 0; j++) {
                        *dst++ = *src++;
                        remaining--;
                    }
                }
                break;
            }
            break;
        }

        if (!matched) {
            /* Not a recognized option, keep it */
            *dst++ = *src++;
            remaining--;
        }
    }

    *argc = (int)(dst - argv);
}

IswDatabaseHandle
_IswPreparseCommandLine(IswOptionDescRec *urlist,
                       Cardinal num_urs,
                       int argc,
                       _IswString *argv,
                       String *applName,
                       String *displayName,
                       String *language)
{
    IswDatabaseHandle db = NULL;
    IswOptionDescRec *options;
    Cardinal num_options;
    _IswString *targv;
    int targc = argc;
    char *value = NULL;

    targv = IswMallocArray((Cardinal) argc, (Cardinal) sizeof(_IswString *));
    (void) memcpy(targv, argv, sizeof(char *) * (size_t) argc);
    _MergeOptionTables(opTable, IswNumber(opTable), urlist, num_urs,
                       &options, &num_options);
    _IswParseCommand(&db, options, (int) num_options, ".", &targc, targv);

    if (db != NULL) {
        if (applName) {
            if (_IswPlatformResourceGetString(db, "..name", "..name",
                                             &value) >= 0 && value != NULL) {
                *applName = value;
                value = NULL;
            }
        }
        if (displayName) {
            if (_IswPlatformResourceGetString(db, "..display", "..display",
                                             &value) >= 0 && value != NULL) {
                *displayName = value;
                value = NULL;
            }
        }
        if (language) {
            if (_IswPlatformResourceGetString(db, "..xnlLanguage",
                                             "..XnlLanguage",
                                             &value) >= 0 && value != NULL) {
                *language = value;
                value = NULL;
            }
        }
    }

    IswFree((char *) targv);
    IswFree((char *) options);
    return db;
}

static void
GetLanguage(IswDisplay dpy, IswPerDisplay pd)
{
    LOCK_PROCESS;
    if (!pd->language && pd->server_db != NULL) {
        char *name_str = NULL;
        char *class_str = NULL;
        char *value = NULL;

        /* Build resource name: <appname>.xnlLanguage */
        if (pd->name)
            IswAsprintf(&name_str, "%s.xnlLanguage", pd->name);
        if (pd->class)
            IswAsprintf(&class_str, "%s.XnlLanguage", pd->class);

        if (name_str && class_str &&
            _IswPlatformResourceGetString(pd->server_db, name_str, class_str,
                                         &value) >= 0 && value != NULL) {
            pd->language = value;  /* takes ownership */
        }
        IswFree(name_str);
        IswFree(class_str);
    }

    if (pd->appContext->langProcRec.proc) {
        if (!pd->language)
            pd->language = "";
        pd->language = (*pd->appContext->langProcRec.proc)
            ((IswDisplay) dpy, pd->language, pd->appContext->langProcRec.closure);
    }
    else if (!pd->language || pd->language[0] == '\0')  /* R4 compatibility */
        pd->language = getenv("LANG");

    if (pd->language)
        pd->language = IswNewString(pd->language);
    UNLOCK_PROCESS;
}

double
_IswGetScaleFactor(IswDisplay dpy)
{
    PerDisplayTablePtr pdt;

    if (!dpy)
        return 1.0;

    /* Walk the list directly instead of _IswGetPerDisplay to avoid
     * fatal error if display not yet registered (early converter calls) */
    for (pdt = _IswperDisplayList; pdt != NULL; pdt = pdt->next) {
        if (pdt->dpy == dpy) {
            if (pdt->perDpy.scale_factor > 0.0)
                return pdt->perDpy.scale_factor;
            return 1.0;
        }
    }
    return 1.0;
}

void
_IswDisplayInitialize(IswDisplay dpy,
                     IswPerDisplay pd,
                     _Xconst char *name,
                     //IswOptionDescRec *urlist,
                     Cardinal num_urs,
                     int *argc,
                     char **argv)
{
    IswOptionDescRec *options;
    Cardinal num_options;

    GetLanguage(dpy, pd);

    /* Parse the command line and remove Xt arguments from argv */
    _MergeOptionTables(opTable, IswNumber(opTable), NULL, num_urs,
                       &options, &num_options);
    _IswParseCommand(&pd->cmd_db, options, (int) num_options, name, argc, argv);
    IswFree((char *) options);

    pd->multi_click_time = 200;

    /* Detect HiDPI scale factor */
    {
        const char *env = getenv("ISW_SCALE_FACTOR");
        double scale = 0.0;

        if (env) {
            scale = atof(env);
        }

        if (scale <= 0.0 && pd->server_db) {
            char *value = NULL;
            if (_IswPlatformResourceGetString(pd->server_db,
                                             "Xft.dpi", "Xft.Dpi",
                                             &value) >= 0 && value) {
                double dpi = atof(value);
                if (dpi > 0.0)
                    scale = dpi / 96.0;
                free(value);
            }
        }

        if (scale < 1.0)
            scale = 1.0;

        pd->scale_factor = scale;

        if (scale != 1.0)
            fprintf(stderr, "ISW: HiDPI scale factor: %.2f\n", scale);

        /* Set XCURSOR_SIZE for xcb-cursor if not already set by the user.
         * The standard base cursor size is 24; scale it to match DPI. */
        if (!getenv("XCURSOR_SIZE") && scale > 1.0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", (int)(24 * scale + 0.5));
            setenv("XCURSOR_SIZE", buf, 0);
        }
    }
}

/*      Function Name: IswAppSetFallbackResources
 *      Description: Sets the fallback resource list that will be loaded
 *                   at display initialization time.
 *      Arguments: app_context - the app context.
 *                 specification_list - the resource specification list.
 *      Returns: none.
 */

void
IswAppSetFallbackResources(IswAppContext app_context, String *specification_list)
{
    LOCK_APP(app_context);
    app_context->fallback_resources = specification_list;
    UNLOCK_APP(app_context);
}

Widget
IswOpenApplication(IswAppContext *app_context_return,
                  _Xconst char *application_class,
                  IswOptionDescRec *options,
                  Cardinal num_options,
                  int *argc_in_out,
                  _IswString *argv_in_out,
                  String *fallback_resources,
                  WidgetClass widget_class,
                  ArgList args_in,
                  Cardinal num_args_in)
{
    IswAppContext app_con;
    IswDisplay dpy;
    register int saved_argc = *argc_in_out;
    Widget root;
    IswArgBuilder ab = IswArgBuilderInit();
    ArgList merged_args;

    IswToolkitInitialize();      /* cannot be moved into _IswAppInit */

    dpy = _IswAppInit(&app_con, (String) application_class, options, num_options,
                     argc_in_out, &argv_in_out, fallback_resources);

    LOCK_APP(app_con);
    IswArgScreen(&ab, _IswDefaultScreenOf(dpy));
    IswArgArgc(&ab, saved_argc);
    IswArgArgv(&ab, argv_in_out);

    merged_args = IswMergeArgLists(args_in, num_args_in, ab.args, ab.count);
    Cardinal num = ab.count + num_args_in;

    root = IswAppCreateShell(NULL, application_class, widget_class, dpy,
                            merged_args, num);

    if (app_context_return)
        *app_context_return = app_con;

    IswFree((IswPointer) merged_args);
    IswFree((IswPointer) argv_in_out);
    UNLOCK_APP(app_con);
    return root;
}

Widget
IswAppInitialize(IswAppContext *app_context_return,
                _Xconst char *application_class,
                IswOptionDescRec *options,
                Cardinal num_options,
                int *argc_in_out,
                _IswString *argv_in_out,
                String *fallback_resources,
                ArgList args_in,
                Cardinal num_args_in)
{
    return IswOpenApplication(app_context_return, application_class,
                             options, num_options,
                             argc_in_out, argv_in_out, fallback_resources,
                             applicationShellWidgetClass, args_in, num_args_in);
}
