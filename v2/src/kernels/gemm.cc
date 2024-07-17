# include "blas.h"
# include "min-max.h"
# include "logger/todo.h"
# include "logger/logger.h"

# include "sync/alignedas.h"
# include "scheduler/thread.hpp"
# include "sync/access.hpp"

# include <cassert>

typedef struct alignas(std::hardware_constructive_interference_size)    args_t
{
    args_t(int transA, int transB,
            int M, int N, int K,
            const TYPE * alpha,
            const TYPE * A, int Atm, int Atn, int LDA,
            const TYPE * B, int Btm, int Btn, int LDB,
            const TYPE * beta,
                  TYPE * C, int Ctm, int Ctn, int LDC) :
        transA(transA), transB(transB),
        M(M), N(N), K(K),
        alpha(alpha),
        A(A), Atm(Atm), Atn(Atn), LDA(LDA),
        B(B), Btm(Btm), Btn(Btn), LDB(LDB),
        beta(beta),
        C(C), Ctm(Ctm), Ctn(Ctn), LDC(LDC)
    {}

    ~args_t() {}

    int transA;
    int transB;
    int M;
    int N;
    int K;
    const TYPE * alpha;
    const TYPE * A;
    int Atm;
    int Atn;
    int LDA;
    const TYPE * B;
    int Btm;
    int Btn;
    int LDB;
    const TYPE * beta;
    TYPE * C;
    int Ctm;
    int Ctn;
    int LDC;
}                                                                       args_t;

int
xkblas_£gemm_tile_async(
    int transA, int transB,
    int M, int N, int K,
    const TYPE * alpha,
    const TYPE * A, int Atm, int Atn, int LDA,
    const TYPE * B, int Btm, int Btn, int LDB,
    const TYPE * beta,
          TYPE * C, int Ctm, int Ctn, int LDC
) {
    # pragma message(TODO "Implement tile sending")
    Thread * thread = Thread::get();

    // const uint64_t task_size = alignedas(sizeof(Task),   std::hardware_constructive_interference_size);
    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, std::hardware_constructive_interference_size));
    assert(is_alignedas(args_size, std::hardware_constructive_interference_size));

    uint8_t * mem  = thread->allocate(task_size + args_size);

    Task    * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(TASK_BODY_GEMM);

    # pragma message(TODO "Can we call and could it improve performance simply calling a 'memcpy' from 'transA' to 'LDC' ?")
    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(transA, transB, M, N, K, alpha, A, Atm, Atn, LDA, B, Btm, Btn, LDB, beta, C, Ctm, Ctn, LDC);

    # pragma message(TODO "If (A == C) or (B == C) or (beta == 0), then it can be optimized with only 2 accesses")

    // block size
    int BS = M / 4;

    # define NACCESSES 3
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);

    task->accesses[0].mode    = ACCESS_MODE_R;
    task->accesses[0].region  = Intervals<2>(reinterpret_cast<uintptr_t>(A), LDA, BS, BS);

    task->accesses[1].mode    = ACCESS_MODE_R;
    task->accesses[1].region  = Intervals<2>(reinterpret_cast<uintptr_t>(B), LDB, BS, BS);

    task->accesses[2].mode    = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    task->accesses[2].region  = Intervals<2>(reinterpret_cast<uintptr_t>(C), LDC, BS, BS);

    thread->commit<NACCESSES>(task);

    # undef NACCESSES

    return 0;
}

int
xkblas_£gemm_async(
    int transA, int transB,
    int M, int N, int K,
    const TYPE * alpha,
    const TYPE * A, int LDA,
    const TYPE * B, int LDB,
    const TYPE * beta,
          TYPE * C, int LDC
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
    int NB = 16;

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
