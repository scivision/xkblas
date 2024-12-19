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

# include <ptr/runtime.h>

//  Some ideas:
//      - can we pin memory independently of CUDA / HIP / ... via 'mlock' and
//      notify the driver that the memory is pinned ? Would be better in case
//      of server with different GPU vendors...

# pragma message(TODO "The current implementation is synchronous")

extern "C"
uint64_t
ptr_register_memory_async(void * ptr, uint64_t size)
{
    # if USE_CUDA

    ptr_driver_t * driver = ptr_driver_get(PTR_DRIVER_TYPE_CUDA);
    assert(driver);
    assert(driver->f_memory_register);
    driver->f_memory_register(ptr, size);

    # else /* USE_CUDA */

    LOGGER_FATAL("Not implemented");

    # endif /* USE_CUDA */

    return 0;
}

extern "C"
int
ptr_unregister_memory(void * ptr, uint64_t size)
{
    # if USE_CUDA

    ptr_driver_t * driver = ptr_driver_get(PTR_DRIVER_TYPE_CUDA);
    assert(driver);
    assert(driver->f_memory_unregister);
    driver->f_memory_unregister(ptr, size);

    # else /* USE_CUDA */

    LOGGER_FATAL("Not implemented");

    # endif /* USE_CUDA */

    return 0;
}

extern "C"
int
ptr_unregister_memory_async(void * ptr, uint64_t size)
{
    return ptr_unregister_memory( ptr, size ); // not async ...
}

extern "C"
int
ptr_register_memory_waitall(void)
{
    // nothing to do, as we are synchronous
    return 0;
}

extern "C"
uint64_t
ptr_register_memory(void * ptr, uint64_t size)
{
    uint64_t ret = ptr_register_memory_async(ptr, size);
    ptr_register_memory_waitall();
    return ret;
}
