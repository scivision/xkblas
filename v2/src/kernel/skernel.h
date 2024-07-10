#ifndef __sKERNEL_H__
# define __sKERNEL_H__

int
xkblas_sgemm_async(
    int transA, int transB,
    int M, int N, int K,
    const float *alpha,
    const float *A, int LDA,
    const float *B, int LDB,
    const float *beta,
    float *C, int LDC
);

int
xkblas_sgemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const float *alpha,
    const float *A, int LDA,
    const float *B, int LDB,
    const float *beta,
    float *C, int LDC
);

int
xkblas_strsm_async(
    int side, int uplo,
    int transA, int diag,
    int N, int NRHS,
    const float *alpha,
    const float *A, int LDA,
    float *B, int LDB
);

int
xkblas_scopyscale_async(
    int M, int N,
    int should_copy, int *IW,
    float *D, int ldd,
    float *L, int ldl,
    float *U, int ldu
);

#endif /* __sKERNEL_H__ */
