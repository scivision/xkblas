/* ************************************************************************** */
/*                                                                            */
/*   memory-alloc.cc                                              .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/07 14:28:00 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:24:08 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/logger/todo.h>
# pragma message(TODO "Should we instead use an abstract interface on a specific device ? to fallback onto the driver")

# include <xkrt/xkrt.h>
# include <xkrt/support.h>
# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stddef.h>
# include <stdlib.h>

# include "context.h"

extern "C"
void *
xkblas_host_alloc(size_t size)
{
    void * ptr = malloc(size);
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
    free(ptr);
}

extern "C"
void *
xkblas_unified_alloc(size_t size)
{
    // TODO
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_unified_allocate(1, size);
}

extern "C"
void
xkblas_unified_free(void * ptr, size_t size)
{
    // TODO
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_unified_deallocate(1, ptr, size);
}

extern "C"
void *
xkblas_malloc(size_t size)
{
    return xkblas_host_alloc(size);
}

extern "C"
void
xkblas_free(void * ptr, size_t size)
{
    xkblas_host_free(ptr, size);
}

// Added symbols for compatibility with previous version and unified version
extern "C"
void xkblas_activate_custom_alloc(){}

extern "C"
void xkblas_deactivate_custom_alloc(){}

extern "C"
void xkblas_2D_copy(){}
