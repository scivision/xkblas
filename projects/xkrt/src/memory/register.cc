/* ************************************************************************** */
/*                                                                            */
/*   register.cc                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/21 21:57:55 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/logger/todo.h>
# include <xkrt/runtime.h>

//  Some ideas:
//      - can we pin memory independently of CUDA / HIP / ... via 'mlock' and
//      notify the driver that the memory is pinned ? Would be better in case
//      of server with different GPU vendors...

# pragma message(TODO "The current implementation is synchronous")

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
    return xkrt_memory_register(runtime, ptr, size); // not async for now
}

extern "C"
int
xkrt_memory_unregister_async(xkrt_runtime_t * runtime, void * ptr, size_t size)
{
    return xkrt_memory_unregister(runtime, ptr, size); // not async for now
}

extern "C"
int
xkrt_memory_register_waitall(xkrt_runtime_t * runtime)
{
    // nothing to do, as we are synchronous
    (void) runtime;
    return 0;
}
