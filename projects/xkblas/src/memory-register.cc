/* ************************************************************************** */
/*                                                                            */
/*   memory-register.cc                                           .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/24 22:25:39 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:24:22 by Romain PEREIRA         / _______ \       */
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
# pragma message(TODO "Replace uint64_t with size_t")

# include <xkblas.h>

# include <assert.h>
# include <stddef.h>
# include <stdlib.h>

# include "context.h"

extern "C"
int
xkblas_register_memory(void * ptr, uint64_t size)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_memory_register(runtime, ptr, size);
}

extern "C"
int
xkblas_unregister_memory(void * ptr, uint64_t size)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_memory_unregister(runtime, ptr, size);
}

extern "C"
uint64_t
xkblas_register_memory_async(void * ptr, uint64_t size)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_memory_register_async(runtime, ptr, size);
}

extern "C"
int
xkblas_unregister_memory_async(void * ptr, uint64_t size)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_memory_unregister_async(runtime, ptr, size);
}

extern "C"
int
xkblas_register_memory_waitall(void)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_memory_register_waitall(runtime);
}
