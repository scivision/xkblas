/* ************************************************************************** */
/*                                                                            */
/*   kernel.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __cKERNEL_H__
# define __cKERNEL_H__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    int
    xkblas_cgemm_async(
        int transA, int transB,
        int m, int n, int k,
        const float _Complex * alpha,
        const float _Complex * A, int lda,
        const float _Complex * B, int ldb,
        const float _Complex * beta,
              float _Complex * C, int ldc
    );

    int
    xkblas_csyrk_async(
        int uplo, int trans,
        int n, int k,
        const float _Complex * alpha,
        const float _Complex * A, int lda,
        const float _Complex * beta,
              float _Complex * C, int ldc
    );

    int
    xkblas_ctrsm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const float _Complex * alpha,
        const float _Complex * A, int lda,
              float _Complex * B, int ldb
    );

    int
    xkblas_ccopyscale_async(
        int m, int n,
        int should_copy, int * IW,
        const float _Complex * D, int ldd,
              float _Complex * L, int ldl,
              float _Complex * U, int ldu
    );

    int
    xkblas_cgemmt_async(
        int uplo,
        int transA, int transB,
        int n, int k,
        const float _Complex * alpha,
        const float _Complex * A, int lda,
        const float _Complex * B, int ldb,
        const float _Complex * beta,
              float _Complex * C, int ldc
    );

    int
    xkblas_ctrmm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const float _Complex * alpha,
        const float _Complex * A, int lda,
              float _Complex * B, int ldb
    );

    int
    xkblas_csyr2k_async(
        int uplo, int trans,
        int n, int k,
        const float _Complex * alpha,
        const float _Complex * A, int lda,
        const float _Complex * B, int ldb,
        const float _Complex * beta,
              float _Complex * C, int ldc
    );

    int
    xkblas_csymm_async(
        int side, int uplo,
        int m, int n,
        const float _Complex * alpha,
        const float _Complex * A, int lda,
        const float _Complex * B, int ldb,
        const float _Complex * beta,
              float _Complex * C, int ldc
    );

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */


#endif /* __cKERNEL_H__ */
