/* ************************************************************************** */
/*                                                                            */
/*   xkblas-bkernel.h                                                         */
/*                                                                     *      */
/*   Authors: rpereira <romain.pereira@inria.fr>         *      .-*-.         */
/*                                                            .'* *.'         */
/*   Created: 2024/12/17 12:50:10 by rpereira        *     __/_*_*(_     *    */
/*   Updated: 2024/12/17 13:03:49 by Romain PEREIRA            \_)     (_/    */
/*                                                        \_)     (_/         */
/*   License: CeCILL-C                                *                *      */
/*                                                                            */
/* ************************************************************************** */

#ifndef __bKERNEL_H__
# define __bKERNEL_H__

int
xkblas_bgemm_async(
    int transA, int transB,
    int M, int N, int K,
    const char *alpha,
    const char *A, int LDA,
    const char *B, int LDB,
    const char *beta,
    char *C, int LDC
);

int
xkblas_bgemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const char *alpha,
    const char *A, int LDA,
    const char *B, int LDB,
    const char *beta,
    char *C, int LDC
);

int
xkblas_btrsm_async(
    int side, int uplo,
    int transA, int diag,
    int N, int NRHS,
    const char *alpha,
    const char *A, int LDA,
    char *B, int LDB
);

int
xkblas_bcopyscale_async(
    int M, int N,
    int should_copy, int *IW,
    char *D, int ldd,
    char *L, int ldl,
    char *U, int ldu
);

#endif /* __bKERNEL_H__ */
