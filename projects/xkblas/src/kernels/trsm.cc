/* ************************************************************************** */
/*                                                                            */
/*   trsm.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:23:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <cblas.h>

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

# include <cassert>

typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    args_t(
        const int side, const int uplo,
        const int transA, const int diag,
        const size_t m, const size_t n,
        const TYPE alpha
    ) :
        side(side),
        uplo(uplo),
        transA(transA),
        diag(diag),
        m(m),
        n(n),
        alpha(alpha)
    {}

    ~args_t() {}

     const int side;
     const int uplo;
     const int transA;
     const int diag;
     const size_t m;
     const size_t n;
     const TYPE alpha;

} args_t;

static task_format_id_t format_id;

/* m, n, k are matrix sizes
 * Am, An, ..., Cn are index of the tile begining */
int
xkblas_£trsm_tile_async(
    xkblas_context_t * context,
    int side, int uplo,
    int transA, int diag,
    const size_t m, const size_t n,
    const TYPE * alpha,
    const TYPE * A, const ssize_t A_offset_m, const ssize_t A_offset_n, const size_t lda,
          TYPE * B, const ssize_t B_offset_m, const ssize_t B_offset_n, const size_t ldb
) {
    ThreadProducer * thread = ThreadProducer::self();

    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    assert(is_alignedas(args_size, CACHE_LINE_SIZE));

    uint8_t * mem  = thread->allocate(task_size + args_size);
    assert(mem);

    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id, UNSPECIFIED_TASK_ACCESS, UNSPECIFIED_DEVICE_GLOBAL_ID);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "trsm(A=(%d,%d) ; B=(%d,%d))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n);
    # endif /* NDEBUG */

    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(side, uplo, transA, diag, m, n, *alpha);

    /* TODO: block size, is that correct ? */
    const size_t Am = (side == CblasLeft) ? m : n;
    const size_t An = (side == CblasLeft) ? m : n;
    const size_t Bm = m;
    const size_t Bn = n;

    # define NACCESSES 2
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_RW);
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

