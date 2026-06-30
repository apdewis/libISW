/*
 * thread_test.c — Exercise ISW_THREADS locking from multiple pthreads.
 *
 * Build with -DISW_THREADS=ON.  Three worker threads each grab the app
 * lock, IswSetValues a label, release the lock, and sleep briefly.
 * The main thread runs IswAppMainLoop.  A timeout-driven counter on the
 * main thread verifies that timeouts and worker-thread SetValues coexist
 * without deadlock.
 *
 * Quit button or 10-second auto-exit stops the workers and joins them.
 */

#include <ISW/Intrinsic.h>
#include <ISW/StringDefs.h>
#include <ISW/Shell.h>
#include <ISW/IswArgMacros.h>

#include <ISW/Box.h>
#include <ISW/Label.h>
#include <ISW/Command.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

#define NUM_WORKERS  3
#define AUTO_EXIT_MS 10000

static IswAppContext g_app;
static Widget        g_shell;
static Widget        g_worker_labels[NUM_WORKERS];
static Widget        g_main_label;
static pthread_t     g_threads[NUM_WORKERS];
static atomic_int    g_running = 1;

typedef struct {
    int id;
} WorkerArg;

static WorkerArg g_wargs[NUM_WORKERS];

static void
shutdown_app(void)
{
    atomic_store(&g_running, 0);

    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_join(g_threads[i], NULL);

    fprintf(stderr, "[thread_test] all workers joined, exiting\n");
    IswAppSetExitFlag(g_app);
}

static void
quit_cb(Widget w, IswPointer client, IswPointer call)
{
    (void)w; (void)client; (void)call;
    shutdown_app();
}

static void
auto_exit_cb(IswPointer client, IswIntervalId *id)
{
    (void)client; (void)id;
    fprintf(stderr, "[thread_test] auto-exit timer fired\n");
    shutdown_app();
}

static int g_main_counter;

static void
main_tick_cb(IswPointer client, IswIntervalId *id)
{
    (void)client; (void)id;
    char buf[64];

    if (!atomic_load(&g_running))
        return;

    g_main_counter++;
    snprintf(buf, sizeof buf, "main tick: %d", g_main_counter);

    IswArgBuilder ab = IswArgBuilderInit();
    IswArgLabel(&ab, buf);
    IswSetValues(g_main_label, ab.args, ab.count);

    IswAppAddTimeOut(g_app, 200, main_tick_cb, NULL);
}

static void *
worker_func(void *arg)
{
    WorkerArg *wa = arg;
    int id = wa->id;
    int counter = 0;
    char buf[64];

    fprintf(stderr, "[worker %d] started\n", id);

    while (atomic_load(&g_running)) {
        counter++;
        snprintf(buf, sizeof buf, "worker %d: %d", id, counter);

        IswAppLock(g_app);
        {
            IswArgBuilder ab = IswArgBuilderInit();
            IswArgLabel(&ab, buf);
            IswSetValues(g_worker_labels[id], ab.args, ab.count);
        }
        IswAppUnlock(g_app);

        usleep(80000 + (id * 30000));
    }

    fprintf(stderr, "[worker %d] exiting after %d iterations\n", id, counter);
    return NULL;
}

int
main(int argc, char *argv[])
{
    Boolean thread_ok;

    setvbuf(stderr, NULL, _IOLBF, 0);

    thread_ok = IswToolkitThreadInitialize();
    fprintf(stderr, "[thread_test] IswToolkitThreadInitialize() = %s\n",
            thread_ok ? "True" : "False");

    if (!thread_ok) {
        fprintf(stderr, "[thread_test] FAIL — rebuild with -DISW_THREADS=ON\n");
        return 1;
    }

    g_shell = IswAppInitialize(&g_app, "ThreadTest",
                               NULL, 0, &argc, argv, NULL, NULL, 0);

    IswArgBuilder ab = IswArgBuilderInit();
    IswArgWidth(&ab, 400);
    IswArgHeight(&ab, 200);
    IswArgTitle(&ab, "ISW Threading Test");
    IswSetValues(g_shell, ab.args, ab.count);

    Widget box = IswCreateManagedWidget("box", boxWidgetClass, g_shell,
                                        NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "main tick: 0");
    IswArgWidth(&ab, 200);
    IswArgBorderWidth(&ab, 1);
    g_main_label = IswCreateManagedWidget("mainLabel", labelWidgetClass,
                                          box, ab.args, ab.count);

    for (int i = 0; i < NUM_WORKERS; i++) {
        char name[32], text[32];
        snprintf(name, sizeof name, "workerLabel%d", i);
        snprintf(text, sizeof text, "worker %d: 0", i);

        IswArgBuilderReset(&ab);
        IswArgLabel(&ab, text);
        IswArgWidth(&ab, 200);
        IswArgBorderWidth(&ab, 1);
        g_worker_labels[i] = IswCreateManagedWidget(name, labelWidgetClass,
                                                     box, ab.args, ab.count);
    }

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Quit");
    Widget quit = IswCreateManagedWidget("quit", commandWidgetClass,
                                         box, ab.args, ab.count);
    IswAddCallback(quit, IswNcallback, quit_cb, NULL);

    IswRealizeWidget(g_shell);

    IswAppAddTimeOut(g_app, 200, main_tick_cb, NULL);
    IswAppAddTimeOut(g_app, AUTO_EXIT_MS, auto_exit_cb, NULL);

    for (int i = 0; i < NUM_WORKERS; i++) {
        g_wargs[i].id = i;
        pthread_create(&g_threads[i], NULL, worker_func, &g_wargs[i]);
    }

    fprintf(stderr, "[thread_test] entering main loop with %d workers\n",
            NUM_WORKERS);

    IswAppMainLoop(g_app);

    return 0;
}
