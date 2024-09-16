# include <cblas.h>

# include "min-max.h"
# include "xkblas-context.h"

# include "device/thread-producer.hpp"
# include "logger/todo.h"
# include "logger/logger.h"
# include "kernels/kernel-param.h"
# include "kernels/auto-tile.h"
# include "kernels/kernel-type.h"
# include "sync/access.hpp"
# include "sync/alignedas.h"
# include "sync/cache-line-size.hpp"

# include <cassert>

typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    args_t(
        int transA, int transB,
        int m, int n, int k,
        const TYPE alpha,
        const TYPE beta
    ) :
        transA(transA),
        transB(transB),
        m(m),
        n(n),
        k(k),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    const int transA;
    const int transB;
    const int m;
    const int n;
    const int k;
    const TYPE alpha;
    const TYPE beta;

} args_t;

static task_format_id_t format_id;

int
xkblas_£gemm_tile_async(
    xkblas_context_t * context,
    int transA, int transB,
    int bs_m, int bs_n, int bs_k,
    const TYPE * alpha,
    const TYPE * A, int Atm, int Atn, int lda,
    const TYPE * B, int Btm, int Btn, int ldb,
    const TYPE * beta,
          TYPE * C, int Ctm, int Ctn, int ldc
) {
    assert((uintptr_t)A % lda == 0);
    assert((uintptr_t)B % ldb == 0);
    assert((uintptr_t)C % ldc == 0);

    ThreadProducer * thread = ThreadProducer::self();

    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    assert(is_alignedas(args_size, CACHE_LINE_SIZE));

    uint8_t * mem  = thread->allocate(task_size + args_size);

    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id);

    # ifndef NDEBUG
    assert(transA == CblasNoTrans);
    assert(transB == CblasNoTrans);
    snprintf(task->label, sizeof(task->label), "gemm(A=(%d,%d) ; B=(%d,%d) ; C=(%d,%d))", Atm, Atn, Btm, Btn, Ctm, Ctn);
    # endif /* NDEBUG */

    # pragma message(TODO "Can we call and could it improve performance simply calling a 'memcpy' from 'transA' to 'ldc' ?")
    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(transA, transB, bs_m, bs_n, bs_k, *alpha, *beta);

    # pragma message(TODO "If (A == C) or (B == C) or (beta == 0), then it can be optimized with only 2 accesses")

    // block size
    const int BS = bs_m;
    assert(bs_m == bs_n);
    assert(bs_m == bs_k);

    # define NACCESSES 3
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new(task->accesses + 0) Access(A, lda, Atm, Atn, BS, BS, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(B, ldb, Btm, Btn, BS, BS, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 2) Access(C, ldc, Ctm, Ctn, BS, BS, sizeof(TYPE), Cmode        );
    thread->commit<NACCESSES>(context, task);
    # undef NACCESSES
    return 0;
}

