# include "logger/logger.h"

extern "C"
int
xkblas_£trmm_async(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    XKBLAS_FATAL("Not implemented");
    return 0;
}

void
register_£trmm_format(void)
{
    XKBLAS_IMPL("Not implemented");
}
