# include "blas.h"
# include "logger.h"

#include <coroutine>

int
xkblas_£gemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const TYPE * alpha,
    const TYPE *A, int Atm, int Atn, int LDA,
    const TYPE *B, int Btm, int Btn, int LDB,
    const TYPE* beta,
    TYPE *C, int Ctm, int Ctn, int LDC
) {

    return 0;
}

int
xkblas_£gemm_async(
    int transA, int transB,
    int M, int N, int K,
    const TYPE * alpha,
    const TYPE *A, int LDA,
    const TYPE *B, int LDB,
    const TYPE* beta,
    TYPE *C, int LDC
) {
    /* Check input arguments */
    if ((transA < CblasNoTrans) || (transA > CblasConjTrans))
    {
        XKBLAS_FATAL("illegal value of transA");
        return -1;
    }

    if ((transB < CblasNoTrans) || (transB > CblasConjTrans))
    {
        XKBLAS_FATAL("illegal value of transB");
        return -2;
    }

    if (M < 0)
    {
        XKBLAS_FATAL( "illegal value of M");
        return -3;
    }

    if (N < 0)
    {
        XKBLAS_FATAL("illegal value of N");
        return -4;
    }

    if (K < 0)
    {
        XKBLAS_FATAL("illegal value of N");
        return -5;
    }

    int Am, An, Bm, Bn;
    const int Cm = M;
    const int Cn = N;

    if ( transA == CblasNoTrans ) {
        Am = M; An = K;
    } else {
        Am = K; An = M;
    }
    if ( transB == CblasNoTrans ) {
        Bm = K; Bn = N;
    } else {
        Bm = N; Bn = K;
    }

    if (LDA < MAX(1, Am)) {
        XKBLAS_FATAL("illegal value of LDA");
        return -8;
    }
    if (LDB < MAX(1, Bm)) {
        XKBLAS_FATAL("illegal value of LDB");
        return -10;
    }
    if (LDC < MAX(1, M)) {
        XKBLAS_FATAL("illegal value of LDC");
        return -13;
    }

    /* Quick return */
    if (M == 0 || N == 0 ||
      ((*alpha == 0.0 || K == 0) && *beta == 1.0))
        return 0;

    // xkblas_context_t * xkctxt = xkblas_context_get();
    // int NB = xkblas_auto_tilesize(xkctxt, KERN_GEMM, M, N, K);
    int NB = 512;

    // TODO : set tiling parameters
    int Amb = NB;
    int Anb = NB;
    int Bmb = NB;
    int Bnb = NB;
    int Cmb = NB;
    int Cnb = NB;

    int Amt = XKBLAS_NUM_OF_TILES(Am, Amb);
    int Ant = XKBLAS_NUM_OF_TILES(An, Anb);
    int Bmt = XKBLAS_NUM_OF_TILES(Bm, Bmb);
    int Bnt = XKBLAS_NUM_OF_TILES(Bn, Bnb);
    int Cmt = XKBLAS_NUM_OF_TILES(Cm, Cmb);
    int Cnt = XKBLAS_NUM_OF_TILES(Cn, Cnb);

    int bs_mm, bs_nn, bs_kn, bs_km;

    // iterator on tiles
    for (int tm = 0; tm < Cmt; ++tm)
    {
        int bs_mm = (tm == Cmt-1) ? (M-tm*Cmb) : Cmb;
        for (int tn = 0; tn < Cnt; tn++)
        {
            bs_nn = (tn == Cnt-1) ? (N-tn*Cnb) : Cnb;

            // A: CblasNoTrans / B: CblasNoTrans
            if (transA == CblasNoTrans)
            {
                if (transB == CblasNoTrans)
                {
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A, tm, tk, LDA,
                                B, tk, tn, LDB,
                                &zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
                // A: CblasNoTrans / B: Cham[Conj]Trans
                else
                {
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A, tm, tk, LDA,
                                B, tn, tk, LDB,
                                &zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
            }
            // A: Cham[Conj]Trans / B: CblasNoTrans
            else
            {
                if (transB == CblasNoTrans)
                {
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A, tk, tm, LDA,
                                B, tk, tn, LDB,
                                &zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
                // A: Cham[Conj]Trans / B: Cham[Conj]Trans
                else
                {
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A, tk, tm, LDA,
                                B, tn, tk, LDB,
                                &zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
            }
        }
    }
    return 0;
}