int
xkblas_£gemm_async(
    int transA, int transB,
    int M, int N, int K,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
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

    const int Am = (transA == CblasNoTrans) ? M : K;
    const int An = (transA == CblasNoTrans) ? K : M;
    const int Bm = (transB == CblasNoTrans) ? K : N;
    const int Bn = (transB == CblasNoTrans) ? N : K;
    const int Cm = M;
    const int Cn = N;

    if (lda < MAX(1, Am))
    {
        XKBLAS_FATAL("illegal value of lda");
        return -8;
    }

    if (ldb < MAX(1, Bm))
    {
        XKBLAS_FATAL("illegal value of ldb");
        return -10;
    }

    if (ldc < MAX(1, M))
    {
        XKBLAS_FATAL("illegal value of ldc");
        return -13;
    }

    /* Quick return */
    if (M == 0 || N == 0 ||
            ((*alpha == 0.0 || K == 0) && *beta == 1.0))
        return 0;

    /* currently only support 1 size */
    xkblas_context_t * context = xkblas_context_get();
    int args[3] = {M, N, K};
    int * tile = context->conf.kernels.gemm.tile;
    if (tile[0] == 0 || tile[1] == 0)
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_GEMM, args, tile);
    assert(tile[0] == tile[1]);
    const int BS = tile[0];

    assert(M % BS == 0);
    assert(N % BS == 0);
    assert(K % BS == 0);

    /* set tiling parameters */
    int Amb = BS;
    int Anb = BS;
    int Bmb = BS;
    int Bnb = BS;
    int Cmb = BS;
    int Cnb = BS;

    int Amt = XKBLAS_NUM_OF_TILES(Am, Amb);
    int Ant = XKBLAS_NUM_OF_TILES(An, Anb);
    int Bmt = XKBLAS_NUM_OF_TILES(Bm, Bmb);
    int Bnt = XKBLAS_NUM_OF_TILES(Bn, Bnb);
    int Cmt = XKBLAS_NUM_OF_TILES(Cm, Cmb);
    int Cnt = XKBLAS_NUM_OF_TILES(Cn, Cnb);

    // iterator on tiles
    for (int tm = 0; tm < Cmt; ++tm)
    {
        int bs_mm = (tm == Cmt-1) ? (M-tm*Cmb) : Cmb;
        for (int tn = 0; tn < Cnt; tn++)
        {
            int bs_nn = (tn == Cnt-1) ? (N-tn*Cnb) : Cnb;
            // A: CblasNoTrans / B: CblasNoTrans
            if (transA == CblasNoTrans)
            {
                if (transB == CblasNoTrans)
                {
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        int bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A, tm, tk, lda,
                                B, tk, tn, ldb,
                                &zbeta,
                                C, tm, tn, ldc
                        );
                    }
                }
                // A: CblasNoTrans / B: Cham[Conj]Trans
                else
                {
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        int bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A, tm, tk, lda,
                                B, tn, tk, ldb,
                                &zbeta,
                                C, tm, tn, ldc
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
                        int bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A, tk, tm, lda,
                                B, tk, tn, ldb,
                                &zbeta,
                                C, tm, tn, ldc
                        );
                    }
                }
                // A: Cham[Conj]Trans / B: Cham[Conj]Trans
                else
                {
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        int bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A, tk, tm, lda,
                                B, tn, tk, ldb,
                                &zbeta,
                                C, tm, tn, ldc
                        );
                    }
                }
            }
        }
    }

    XKBLAS_INFO("GEMM dependency graph submitted");

    return 0;
}

# pragma message(TODO "The current design has the following flaws: (1) per-driver routine should be implemented in the driver(so they can be loaded dynamically), (2) there is yet another global 'task format' variable and (3) task format must be explicitely registered")

# if USE_CUDA
#  include "device/cublas-helper.h"

static void
body_cuda(void * vparam)
{
    task_kernel_param_t * param = (task_kernel_param_t *) vparam;
    assert(param);

    cublasStatus_t res;
    cublasHandle_t handle = (cublasHandle_t) param->handle;

    const Access * A = param->task->accesses + 0;
    const Access * B = param->task->accesses + 1;
    const Access * C = param->task->accesses + 2;

    args_t * args = (args_t *) (param->task + 1);
    assert(args->transA == CblasNoTrans);
    assert(args->transB == CblasNoTrans);

    # ifndef NDEBUG
    XKBLAS_WARN("Calling cublasGemm(m=%d, n=%d, k=%d, A=%p, lda=%d, B=%p, ldb=%d, C=%p, ldc=%d) on task=`%s`",
        args->m, args->n, args->k,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) B->device_view.addr,
        B->device_view.ld,
        (void *) C->device_view.addr,
        C->device_view.ld,
        param->task->label
    );
    #endif /* NDEBUG */

    assert(handle);
    res = cublasSetMathMode(handle, CUBLAS_DEFAULT_MATH);
    assert(res == CUBLAS_STATUS_SUCCESS);

    res = cublas££gemm(
        handle,
        cblas2cublas_op(args->transA), cblas2cublas_op(args->transB),
        args->m, args->n, args->k,
        (const CU_TYPE *) &args->alpha,
        (const CU_TYPE *) A->device_view.addr, A->device_view.ld,
        (const CU_TYPE *) B->device_view.addr, B->device_view.ld,
        (const CU_TYPE *) &args->beta,
        (      CU_TYPE *) C->device_view.addr, C->device_view.ld
    );
    xkblas_cublas_status_check(res);
    assert(res == CUBLAS_STATUS_SUCCESS);
}
# endif /* USE_CUDA */

static void
body_host(void * args)
{
    XKBLAS_DEBUG("Executing a gemm on host");
}

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£gemm_format(void)
{
    task_format_t format;
    strcpy(format.label, "£gemm");
    format.f[XKBLAS_DRIVER_TYPE_CPU] = body_host;
# ifdef USE_CUDA
    format.f[XKBLAS_DRIVER_TYPE_CUDA] = body_cuda;
# endif /* USE_CUDA */
    format_id = task_format_create(&format);
}
