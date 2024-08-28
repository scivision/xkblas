# include "xkblas.h"

# include <assert.h>
# include <stdlib.h>
# include <stdint.h>

# if 1
# include "xkblas-skernel.h"
# define TYPE               float
# define xkblas_gemm_async  xkblas_sgemm_async
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
    int LD = K+M;
    TYPE alpha = 1.0;
    TYPE beta  = 1.0;

    # if 1
    uintptr_t Ap = (uintptr_t) malloc(sizeof(TYPE) * (LD * LD + LD));
    uintptr_t Bp = (uintptr_t) malloc(sizeof(TYPE) * (LD * LD + LD));
    uintptr_t Cp = (uintptr_t) malloc(sizeof(TYPE) * (LD * LD + LD));

    Ap += (LD - (Ap % LD));
    Bp += (LD - (Bp % LD));
    Cp += (LD - (Cp % LD));

    assert(Ap % LD == 0);
    assert(Bp % LD == 0);
    assert(Cp % LD == 0);

    const TYPE * A = (const TYPE *) Ap;
    const TYPE * B = (const TYPE *) Bp;
          TYPE * C = (      TYPE *) Cp;

    # else
    assert(M == N);
    assert(N == K);
    const TYPE * A = (const TYPE *) (      N * sizeof(TYPE)                   );
    const TYPE * B = (const TYPE *) (LD * N * sizeof(TYPE)                   );
          TYPE * C = (      TYPE *) (LD * N * sizeof(TYPE) + N * sizeof(TYPE));
    # endif
    xkblas_gemm_async(transA, transB, M, N, K, &alpha, A, LD, B, LD, &beta, C, LD);
    xkblas_sync();

    xkblas_thread_deinit();
    xkblas_deinit();

    return 0;
}
