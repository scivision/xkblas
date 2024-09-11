# include "kernels/kernel-type.h"
# include "logger/logger.h"

void
xkblas_kernel_auto_tile(
    xkblas_kernel_type_t kernel,
    int * args,
    int * bs
) {
    switch (kernel)
    {
        case (XKBLAS_KERNEL_TYPE_GEMM):
        {
            int m = args[0];
            int n = args[1];
            int k = args[2];

            /* just one big tile */
            # if 1
            bs[0] = m / 2;
            bs[1] = n / 2;
            # else
            bs[0] = m;
            bs[1] = n;
            # endif

            break ;
        }

        default:
        {
            XKBLAS_FATAL("Tile for kernel type %d is not implemented", kernel);
            break ;
        }
    }
}
