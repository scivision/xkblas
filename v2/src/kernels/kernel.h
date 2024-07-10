#ifndef __£KERNEL_H__
# define __£KERNEL_H__

int
xkblas_£gemm_async(
    int transA, int transB,
    int M, int N, int K,
    const TYPE *alpha,
    const TYPE *A, int LDA,
    const TYPE *B, int LDB,
    const TYPE *beta,
    TYPE *C, int LDC
);

int
xkblas_£gemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const TYPE *alpha,
    const TYPE *A, int LDA,
    const TYPE *B, int LDB,
    const TYPE *beta,
    TYPE *C, int LDC
);

int
xkblas_£trsm_async(
    int side, int uplo,
    int transA, int diag,
    int N, int NRHS,
    const TYPE *alpha,
    const TYPE *A, int LDA,
    TYPE *B, int LDB
);

int
xkblas_£copyscale_async(
    int M, int N,
    int should_copy, int *IW,
    TYPE *D, int ldd,
    TYPE *L, int ldl,
    TYPE *U, int ldu
);

#endif /* __£KERNEL_H__ */
