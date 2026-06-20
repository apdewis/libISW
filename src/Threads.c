/************************************************************
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

********************************************************/

/*

Copyright 1994, 1998  The Open Group

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"

#ifdef ISW_THREADS

#include <pthread.h>

#ifndef NDEBUG
#define NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

typedef struct _ThreadStack {
    unsigned int size;
    int sp;
    struct _Tstack {
        pthread_t t;
        pthread_cond_t c;
    } *st;
} ThreadStack;

typedef struct _LockRec {
    pthread_mutex_t mutex;
    int level;
    ThreadStack stack;
    pthread_t holder;
    pthread_cond_t cond;
    Boolean has_holder;
} LockRec;

#define STACK_INCR 16

static LockPtr process_lock = NULL;

static void
InitProcessLock(void)
{
    if (!process_lock) {
        process_lock = IswNew(LockRec);
        pthread_mutex_init(&process_lock->mutex, NULL);
        process_lock->level = 0;
        pthread_cond_init(&process_lock->cond, NULL);
        process_lock->has_holder = FALSE;
    }
}

static void
ProcessLock(void)
{
    pthread_t this_thread = pthread_self();

    pthread_mutex_lock(&process_lock->mutex);

    if (!process_lock->has_holder) {
        process_lock->holder = this_thread;
        process_lock->has_holder = TRUE;
        pthread_mutex_unlock(&process_lock->mutex);
        return;
    }

    if (pthread_equal(process_lock->holder, this_thread)) {
        process_lock->level++;
        pthread_mutex_unlock(&process_lock->mutex);
        return;
    }

    while (process_lock->has_holder)
        pthread_cond_wait(&process_lock->cond, &process_lock->mutex);

    process_lock->holder = this_thread;
    process_lock->has_holder = TRUE;
    assert(pthread_equal(process_lock->holder, this_thread));
    pthread_mutex_unlock(&process_lock->mutex);
}

static void
ProcessUnlock(void)
{
    pthread_mutex_lock(&process_lock->mutex);
    assert(pthread_equal(process_lock->holder, pthread_self()));
    if (process_lock->level != 0) {
        process_lock->level--;
        pthread_mutex_unlock(&process_lock->mutex);
        return;
    }

    process_lock->has_holder = FALSE;
    pthread_cond_signal(&process_lock->cond);

    pthread_mutex_unlock(&process_lock->mutex);
}

static void
AppLock(IswAppContext app)
{
    LockPtr app_lock = app->lock_info;
    pthread_t self = pthread_self();

    pthread_mutex_lock(&app_lock->mutex);
    if (!app_lock->has_holder) {
        app_lock->holder = self;
        app_lock->has_holder = TRUE;
        assert(pthread_equal(app_lock->holder, self));
        pthread_mutex_unlock(&app_lock->mutex);
        return;
    }
    if (pthread_equal(app_lock->holder, self)) {
        app_lock->level++;
        pthread_mutex_unlock(&app_lock->mutex);
        return;
    }
    while (app_lock->has_holder) {
        pthread_cond_wait(&app_lock->cond, &app_lock->mutex);
    }
    app_lock->holder = self;
    app_lock->has_holder = TRUE;
    assert(pthread_equal(app_lock->holder, self));
    pthread_mutex_unlock(&app_lock->mutex);
}

static void
AppUnlock(IswAppContext app)
{
    LockPtr app_lock = app->lock_info;

    pthread_mutex_lock(&app_lock->mutex);
    assert(pthread_equal(app_lock->holder, pthread_self()));
    if (app_lock->level != 0) {
        app_lock->level--;
        pthread_mutex_unlock(&app_lock->mutex);
        return;
    }
    app_lock->has_holder = FALSE;
    pthread_cond_signal(&app_lock->cond);
    pthread_mutex_unlock(&app_lock->mutex);
}

static void
YieldAppLock(IswAppContext app,
             Boolean *push_thread,
             Boolean *pushed_thread,
             int *level)
{
    LockPtr app_lock = app->lock_info;
    pthread_t self = pthread_self();

    pthread_mutex_lock(&app_lock->mutex);
    assert(pthread_equal(app_lock->holder, self));
    *level = app_lock->level;
    if (*push_thread) {
        *push_thread = FALSE;
        *pushed_thread = TRUE;

        if (app_lock->stack.sp == (int) app_lock->stack.size - 1) {
            unsigned ii;

            app_lock->stack.st = (struct _Tstack *)
                IswReallocArray(app_lock->stack.st,
                               (Cardinal) (app_lock->stack.size + STACK_INCR),
                               (Cardinal) sizeof(struct _Tstack));
            ii = app_lock->stack.size;
            app_lock->stack.size += STACK_INCR;
            for (; ii < app_lock->stack.size; ii++) {
                pthread_cond_init(&app_lock->stack.st[ii].c, NULL);
            }
        }
        app_lock->stack.st[++(app_lock->stack.sp)].t = self;
    }
    pthread_cond_signal(&app_lock->cond);
    app_lock->level = 0;
    app_lock->has_holder = FALSE;
    pthread_mutex_unlock(&app_lock->mutex);
}

static void
RestoreAppLock(IswAppContext app, int level, Boolean *pushed_thread)
{
    LockPtr app_lock = app->lock_info;
    pthread_t self = pthread_self();

    pthread_mutex_lock(&app_lock->mutex);
    while (app_lock->has_holder) {
        pthread_cond_wait(&app_lock->cond, &app_lock->mutex);
    }
    if (!pthread_equal(app_lock->stack.st[app_lock->stack.sp].t, self)) {
        int ii;

        for (ii = app_lock->stack.sp - 1; ii >= 0; ii--) {
            if (pthread_equal(app_lock->stack.st[ii].t, self)) {
                pthread_cond_wait(&app_lock->stack.st[ii].c, &app_lock->mutex);
                break;
            }
        }
        while (app_lock->has_holder) {
            pthread_cond_wait(&app_lock->cond, &app_lock->mutex);
        }
    }
    app_lock->holder = self;
    app_lock->has_holder = TRUE;
    app_lock->level = level;
    assert(pthread_equal(app_lock->holder, self));
    if (*pushed_thread) {
        *pushed_thread = FALSE;
        (app_lock->stack.sp)--;
        if (app_lock->stack.sp >= 0) {
            pthread_cond_signal(&app_lock->stack.st[app_lock->stack.sp].c);
        }
    }
    pthread_mutex_unlock(&app_lock->mutex);
}

static void
FreeAppLock(IswAppContext app)
{
    unsigned ii;
    LockPtr app_lock = app->lock_info;

    if (app_lock) {
        pthread_mutex_destroy(&app_lock->mutex);
        pthread_cond_destroy(&app_lock->cond);
        if (app_lock->stack.st != (struct _Tstack *) NULL) {
            for (ii = 0; ii < app_lock->stack.size; ii++) {
                pthread_cond_destroy(&app_lock->stack.st[ii].c);
            }
            IswFree((char *) app_lock->stack.st);
        }
        IswFree((char *) app_lock);
        app->lock_info = NULL;
    }
}

static void
InitAppLock(IswAppContext app)
{
    int ii;
    LockPtr app_lock;

    app->lock = AppLock;
    app->unlock = AppUnlock;
    app->yield_lock = YieldAppLock;
    app->restore_lock = RestoreAppLock;
    app->free_lock = FreeAppLock;

    app_lock = app->lock_info = IswNew(LockRec);
    pthread_mutex_init(&app_lock->mutex, NULL);
    app_lock->level = 0;
    pthread_cond_init(&app_lock->cond, NULL);
    app_lock->has_holder = FALSE;
    app_lock->stack.size = STACK_INCR;
    app_lock->stack.sp = -1;
    app_lock->stack.st = IswMallocArray(STACK_INCR, sizeof(struct _Tstack));
    for (ii = 0; ii < STACK_INCR; ii++) {
        pthread_cond_init(&app_lock->stack.st[ii].c, NULL);
    }
}

#endif                          /* defined(ISW_THREADS) */

void
IswAppLock(IswAppContext app)
{
#ifdef ISW_THREADS
    if (app->lock)
        (*app->lock) (app);
#endif
}

void
IswAppUnlock(IswAppContext app)
{
#ifdef ISW_THREADS
    if (app->unlock)
        (*app->unlock) (app);
#endif
}

void
IswProcessLock(void)
{
#ifdef ISW_THREADS
    if (_IswProcessLock)
        (*_IswProcessLock) ();
#endif
}

void
IswProcessUnlock(void)
{
#ifdef ISW_THREADS
    if (_IswProcessUnlock)
        (*_IswProcessUnlock) ();
#endif
}

Boolean
IswToolkitThreadInitialize(void)
{
#ifdef ISW_THREADS
    if (_IswProcessLock == NULL) {
        InitProcessLock();
        _IswProcessLock = ProcessLock;
        _IswProcessUnlock = ProcessUnlock;
        _IswInitAppLock = InitAppLock;
    }
    return True;
#else
    return False;
#endif
}
