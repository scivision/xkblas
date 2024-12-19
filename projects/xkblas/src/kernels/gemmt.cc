/* ************************************************************************** */
/*                                                                            */
/*   gemmt.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:23:02 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <cblas.h>
# include <cassert>

# include "context.h"

# include <ptr/device/task-launcher.h>
# include <ptr/device/thread-producer.hpp>
# include <ptr/kernels/auto-tile.h>
# include <ptr/logger/logger.h>
# include <ptr/logger/todo.h>
# include <ptr/min-max.h>
# include <ptr/sync/access.hpp>
# include <ptr/sync/alignedas.h>
# include <ptr/sync/cache-line-size.hpp>
# include <ptr/xkblas-kernel-type.h>

typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    args_t(
        int uplo,
        int transA, int transB,
        size_t n, size_t k,
        const TYPE alpha,
        const TYPE beta
    ) :
        uplo(uplo),
        transA(transA),
        transB(transB),
        n(n),
        k(k),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    const int uplo;
    const int transA;
    const int transB;
    const size_t n;
    const size_t k;
    const TYPE alpha;
    const TYPE beta;

} args_t;

static task_format_id_t format_id;

int
xkblas_£gemmt_tile_async(
    xkblas_context_t * context,
    int uplo,
    int transA, int transB,
    const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const ssize_t A_offset_m, const ssize_t A_offset_n, const size_t lda,
    const TYPE * B, const ssize_t B_offset_m, const ssize_t B_offset_n, const size_t ldb,
    const TYPE * beta,
          TYPE * C, const ssize_t C_offset_m, const ssize_t C_offset_n, const size_t ldc
) {
    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    assert(is_alignedas(args_size, CACHE_LINE_SIZE));

    ThreadProducer * thread = ThreadProducer::self();
    uint8_t * mem = thread->allocate(task_size + args_size);
    assert(mem);

    // const size_t ocr_access = UNSPECIFIED_TASK_ACCESS;
    const size_t ocr_access = 2;
    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id, ocr_access, UNSPECIFIED_DEVICE_GLOBAL_ID);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "gemm(A=(%d,%d) ; B=(%d,%d) ; C=(%d,%d))", A_offset_m, A_offset_n, B_offset_m, B_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(uplo, transA, transB, n, k, *alpha, *beta);

    const size_t Am = (transA == CblasNoTrans) ? n : k;
    const size_t An = (transA == CblasNoTrans) ? k : n;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
    const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = n;
    const size_t Cn = n;

    # define NACCESSES 3
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 2) Access(MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, Cm, Cn, sizeof(TYPE), Cmode        );
    thread->resolve<NACCESSES>(task);
    # undef NACCESSES

    thread->commit(task);

    return 0;
}

int
xkblas_£gemm_tile_async(
    xkblas_context_t * context,
    int transA, int transB,
    const size_t m, const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const ssize_t A_offset_m, const ssize_t A_offset_n, const size_t lda,
    const TYPE * B, const ssize_t B_offset_m, const ssize_t B_offset_n, const size_t ldb,
    const TYPE * beta,
          TYPE * C, const ssize_t C_offset_m, const ssize_t C_offset_n, const size_t ldc
);

extern "C"
int
xkblas_£gemm_async(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
);

extern "C"
int
xkblas_£gemmt_async(
    int uplo,
    int transA, int transB,
    int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    if (uplo == 0)
        return xkblas_£gemm_async( transA, transB, n, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
    assert(uplo == CblasLower || uplo == CblasUpper);

    if (n == 0 || ((*alpha == 0.0 || k == 0) && *beta == 1.0))
        return 0;

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

    if (n < 0)
    {
        XKBLAS_FATAL("illegal value of n");
        return -4;
    }

    if (k < 0)
    {
        XKBLAS_FATAL("illegal value of k");
        return -5;
    }

    const size_t Am = (transA == CblasNoTrans) ? n : k;
    const size_t An = (transA == CblasNoTrans) ? k : n;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
    const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = n;
    const size_t Cn = n;

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

    if (ldc < MAX(1, Cm))
    {
        XKBLAS_FATAL("illegal value of ldc");
        return -13;
    }

    xkblas_context_t * context = xkblas_context_get();

    size_t ts = context->conf.kernels[XKBLAS_KERNEL_TYPE_GEMM].tile;
    if (ts == 0)
    {
        int args[2] = {n, n};
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_GEMM, args, &ts);
    }

    /* set tiling parameters */
    const size_t Amb = ts;
    const size_t Anb = ts;
    const size_t Bmb = ts;
    const size_t Bnb = ts;
    const size_t Cmb = ts;
    const size_t Cnb = ts;

    const size_t Amt = XKBLAS_NUM_OF_TILES(Am, Amb);
    const size_t Ant = XKBLAS_NUM_OF_TILES(An, Anb);
    const size_t Bmt = XKBLAS_NUM_OF_TILES(Bm, Bmb);
    const size_t Bnt = XKBLAS_NUM_OF_TILES(Bn, Bnb);
    const size_t Cmt = XKBLAS_NUM_OF_TILES(Cm, Cmb);
    const size_t Cnt = XKBLAS_NUM_OF_TILES(Cn, Cnb);

    const TYPE one = (TYPE) 1.0;

    # define A(I, J) A, (I)*Amb, (J)*Anb, lda
    # define B(I, J) B, (I)*Bmb, (J)*Bnb, ldb
    # define C(I, J) C, (I)*Cmb, (J)*Cnb, ldc

    // iterator on tiles
    for (size_t tm = 0; tm < Cmt; ++tm)
    {
        const size_t bs_mm = (tm == Cmt-1) ? (n-tm*Cmb) : Cmb;
        const size_t tn_min = (uplo == CblasLower) ?   0  :  tm;
        const size_t tn_max = (uplo == CblasLower) ? tm+1 : Cnt;

        for (size_t tn = tn_min ; tn < tn_max; ++tn)
        {
            const size_t bs_nn = (tn == Cnt-1) ? (n-tn*Cnb) : Cnb;

            // A: CblasNoTrans / B: CblasNoTrans
            if (transA == CblasNoTrans)
            {
                if (transB == CblasNoTrans)
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        const size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            xkblas_£gemmt_tile_async(context, uplo, transA, transB,        bs_nn, bs_kn, alpha, A(tm, tk), B(tk, tn), &zbeta, C(tm, tn));
                        else
                             xkblas_£gemm_tile_async(context,       transA, transB, bs_mm, bs_nn, bs_kn, alpha, A(tm, tk), B(tk, tn), &zbeta, C(tm, tn));
                    }
                }
                // A: CblasNoTrans / B: CBlasTrans
                else
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        const size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            xkblas_£gemmt_tile_async(context, uplo, transA, transB,        bs_nn, bs_kn, alpha, A(tm, tk), B(tn, tk), &zbeta, C(tm, tn));
                        else
                             xkblas_£gemm_tile_async(context,       transA, transB, bs_mm, bs_nn, bs_kn, alpha, A(tm, tk), B(tn, tk), &zbeta, C(tm, tn));
                    }
                }
            }
            // A: CblasTrans / B: CblasNoTrans
            else
            {
                if (transB == CblasNoTrans)
                {
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        const size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            xkblas_£gemmt_tile_async(context, uplo, transA, transB,        bs_nn, bs_km, alpha, A(tk, tm), B(tk, tn), &zbeta, C(tm, tn));
                        else
                             xkblas_£gemm_tile_async(context,       transA, transB, bs_mm, bs_nn, bs_km, alpha, A(tk, tm), B(tk, tn), &zbeta, C(tm, tn));
                    }
                }
                // A: CblasTrans / B: CBlasTrans
                else
                {
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        const size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            xkblas_£gemmt_tile_async(context, uplo, transA, transB,        bs_nn, bs_km, alpha, A(tk, tm), B(tn, tk), &zbeta, C(tm, tn));
                        else
                             xkblas_£gemm_tile_async(context,       transA, transB, bs_mm, bs_nn, bs_km, alpha, A(tk, tm), B(tn, tk), &zbeta, C(tm, tn));
                    }
                }
            }
        }
    }

    # undef A
    # undef B
    # undef C

    XKBLAS_INFO("GEMMT dependency graph submitted");

    return 0;
}


