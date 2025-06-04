/* ************************************************************************** */
/*                                                                            */
/*   runtime.cc                                                   .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/15 17:01:38 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/04 16:31:07 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

static inline void
task_format_register(xkrt_runtime_t * runtime)
{
    task_formats_init(&(runtime->formats.list));
    xkrt_memory_copy_async_register_format(runtime);
    xkrt_task_host_capture_register_format(runtime);
    xkrt_memory_touch_async_register_format(runtime);
}

extern "C"
int
xkrt_init(xkrt_runtime_t * runtime)
{
    LOGGER_INFO("Initializing XKRT");

    // set TLS
    xkrt_team_t * team = NULL;
    int tid = 0;
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

    // create topology
    hwloc_topology_init(&runtime->topology);
    hwloc_topology_load(runtime->topology);

    // load
    xkrt_init_conf(&(runtime->conf));
    task_format_register(runtime);

    // the '+1' is to enforce the host device, always
    xkrt_drivers_init(runtime);
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

    runtime->state = XKRT_RUNTIME_DEINITIALIZED;
    xkrt_drivers_deinit(runtime);
    hwloc_topology_destroy(runtime->topology);

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
    runtime->task_wait();

    return 0;
}

///////////////
// UTILITIES //
///////////////

extern "C"
int
xkrt_get_ndevices_max(xkrt_runtime_t * runtime, int * count)
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

unsigned int
xkrt_runtime_t::get_ndevices(void)
{
    return this->drivers.devices.n;
}
