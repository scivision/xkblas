/* ************************************************************************** */
/*                                                                            */
/*   auto-tile.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/11 16:58:39 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/25 18:56:56 by Romain PEREIRA         / _______ \       */
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
# include <xkrt/logger/logger.h>

# include <assert.h>
# include <math.h>

# if 0
static int
is_power_of_two(int n)
{
    return n && (n & (n - 1)) == 0;
}
# endif

void
xkblas_kernel_auto_tile(
    xkblas_kernel_t kernel,
    int * args,
    size_t * ts
) {
    xkblas_t * context = xkblas_get();
    assert(context);

    const int ngpus        = context->runtime.drivers.devices.n;
    const int nstream_kern = context->runtime.conf.device.offloader.streams[xkrt::STREAM_TYPE_KERN].n;

    size_t ts_auto = 0;
    switch (kernel)
    {
        case (AXPY):
        case (GEMV):
        {
            ts_auto = 2048;
            break ;
        }

        case (GEMM):
        case (TRSM):
        case (COPYSCALE):
        {
            int m = args[0];
            int n = args[1];

            const double f = 4.0;
            ts_auto = (size_t) ceil(sqrt((double)m*(double)n / (f * (double)(ngpus * nstream_kern))));

            break ;
        }

        case (SPMV):
        {
            int m   = args[0];
            int n   = args[1];
            int nnz = args[2];
            ts_auto = (size_t) m;
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
