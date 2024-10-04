# include "logger/logger.h"

extern "C"
int
xkblas_£symm_async(
    int side, int uplo,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    XKBLAS_FATAL("Not implemented");
    return 0;
}

void
register_£symm_format(void)
{
    XKBLAS_IMPL("Not implemented");
}

