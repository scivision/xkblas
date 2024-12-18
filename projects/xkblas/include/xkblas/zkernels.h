/* ************************************************************************** */
/*                                                                            */
/*   zkernels.h                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/18 16:21:58 by                           \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __zKERNELS_H__
# define __zKERNELS_H__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    int
    xkblas_zgemm_async(
        int transA, int transB,
        int m, int n, int k,
        const double _Complex * alpha,
        const double _Complex * A, int lda,
        const double _Complex * B, int ldb,
        const double _Complex * beta,
              double _Complex * C, int ldc
    );

    int
    xkblas_zsyrk_async(
        int uplo, int trans,
        int n, int k,
        const double _Complex * alpha,
        const double _Complex * A, int lda,
        const double _Complex * beta,
              double _Complex * C, int ldc
    );

    int
    xkblas_ztrsm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const double _Complex * alpha,
        const double _Complex * A, int lda,
              double _Complex * B, int ldb
    );

    int
    xkblas_zcopyscale_async(
        int m, int n,
        int should_copy, int * IW,
        const double _Complex * D, int ldd,
              double _Complex * L, int ldl,
              double _Complex * U, int ldu
    );

    int
    xkblas_zgemmt_async(
        int uplo,
        int transA, int transB,
        int n, int k,
        const double _Complex * alpha,
        const double _Complex * A, int lda,
        const double _Complex * B, int ldb,
        const double _Complex * beta,
              double _Complex * C, int ldc
    );

    int
    xkblas_ztrmm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const double _Complex * alpha,
        const double _Complex * A, int lda,
              double _Complex * B, int ldb
    );

    int
    xkblas_zsyr2k_async(
        int uplo, int trans,
        int n, int k,
        const double _Complex * alpha,
        const double _Complex * A, int lda,
        const double _Complex * B, int ldb,
        const double _Complex * beta,
              double _Complex * C, int ldc
    );

    int
    xkblas_zsymm_async(
        int side, int uplo,
        int m, int n,
        const double _Complex * alpha,
        const double _Complex * A, int lda,
        const double _Complex * B, int ldb,
        const double _Complex * beta,
              double _Complex * C, int ldc
    );

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */


#endif /* __zKERNEL_H__ */
