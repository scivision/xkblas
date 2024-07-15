#ifndef __zKERNEL_H__
# define __zKERNEL_H__

int
xkblas_zgemm_async(
    int transA, int transB,
    int M, int N, int K,
    const double _Complex *alpha,
    const double _Complex *A, int LDA,
    const double _Complex *B, int LDB,
    const double _Complex *beta,
    double _Complex *C, int LDC
);

int
xkblas_zgemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const double _Complex *alpha,
    const double _Complex *A, int LDA,
    const double _Complex *B, int LDB,
    const double _Complex *beta,
    double _Complex *C, int LDC
);

int
xkblas_ztrsm_async(
    int side, int uplo,
    int transA, int diag,
    int N, int NRHS,
    const double _Complex *alpha,
    const double _Complex *A, int LDA,
    double _Complex *B, int LDB
);

int
xkblas_zcopyscale_async(
    int M, int N,
    int should_copy, int *IW,
    double _Complex *D, int ldd,
    double _Complex *L, int ldl,
    double _Complex *U, int ldu
);

#endif /* __zKERNEL_H__ */
