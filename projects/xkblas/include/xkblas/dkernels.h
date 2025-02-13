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

#ifndef __dKERNEL_H__
# define __dKERNEL_H__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    int
    xkblas_dgemm_async(
        int transA, int transB,
        int m, int n, int k,
        const double * alpha,
        const double * A, int lda,
        const double * B, int ldb,
        const double * beta,
              double * C, int ldc
    );

    int
    xkblas_dsyrk_async(
        int uplo, int trans,
        int n, int k,
        const double * alpha,
        const double * A, int lda,
        const double * beta,
              double * C, int ldc
    );

    int
    xkblas_dtrsm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const double * alpha,
        const double * A, int lda,
              double * B, int ldb
    );

    int
    xkblas_dcopyscale_async(
        int m, int n,
        int should_copy, int * IW,
        const double * D, int ldd,
              double * L, int ldl,
              double * U, int ldu
    );

    int
    xkblas_dgemmt_async(
        int uplo,
        int transA, int transB,
        int n, int k,
        const double * alpha,
        const double * A, int lda,
        const double * B, int ldb,
        const double * beta,
              double * C, int ldc
    );

    int
    xkblas_dtrmm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const double * alpha,
        const double * A, int lda,
              double * B, int ldb
    );

    int
    xkblas_dsyr2k_async(
        int uplo, int trans,
        int n, int k,
        const double * alpha,
        const double * A, int lda,
        const double * B, int ldb,
        const double * beta,
              double * C, int ldc
    );

    int
    xkblas_dsymm_async(
        int side, int uplo,
        int m, int n,
        const double * alpha,
        const double * A, int lda,
        const double * B, int ldb,
        const double * beta,
              double * C, int ldc
    );

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */


#endif /* __dKERNEL_H__ */
