/* ************************************************************************** */
/*                                                                            */
/*   auto-tile.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/11 16:58:39 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:22:52 by Romain PEREIRA         / _______ \       */
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
# include "xkblas/kernel-type.h"

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <math.h>

static int
is_power_of_two(int n)
{
    return n && (n & (n - 1)) == 0;
}

void
xkblas_kernel_auto_tile(
    xkblas_kernel_type_t kernel,
    int * args,
    size_t * ts
) {
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    const int ngpus        = context->runtime.drivers.devices.n;
    const int nstream_kern = context->runtime.conf.device.offloader.streams[XKRT_STREAM_TYPE_KERN].n;

    size_t ts_auto = 0;
    switch (kernel)
    {
        case (XKBLAS_KERNEL_TYPE_GEMM):
        case (XKBLAS_KERNEL_TYPE_TRSM):
        case (XKBLAS_KERNEL_TYPE_COPYSCALE):
        {
            int m = args[0];
            int n = args[1];

            const double f = 4.0;
            ts_auto = (size_t) ceil(sqrt((double)m*(double)n / (f * (double)(ngpus * nstream_kern))));

            break ;
        }

        default:
        {
            LOGGER_FATAL("Tile for kernel type %d is not implemented", kernel);
            break ;
        }
    }

    # define MIN_BS 2048
    if (ts_auto < MIN_BS)
        ts_auto = MIN_BS;

    *ts = ts_auto;

    LOGGER_DEBUG("Return tile size = %lu", *ts);
}
