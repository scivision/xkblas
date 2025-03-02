/* ************************************************************************** */
/*                                                                            */
/*   memory-alloc.cc                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 17:01:12 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/logger/todo.h>
# pragma message(TODO "Should we instead use an abstract interface on a specific device ? to fallback onto the driver")

# include <xkrt/xkrt.h>
# include <xkrt/xkrt-support.h>
# if XKRT_SUPPORT_CUDA
#  include <cuda_runtime.h>
# endif /* XKRT_SUPPORT_CUDA */
# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stddef.h>
# include <stdlib.h>

# include "context.h"

extern "C"
void *
xkblas_malloc(size_t size)
{
    return malloc(size);
}

extern "C"
void
xkblas_free(void * ptr, size_t size)
{
    (void) size;
    free(ptr);
}

extern "C"
void *
xkblas_host_alloc(size_t size)
{
    void * ptr = xkblas_malloc(size);
    if (ptr == NULL)
        return NULL;

    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    xkrt_memory_register(runtime, ptr, size);

    return ptr;
}

extern "C"
void
xkblas_host_free(void * ptr, size_t size)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    xkrt_memory_unregister(runtime, ptr, size);
    xkblas_free(ptr, size);
}

extern "C"
void *
xkblas_unified_alloc(size_t size)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_unified_allocate(0, size);
}

extern "C"
void
xkblas_unified_free(void * ptr, size_t size)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_unified_deallocate(0, ptr, size);
}

// Added symbols for compatibility with previous version and unified version
extern "C"
void xkblas_activate_custom_alloc(){}

extern "C"
void xkblas_deactivate_custom_alloc(){}

extern "C"
void xkblas_2D_copy(){}
