# include "logger/logger.h"

extern "C"
int
xkblas_£syr2k_async(
    int uplo, int trans,
    int n, int k,
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
register_£syr2k_format(void)
{
    XKBLAS_IMPL("Not implemented");
}