// A*X = B or X*A = B
// B is (M, N)
// A is whether (M, M) if side is left, whether (N, N) if side is right
extern "C"
int
xkblas_£trsm_async(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    if (m == 0 || n == 0)
        return 0;

    /* Check input arguments */
    if (side != CblasLeft && side != CblasRight)
    {
        XKBLAS_ERROR("illegal value of side");
        return -1;
    }

    if ((uplo != CblasUpper) && (uplo != CblasLower))
    {
        XKBLAS_ERROR("illegal value of uplo");
        return -2;
    }

    if (((transA < CblasNoTrans) || (transA > CblasConjTrans)))
    {
        XKBLAS_ERROR("illegal value of transA");
        return -3;
    }

    if ((diag != CblasUnit) && (diag != CblasNonUnit))
    {
        XKBLAS_ERROR("illegal value of diag");
        return -4;
    }

    if (m < 0)
    {
        XKBLAS_ERROR("illegal value of m");
        return -5;
    }

    if (n < 0)
    {
        XKBLAS_ERROR("illegal value of n");
        return -6;
    }

    const size_t Am = (side == CblasLeft) ? m : n;
    const size_t An = Am;
    const size_t Bm = m;
    const size_t Bn = n;

    if (lda < MAX(1, An))
    {
        XKBLAS_ERROR("illegal value of lda");
        return -8;
    }

    if (ldb < MAX(1, Bn))
    {
        XKBLAS_ERROR("illegal value of ldb");
        return -10;
    }

    xkblas_context_t * context = xkblas_context_get();
    size_t ts = context->conf.kernels[XKBLAS_KERNEL_TYPE_TRSM].tile;
    if (ts == 0)
    {
        int args[2] = {m, n};
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_TRSM, args, &ts);
    }

    /* set tiling parameters */
    const size_t Amb = ts;
    const size_t Anb = ts;
    const size_t Bmb = ts;
    const size_t Bnb = ts;

    const size_t Amt = XKBLAS_NUM_OF_TILES(Am, Amb);
    const size_t Ant = XKBLAS_NUM_OF_TILES(An, Anb);
    const size_t Bmt = XKBLAS_NUM_OF_TILES(Bm, Bmb);
    const size_t Bnt = XKBLAS_NUM_OF_TILES(Bn, Bnb);

    TYPE one        = (TYPE) 1.0;
    TYPE mone       = (TYPE)-1.0;
    TYPE minvalpha  = (TYPE)-1.0 / *alpha;

    # pragma message(TODO "Block sizes truncation are suspicious to me here, double check")

    # define A(I, J) A, (I)*Amb, (J)*Anb, lda
    # define B(I, J) B, (I)*Bmb, (J)*Bnb, ldb

    /* CblasLeft / CblasUpper / CblasNoTrans  */
    if (side == CblasLeft) {
        if (uplo == CblasUpper) {
            if (transA == CblasNoTrans) {
                for (int tk = 0; tk < Bmt; tk++) {
                    size_t bs_km  = (tk == 0) ? Bm-(Bmt-1)*Bmb : Bmb;
                    TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (int tn = 0; tn < Bnt; tn++) {
                        size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Bmt-1-tk, Bmt-1-tk),
                            B(Bmt-1-tk,       tn)
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; ++tm) {
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            xkblas_£gemm_tile_async(
                                context,
                                CblasNoTrans, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                &mone,
                                A(Bmt-1-tm, Bmt-1-tk),
                                B(Bmt-1-tk,       tn),
                                &lalpha,
                                B(Bmt-1-tm,       tn)
                            );
                        }
                    }
                }
            }
            /*
             *  CblasLeft / CblasUpper / CblasTrans
             */
            else {
                for (int tk = 0; tk < Bmt; ++tk) {
                    size_t bs_km  = (tk == Bmt-1) ? Bm-tk*Bmb : Bmb;
                    TYPE lalpha = (tk == 0)     ? *alpha : one;
                    for (int tn = 0; tn < Bnt; ++tn) {
                        size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(tk, tk),
                            B(tk, tn)
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; tm++) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            xkblas_£gemm_tile_async(
                                context,
                                transA, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                A(tk, tm),
                                B(tk, tn),
                                &lalpha,
                                B(tm, tn)
                            );
                        }
                    }
                }
            }
        }
        /*
         *  CblasLeft / CblasLower / CblasNoTrans
         */
        else {
            if (transA == CblasNoTrans) {
                for (int tk = 0; tk < Bmt; ++tk) {
                    size_t bs_km  = (tk == Bmt-1) ? (Bm-tk*Bmb) : Bmb;
                    TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (int tn = 0; tn < Bnt; ++tn) {
                        size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(tk, tk),
                            B(tk, tn)
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; ++tm) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            xkblas_£gemm_tile_async(
                                context,
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                A(tm, tk),
                                B(tk, tn),
                                &lalpha,
                                B(tm, tn)
                            );
                        }
                    }
                }
            }
            /*
             *  CblasLeft / CblasLower / Cblas[Conj]Trans
             */
            else {
                for (int tk = 0; tk < Bmt; ++tk) {
                    size_t bs_km  = (tk == 0) ? Bm-(Bmt-1)*Bmb : Bmb;
                    TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (int tn = 0; tn < Bnt; ++tn) {
                        size_t bs_nn = tn == Bnt-1 ? Bn-tn*Bnb : Bnb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo, transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Bmt-1-tk, Bmt-1-tk),
                            B(Bmt-1-tk,       tn)
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; ++tm) {
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            xkblas_£gemm_tile_async(
                                context,
                                transA, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                &mone,
                                A(Bmt-1-tk, Bmt-1-tm),
                                B(Bmt-1-tk,       tn),
                                &lalpha,
                                B(Bmt-1-tm,       tn)
                            );
                        }
                    }
                }
            }
        }
    }
    /*
     *  CblasRight / CblasUpper / CblasNoTrans
     */
    else {
        if (uplo == CblasUpper) {
            if (transA == CblasNoTrans) {
                for (int tk = 0; tk < Bnt; ++tk) {
                    size_t bs_kn = (tk == Bnt-1) ? (Bn-tk*Bnb) : Bnb;
                    TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (int tm = 0; tm < Bmt; ++tm) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo, transA, diag,
                            bs_mm, bs_kn,
                            &lalpha,
                            A(tk, tk),
                            B(tm, tk)
                        );
                    }
                    for (int tm = 0; tm < Bmt; ++tm) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            xkblas_£gemm_tile_async(
                                context,
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                B(tm, tk),
                                A(tk, tn),
                                &lalpha,
                                B(tm, tn)
                            );
                        }
                    }
                }
            }
            /*
             *  CblasRight / CblasUpper / CblasConjTrans
             */
            else {
                for (int tk = 0; tk < Bnt; ++tk) {
                    size_t bs_kn = tk == 0 ? Bn-(Bnt-1)*Bnb : Bnb;
                    for (int tm = 0; tm < Bmt; ++tm) {
                        size_t bs_mm = tm == Bmt-1 ? Bm-tm*Bmb : Bmb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            alpha,
                            A, (Bnt-1-tk)*Amb, (Bnt-1-tk)*Anb, lda,
                            B,         tm*Bmb, (Bnt-1-tk)*Bnb, ldb
                        );

                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            xkblas_£gemm_tile_async(
                                context,
                                CblasNoTrans, transA,
                                bs_mm, Bnb, bs_kn,
                                &minvalpha,
                                B,         tm*Bmb, (Bnt-1-tk)*Bnb, ldb,
                                A, (Bnt-1-tn)*Amb, (Bnt-1-tk)*Anb, lda,
                                &one,
                                B,         tm*Bmb, (Bnt-1-tn)*Bnb, ldb
                            );
                        }
                    }
                }
            }
        }
        /*
         *  CblasRight / CblasLower / CblasNoTrans
         */
        else {
            if (transA == CblasNoTrans) {
                for (int tk = 0; tk < Bnt; ++tk) {
                    size_t bs_kn  = tk == 0 ? Bn-(Bnt-1)*Bnb : Bnb;
                    TYPE lalpha = tk == 0 ? *alpha : one;
                    for (int tm = 0; tm < Bmt; ++tm) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            &lalpha,
                            A, (Bnt-1-tk)*Amb, (Bnt-1-tk)*Anb, lda,
                            B,         tm*Bmb, (Bnt-1-tk)*Bnb, ldb
                        );

                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            xkblas_£gemm_tile_async(
                                context,
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, Bnb, bs_kn,
                                &mone,
                                B, tm*Bmb,         (Bnt-1-tk)*Bnb, ldb,
                                A, (Bnt-1-tk)*Amb, (Bnt-1-tn)*Anb, lda,
                                &lalpha,
                                B, tm*Bmb,         (Bnt-1-tn)*Bnb, ldb
                            );
                        }
                    }
                }
            }
            else {
                for (int tk = 0; tk < Bnt; ++tk) {
                    size_t bs_kn = tk == Bnt-1 ? Bn-tk*Bnb : Bnb;
                    for (int tm = 0; tm < Bmt; ++tm) {
                        size_t bs_mm = tm == Bmt-1 ? Bm-tm*Bmb : Bmb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            alpha,
                            A, tk*Amb, tk*Anb, lda,
                            B, tm*Bmb, tk*Bnb, ldb
                        );

                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            size_t bs_nn = tn == Bnt-1 ? Bn-tn*Bnb : Bnb;
                            xkblas_£gemm_tile_async(
                                context,
                                CblasNoTrans, transA,
                                bs_mm, bs_nn, Bmb,
                                &minvalpha,
                                B, tm*Bmb, tk*Bnb, ldb,
                                A, tn*Amb, tk*Anb, lda,
                                &one,
                                B, tm*Bmb, tn*Bnb, ldb
                            );
                        }
                    }
                }
            }
        }
    }

    # undef A
    # undef B

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

    cublasHandle_t handle = (cublasHandle_t) launcher->handle;
    assert(handle);

    args_t * args = (args_t *) (launcher->task + 1);
    // assert(args->transA == CblasNoTrans);
    // assert(args->side   == CblasLeft);

    const Access * A = launcher->task->accesses + 0;
    const Access * B = launcher->task->accesses + 1;

    # ifndef NDEBUG
    XKBLAS_INFO("Calling cublasTrsm(side=%d, uplo=%d, transA=%d, diag=%d, alpha=%lf, m=%lu, n=%lu, A=%p, lda=%d, B=%p, ldb=%d) on task=`%s`",
        args->side, args->uplo,
        args->transA, args->diag,
        args->alpha,
        args->m, args->n,
        (void *) A->device_view.addr, (int) A->device_view.ld,
        (void *) B->device_view.addr, (int) B->device_view.ld,
        launcher->task->label
    );
    #endif /* NDEBUG */

    cublasStatus_t res;
    res = cublas££trsm(
        handle,
        cblas2cublas_side(args->side), cblas2cublas_uplo(args->uplo),
        cblas2cublas_op(args->transA), cblas2cublas_diag(args->diag),
        (int) args->m, (int) args->n,
        (const CU_TYPE *) &(args->alpha),
        (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
              (CU_TYPE *) B->device_view.addr, (int) B->device_view.ld
    );
    xkblas_cublas_status_check(res);
    assert(res == CUBLAS_STATUS_SUCCESS);
}
# endif /* USE_CUDA */

# ifdef USE_CPU
static void
body_cpu(void * args)
{
    XKBLAS_DEBUG("Executing a trsm on cpu");
}
# endif /* USE_CPU */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£trsm_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

# ifdef USE_CPU
    format.f[XKBLAS_DRIVER_TYPE_CPU] = body_cpu;
# endif /* USE_CPU */
# ifdef USE_CUDA
    format.f[XKBLAS_DRIVER_TYPE_CUDA] = body_cuda;
# endif /* USE_CUDA */
    snprintf(format.label, sizeof(format.label), "£trsm");
    format.target = TASK_FORMAT_TARGET_DRIVER;
    format_id = task_format_create(&format);
}