# if USE_HIP

static inline void
body_hip(void * vlauncher)
{
}
# endif /* USE_HIP */

# if USE_CUDA
#  include "device/cublas-helper.h"

static void
body_cuda(void * vlauncher)
{
    task_launcher_t * launcher = (task_launcher_t *) vlauncher;
    assert(launcher);

    cublasStatus_t res;
    cublasHandle_t handle = (cublasHandle_t) launcher->handle;

    const Access * A = launcher->task->accesses + 0;
    const Access * B = launcher->task->accesses + 1;
    const Access * C = launcher->task->accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    args_t * args = (args_t *) (launcher->task + 1);
    assert(args);

    # ifndef NDEBUG
    XKBLAS_INFO("Calling cublasGemm(m=%d, n=%d, k=%d, A=%p, lda=%d, B=%p, ldb=%d, C=%p, ldc=%d) on task=`%s`",
        args->n, args->n, args->k,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) B->device_view.addr,
        B->device_view.ld,
        (void *) C->device_view.addr,
        C->device_view.ld,
        launcher->task->label
    );
    #endif /* NDEBUG */

    # if 0
    assert(handle);
    res = cublasSetMathMode(handle, CUBLAS_DEFAULT_MATH);
    assert(res == CUBLAS_STATUS_SUCCESS);
    # endif

    res = cublas££gemm(
        handle,
        cblas2cublas_op(args->transA), cblas2cublas_op(args->transB),
        (int) args->n, (int) args->n, (int) args->k,
        (const CU_TYPE *) &args->alpha,
        (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
        (const CU_TYPE *) B->device_view.addr, (int) B->device_view.ld,
        (const CU_TYPE *) &args->beta,
        (      CU_TYPE *) C->device_view.addr, (int) C->device_view.ld
    );
    xkblas_cublas_status_check(res);
    assert(res == CUBLAS_STATUS_SUCCESS);
}
# endif /* USE_CUDA */

static void
body_cpu(void * args)
{
    XKBLAS_DEBUG("Executing a gemm on cpu");
}


//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£gemmt_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

    # pragma message(TODO "Use templated function to generate code instead of dupplicating HIP/Cuda kernels")

    # if USE_CPU
    format.f[XKBLAS_DRIVER_TYPE_CPU]    = body_cpu;
    # endif /* USE_CPU */

    # if USE_CUDA
    format.f[XKBLAS_DRIVER_TYPE_CUDA]   = body_cuda;
    # endif /* USE_CUDA */

    # if USE_HIP
    format.f[XKBLAS_DRIVER_TYPE_HIP]    = body_hip;
    # endif /* USE_HIP */

    snprintf(format.label, sizeof(format.label), "£gemmt");
    format.target = TASK_FORMAT_TARGET_DRIVER;
    format_id = task_format_create(&format);
}
