/* ************************************************************************** */
/*                                                                            */
/*   runtime.cc                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 16:00:14 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/logger/logger.h>
# include <xkrt/driver/driver.h>
# include <xkrt/memory/alignedas.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdlib.h>
# include <string.h>
# include <signal.h>

# include <hwloc.h>

/////////////////////////////////////////////////////////////////////
//  All runtimes instances, for cleaning up if crash / interupting //
/////////////////////////////////////////////////////////////////////

// TODO : cannot do that, runtime may not be reachable memory :-(

static struct {
    xkrt_runtime_t * list;
    spinlock_t lock;
} runtimes;

static void
xkrt_runtimes_cleanup(void)
{
    xkrt_runtime_t * runtime = runtimes.list;
    while (runtime)
    {
        xkrt_drivers_deinit(runtime);
        runtime = runtime->next;
    }
}

static void
xkrt_runtimes_cleanup_signal(int signum)
{
    LOGGER_WARN("Caught signal %d, cleaning up...", signum);
    xkrt_runtimes_cleanup();
}

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

static inline void
task_format_register(xkrt_runtime_t * runtime)
{
    task_formats_init(&(runtime->formats.list));
    xkrt_memory_copy_async_register_format(runtime);
}

extern "C"
int
xkrt_init(xkrt_runtime_t * runtime)
{
    LOGGER_INFO("Initializing XKRT");

    // set TLS
    xkrt_team_t * team = NULL;
    int tid = 0;
    pthread_t pthread = pthread_self();
    xkrt_device_global_id_t device_global_id = HOST_DEVICE_GLOBAL_ID;
    xkrt_thread_place_t place;
    xkrt_runtime_t::thread_getaffinity(place);
    xkrt_thread_t * thread = new xkrt_thread_t(team, tid, pthread_self(), device_global_id, place);
    assert(thread);
    xkrt_thread_t::save_tls(thread);

    # if XKRT_SUPPORT_STATS
    memset(&runtime->stats, 0, sizeof(runtime->stats));
    # endif /* XKRT_SUPPORT_STATS */

    // set affinities to 0
    memset(&runtime->router.affinity, 0, sizeof(runtime->router.affinity));

    // init spinlock
    runtime->memcontrollers_lock = SPINLOCK_INITIALIZER;

    // create topology
    hwloc_topology_init(&runtime->topology);
    hwloc_topology_load(runtime->topology);

    // load
    xkrt_init_conf(&(runtime->conf));
    task_format_register(runtime);

    // the '+1' is to enforce the host device, always
    const int ndevices = MIN(XKRT_DEVICES_MAX, runtime->conf.device.ngpus + 1);
    xkrt_drivers_init(runtime);
    runtime->state = XKRT_RUNTIME_INITIALIZED;

    // register signal and exit function for cleaning up drivers
    SPINLOCK_LOCK(runtimes.lock);
    {
        if (runtimes.list)
        {
            runtimes.list->prev = runtime;
            runtime->next = runtimes.list;
            runtime->prev = NULL;
        }
        else
        {
            runtime->next = NULL;
            runtime->prev = NULL;
            # if 0
            signal(SIGINT,  xkrt_runtimes_cleanup_signal);
            signal(SIGTERM, xkrt_runtimes_cleanup_signal);
            atexit(xkrt_runtimes_cleanup);
            # endif
        }
        runtimes.list = runtime;
    }
    SPINLOCK_UNLOCK(runtimes.lock);

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

    SPINLOCK_LOCK(runtimes.lock);
    {
        if (runtimes.list == runtime)
            runtimes.list = runtime->next;
        if (runtime->next)
            runtime->next->prev = NULL;
    }
    SPINLOCK_UNLOCK(runtimes.lock);

    runtime->state = XKRT_RUNTIME_DEINITIALIZED;
    xkrt_drivers_deinit(runtime);
    hwloc_topology_destroy(runtime->topology);

    return 0;
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

# include <xkrt/memory-tree.hpp>

extern "C"
int
xkrt_sync(xkrt_runtime_t * runtime)
{
    assert(runtime);
    runtime->task_wait();

    # if 0
    // task dependency graph
    LOGGER_INFO("Exporting Dependency Tree...");
    Thread * thread = Thread::self();
    FILE * f = fopen("tasks.dot", "w");
    thread->dump_tasks(f);
    fclose(f);
    system("dot -Tpdf tasks.dot > tasks.pdf");
    # endif

# if 0
    LOGGER_INFO("Exporting memory tree...");

    // memory kinterval btree
    MemoryTree * memtree = (MemoryTree *) runtime->memcontrollers[0];
    memtree->export_pdf("memory");

# endif
    return 0;
}

///////////////
// UTILITIES //
///////////////

extern "C"
int
xkrt_get_ndevices(xkrt_runtime_t * runtime, int * count)
{
    assert(count);

    *count = 0;
    for (int i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
    {
        if (i != XKRT_DRIVER_TYPE_HOST)
        {
            xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t) i);
            if (driver && driver->f_get_ndevices_max)
                *count += driver->f_get_ndevices_max();
        }
    }
    return 0;
}
