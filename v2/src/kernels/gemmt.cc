# include "logger/logger.h"

extern "C"
int
xkblas_£gemmt_async(
    int uplo,
    int transA, int transB,
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
register_£gemmt_format(void)
{
    XKBLAS_IMPL("Not implemented");
}
