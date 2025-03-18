/* ************************************************************************** */
/*                                                                            */
/*   impl.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/18 21:15:04 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <assert.h>
# include <unistd.h>

# include "common/impl.hpp"

# include <xkblas/xkblas.h>
# include <xkblas/skernels.h>
# include <xkblas/xkblas-experimental.h>

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
    // xkblas_deinit();
    xkblas_finalize();
}

/* allocate host memory */
uintptr_t
impl_t::alloc(size_t size)
{
    # if XKBLAS_ACCESS_SCOPE == ACCESS_SCOPE_NONUNIFIED
    return (uintptr_t) xkblas_host_alloc(size);
    # elif XKBLAS_ACCESS_SCOPE == ACCESS_SCOPE_UNIFIED
    return (uintptr_t) xkblas_unified_alloc(size);
    # else
    LOGGER_FATAL("Wtf");
    # endif
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
impl_t::syrk(
    int uplo, int trans,
    int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    xkblas_ssyrk_async(
        uplo, trans,
        n, k,
        alpha,
        A, lda,
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
impl_t::set_tile(int ts)
{
    int p = 0;
    xkblas_set_param(ts, p);
}

void
impl_t::reset(void)
{
    xkblas_memory_invalidate_caches();
}
