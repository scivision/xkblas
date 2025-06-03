/* ************************************************************************** */
/*                                                                            */
/*   register.cc                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/07 14:28:00 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 17:57:27 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/logger/todo.h>
# include <xkrt/runtime.h>

///////////////////////////////////
//  ORIGINAL KAAPI 1.0 INTERFACE //
///////////////////////////////////

//  General idea of these interfaces
//      - Nvidia GPUs serializes memory pinning anyway
//      - We have a single thread dedicated to pinning memory to any device
//      - it schedule 'pinning tasks' that are tasks with read/write access on the memory


//  Some ideas:
//      - can we pin memory independently of CUDA / HIP / ... via 'mlock' and
//      notify the driver that the memory is pinned ? Would be better in case
//      of server with different GPU vendors...


# pragma message(TODO "The current implementation spawn independent tasks. Maybe make it dependent to a specific type of access")

extern "C"
int
xkrt_memory_register(
    xkrt_runtime_t * runtime,
    void * ptr,
    size_t size
) {
    for (uint8_t driver_id = 0 ; driver_id < XKRT_DRIVER_TYPE_MAX; ++driver_id)
    {
        xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t) driver_id);
        if (!driver)
            continue ;
        if (!driver->f_memory_host_register)
            LOGGER_WARN("Driver `%u` does not implement memory register", driver_id);
        else if (driver->f_memory_host_register(ptr, size))
            LOGGER_ERROR("Could not register memory for driver `%s`", driver->f_get_name());
    }
    return 0;
}

extern "C"
int
xkrt_memory_unregister(xkrt_runtime_t * runtime, void * ptr, size_t size)
{
    for (uint8_t driver_id = 0 ; driver_id < XKRT_DRIVER_TYPE_MAX; ++driver_id)
    {
        xkrt_driver_t * driver = runtime->driver_get((xkrt_driver_type_t) driver_id);
        if (!driver)
            continue ;
        if (!driver->f_memory_host_unregister)
            LOGGER_WARN("Driver `%u` does not implement memory unregister", driver_id);
        else if (driver->f_memory_host_unregister(ptr, size))
            LOGGER_ERROR("Could not unregister memory for driver `%s`", driver->f_get_name());
    }
    return 0;
}

extern "C"
size_t
xkrt_memory_register_async(xkrt_runtime_t * runtime, void * ptr, size_t size)
{
    xkrt_driver_t * driver = runtime->driver_get(XKRT_DRIVER_TYPE_HOST);
    assert(driver);

    xkrt_team_t * team = &driver->team;

    // TODO : could be optimized using a custom format for register tasks
    runtime->team_task_spawn(
        team,
        [runtime, ptr, size] (task_t * task) {
            LOGGER_DEBUG("register memory...");
            xkrt_memory_register(runtime, ptr, size);
        }
    );

    return 0;
}

extern "C"
int
xkrt_memory_unregister_async(xkrt_runtime_t * runtime, void * ptr, size_t size)
{
    xkrt_driver_t * driver = runtime->driver_get(XKRT_DRIVER_TYPE_HOST);
    assert(driver);

    xkrt_team_t * team = &driver->team;

    // TODO : could be optimized using a custom format for unregister tasks
    runtime->team_task_spawn(
        team,
        [runtime, ptr, size] (task_t * task) {
            LOGGER_DEBUG("unregistering memory...");
            xkrt_memory_unregister(runtime, ptr, size);
        }
    );
    return 0;
}

extern "C"
int
xkrt_memory_register_waitall(xkrt_runtime_t * runtime)
{
    // atm, waits for all children tasks
    // instead, we probably want to wait only on register tasks

    runtime->task_wait();
    return 0;
}

//////////////////////////
//  KAAPI 2.0 INTERFACE //
//////////////////////////


