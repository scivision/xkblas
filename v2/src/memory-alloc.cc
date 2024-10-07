# include "xkblas-context.h"

# pragma message(TODO "Should we instead use an abstract interface on a specific device ? to fallback onto the driver")

# if USE_CUDA
#  include <cuda_runtime.h>
# endif /* USE_CUDA */

# include <stddef.h>

extern "C"
void *
xkblas_host_alloc(size_t size)
{
    # if USE_HIP
    #   error "Implement me"
    # elif USE_CUDA
    void * ptr;
    int err = cudaHostAlloc(&ptr, size, cudaHostAllocPortable);
    assert(err == cudaSuccess);
    return ptr;
    # else
    return malloc(size);
    # endif
}

extern "C"
void *
xkblas_malloc(size_t size)
{
    return xkblas_host_alloc(size);
}

extern "C"
void
xkblas_host_free(void * ptr, size_t size)
{
    # if USE_HIP
    #   error "Implement me"
    # elif USE_CUDA
    int err = cudaFreeHost(ptr);
    assert(err == cudaSuccess);
    # else
    free(ptr);
    # endif
}

extern "C"
void
xkblas_free(void * ptr, size_t size)
{
    return xkblas_host_free(ptr, size);
}
