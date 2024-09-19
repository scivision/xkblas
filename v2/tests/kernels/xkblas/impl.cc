# include <assert.h>

# include "common/impl.hpp"
# include "xkblas.h"
# include "xkblas-kernel.h"

/* Implementation name */
const char *
impl_t::name(void) const
{
    return "xkblas";
}

/* Init */
void
impl_t::init(void)
{
    xkblas_init();
}

void
impl_t::deinit(void)
{
    xkblas_deinit();
}

/* wait for kernels completion */
void
impl_t::wait(void)
{
    xkblas_sync();
}

void
impl_t::gemm(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    xkblas_sgemm_async(
        transA, transB,
        m, n, k,
        alpha,
        A, lda,
        B, ldb,
        beta,
        C, ldc
    );

    int uplo = 0;
    int memflag = 0;
    xkblas_memory_coherent_async(uplo, memflag, m, n, C, ldc, sizeof(TYPE));
}

void
impl_t::trsm(
    int side, int uplo,
    CBLAS_TRANSPOSE transA, int diag,
    const BLAS_INT m, const BLAS_INT n,
    const TYPE * alpha,
    const TYPE * A, const BLAS_INT lda,
          TYPE * B, const BLAS_INT ldb
) {
    xkblas_strsm_async(
        side, uplo,
        transA, diag,
        m, n,
        alpha,
        A, lda,
        B, ldb
    );

    int memflag = 0;
    xkblas_memory_coherent_async(uplo, memflag, m, n, B, ldb, sizeof(TYPE));
}
