/* ************************************************************************** */
/*                                                                            */
/*   auto-tile.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 19:36:30 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "xkblas/kernel-type.h"

# include <kaapi/runtime.h>
# include <kaapi/logger/logger.h>

# include <assert.h>
# include <math.h>

void
xkblas_kernel_auto_tile(
    xkblas_kernel_type_t kernel,
    int * args,
    size_t * ts
) {
    kaapi_runtime_t * runtime = kaapi_runtime_get();
    assert(runtime);

    const int ngpus        = runtime->drivers.devices.n;
    const int nstream_kern = runtime->conf.device.offloader.streams[KAAPI_STREAM_TYPE_KERN].n;

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
