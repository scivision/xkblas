#ifndef __dKERNEL_H__
# define __dKERNEL_H__

int
xkblas_dgemm_async(
    int transA, int transB,
    int M, int N, int K,
    const double *alpha,
    const double *A, int LDA,
    const double *B, int LDB,
    const double *beta,
    double *C, int LDC
);

int
xkblas_dgemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const double *alpha,
    const double *A, int LDA,
    const double *B, int LDB,
    const double *beta,
    double *C, int LDC
);

int
xkblas_dtrsm_async(
    int side, int uplo,
    int transA, int diag,
    int N, int NRHS,
    const double *alpha,
    const double *A, int LDA,
    double *B, int LDB
);

int
xkblas_dcopyscale_async(
    int M, int N,
    int should_copy, int *IW,
    double *D, int ldd,
    double *L, int ldl,
    double *U, int ldu
);

#endif /* __dKERNEL_H__ */
