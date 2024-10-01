# include <assert.h>

# include "common/impl.hpp"
# include "xkblas.h"
# include "xkblas-kernel.h"
# include "xkblas-context.h"

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
}

void
impl_t::copyscale(
    const BLAS_INT m, const BLAS_INT n,
    bool should_copy, int * IW,
    const TYPE * D, const int ldd,
          TYPE * L, const int ldl,
          TYPE * U, const int ldu
) {
    xkblas_scopyscale_async(m, n, should_copy, IW, D, ldd, L, ldl, U, ldu);
}

void
impl_t::coherent(
    TYPE * M,
    int m, int n,
    int ld
) {
    int memflag = 0;
    int uplo = 0;
    xkblas_memory_coherent_async(uplo, memflag, m, n, M, ld, sizeof(TYPE));
}

void
impl_t::set_tile(
    int m, int n
) {
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    for (int i = 0 ; i < XKBLAS_KERNEL_TYPE_MAX ; ++i)
    {
        context->conf.kernels[i].tile[0] = m;
        context->conf.kernels[i].tile[1] = n;
    }
}

