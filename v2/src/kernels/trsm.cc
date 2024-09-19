# include <cblas.h>

# include "min-max.h"
# include "xkblas-context.h"

# include "device/task-launcher.h"
# include "device/thread-producer.hpp"
# include "logger/todo.h"
# include "logger/logger.h"
# include "kernels/auto-tile.h"
# include "kernels/kernel-type.h"
# include "sync/access.hpp"
# include "sync/alignedas.h"
# include "sync/cache-line-size.hpp"

# include <cassert>

typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    args_t(
        const int side, const int uplo,
        const int transA, const int diag,
        const int m, const int n,
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
     const int m;
     const int n;
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
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int A_offset_m, int A_offset_n, int lda,
          TYPE * B, int B_offset_m, int B_offset_n, int ldb
) {
    assert((uintptr_t)A % lda == 0);
    assert((uintptr_t)B % ldb == 0);

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
    assert(transA == CblasNoTrans);
    assert(side == CblasLeft);
    snprintf(task->label, sizeof(task->label), "trsm(A=(%d,%d) ; B=(%d,%d))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n);
    # endif /* NDEBUG */

    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(side, uplo, transA, diag, m, n, *alpha);

    /* TODO: block size, is that correct ? */
    const int Am = (side == CblasLeft) ? m : n;
    const int An = (side == CblasLeft) ? m : n;
    const int Bm = m;
    const int Bn = n;

    # define NACCESSES 2
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_RW);
    thread->commit<NACCESSES>(context, task);
    # undef NACCESSES

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

// A*X = B or X*A = B
// B is (M, N)
// A is whether (M, M) if side is left, whether (N, N) if side is right
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

    const int Am = (side == CblasLeft) ? m : n;
    const int An = Am;
    const int Bm = m;
    const int Bn = n;

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

    /* currently only support 1 size */
    xkblas_context_t * context = xkblas_context_get();
    int args[2] = {m, n};
    int * tile = context->conf.kernels.trsm.tile;
    if (tile[0] == 0 || tile[1] == 0)
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_TRSM, args, tile);

    XKBLAS_IMPL("TODO: trsm tiling");
    assert(tile[0] == tile[1]);

    /* set tiling parameters */
    int Amb = tile[0];
    int Anb = tile[0];
    int Amt = XKBLAS_NUM_OF_TILES(Am, Amb);
    int Ant = XKBLAS_NUM_OF_TILES(An, Anb);

    int Bmb = tile[0];
    int Bnb = tile[0];
    int Bmt = XKBLAS_NUM_OF_TILES(Bm, Bmb);
    int Bnt = XKBLAS_NUM_OF_TILES(Bn, Bnb);

    int tk, tm, tn;
    int bs_km, bs_kn, bs_mm, bs_nn;

    TYPE zone       = (TYPE) 1.0;
    TYPE mzone      = (TYPE)-1.0;
    TYPE minvalpha  = (TYPE)-1.0 / *alpha;
    TYPE lalpha;

    /* CblasLeft / CblasUpper / CblasNoTrans  */
    if (side == CblasLeft) {
        if (uplo == CblasUpper) {
            if (transA == CblasNoTrans) {
                for (tk = 0; tk < Bmt; tk++) {
                    bs_km  = (tk == 0) ? Bm-(Bmt-1)*Bmb : Bmb;
                    lalpha = (tk == 0) ? *alpha : zone;
                    for (tn = 0; tn < Bnt; tn++) {
                        bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                        xkblas_£trsm_tile_async(
                            context,
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A, (Bmt-1-tk)*Amb, (Bmt-1-tk)*Anb, lda,
                            B, (Bmt-1-tk)*Bmb,         tn*Bnb, ldb
                        );
                    }
                    for (tm = tk+1; tm < Bmt; ++tm) {
                        for (tn = 0; tn < Bnt; ++tn) {
                            bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            xkblas_£gemm_tile_async(
                                context,
                                CblasNoTrans, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                &mzone,
                                A, (Bmt-1-tm)*Amb, (Bmt-1-tk)*Anb, lda,
                                B, (Bmt-1-tk)*Bmb,         tn*Bnb, ldb,
                                &lalpha,
                                B, (Bmt-1-tm)*Bmb,         tn*Bnb, ldb
                            );
                        }
                    }
                }
            }

    # if 0 // REMOVE ME WHEN SUPPORING OTHER CONF
            /*
             *  CblasLeft / CblasUpper / Cblas[Conj]Trans
             */
            else {
                for (k = 0; k < Bmt; k++) {
                    bs_km = k == Bmt-1 ? Bm-k*Bmb : Bmb;
                    lda = lda;
                    ldb = ldb;
                    lalpha = k == 0 ? *alpha : zone;
                    for (n = 0; n < Bnt; n++) {
                        bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                        INSERT_TASK_ztrsm(
                            side, uplo, transA, diag,
                            bs_km, bs_nn,
                            lalpha, A(k, k), lda,
                                    B(k, n), ldb);
                    }
                    for (m = k+1; m < Bmt; m++) {
                        bs_mm = m == Bmt-1 ? Bm-m*Bmb : Bmb;
                        ldbm = ldb;
                        for (n = 0; n < Bnt; n++) {
                            bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                            xkblas_£trsm_tile_async(
                                transA, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                mzone,  A(k, m), lda,
                                        B(k, n), ldb,
                                lalpha, B(m, n), ldbm);
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
                for (k = 0; k < Bmt; k++) {
                    bs_km = k == Bmt-1 ? Bm-k*Bmb : Bmb;
                    lda = lda;
                    ldb = ldb;
                    lalpha = k == 0 ? *alpha : zone;
                    for (n = 0; n < Bnt; n++) {
                        bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                        INSERT_TASK_ztrsm(
                            side, uplo, transA, diag,
                            bs_km, bs_nn,
                            lalpha, A(k, k), lda,
                                    B(k, n), ldb);
                    }
                    for (m = k+1; m < Bmt; m++) {
                        bs_mm = m == Bmt-1 ? Bm-m*Bmb : Bmb;
                        lda = lda;
                        ldbm = ldb;
                        for (n = 0; n < Bnt; n++) {
                            bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                            xkblas_£trsm_tile_async(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                mzone,  A(m, k), lda,
                                        B(k, n), ldb,
                                lalpha, B(m, n), ldbm);
                        }
                    }
                }
            }
            /*
             *  CblasLeft / CblasLower / Cblas[Conj]Trans
             */
            else {
                for (k = 0; k < Bmt; k++) {
                    bs_km = k == 0 ? Bm-(Bmt-1)*Bmb : Bmb;
                    lda = lda;
                    ldb = ldb;
                    lalpha = k == 0 ? *alpha : zone;
                    for (n = 0; n < Bnt; n++) {
                        bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                        INSERT_TASK_ztrsm(
                            side, uplo, transA, diag,
                            bs_km, bs_nn,
                            lalpha, A(Bmt-1-k, Bmt-1-k), lda,
                                    B(Bmt-1-k,        n), ldb);
                    }
                    for (m = k+1; m < Bmt; m++) {
                        ldbm = ldb;
                        for (n = 0; n < Bnt; n++) {
                            bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                            xkblas_£trsm_tile_async(
                                transA, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                mzone,  A(Bmt-1-k, Bmt-1-m), lda,
                                        B(Bmt-1-k, n       ), ldb,
                                lalpha, B(Bmt-1-m, n       ), ldbm);
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
                for (k = 0; k < Bnt; k++) {
                    bs_kn = k == Bnt-1 ? Bn-k*Bnb : Bnb;
                    lda = lda;
                    lalpha = k == 0 ? *alpha : zone;
                    for (m = 0; m < Bmt; m++) {
                        bs_mm = m == Bmt-1 ? Bm-m*Bmb : Bmb;
                        ldbm = ldb;
                        INSERT_TASK_ztrsm(
                            side, uplo, transA, diag,
                            bs_mm, bs_kn,
                            lalpha, A(k, k), lda,  /* lda * bs_kn */
                                    B(m, k), ldbm); /* ldb * bs_kn */
                    }
                    for (m = 0; m < Bmt; m++) {
                        bs_mm = m == Bmt-1 ? Bm-m*Bmb : Bmb;
                        ldbm = ldb;
                        for (n = k+1; n < Bnt; n++) {
                            bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                            xkblas_£trsm_tile_async(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                mzone,  B(m, k), ldbm,  /* ldb * Bmb   */
                                        A(k, n), lda,  /* lda * bs_nn */
                                lalpha, B(m, n), ldbm); /* ldb * bs_nn */
                        }
                    }
                }
            }
            /*
             *  CblasRight / CblasUpper / Cblas[Conj]Trans
             */
            else {
                for (k = 0; k < Bnt; k++) {
                    bs_kn = k == 0 ? Bn-(Bnt-1)*Bnb : Bnb;
                    lda = lda;
                    for (m = 0; m < Bmt; m++) {
                        bs_mm = m == Bmt-1 ? Bm-m*Bmb : Bmb;
                        ldbm = ldb;
                        INSERT_TASK_ztrsm(
                            side, uplo, transA, diag,
                            bs_mm, bs_kn,
                            *alpha, A(Bnt-1-k, Bnt-1-k), lda,  /* lda * bs_kn */
                                   B(       m, Bnt-1-k), ldbm); /* ldb * bs_kn */

                        for (n = k+1; n < Bnt; n++) {
                            lda = lda;
                            xkblas_£trsm_tile_async(
                                CblasNoTrans, transA,
                                bs_mm, Bnb, bs_kn,
                                minvalpha, B(m,       Bnt-1-k), ldbm,  /* ldb  * bs_kn */
                                           A(Bnt-1-n, Bnt-1-k), lda, /* Amb * bs_kn (Never last row) */
                                zone,      B(m,       Bnt-1-n), ldbm); /* ldb  * Bnb   */
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
                for (k = 0; k < Bnt; k++) {
                    bs_kn = k == 0 ? Bn-(Bnt-1)*Bnb : Bnb;
                    lda = lda;
                    lalpha = k == 0 ? *alpha : zone;
                    for (m = 0; m < Bmt; m++) {
                        bs_mm = m == Bmt-1 ? Bm-m*Bmb : Bmb;
                        ldbm = ldb;
                        INSERT_TASK_ztrsm(
                            side, uplo, transA, diag,
                            bs_mm, bs_kn,
                            lalpha, A(Bnt-1-k, Bnt-1-k), lda,  /* lda * bs_kn */
                                    B(      m, Bnt-1-k), ldbm); /* ldb * bs_kn */

                        for (n = k+1; n < Bnt; n++) {
                            xkblas_£trsm_tile_async(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, Bnb, bs_kn,
                                mzone,  B(m,       Bnt-1-k), ldbm,  /* ldb * bs_kn */
                                        A(Bnt-1-k, Bnt-1-n), lda,  /* lda * Bnb   */
                                lalpha, B(m,       Bnt-1-n), ldbm); /* ldb * Bnb   */
                        }
                    }
                }
            }
            else {
                for (k = 0; k < Bnt; k++) {
                    bs_kn = k == Bnt-1 ? Bn-k*Bnb : Bnb;
                    lda = lda;
                    for (m = 0; m < Bmt; m++) {
                        bs_mm = m == Bmt-1 ? Bm-m*Bmb : Bmb;
                        ldbm = ldb;
                        INSERT_TASK_ztrsm(
                            side, uplo, transA, diag,
                            bs_mm, bs_kn,
                            *alpha, A(k, k), lda,  /* lda * bs_kn */
                                    B(m, k), ldbm); /* ldb * bs_kn */

                        for (n = k+1; n < Bnt; n++) {
                            bs_nn = n == Bnt-1 ? Bn-n*Bnb : Bnb;
                            lda = lda;
                            xkblas_£trsm_tile_async(
                                CblasNoTrans, transA,
                                bs_mm, bs_nn, Bmb,
                                minvalpha, B(m, k), ldbm,  /* ldb  * bs_kn */
                                           A(n, k), lda, /* lda * bs_kn */
                                zone,      B(m, n), ldbm); /* ldb  * bs_nn */
                        }
                    }
                }
            }
    # endif
        }
    }

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
    assert(args->transA == CblasNoTrans);
    assert(args->side   == CblasLeft);

    const Access * A = launcher->task->accesses + 0;
    const Access * B = launcher->task->accesses + 1;

    # ifndef NDEBUG
    XKBLAS_INFO("Calling cublasTrsm(side=%d, uplo=%d, transA=%d, diag=%d, m=%d, n=%d, alpha=%p, A=%p, lda=%d, B=%p, ldb=%d) on task=`%s`",
        args->side, args->uplo,
        args->transA, args->diag,
        &(args->alpha),
        args->m, args->n,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) B->device_view.addr,
        B->device_view.ld,
        launcher->task->label
    );
    #endif /* NDEBUG */

    cublasStatus_t res;
    res = cublas££trsm(
        handle,
        cblas2cublas_side(args->side), cblas2cublas_uplo(args->uplo),
        cblas2cublas_op(args->transA), cblas2cublas_diag(args->diag),
        args->m, args->n,
        (const CU_TYPE *) &(args->alpha),
        (const CU_TYPE *) A->device_view.addr, A->device_view.ld,
              (CU_TYPE *) B->device_view.addr, B->device_view.ld
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
