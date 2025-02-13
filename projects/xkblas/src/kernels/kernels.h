/* ************************************************************************** */
/*                                                                            */
/*   kernels.h                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __£KERNEL_H__
# define __£KERNEL_H__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

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
    xkblas_£copyscale_async(
        int m, int n,
        int should_copy, int * IW,
        const TYPE * D, int ldd,
              TYPE * L, int ldl,
              TYPE * U, int ldu
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

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */


#endif /* __£KERNEL_H__ */
