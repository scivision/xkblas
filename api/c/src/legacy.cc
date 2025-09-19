/* ************************************************************************** */
/*                                                                            */
/*   legacy.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/04 15:55:36 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/18 21:30:26 by Romain PEREIRA         / _______ \       */
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
    xkblas_t * context = xkblas_get();
    assert(context);

    return context->runtime.get_ndevices_max();
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

    xkblas_t * context = xkblas_get();
    assert(context);

    for (int i = 0 ; i < XKBLAS_KERNEL_MAX ; ++i)
        context->conf.kernels[i].tile = nb;
}

extern "C"
void
xkblas_set_tile_parameter(xkblas_kernel_t kernel, size_t ts)
{
    xkblas_t * context = xkblas_get();
    assert(context);

    context->conf.kernels[kernel].tile = ts;
}

extern "C"
void
xkblas_finalize(void)
{
    xkblas_deinit();
}
