# include <cblas.h>

# include "min-max.h"
# include "xkblas-context.h"

# include "device/thread-producer.hpp"
# include "logger/todo.h"
# include "logger/logger.h"
# include "kernels/kernel-param.h"
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

    ThreadProducer * thread = ThreadProducer::get();

    // const uint64_t task_size = alignedas(sizeof(Task), CACHE_LINE_SIZE);
    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    assert(is_alignedas(args_size, CACHE_LINE_SIZE));

    uint8_t * mem  = thread->allocate(task_size + args_size);
//    uint8_t * mem = (uint8_t *) malloc(task_size + args_size);

    Task    * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id);

    # ifndef NDEBUG
    assert(transA == CblasNoTrans);
    assert(transB == CblasNoTrans);
    snprintf(task->label, sizeof(task->label), "gemm(%d, %d, %d)", Atm, Atn, Btn);
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

    if (lda < MAX(1, Am)) {
        XKBLAS_FATAL("illegal value of lda");
        return -8;
    }
    if (ldb < MAX(1, Bm)) {
        XKBLAS_FATAL("illegal value of ldb");
        return -10;
    }
    if (ldc < MAX(1, M)) {
        XKBLAS_FATAL("illegal value of ldc");
        return -13;
    }

    /* Quick return */
    if (M == 0 || N == 0 ||
      ((*alpha == 0.0 || K == 0) && *beta == 1.0))
        return 0;

    xkblas_context_t * context = xkblas_context_get();
    // int BS = xkblas_auto_tilesize(xkctxt, KERN_GEMM, M, N, K);
    const int NTILES = 2;
    const int BS = M / NTILES;

    assert(M % BS == 0);
    assert(N % BS == 0);
    assert(K % BS == 0);

    // TODO : set tiling parameters
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
                        bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
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
                        bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
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
                        bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : 1.0;
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
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

#ifndef NDEBUG
    XKBLAS_INFO("Exporting Dependency Tree...");
    ThreadProducer * thread = ThreadProducer::get();
    FILE * f = fopen("gemm.dot", "w");
    thread->dump_tasks(f);
    fclose(f);
    system("dot -Tpdf gemm.dot > gemm.pdf");

    thread->deptree.export_pdf("dependency");
    XKBLAS_DEBUG("Done");
# endif /* NDEBUG */

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

    void * handle = param->handle;

    const Access * A = param->task->accesses + 0;
    const Access * B = param->task->accesses + 1;
    const Access * C = param->task->accesses + 2;

    args_t * args = (args_t *) (param->task + 1);

    // TODO : cublasSetMathMode ???

    # pragma message(TODO "Call cublas with allocated device accesses")
    # pragma message(TODO "Does alpha/beta host or device pointers ?")

    XKBLAS_DEBUG("Calling cublasGemm(A=%p, B=%p, C=%p) - handle=%p",
        (void *) A->device_view.addr,
        (void *) B->device_view.addr,
        (void *) C->device_view.addr,
        handle
    );

    assert(handle);

    cublasStatus_t res = cublas££gemm(
        (cublasHandle_t) handle,
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
    task_format_create(&format);
}
