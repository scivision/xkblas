/* ************************************************************************** */
/*                                                                            */
/*   register.cc                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/24 22:25:39 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/09/02 19:38:32 by Romain PEREIRA         / _______ \       */
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

# include <assert.h>
# include <stddef.h>
# include <stdlib.h>

# include <xkblas/xkblas.h>
# include <xkblas/xkblas.hpp>

XKRT_NAMESPACE_USE;

static inline team_t *
__get_registering_team(runtime_t * runtime)
{
    team_t * team = runtime->team_get_any(~(1 << XKRT_DRIVER_TYPE_HOST));
    if (team == NULL)
        team = runtime->team_get(XKRT_DRIVER_TYPE_HOST);
    assert(team);
    return team;
}

extern "C"
int
xkblas_memory_register_tiled_async(void * ptr, size_t size, int n)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    team_t * team = __get_registering_team(runtime);
    return runtime->memory_register_async(team, ptr, size, n);
}

extern "C"
int
xkblas_memory_unregister_tiled_async(void * ptr, size_t size, int n)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    team_t * team = __get_registering_team(runtime);
    return runtime->memory_unregister_async(team, ptr, size, n);
}

extern "C"
int
xkblas_memory_touch_tiled_async(void * ptr, size_t size, int n)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    team_t * team = __get_registering_team(runtime);
    return runtime->memory_touch_async(team, ptr, size, n);
}

extern "C"
int
xkblas_register_memory(void * ptr, uint64_t size)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_register(ptr, size);
}

extern "C"
int
xkblas_unregister_memory(void * ptr, uint64_t size)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_unregister(ptr, size);
}

extern "C"
uint64_t
xkblas_register_memory_async(void * ptr, uint64_t size)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_register_async(ptr, size);
}

extern "C"
int
xkblas_unregister_memory_async(void * ptr, uint64_t size)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_unregister_async(ptr, size);
}

extern "C"
int
xkblas_register_memory_waitall(void)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    runtime->task_wait();
    return 0;
}
