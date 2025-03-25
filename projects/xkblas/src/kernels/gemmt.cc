/* ************************************************************************** */
/*                                                                            */
/*   gemmt.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/25 21:57:34 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <cassert>

# include "auto-tile.h"
# include "context.h"

# include "xkblas/kernel-type.h"
# include "xkblas/cblas.h"
# include "xkblas/xkblas-experimental.h"

# include <xkrt/xkrt-support.h>
# include <xkrt/driver/thread.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/min-max.h>
# include <xkrt/memory/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>

typedef struct args_t
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
    Thread * thread = Thread::self();

    # define AC 3
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t);

    task_t * task = thread->allocate_task(task_size + args_size);
    new(task) task_t(format_id, flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 2;
    new (dev) task_dev_info_t(UNSPECIFIED_DEVICE_GLOBAL_ID, ocr_access);

    args_t * args = (args_t *) TASK_ARGS(task, task_size);
    new(args) args_t(uplo, transA, transB, n, k, *alpha, *beta);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label),
            "gemm(A=(%zu,%zu) ; B=(%zu,%zu) ; C=(%zu,%zu))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    const size_t Am = (transA == CblasNoTrans) ? n : k;
    const size_t An = (transA == CblasNoTrans) ? k : n;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
    const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = n;
    const size_t Cn = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new(accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R, ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    new(accesses + 1) access_t(task, MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_R, ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    new(accesses + 2) access_t(task, MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, Cm, Cn, sizeof(TYPE), Cmode        , ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    thread->resolve<AC>(task, accesses);
    # undef AC

    context->runtime.task_commit(task);

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
        LOGGER_FATAL("illegal value of transA");
        return -1;
    }

    if ((transB < CblasNoTrans) || (transB > CblasConjTrans))
    {
        LOGGER_FATAL("illegal value of transB");
        return -2;
    }

    if (n < 0)
    {
        LOGGER_FATAL("illegal value of n");
        return -4;
    }

    if (k < 0)
    {
        LOGGER_FATAL("illegal value of k");
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
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    if (ldb < MAX(1, Bm))
    {
        LOGGER_FATAL("illegal value of ldb");
        return -10;
    }

    if (ldc < MAX(1, Cm))
    {
        LOGGER_FATAL("illegal value of ldc");
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

    const size_t Amt = NUM_OF_TILES(Am, Amb);
    const size_t Ant = NUM_OF_TILES(An, Anb);
    const size_t Bmt = NUM_OF_TILES(Bm, Bmb);
    const size_t Bnt = NUM_OF_TILES(Bn, Bnb);
    const size_t Cmt = NUM_OF_TILES(Cm, Cmb);
    const size_t Cnt = NUM_OF_TILES(Cn, Cnb);

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

    LOGGER_INFO("GEMMT dependency graph submitted");

    return 0;
}

# if XKRT_SUPPORT_CUDA
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

static void
body_cuda(
    xkrt_stream_cu_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    assert(stream);

    cublasHandle_t handle = stream->cu.blas.handle;
    assert(handle);

    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    args_t * args = (args_t *) TASK_ARGS(task);
    assert(args);

    # ifndef NDEBUG
    LOGGER_INFO("Calling cublasGemmt(m=%zu, n=%zu, k=%zu, A=%p, lda=%zu, B=%p, ldb=%zu, C=%p, ldc=%zu) on task=`%s`",
        args->n, args->n, args->k,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) B->device_view.addr,
        B->device_view.ld,
        (void *) C->device_view.addr,
        C->device_view.ld,
        task->label
    );
    #endif /* NDEBUG */

    XKBLAS_CUBLAS_CALL(
        cublas££gemm(
            handle,
            cblas2cublas_op(args->transA), cblas2cublas_op(args->transB),
            (int) args->n, (int) args->n, (int) args->k,
            (const CU_TYPE *) &args->alpha,
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const CU_TYPE *) B->device_view.addr, (int) B->device_view.ld,
            (const CU_TYPE *) &args->beta,
            (      CU_TYPE *) C->device_view.addr, (int) C->device_view.ld
        )
    );
}
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_HOST
static void
body_cpu(void * args)
{
    LOGGER_DEBUG("Executing a gemm on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£gemmt_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

    # if XKRT_SUPPORT_HOST
    format.f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format.f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda;
    # endif /* XKRT_SUPPORT_CUDA */

    snprintf(format.label, sizeof(format.label), "£gemmt");
    format_id = xkblas_task_format_create(&format);
}
