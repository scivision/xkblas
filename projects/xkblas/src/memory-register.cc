/* ************************************************************************** */
/*                                                                            */
/*   memory-register.cc                                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/11 23:14:07 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
