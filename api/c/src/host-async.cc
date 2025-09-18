/* ************************************************************************** */
/*                                                                            */
/*   host-async.cc                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/04 15:55:36 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/18 02:46:31 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkblas/xkblas.hpp>
# include <xkblas/xkblas.h>

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>

# include <assert.h>

XKRT_NAMESPACE_USE;

extern "C"
void
xkblas_host_async(
    void (*func)(void *),
    void * args
) {
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    team_t * team = runtime->team_get(XKRT_DRIVER_TYPE_HOST);
    runtime->team_task_spawn(team, [=] (task_t *) { func(args); });
}
