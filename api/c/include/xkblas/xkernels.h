/* ************************************************************************** */
/*                                                                            */
/*   xkernels.h                                                   .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/26 15:26:58 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __£KERNEL_H__
# define __£KERNEL_H__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    /* Level 1 */
    int xkblas_£axpby_async(int n, const TYPE alpha, const TYPE * x, const TYPE beta, TYPE * y);

    int xkblas_£axpy_async(int n, const TYPE alpha, const TYPE * x, TYPE * y);

    int xkblas_£dot_async(const TYPE * x, const TYPE * y, TYPE * result);

    int xkblas_£divcopy_async();    // TODO

    int xkblas_£fill(int n, TYPE * x, const TYPE v);

    int xkblas_£nrm2_async(int n, const TYPE * x, float * result);

    int xkblas_£scalcopy_async();    // TODO

    int xkblas_£scale_async(int n, const TYPE s, const TYPE * x);

    /* Level 2 */

    int
    xkblas_£copyscale_async(
        int m, int n,
        int should_copy, int * IW,
        const TYPE * D, int ldd,
              TYPE * L, int ldl,
              TYPE * U, int ldu
    );

    int
    xkblas_£gemv_async(
        int transA,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * x, int incx,
        const TYPE * beta,
              TYPE * y, int incy
    );

    /* Level 3 */
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
    );

    int
    xkblas_£herk_async(
        int uplo, int transA,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    int
    xkblas_£syrk_async(
        int uplo, int trans,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    int
    xkblas_£trsm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
              TYPE * B, int ldb
    );

    int
    xkblas_£trmm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
              TYPE * B, int ldb
    );

    int
    xkblas_£syr2k_async(
        int uplo, int trans,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    int
    xkblas_£symm_async(
        int side, int uplo,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    int
    xkblas_£potrf_async(
        int uplo,
        int n,
        TYPE * A,
        int lda
    );

    int
    xkblas_£spmv_async(
        const TYPE * alpha,
        /* matrix A (in) */
        int transA,
        int index_base,
        int index_type,
        const int nrows,
        const int ncols,
        const int nnz,
        const int format,
        const void * csr_row_offsets,
        const void * csr_col_indices,
        const TYPE * csr_values,
        /* vector X (in) */
        TYPE * X,
        const TYPE * beta,
        /* vector Y (inout) */
        TYPE * Y
    );

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */


#endif /* __£KERNEL_H__ */
