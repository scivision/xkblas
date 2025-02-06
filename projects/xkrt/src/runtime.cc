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

# include <xkrt/hwloc.h>
# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/logger/logger.h>
# include <xkrt/device/driver.h>
# include <xkrt/device/thread-producer.hpp>
# include <xkrt/memory/alignedas.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

hwloc_topology_t XKRT_HWLOC_TOPOLOGY;

// singleton of runtime
xkrt_runtime_t *
xkrt_runtime_get(void)
{
    # pragma message(TODO "Optimize this default conf")

    static xkrt_runtime_t runtime = {
        .state = {
            .spinlock = 0,
            .current = { XKRT_RUNTIME_DEINITIALIZED }
        },
        .conf = {},
        .memory_coherent_worker_thread = nullptr,
        // .memtrees = std::vector<MemoryTree *>(),
        .drivers = {}
    };

    return &runtime;
}

static inline void
xkrt_task_format_register(void)
{
    xkrt_memory_coherent_async_register_format();
}

extern "C"
int
xkrt_init(void)
{
    LOGGER_INFO("Initializing XKRT");

    xkrt_runtime_t * runtime = xkrt_runtime_get();
    if (runtime->state.current == XKRT_RUNTIME_DEINITIALIZED)
    {
        SPINLOCK_LOCK(runtime->state.spinlock);
        {
            hwloc_topology_init(&XKRT_HWLOC_TOPOLOGY);
            hwloc_topology_load(XKRT_HWLOC_TOPOLOGY);

            if (runtime->state.current == XKRT_RUNTIME_DEINITIALIZED)
            {
                // load
                xkrt_init_conf(&(runtime->conf));
#if USE_STATS == 1
                xkrt_stats_init(&(runtime->stats));
#endif // USE_STATS == 1
                xkrt_task_format_register();
                xkrt_memory_coherent_async_worker_thread_init(runtime);
                xkrt_drivers_init(&(runtime->drivers), runtime->conf.ngpus);
                runtime->state.current = XKRT_RUNTIME_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(runtime->state.spinlock);
    }
    return 0;
}

extern "C"
int
xkrt_deinit(void)
{
    LOGGER_INFO("Deinitializing XKRT");

# if USE_STATS == 1
    xkrt_stats_report();
# endif // USE_STATS == 1

    xkrt_runtime_t * runtime = xkrt_runtime_get();
    if (runtime->state.current == XKRT_RUNTIME_INITIALIZED)
    {
        SPINLOCK_LOCK(runtime->state.spinlock);
        {
            if (runtime->state.current == XKRT_RUNTIME_INITIALIZED)
            {
                xkrt_drivers_deinit(&runtime->drivers);
                runtime->state.current = XKRT_RUNTIME_DEINITIALIZED;
                hwloc_topology_destroy(XKRT_HWLOC_TOPOLOGY);
            }
        }
        SPINLOCK_UNLOCK(runtime->state.spinlock);
    }
    return 0;
}

/* legacy compatibility (deprecated) */
extern "C"
void
xkrt_finalize(void)
{
    xkrt_deinit();
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
int
xkrt_sync(void)
{
    LOGGER_INFO("Synchronizing XKRT");

    xkrt_runtime_t * runtime = xkrt_runtime_get();
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
        xkrt_device_t * device = runtime->drivers.devices.list[i];
        if (!device->offloader.is_empty(XKRT_STREAM_TYPE_ALL))
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
    LOGGER_INFO("Synchronized XKRT");

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
xkrt_get_ngpus(int * count)
{
    assert(count);

    xkrt_runtime_t * runtime = xkrt_runtime_get();
    assert(runtime);

    *count = 0;
    for (int i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
    {
        xkrt_driver_t * driver = runtime->drivers.list + i;
        assert(driver);

        if (driver->f_get_ndevices_max)
            *count += driver->f_get_ndevices_max();
    }
    return 0;
}
