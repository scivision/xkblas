# include "xkblas-kernel-type.h"
# include "logger/logger.h"

void
xkblas_kernel_auto_tile(
    xkblas_kernel_type_t kernel,
    int * args,
    size_t * bs
) {
    switch (kernel)
    {
        case (XKBLAS_KERNEL_TYPE_GEMM):
        {
            int m = args[0];
            int n = args[1];
            int k = args[2];

            /* just one big tile */
            bs[0] = (size_t) m;
            bs[1] = (size_t) n;

            break ;
        }

        case (XKBLAS_KERNEL_TYPE_TRSM):
        {
            int m = args[0];
            int n = args[1];

            /* just one big tile */
            bs[0] = (size_t) m;
            bs[1] = (size_t) n;
            break ;
        }

        case (XKBLAS_KERNEL_TYPE_COPYSCALE):
        {
            int m = args[0];
            int n = args[1];

            /* just one big tile */
            bs[0] = (size_t) m;
            bs[1] = (size_t) n;

            break ;
        }

        default:
        {
            XKBLAS_FATAL("Tile for kernel type %d is not implemented", kernel);
            break ;
        }
    }
}
