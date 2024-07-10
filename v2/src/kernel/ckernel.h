#ifndef __cKERNEL_H__
# define __cKERNEL_H__

int
xkblas_cgemm_async(
    int transA, int transB,
    int M, int N, int K,
    const float _Complex *alpha,
    const float _Complex *A, int LDA,
    const float _Complex *B, int LDB,
    const float _Complex *beta,
    float _Complex *C, int LDC
);

int
xkblas_cgemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const float _Complex *alpha,
    const float _Complex *A, int LDA,
    const float _Complex *B, int LDB,
    const float _Complex *beta,
    float _Complex *C, int LDC
);

int
xkblas_ctrsm_async(
    int side, int uplo,
    int transA, int diag,
    int N, int NRHS,
    const float _Complex *alpha,
    const float _Complex *A, int LDA,
    float _Complex *B, int LDB
);

int
xkblas_ccopyscale_async(
    int M, int N,
    int should_copy, int *IW,
    float _Complex *D, int ldd,
    float _Complex *L, int ldl,
    float _Complex *U, int ldu
);

#endif /* __cKERNEL_H__ */
