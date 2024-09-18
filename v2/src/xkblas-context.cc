# include "xkblas-context.h"

# include "conf/conf.h"
# include "logger/logger.h"
# include "device/driver.h"
# include "device/thread-producer.hpp"
# include "sync/alignedas.h"
# include "sync/spinlock.h"

# if __has_include("kernels/kernel-task-format-register.h")
#  include "kernels/kernel-task-format-register.h"
# else
#  error "Please run 'python3 generate.py' in the 'kernels/' directory to generate source files"
# endif

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

// singleton of runtime context
xkblas_context_t *
xkblas_context_get(void)
{
    # pragma message(TODO "Optimize this default conf")

    static xkblas_context_t context = {
        .state = {
            .spinlock = 0,
            .current = { XKBLAS_CONTEXT_DEINITIALIZED }
        },
        .conf = {},
        .memory_coherent_worker_thread = nullptr,
        .drivers = {}
    };

    return &context;
}

static inline void
xkblas_task_format_register(void)
{
    xkblas_memory_coherent_async_register_format();
    # include "kernels/kernel-task-format-register.cc"
}

extern "C"
void
xkblas_init(void)
{
    XKBLAS_INFO("Initializing Xkblas");

    xkblas_context_t * context = xkblas_context_get();
    if (context->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        {
            if (context->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
            {
                // load
                xkblas_init_conf(&(context->conf));
                xkblas_stats_init(&(context->stats));
                xkblas_task_format_register();
                xkblas_memory_coherent_async_worker_thread_init(context);
                xkblas_drivers_init(&(context->drivers), context->conf.ngpus);
                context->state.current = XKBLAS_CONTEXT_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }
}

extern "C"
void
xkblas_invalidate_caches(void)
{
    XKBLAS_INFO("Invalidate XKBlas caches");
    xkblas_context_t * context = xkblas_context_get();
    if( context->state.current == XKBLAS_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        if( context->state.current == XKBLAS_CONTEXT_INITIALIZED)
        {
	    /*
	    // TODO : add a driver count so we can stop earlier ...
            for( int driver_id = 0; driver_id < MAX_DRIVER_COUNT; driver_id++ )
	    {
	        xkblas_driver_invalidate_caches( context->drivers.list + driver_id );
	    }
	    */
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }
}

extern "C"
void
xkblas_deinit(void)
{
    XKBLAS_INFO("Deinitializing Xkblas");

    xkblas_context_t * context = xkblas_context_get();
    if (context->state.current == XKBLAS_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        {
            if (context->state.current == XKBLAS_CONTEXT_INITIALIZED)
            {
                xkblas_drivers_deinit(&context->drivers);
            }
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
void
xkblas_sync(void)
{
    XKBLAS_INFO("Synchronizing Xkblas");

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    /* other threads */
    ThreadWorker * workers[] = {
        ThreadWorker::self(),
        context->memory_coherent_worker_thread
    };
    const int nworkers = sizeof(workers) / sizeof(ThreadWorker *);

    /* wait for all threads */
    const int ndevices = context->drivers.devices.n;

retry:
    /* wait for all devices thread */
    int32_t wc_global = 0;
    for (int i = 0 ; i < ndevices ; ++i)
    {
        xkblas_device_t * device = context->drivers.devices.list[i];
        if (!device->offloader.is_empty(XKBLAS_STREAM_TYPE_ALL))
            goto retry;
        wc_global += device->thread->wc;
    }

    /* wait for all other threads */
    for (ThreadWorker * & thread : workers)
        wc_global += thread->wc;

    /* if there is still work on-going */
    if (wc_global)
        goto retry;

    /* all threads completed :-) */
    XKBLAS_INFO("Synchronized Xkblas");
    xkblas_stats_report(&(context->stats));

# if 0
    XKBLAS_INFO("Exporting memory tree...");
    context->memtree.export_pdf("memory");

    XKBLAS_INFO("Exporting Dependency Tree...");
    ThreadProducer * thread = ThreadProducer::get();
    FILE * f = fopen("tasks.dot", "w");
    thread->dump_tasks(f);
    fclose(f);
    system("dot -Tpdf tasks.dot > tasks.pdf");

    thread->deptree.export_pdf("dependency");
    XKBLAS_DEBUG("Done");
# endif

}
