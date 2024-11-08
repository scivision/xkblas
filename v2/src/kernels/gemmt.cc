# include "logger/logger.h"

extern "C"
int
xkblas_£gemm_async(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
);

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
    return xkblas_£gemm_async( transA, transB, n, n, k, alpha, A, lda, B, ldb, beta, C, ldc );
    #pragma TODO "Implement gemmt"
    //XKBLAS_FATAL("Not implemented");
    //return 0;
}

void
register_£gemmt_format(void)
{
    XKBLAS_IMPL("Not implemented");
}
