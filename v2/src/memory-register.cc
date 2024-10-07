# include "xkblas-context.h"

//  Some ideas:
//      - can we pin memory independently of CUDA / HIP / ... via 'mlock' and
//      notify the driver that the memory is pinned ? Would be better in case
//      of server with different GPU vendors...

# pragma message(TODO "The current implementation is synchronous")

extern "C"
uint64_t
xkblas_register_memory_async(void * ptr, uint64_t size)
{
    # if USE_CUDA

    xkblas_driver_t * driver = xkblas_driver_get(XKBLAS_DRIVER_TYPE_CUDA);
    assert(driver);
    assert(driver->f_memory_register);
    driver->f_memory_register(ptr, size);

    # else /* USE_CUDA */

    XKBLAS_FATAL("Not implemented");

    # endif /* USE_CUDA */

    return 0;
}

extern "C"
int
xkblas_unregister_memory(void * ptr, uint64_t size)
{
    # if USE_CUDA

    xkblas_driver_t * driver = xkblas_driver_get(XKBLAS_DRIVER_TYPE_CUDA);
    assert(driver);
    assert(driver->f_memory_unregister);
    driver->f_memory_unregister(ptr, size);

    # else /* USE_CUDA */

    XKBLAS_FATAL("Not implemented");

    # endif /* USE_CUDA */

    return 0;
}

extern "C"
int
xkblas_register_memory_waitall(void)
{
    // nothing to do, as we are synchronous
    return 0;
}
