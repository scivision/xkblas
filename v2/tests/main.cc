# include "xkblas.h"
# include "xkblas-zkernel.h"

# include <stdlib.h>

int
main(void)
{
    xkblas_init();
    xkblas_thread_init();

    int transA = CblasNoTrans;
    int transB = CblasNoTrans;
    int M = 128;
    int N = M;
    int K = M;
    int LDA = 3*M;
    int LDB = 3*M;
    int LDC = 3*M;
    double _Complex alpha = 1.0;
    double _Complex beta  = 1.0;

    # if 0
    const double _Complex * A = (const double _Complex *) malloc(sizeof(double _Complex) * LDA * LDA);
    const double _Complex * B = (const double _Complex *) malloc(sizeof(double _Complex) * LDB * LDB);
          double _Complex * C = (      double _Complex *) malloc(sizeof(double _Complex) * LDC * LDC);
    # else
    const double _Complex * A = (const double _Complex *) 0;
    const double _Complex * B = (const double _Complex *) (A + LDA * LDA);
          double _Complex * C = (      double _Complex *) (B + LDB * LDB);
    # endif
    xkblas_zgemm_async(transA, transB, M, N, K, &alpha, A, LDA, B, LDB, &beta, C, LDC);

    xkblas_sync();

    xkblas_thread_deinit();
    xkblas_deinit();

    return 0;
}
