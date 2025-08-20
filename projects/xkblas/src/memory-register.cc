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
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
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

# include "xkblas/v2.hpp"

static inline xkrt_team_t *
__get_registering_team(xkrt_runtime_t * runtime)
{
    xkrt_team_t * team = runtime->team_get_any(~(1 << XKRT_DRIVER_TYPE_HOST));
    if (team == NULL)
        team = runtime->team_get(XKRT_DRIVER_TYPE_HOST);
    assert(team);
    return team;
}

extern "C"
int
xkblas_memory_register_tiled_async(void * ptr, size_t size, int n)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    xkrt_team_t * team = __get_registering_team(runtime);
    return runtime->memory_register_async(team, ptr, size, n);
}

extern "C"
int
xkblas_memory_unregister_tiled_async(void * ptr, size_t size, int n)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    xkrt_team_t * team = __get_registering_team(runtime);
    return runtime->memory_unregister_async(team, ptr, size, n);
}

extern "C"
int
xkblas_memory_touch_tiled_async(void * ptr, size_t size, int n)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    xkrt_team_t * team = __get_registering_team(runtime);
    return runtime->memory_touch_async(team, ptr, size, n);
}

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
