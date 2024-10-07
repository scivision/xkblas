# include <cblas.h>

# include "min-max.h"
# include "xkblas-context.h"

# include "device/task-launcher.h"
# include "device/thread-producer.hpp"
# include "logger/todo.h"
# include "logger/logger.h"
# include "kernels/auto-tile.h"
# include "xkblas-kernel-type.h"
# include "sync/access.hpp"
# include "sync/alignedas.h"
# include "sync/cache-line-size.hpp"

# include <cassert>

typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    args_t(
        int uplo,
        int trans,
        int n, int k,
        const TYPE alpha,
        const TYPE beta
    ) :
        uplo(uplo),
        trans(trans),
        n(n),
        k(k),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    const int uplo;
    const int trans;
    const int n;
    const int k;
    const TYPE alpha;
    const TYPE beta;

} args_t;

static task_format_id_t format_id;

/* m, n, k are matrix sizes
 * Am, An, ..., Cn are index of the tile begining */
int
xkblas_£syrk_tile_async(
    xkblas_context_t * context,
    int uplo, int trans,
    int n, int k,
    const TYPE * alpha,
    const TYPE * A, int A_offset_m, int A_offset_n, int lda,
    const TYPE * beta,
          TYPE * C, int C_offset_m, int C_offset_n, int ldc
) {
    assert((uintptr_t)A % lda == 0);
    assert((uintptr_t)C % ldc == 0);

    XKBLAS_INFO("Submitting tile C=(%d,%d) of size (%d,%d)", C_offset_m, C_offset_n, n, k);

    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    assert(is_alignedas(args_size, CACHE_LINE_SIZE));

    ThreadProducer * thread = ThreadProducer::self();
    uint8_t * mem = thread->allocate(task_size + args_size);
    assert(mem);

    // const int ocr_access = UNSPECIFIED_TASK_ACCESS;
    const int ocr_access = 1;
    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id, ocr_access, UNSPECIFIED_DEVICE_GLOBAL_ID);

    # ifndef NDEBUG
    assert(trans == CblasNoTrans);
    snprintf(task->label, sizeof(task->label), "syrk(A=(%d,%d) ; C=(%d,%d))", A_offset_m, A_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(uplo, trans, n, k, *alpha, *beta);

    # define NACCESSES 2
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, n, k, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, n, n, sizeof(TYPE), Cmode        );
    thread->resolve<NACCESSES>(task);
    # undef NACCESSES

    thread->commit(context, task);

    return 0;
}

int
xkblas_£gemm_tile_async(
    xkblas_context_t * context,
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int Am, int An, int lda,
    const TYPE * B, int Bm, int Bn, int ldb,
    const TYPE * beta,
          TYPE * C, int Cm, int Cn, int ldc
);

