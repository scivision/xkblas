#ifndef __£KERNEL_H__
# define __£KERNEL_H__

int
xkblas_£gemm_async(
    int transA, int transB,
    int m, int n, int k,
    const TYPE *alpha,
    const TYPE *A, int lda,
    const TYPE *B, int ldb,
    const TYPE *beta,
    TYPE *C, int ldc
);

int
xkblas_£trsm_async(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE *alpha,
    const TYPE *A, int lda,
    TYPE *B, int ldb
);

int
xkblas_£copyscale_async(
    int m, int n,
    int should_copy, int *IW,
    TYPE *D, int ldd,
    TYPE *L, int ldl,
    TYPE *U, int ldu
);

#endif /* __£KERNEL_H__ */
