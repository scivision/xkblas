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

#ifndef __sKERNEL_H__
# define __sKERNEL_H__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    int
    xkblas_sgemm_async(
        int transA, int transB,
        int m, int n, int k,
        const float * alpha,
        const float * A, int lda,
        const float * B, int ldb,
        const float * beta,
              float * C, int ldc
    );

    int
    xkblas_ssyrk_async(
        int uplo, int trans,
        int n, int k,
        const float * alpha,
        const float * A, int lda,
        const float * beta,
              float * C, int ldc
    );

    int
    xkblas_strsm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const float * alpha,
        const float * A, int lda,
              float * B, int ldb
    );

    int
    xkblas_scopyscale_async(
        int m, int n,
        int should_copy, int * IW,
        const float * D, int ldd,
              float * L, int ldl,
              float * U, int ldu
    );

    int
    xkblas_sgemmt_async(
        int uplo,
        int transA, int transB,
        int n, int k,
        const float * alpha,
        const float * A, int lda,
        const float * B, int ldb,
        const float * beta,
              float * C, int ldc
    );

    int
    xkblas_strmm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const float * alpha,
        const float * A, int lda,
              float * B, int ldb
    );

    int
    xkblas_ssyr2k_async(
        int uplo, int trans,
        int n, int k,
        const float * alpha,
        const float * A, int lda,
        const float * B, int ldb,
        const float * beta,
              float * C, int ldc
    );

    int
    xkblas_ssymm_async(
        int side, int uplo,
        int m, int n,
        const float * alpha,
        const float * A, int lda,
        const float * B, int ldb,
        const float * beta,
              float * C, int ldc
    );

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */


#endif /* __sKERNEL_H__ */
