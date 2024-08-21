# include "xkblas.h"
# include "xkblas-zkernel.h"

# include <assert.h>
# include <stdlib.h>

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
    double _Complex alpha = 1.0;
    double _Complex beta  = 1.0;

    # if 0
    const double _Complex * A = (const double _Complex *) malloc(sizeof(double _Complex) * LDA * LDA);
    const double _Complex * B = (const double _Complex *) malloc(sizeof(double _Complex) * LDB * LDB);
          double _Complex * C = (      double _Complex *) malloc(sizeof(double _Complex) * LDC * LDC);
    # else
    assert(M == N);
    assert(N == K);
    const double _Complex * A = (const double _Complex *) (N);
    const double _Complex * B = (const double _Complex *) (LDA * N);
          double _Complex * C = (      double _Complex *) (LDA * N + N);
    # endif
    xkblas_zgemm_async(transA, transB, M, N, K, &alpha, A, LDA, B, LDB, &beta, C, LDC);
    xkblas_sync();

    xkblas_thread_deinit();
    xkblas_deinit();

    return 0;
}