extern "C"
int
xkblas_£syrk_async(
    int uplo, int trans,
    int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    /* quick return */
    if (n == 0 || ((*alpha == 0.0 || k == 0) && *beta == 1.0))
        return 0;

    /* Check input arguments */
    if ((uplo != CblasUpper) && (uplo != CblasLower))
    {
        XKBLAS_FATAL("illegal value of uplo");
        return -1;
    }

    if ((trans != CblasNoTrans) && (trans != CblasTrans))
    {
        XKBLAS_FATAL("illegal value of trans");
        return -2;
    }

    if (n < 0)
    {
        XKBLAS_FATAL("illegal value of N");
        return -3;
    }

    if (k < 0)
    {
        XKBLAS_FATAL("illegal value of K");
        return -4;
    }

    const int Am = (trans == CblasNoTrans) ? n : k;
    const int An = (trans == CblasNoTrans) ? k : n;

    if (lda < MAX(1, Am))
    {
        XKBLAS_FATAL("illegal value of lda");
        return -7;
    }

    if (ldc < MAX(1, n))
    {
        XKBLAS_FATAL("illegal value of ldc");
        return -10;
    }

    const int Cm = n;
    const int Cn = n;

    xkblas_context_t * context = xkblas_context_get();

    int * tile = context->conf.kernels[XKBLAS_KERNEL_TYPE_SYRK].tile;
    if (tile[0] == 0 || tile[1] == 0)
    {
        int args[2] = {n, k};
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_SYRK, args, tile);
    }
    assert(tile[0] == tile[1]);

    /* set tiling parameters */
    int Amb = tile[0];
    int Anb = tile[1];
    int Cmb = tile[0];
    int Cnb = tile[0];

    int Amt = XKBLAS_NUM_OF_TILES(Am, Amb);
    int Ant = XKBLAS_NUM_OF_TILES(An, Anb);
    int Cmt = XKBLAS_NUM_OF_TILES(Cm, Cmb);
    int Cnt = XKBLAS_NUM_OF_TILES(Cn, Cnb);

    const TYPE one = (TYPE) 1.0;

    # define A(tm, tn) A, tm*Amb, tn*Anb
    # define C(tm, tn) C, tm*Cmb, tn*Cnb

    for (int tn = 0; tn < Cnt; ++tn)
    {
        const int bs_nn = (tn == Cnt-1) ? (Cn-tn*Cnb) : Cnb;
        if (trans == CblasNoTrans)
        {
            for (int tk = 0; tk < Ant; ++tk)
            {
                const int bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                const TYPE zbeta = (tk == 0) ? *beta : one;
                xkblas_£syrk_tile_async(
                    context,
                    uplo, trans,
                    bs_nn, bs_kn,
                    alpha,  A(tn, tk), lda,
                    &zbeta, C(tn, tn), ldc
                );
            }

            if (uplo == CblasLower)
            {
                for (int tm = tn+1; tm < Cmt; ++tm)
                {
                    const int bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        const int bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            trans, CblasTrans,
                            bs_mm, bs_nn, bs_kn,
                            alpha,
                            A(tm, tk), lda,
                            A(tn, tk), lda,
                            &zbeta,
                            C(tm, tn), ldc
                        );
                    }
                }
            }
            else
            {
                for (int tm = tn+1; tm < Cmt; ++tm)
                {
                    const int bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        const int bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            trans, CblasTrans,
                            bs_nn, bs_mm, bs_kn,
                            alpha,
                            A(tn, tk), lda,
                            A(tm, tk), lda,
                            &zbeta,
                            C(tn, tm), ldc
                        );
                    }
                }
            }
        }
        else
        {
            for (int tk = 0; tk < Amt; ++tk)
            {
                const int bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                const TYPE zbeta = (tk == 0) ? *beta : one;
                xkblas_£syrk_tile_async(
                    context,
                    uplo, trans,
                    bs_nn, bs_km,
                    alpha,
                    A(tk, tn), lda,
                    &zbeta,
                    C(tn, tn),
                    ldc
                );
            }

            if (uplo == CblasLower)
            {
                for (int tm = tn+1; tm < Cmt ; ++tm)
                {
                    const int bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        const int bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            trans, CblasNoTrans,
                            bs_mm, bs_nn, bs_km,
                            alpha,
                            A(tk, tm), lda,
                            A(tk, tn), lda,
                            &zbeta,
                            C(tm, tn), ldc
                        );
                    }
                }
            }
            else
            {
                for (int tm = tn+1; tm < Cmt; ++tm)
                {
                    const int bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        const int bs_km = (tk == Amt-1) ? (Am-k*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            trans, CblasNoTrans,
                            bs_nn, bs_mm, bs_km,
                            alpha,
                            A(tk, tn), lda,
                            A(tk, tm), lda,
                            &zbeta,
                            C(tn, tm), ldc
                        );
                    }
                }
            }
        }
    }

    XKBLAS_INFO("TRSM dependency graph submitted");
    return 0;
}

# pragma message(TODO "The current design has the following flaws: (1) per-driver routine should be implemented in the driver(so they can be loaded dynamically), (2) there is yet another global 'task format' variable and (3) task format must be explicitely registered")

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
    const Access * C = launcher->task->accesses + 1;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    args_t * args = (args_t *) (launcher->task + 1);
    assert(args->trans == CblasNoTrans);

    # ifndef NDEBUG
    XKBLAS_INFO("Calling cublasSyrk(n=%d, k=%d, A=%p, lda=%d, C=%p, ldc=%d) on task=`%s`",
        args->n, args->k,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) C->device_view.addr,
        C->device_view.ld,
        launcher->task->label
    );
    #endif /* NDEBUG */

    assert(handle);
    res = cublasSetMathMode(handle, CUBLAS_DEFAULT_MATH);
    assert(res == CUBLAS_STATUS_SUCCESS);

    res = cublas££syrk(
        handle,
        cblas2cublas_uplo(args->uplo), cblas2cublas_op(args->trans),
        args->n, args->k,
        (const CU_TYPE *) &args->alpha,
        (const CU_TYPE *) A->device_view.addr, A->device_view.ld,
        (const CU_TYPE *) &args->beta,
        (      CU_TYPE *) C->device_view.addr, C->device_view.ld
    );
    xkblas_cublas_status_check(res);
    assert(res == CUBLAS_STATUS_SUCCESS);
}
# endif /* USE_CUDA */

# ifdef USE_CPU
static void
body_cpu(void * args)
{
    XKBLAS_DEBUG("Executing a syrk on cpu");
}
# endif /* USE_CPU */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£syrk_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

# ifdef USE_CPU
    format.f[XKBLAS_DRIVER_TYPE_CPU] = body_cpu;
# endif /* USE_CPU */
# ifdef USE_CUDA
    format.f[XKBLAS_DRIVER_TYPE_CUDA] = body_cuda;
# endif /* USE_CUDA */
    snprintf(format.label, sizeof(format.label), "£syrk");
    format.target = TASK_FORMAT_TARGET_DRIVER;
    format_id = task_format_create(&format);
}
