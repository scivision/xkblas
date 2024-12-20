/* ************************************************************************** */
/*                                                                            */
/*   runtime.cc                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:09:33 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <kaapi/runtime.h>
# include <kaapi/conf/conf.h>
# include <kaapi/logger/logger.h>
# include <kaapi/device/driver.h>
# include <kaapi/device/thread-producer.hpp>
# include <kaapi/memory/alignedas.h>
# include <kaapi/sync/spinlock.h>

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

// singleton of runtime runtime
kaapi_runtime_t *
kaapi_runtime_get(void)
{
    # pragma message(TODO "Optimize this default conf")

    static kaapi_runtime_t runtime = {
        .state = {
            .spinlock = 0,
            .current = { KAAPI_RUNTIME_DEINITIALIZED }
        },
        .conf = {},
        .memory_coherent_worker_thread = nullptr,
        // .memtrees = std::vector<MemoryTree *>(),
        .drivers = {}
    };

    return &runtime;
}

static inline void
kaapi_task_format_register(void)
{
    kaapi_memory_coherent_async_register_format();
}

extern "C"
int
kaapi_init(void)
{
    LOGGER_INFO("Initializing Xkblas");

    kaapi_runtime_t * runtime = kaapi_runtime_get();
    if (runtime->state.current == KAAPI_RUNTIME_DEINITIALIZED)
    {
        SPINLOCK_LOCK(runtime->state.spinlock);
        {
            if (runtime->state.current == KAAPI_RUNTIME_DEINITIALIZED)
            {
                // load
                kaapi_init_conf(&(runtime->conf));
#if USE_STATS == 1
                kaapi_stats_init(&(runtime->stats));
#endif // USE_STATS == 1
                kaapi_task_format_register();
                kaapi_memory_coherent_async_worker_thread_init(runtime);
                kaapi_drivers_init(&(runtime->drivers), runtime->conf.ngpus);
                runtime->state.current = KAAPI_RUNTIME_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(runtime->state.spinlock);
    }
    return 0;
}

extern "C"
int
kaapi_deinit(void)
{
    LOGGER_INFO("Deinitializing Xkblas");

# if USE_STATS == 1
    kaapi_stats_report();
# endif // USE_STATS == 1

    kaapi_runtime_t * runtime = kaapi_runtime_get();
    if (runtime->state.current == KAAPI_RUNTIME_INITIALIZED)
    {
        SPINLOCK_LOCK(runtime->state.spinlock);
        {
            if (runtime->state.current == KAAPI_RUNTIME_INITIALIZED)
            {
                kaapi_drivers_deinit(&runtime->drivers);
                runtime->state.current = KAAPI_RUNTIME_DEINITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(runtime->state.spinlock);
    }
    return 0;
}

/* legacy compatibility (deprecated) */
extern "C"
void
kaapi_finalize(void)
{
    kaapi_deinit();
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
int
kaapi_sync(void)
{
    LOGGER_INFO("Synchronizing Xkblas");

    kaapi_runtime_t * runtime = kaapi_runtime_get();
    assert(runtime);

    /* other threads */
    ThreadWorker * workers[] = {
        ThreadWorker::self(),
        runtime->memory_coherent_worker_thread
    };
    const int nworkers = sizeof(workers) / sizeof(ThreadWorker *);
    const int ndevices = runtime->drivers.devices.n;
    int32_t wc_global;

retry:
    wc_global = 0;

    /* wait for all devices thread */
    for (int i = 0 ; i < ndevices ; ++i)
    {
        kaapi_device_t * device = runtime->drivers.devices.list[i];
        if (!device->offloader.is_empty(KAAPI_STREAM_TYPE_ALL))
            ++wc_global;
        wc_global += device->thread->wc;
    }

    /* wait for all other threads */
    for (ThreadWorker * & thread : workers)
        wc_global += thread->wc;

    /* if there is still work on-going */
    if (wc_global)
    {
        usleep(5);    // TODO : pthread cond instead ? or maybe workstealing
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
    runtime->memtree.export_pdf("memory");

    // dependency kinterval btree
    thread->deptree.export_pdf("dependency");
# endif
    return 0;
}

///////////////
// UTILITIES //
///////////////

extern "C"
int
kaapi_get_ngpus(int * count)
{
    assert(count);

    kaapi_runtime_t * runtime = kaapi_runtime_get();
    assert(runtime);

    *count = 0;
    for (int i = 0 ; i < KAAPI_DRIVER_TYPE_MAX ; ++i)
    {
        kaapi_driver_t * driver = runtime->drivers.list + i;
        assert(driver);

        if (driver->f_get_ndevices_max)
            *count += driver->f_get_ndevices_max();
    }
    return 0;
}
