/* ************************************************************************** */
/*                                                                            */
/*   runtime.cc                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:04:18 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <ptr/runtime.h>
# include <ptr/conf/conf.h>
# include <ptr/logger/logger.h>
# include <ptr/device/driver.h>
# include <ptr/device/thread-producer.hpp>
# include <ptr/sync/alignedas.h>
# include <ptr/sync/spinlock.h>

# if 0
# if __has_include("kernels/generated/kernel-task-format-register.h")
#  include "kernels/generated/kernel-task-format-register.h"
# else
#  error "Please run 'python3 generate.py' in the 'kernels/' directory to generate source files"
# endif
# endif

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

// singleton of runtime context
ptr_runtime_t *
ptr_runtime_get(void)
{
    # pragma message(TODO "Optimize this default conf")

    static ptr_runtime_t context = {
        .state = {
            .spinlock = 0,
            .current = { PTR_CONTEXT_DEINITIALIZED }
        },
        .conf = {},
        .memory_coherent_worker_thread = nullptr,
        // .memtrees = std::vector<MemoryTree *>(),
        .drivers = {}
    };

    return &context;
}

static inline void
ptr_task_format_register(void)
{
    ptr_memory_coherent_async_register_format();
}

extern "C"
int
ptr_init(void)
{
    LOGGER_INFO("Initializing Xkblas");

    ptr_runtime_t * context = ptr_runtime_get();
    if (context->state.current == PTR_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        {
            if (context->state.current == PTR_CONTEXT_DEINITIALIZED)
            {
                // load
                ptr_init_conf(&(context->conf));
#if USE_STATS == 1
                ptr_stats_init(&(context->stats));
#endif // USE_STATS == 1
                ptr_task_format_register();
                ptr_memory_coherent_async_worker_thread_init(context);
                ptr_drivers_init(&(context->drivers), context->conf.ngpus);
                context->state.current = PTR_CONTEXT_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }
    return 0;
}

extern "C"
void
ptr_deinit(void)
{
    LOGGER_INFO("Deinitializing Xkblas");

# if USE_STATS == 1
    ptr_stats_report();
# endif // USE_STATS == 1

    ptr_runtime_t * context = ptr_runtime_get();
    if (context->state.current == PTR_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        {
            if (context->state.current == PTR_CONTEXT_INITIALIZED)
            {
                ptr_drivers_deinit(&context->drivers);
                context->state.current = PTR_CONTEXT_DEINITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }
}

/* legacy compatibility (deprecated) */
extern "C"
void
ptr_finalize(void)
{
    ptr_deinit();
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
void
ptr_sync(void)
{
    LOGGER_INFO("Synchronizing Xkblas");

    ptr_runtime_t * context = ptr_runtime_get();
    assert(context);

    /* other threads */
    ThreadWorker * workers[] = {
        ThreadWorker::self(),
        context->memory_coherent_worker_thread
    };
    const int nworkers = sizeof(workers) / sizeof(ThreadWorker *);
    const int ndevices = context->drivers.devices.n;
    int32_t wc_global;

retry:
    wc_global = 0;

    /* wait for all devices thread */
    for (int i = 0 ; i < ndevices ; ++i)
    {
        ptr_device_t * device = context->drivers.devices.list[i];
        if (!device->offloader.is_empty(PTR_STREAM_TYPE_ALL))
            ++wc_global;
        wc_global += device->thread->wc;
    }

    /* wait for all other threads */
    for (ThreadWorker * & thread : workers)
        wc_global += thread->wc;

    /* if there is still work on-going */
    if (wc_global)
    {
        usleep(50);    // TODO : pthread cond instead ? or maybe workstealing
        goto retry;
    }

    /* all threads completed :-) */
    LOGGER_INFO("Synchronized Xkblas");

# if 0
# if !defined(NDEBUG)
    // task dependency graph
    LOGGER_INFO("Exporting Dependency Tree...");
    ThreadProducer * thread = ThreadProducer::self();
    FILE * f = fopen("tasks.dot", "w");
    thread->dump_tasks(f);
    fclose(f);
    system("dot -Tpdf tasks.dot > tasks.pdf");
# endif
# endif

# if 0
    LOGGER_INFO("Exporting memory tree...");

    // memory kinterval btree
    context->memtree.export_pdf("memory");

    // dependency kinterval btree
    thread->deptree.export_pdf("dependency");
# endif
}

///////////////
// UTILITIES //
///////////////

extern "C"
int
ptr_get_ngpus(int * count)
{
    assert(count);

    ptr_runtime_t * context = ptr_runtime_get();
    assert(context);

    *count = 0;
    for (int i = 0 ; i < PTR_DRIVER_TYPE_MAX ; ++i)
    {
        ptr_driver_t * driver = context->drivers.list + i;
        assert(driver);

        if (driver->f_get_ndevices_max)
            *count += driver->f_get_ndevices_max();
    }
    return 0;
}
