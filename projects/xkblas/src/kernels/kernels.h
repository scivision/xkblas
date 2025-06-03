/* ************************************************************************** */
/*                                                                            */
/*   kernels.h                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:24:51 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
