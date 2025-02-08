/* ************************************************************************** */
/*                                                                            */
/*   memory-register.cc                                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:04:00 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

//  Some ideas:
//      - can we pin memory independently of CUDA / HIP / ... via 'mlock' and
//      notify the driver that the memory is pinned ? Would be better in case
//      of server with different GPU vendors...

# pragma message(TODO "The current implementation is synchronous")

extern "C"
uint64_t
xkrt_register_memory(xkrt_runtime_t * runtime, void * ptr, uint64_t size)
{
    for (uint8_t driver_id = 0 ; driver_id < XKRT_DRIVER_TYPE_MAX; ++driver_id)
    {
        xkrt_driver_t * driver = runtime->drivers.list + driver_id;
        if (driver->f_memory_register)
            driver->f_memory_register(ptr, size);
    }
    return 0;
}

extern "C"
int
xkrt_unregister_memory(xkrt_runtime_t * runtime, void * ptr, uint64_t size)
{
    for (uint8_t driver_id = 0 ; driver_id < XKRT_DRIVER_TYPE_MAX; ++driver_id)
    {
        xkrt_driver_t * driver = runtime->drivers.list + driver_id;
        if (driver->f_memory_unregister)
            driver->f_memory_unregister(ptr, size);
    }
    return 0;
}

extern "C"
uint64_t
xkrt_register_memory_async(xkrt_runtime_t * runtime, void * ptr, uint64_t size)
{
    return xkrt_register_memory(runtime, ptr, size); // not async for now
}

extern "C"
int
xkrt_unregister_memory_async(xkrt_runtime_t * runtime, void * ptr, uint64_t size)
{
    return xkrt_unregister_memory(runtime, ptr, size); // not async for now
}

extern "C"
int
xkrt_register_memory_waitall(xkrt_runtime_t * runtime)
{
    // nothing to do, as we are synchronous
    return 0;
}
