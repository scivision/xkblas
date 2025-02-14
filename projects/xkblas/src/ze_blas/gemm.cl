__kernel
void
GEMM_NAIVE_SRC(
    int m, int n, int k,
    const TYPE alpha,
    __global const TYPE * A, int lda,
    __global const TYPE * B, int ldb,
    const TYPE beta,
    __global       TYPE * C, int ldc
) {
  int j = get_global_id(0);
  int i = get_global_id(1);
  TYPE sum = 0.0f;
  for (int kk = 0; kk < k; ++kk)
      sum += a[i * lda + k] * b[k * ldb + j];
  C[i * ldc + j] = sum;
}
