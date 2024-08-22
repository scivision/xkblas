# include "xkblas.h"

# include <assert.h>
# include <stdlib.h>

# if 0
# include "xkblas-zkernel.h"
# define TYPE               double _Complex
# define xkblas_gemm_async  xkblas_zgemm_async
# else
# include "xkblas-bkernel.h"
# define TYPE               char
# define xkblas_gemm_async  xkblas_bgemm_async
# endif

int
main(void)
{
    xkblas_init();
    xkblas_thread_init();

    int transA = CblasNoTrans;
    int transB = CblasNoTrans;
    int M = 16;
    int N = M;
    int K = M;
    int LDA = K+M;
    int LDB = K+M;
    int LDC = K+M;
    TYPE alpha = 1.0;
    TYPE beta  = 1.0;

    # if 0
    const double _Complex * A = (const double _Complex *) malloc(sizeof(double _Complex) * LDA * LDA);
    const double _Complex * B = (const double _Complex *) malloc(sizeof(double _Complex) * LDB * LDB);
          double _Complex * C = (      double _Complex *) malloc(sizeof(double _Complex) * LDC * LDC);
    # else
    assert(M == N);
    assert(N == K);
    const TYPE * A = (const TYPE *) (      N * sizeof(TYPE)                   );
    const TYPE * B = (const TYPE *) (LDA * N * sizeof(TYPE)                   );
          TYPE * C = (      TYPE *) (LDA * N * sizeof(TYPE) + N * sizeof(TYPE));
    # endif
    xkblas_gemm_async(transA, transB, M, N, K, &alpha, A, LDA, B, LDB, &beta, C, LDC);
    xkblas_sync();

    xkblas_thread_deinit();
    xkblas_deinit();

    return 0;
}
