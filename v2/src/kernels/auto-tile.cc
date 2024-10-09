# include "xkblas-context.h"
# include "xkblas-kernel-type.h"
# include "logger/logger.h"

# include <math.h>

void
xkblas_kernel_auto_tile(
    xkblas_kernel_type_t kernel,
    int * args,
    size_t * bs
) {

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    const int ngpus        = context->drivers.devices.n;
    const int nstream_kern = context->conf.device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].n;

    size_t bs_auto = 0;
    switch (kernel)
    {
        case (XKBLAS_KERNEL_TYPE_GEMM):
        case (XKBLAS_KERNEL_TYPE_TRSM):
        case (XKBLAS_KERNEL_TYPE_COPYSCALE):
        {
            int m = args[0];
            int n = args[1];

            const double f = 4.0;
            bs_auto = (size_t) ceil(sqrt((double)m*(double)n / (f * (double)(ngpus * nstream_kern))));

            break ;
        }

        default:
        {
            XKBLAS_FATAL("Tile for kernel type %d is not implemented", kernel);
            break ;
        }
    }

    # define MIN_BS 2048
    if (bs_auto < MIN_BS)
        bs_auto = MIN_BS;

    bs[0] = bs_auto;
    bs[1] = bs_auto;

    XKBLAS_DEBUG("Return tile size = (%d,%d)", bs[0], bs[1]);
}
