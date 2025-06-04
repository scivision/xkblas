/* ************************************************************************** */
/*                                                                            */
/*   legacy.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/04 15:55:36 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:24:05 by Romain PEREIRA         / _______ \       */
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

# include "context.h"
# include "xkblas.h"
# include "xkblas/kernel-type.h"

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>

# include <assert.h>

extern "C"
void
xkblas_set_modemath(xkblas_mode_math_t mode)
{
    LOGGER_FATAL("Not implemented");
}

extern "C"
int
xkblas_get_ngpus(void)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    int count = 0;
    xkrt_get_ndevices_max(&(context->runtime), &count);
    return count;
}

extern "C"
int
xkblas_get_device_count(int * count)
{
    LOGGER_IMPL("RETURNED 1 lol");
    *count = 1;
    return 0;

    LOGGER_FATAL("Not implemented");
    return -1;
}

extern "C"
void
xkblas_set_param(size_t nb, size_t p)
{
    (void) p;
//    LOGGER_IMPL("`p` unused");

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    for (int i = 0 ; i < XKBLAS_KERNEL_TYPE_MAX ; ++i)
        context->conf.kernels[i].tile = nb;
}

extern "C"
void
xkblas_finalize(void)
{
    xkblas_deinit();
}
