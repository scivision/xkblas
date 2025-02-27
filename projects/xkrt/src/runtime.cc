/* ************************************************************************** */
/*                                                                            */
/*   runtime.cc                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/26 17:14:13 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/logger/logger.h>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/thread-producer.hpp>
# include <xkrt/memory/alignedas.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

static inline void
xkrt_task_format_register(xkrt_runtime_t * runtime)
{
    task_formats_init(&(runtime->formats.list));
    xkrt_memory_coherent_async_register_format(runtime);
    xkrt_memory_copy_async_register_format(runtime);
}

extern "C"
int
xkrt_init(xkrt_runtime_t * runtime)
{
    LOGGER_INFO("Initializing XKRT");

    memset(runtime, 0, sizeof(xkrt_runtime_t));

    // load
    xkrt_init_conf(&(runtime->conf));
    xkrt_task_format_register(runtime);
    xkrt_memory_coherent_async_worker_thread_init(runtime);

    const int ngpus = MIN(XKRT_DEVICES_MAX, runtime->conf.device.ngpus);
    xkrt_drivers_init(&(runtime->drivers), ngpus, runtime->conf.drivers_mask, xkrt_device_thread_main, runtime);
    runtime->state = XKRT_RUNTIME_INITIALIZED;

    return 0;
}

extern "C"
int
xkrt_deinit(xkrt_runtime_t * runtime)
{
    LOGGER_INFO("Deinitializing XKRT");
    assert(runtime);
    assert(runtime->state == XKRT_RUNTIME_INITIALIZED);

    # if XKRT_SUPPORT_STATS
    if (runtime->conf.report_stats_on_deinit)
        xkrt_runtime_stats_report(runtime);
    # endif /* XKRT_SUPPORT_STATS */
    xkrt_drivers_deinit(&runtime->drivers);
    runtime->state = XKRT_RUNTIME_DEINITIALIZED;

    return 0;
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
int
xkrt_sync(xkrt_runtime_t * runtime)
{
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
        if (!device->offloader_streams_are_empty(XKRT_STREAM_TYPE_ALL))
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
xkrt_get_ngpus(xkrt_runtime_t * runtime, int * count)
{
    assert(count);

    *count = 0;
    for (int i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
    {
        xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t) i);
        if (driver && driver->f_get_ndevices_max)
            *count += driver->f_get_ndevices_max();
    }
    return 0;
}
