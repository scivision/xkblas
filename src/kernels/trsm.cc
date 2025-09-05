/* ************************************************************************** */
/*                                                                            */
/*   trsm.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/19 10:41:41 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/27 16:10:54 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkblas/auto-tile.h>
# include <xkblas/xkblas.hpp>
# include <xkblas/kernel.hpp>
# include <xkblas/cblas.h>

# include <xkrt/support.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/utils/min-max.h>
# include <xkrt/memory/access/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>

# include <cassert>

XKRT_NAMESPACE_USE;

TYPED
struct args_t
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

};

TYPED
int
xkblas_t::trsm_tile_async(
    int side, int uplo,
    int transA, int diag,
    const size_t m, const size_t n,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
          TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    distribution_t * d
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;
    const size_t B_offset_m = Btm * Bmb;
    const size_t B_offset_n = Btn * Bnb;

    # define AC 2
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, TRSM), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 1;
    device_global_id_t device_global_id = d ? distribution2D_get(d, Btm, Btn) : UNSPECIFIED_DEVICE_GLOBAL_ID;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(side, uplo, transA, diag, m, n, *alpha);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label),
            "trsm(A=(%zu,%zu) ; B=(%zu,%zu))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n);
    # endif /* NDEBUG */

    /* TODO: block size, is that correct ? */
    const size_t Am = (side == CblasLeft) ? m : n;
    const size_t An = (side == CblasLeft) ? m : n;
    const size_t Bm = m;
    const size_t Bn = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R , ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve<AC>(task, accesses);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::trsm_async(
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
        LOGGER_ERROR("illegal value of side");
        return -1;
    }

    if ((uplo != CblasUpper) && (uplo != CblasLower))
    {
        LOGGER_ERROR("illegal value of uplo");
        return -2;
    }

    if (((transA < CblasNoTrans) || (transA > CblasConjTrans)))
    {
        LOGGER_ERROR("illegal value of transA");
        return -3;
    }

    if ((diag != CblasUnit) && (diag != CblasNonUnit))
    {
        LOGGER_ERROR("illegal value of diag");
        return -4;
    }

    if (m < 0)
    {
        LOGGER_ERROR("illegal value of m");
        return -5;
    }

    if (n < 0)
    {
        LOGGER_ERROR("illegal value of n");
        return -6;
    }

    const size_t Am = (side == CblasLeft) ? m : n;
    const size_t An = Am;
    const size_t Bm = m;
    const size_t Bn = n;

    if (lda < MAX(1, An))
    {
        LOGGER_ERROR("illegal value of lda");
        return -8;
    }

    if (ldb < MAX(1, Bn))
    {
        LOGGER_ERROR("illegal value of ldb");
        return -10;
    }

    size_t ts = this->conf.kernels[TRSM].tile;
    if (ts == 0)
    {
        int args[2] = {m, n};
        xkblas_kernel_auto_tile(TRSM, args, &ts);
    }

    /* set tiling parameters */
    const size_t Amb = ts;
    const size_t Anb = ts;
    const size_t Bmb = ts;
    const size_t Bnb = ts;

    const size_t Amt = NUM_OF_TILES(Am, Amb);
    const size_t Ant = NUM_OF_TILES(An, Anb);
    const size_t Bmt = NUM_OF_TILES(Bm, Bmb);
    const size_t Bnt = NUM_OF_TILES(Bn, Bnb);

    /* distribute B in a cyclic-block manner */
    const int ngpus = this->runtime.drivers.devices.n - 1;
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Bm, Bn, Bmb, Bnb);

    TYPE one        = (TYPE) 1.0;
    TYPE mone       = (TYPE)-1.0;
    TYPE minvalpha  = (TYPE)-1.0 / *alpha;

    # pragma message(TODO "Block sizes truncation are suspicious to me here, double check")

    # define A(I, J) A, (I), (J), Amb, Anb, lda
    # define B(I, J) B, (I), (J), Bmb, Bnb, ldb

    /* CblasLeft / CblasUpper / CblasNoTrans  */
    if (side == CblasLeft) {
        if (uplo == CblasUpper) {
            if (transA == CblasNoTrans) {
                for (int tk = 0; tk < Bmt; tk++) {
                    size_t bs_km  = (tk == 0) ? Bm-(Bmt-1)*Bmb : Bmb;
                    TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (int tn = 0; tn < Bnt; tn++) {
                        size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Bmt-1-tk, Bmt-1-tk),
                            B(Bmt-1-tk,       tn),
                            &d
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; ++tm) {
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                &mone,
                                A(Bmt-1-tm, Bmt-1-tk),
                                B(Bmt-1-tk,       tn),
                                &lalpha,
                                B(Bmt-1-tm,       tn),
                                &d
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
                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(tk, tk),
                            B(tk, tn),
                            &d
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; tm++) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                transA, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                A(tk, tm),
                                B(tk, tn),
                                &lalpha,
                                B(tm, tn),
                                &d
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
                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(tk, tk),
                            B(tk, tn),
                            &d
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; ++tm) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                A(tm, tk),
                                B(tk, tn),
                                &lalpha,
                                B(tm, tn),
                                &d
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
                        this->trsm_tile_async<P>(
                            side, uplo, transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Bmt-1-tk, Bmt-1-tk),
                            B(Bmt-1-tk,       tn),
                            &d
                        );
                    }
                    for (int tm = tk+1; tm < Bmt; ++tm) {
                        for (int tn = 0; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                transA, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                &mone,
                                A(Bmt-1-tk, Bmt-1-tm),
                                B(Bmt-1-tk,       tn),
                                &lalpha,
                                B(Bmt-1-tm,       tn),
                                &d
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
                        this->trsm_tile_async<P>(
                            side, uplo, transA, diag,
                            bs_mm, bs_kn,
                            &lalpha,
                            A(tk, tk),
                            B(tm, tk),
                            &d
                        );
                    }
                    for (int tm = 0; tm < Bmt; ++tm) {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                B(tm, tk),
                                A(tk, tn),
                                &lalpha,
                                B(tm, tn),
                                &d
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
                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            alpha,
                            A(Bnt-1-tk, Bnt-1-tk),
                            B(tm, Bnt-1-tk),
                            &d
                        );

                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            this->gemm_tile_async<P>(
                                CblasNoTrans, transA,
                                bs_mm, Bnb, bs_kn,
                                &minvalpha,
                                B(tm, Bnt-1-tk),
                                A(Bnt-1-tn, Bnt-1-tk),
                                &one,
                                B(tm, Bnt-1-tn),
                                &d
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
                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            &lalpha,
                            A(Bnt-1-tk, Bnt-1-tk),
                            B(tm, Bnt-1-tk),
                            &d
                        );

                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, Bnb, bs_kn,
                                &mone,
                                B(tm, Bnt-1-tk),
                                A(Bnt-1-tk, Bnt-1-tn),
                                &lalpha,
                                B(tm, Bnt-1-tn),
                                &d
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
                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            alpha,
                            A(tk, tk),
                            B(tm, tk),
                            &d

                        );

                        for (int tn = tk+1; tn < Bnt; ++tn) {
                            size_t bs_nn = tn == Bnt-1 ? Bn-tn*Bnb : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, transA,
                                bs_mm, bs_nn, Bmb,
                                &minvalpha,
                                B(tm, tk),
                                A(tn, tk),
                                &one,
                                B(tm, tn),
                                &d
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

# if XKRT_SUPPORT_CUDA
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static void
body_cuda_run(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    assert(stream);

    cublasHandle_t handle = stream->cu.blas.handle;
    assert(handle);

    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            cblas2cublas_side(args->side), cblas2cublas_uplo(args->uplo),
            cblas2cublas_op(args->transA), cblas2cublas_diag(args->diag),
            (int) args->m, (int) args->n,
            (const CU_TYPE *) &(args->alpha),
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
                  (CU_TYPE *) B->device_view.addr, (int) B->device_view.ld
        )
    );
}

TYPED
static void
body_cuda(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    XKBLAS_CUBLAS_DISPATCH_PRECISION(trsm);
}

# endif /* XKRT_SUPPORT_CUDA */

# ifdef XKRT_SUPPORT_HOST
static void
body_cpu(void * args)
{
    LOGGER_DEBUG("Executing a trsm on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_TRSM(
    task_format_t * format
) {
    # if XKRT_SUPPORT_HOST
    format->f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu<P>;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format->f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKRT_SUPPORT_CUDA */
}

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_TRSM<P>(task_format_t * format); \
    template int xkblas_t::trsm_async<P>(int side, int uplo, int transA, int diag, int m, int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, xkblas_precision_type_t<P> * B, int ldb);    \
    template int xkblas_t::trsm_tile_async<P>(int side, int uplo, int transA, int diag, const size_t m, const size_t n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, xkblas_precision_type_t<P> * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb, distribution_t * d);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE

