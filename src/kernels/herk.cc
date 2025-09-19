/* ************************************************************************** */
/*                                                                            */
/*   herk.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/19 15:02:29 by Romain PEREIRA         / _______ \       */
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

/**
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon zherk wrappers
 *
 * @version 1.0.0
 * @comment This file has been automatically generated
 *          from Plasma 2.5.0 for CHAMELEON 1.0.0
 * @author Mathieu Faverge
 * @author Emmanuel Agullo
 * @author Cedric Castagnede
 * @author Thierry Gautier
 * @date 2018-11-20
 * @precisions normal z -> s d c
 * This file was merged from Chameleon by Thierry Gautier for Kaapi that
 * support natively 2D memory view.
 */

# include <xkrt/support.h>

# include <xkblas/auto-tile.h>
# include <xkblas/xkblas.hpp>
# include <xkblas/cblas.h>

# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/utils/min-max.h>
# include <xkrt/memory/access/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/support.h>

# include <cassert>

# if XKRT_SUPPORT_SYCL
#  include <sycl/sycl.hpp>
#  include <oneapi/mkl.hpp>
#  include <sycl/ext/oneapi/backend/level_zero.hpp>
#  include <xkblas/oneapi-mkl-helper.h>
#  define XKBLAS_NO_DEFAULT_BLAS_ENUM
# endif

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    args_t(
        int uplo, int trans,
        size_t n, size_t k,
        const TYPE_REAL alpha,
        const TYPE_REAL beta
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
    const size_t n;
    const size_t k;
    const TYPE_REAL alpha;
    const TYPE_REAL beta;
};

/* m, n, k are matrix sizes
 * A_offset_m, A_offset_n, ..., C_offset_n are index of the tile begining */
TYPED
int
xkblas_t::herk_tile_async(
    int uplo, int trans,
    const size_t n, const size_t k,
    const TYPE_REAL * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE_REAL * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    distribution_t * d
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;
    const size_t C_offset_m = Ctm * Cmb;
    const size_t C_offset_n = Ctn * Cnb;

    # define AC 2
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, HERK), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 1;
    device_global_id_t device_global_id = d ? distribution2D_get(d, Ctm, Ctn) : UNSPECIFIED_DEVICE_GLOBAL_ID;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(uplo, trans, n, k, *alpha, *beta);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label),
            "herk(A=(%zd,%zd) ; C=(%zd,%zd))",
            A_offset_m, A_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    const size_t Am = (trans == CblasNoTrans) ? n : k;
    const size_t An = (trans == CblasNoTrans) ? k : n;
    const size_t Cm = n;
    const size_t Cn = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, Cm, Cn, sizeof(TYPE), Cmode        , ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::herk_async(
    int uplo, int trans,
    int n, int k,
    const TYPE_REAL * alpha,
    const TYPE * A, int lda,
    const TYPE_REAL * beta,
          TYPE * C, int ldc
) {
    if (n == 0 || ((*alpha == 0.0 || k == 0) && *beta == 1.0))
        return 0;

    if ((uplo != CblasUpper) && (uplo != CblasLower))
    {
        LOGGER_FATAL("illegal value of uplo");
        return -1;
    }

    /* Check input arguments */
    if ((trans != CblasNoTrans) && (trans != CblasConjTrans))
    {
        LOGGER_FATAL("illegal value of trans");
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

    const size_t Am = (trans == CblasNoTrans) ? n : k;
    const size_t An = (trans == CblasNoTrans) ? k : n;
    const size_t Cm = n;
    const size_t Cn = n;

    if (lda < MAX(1, Am))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    if (ldc < MAX(1, Cm))
    {
        LOGGER_FATAL("illegal value of ldc");
        return -13;
    }

    xkblas_t * context = xkblas_get();

    size_t ts = context->conf.kernels[HERK].tile;
    if (ts == 0)
    {
        int args[2] = {n, k};
        xkblas_kernel_auto_tile(HERK, args, &ts);
    }

    /* set tiling parameters */
    const size_t Amb = ts;
    const size_t Anb = ts;
    const size_t Cmb = ts;
    const size_t Cnb = ts;

    const size_t Amt = NUM_OF_TILES(Am, Amb);
    const size_t Ant = NUM_OF_TILES(An, Anb);
    const size_t Cmt = NUM_OF_TILES(Cm, Cmb);
    const size_t Cnt = NUM_OF_TILES(Cn, Cnb);

    /* distribute C in a cyclic-block manner */
    const int ngpus = context->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Cm, Cn, Cmb, Cnb);

    const TYPE one              = (TYPE) 1.0;
    const TYPE complex_alpha    = (TYPE) *alpha;
    const TYPE complex_beta     = (TYPE) *beta;

    # define A(I, J) A, (I), (J), Amb, Anb, lda
    # define C(I, J) C, (I), (J), Cmb, Cnb, ldc

    for (size_t tn = 0; tn < Cnt; ++n)
    {
        size_t bs_nn = (tn == Cnt-1) ? (Cn-tn*Cnb) : Cnb;

        if (trans == CblasNoTrans)
        {
            for (size_t tk = 0; tk < Ant; ++tk)
            {
                size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                TYPE_REAL dbeta = (tk == 0) ? *beta : 1.0;
                this->herk_tile_async<P>(
                    uplo, trans,
                    bs_nn, bs_kn,
                    alpha,
                    A(tn, tk),
                    beta,
                    C(tn, tn),
                    &d
                );
            }
            if (uplo == CblasLower)
            {
                for (size_t tm = tn+1; tm < Cmt; ++tm)
                {
                    size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE b = (tk == 0) ? complex_beta : one;
                        this->gemm_tile_async<P>(
                            trans, CblasConjTrans,
                            bs_mm, bs_nn, bs_kn,
                            &complex_alpha,
                            A(tm, tk),
                            A(tn, tk),
                            &b,
                            C(tm, tn),
                            &d
                        );
                    }
                }
            }
            else
            {
                for (size_t tm = tn+1; tm < Cmt; ++tm)
                {
                    size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE b = (tk == 0) ? complex_beta : one;
                        this->gemm_tile_async<P>(
                            trans, CblasConjTrans,
                            bs_nn, bs_mm, bs_kn,
                            &complex_alpha,
                            A(tn, tk),
                            A(tm, tk),
                            &b,
                            C(tn, tm),
                            &d
                        );
                    }
                }
            }
        }
        else
        {
            for (size_t tk = 0; tk < Amt; ++tk)
            {
                size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                this->herk_tile_async<P>(
                    uplo, trans,
                    bs_nn, bs_km,
                    alpha,
                    A(tk, tn),
                    beta,
                    C(tn, tn),
                    &d
                );
            }
            if (uplo == CblasLower)
            {
                for (size_t tm = tn+1; tm < Cmt; ++tm)
                {
                    size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE b = (tk == 0) ? complex_beta : one;
                        this->gemm_tile_async<P>(
                            trans, CblasNoTrans,
                            bs_mm, bs_nn, bs_km,
                            &complex_alpha,
                            A(tk, tm),
                            A(tk, tn),
                            &b,
                            C(tm, tn),
                            &d
                        );
                    }
                }
            }
            else
            {
                for (size_t tm = tn+1; tm < Cmt; ++tm)
                {
                    size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE b = (tk == 0) ? complex_beta : one;
                        this->gemm_tile_async<P>(
                            trans, CblasNoTrans,
                            bs_nn, bs_mm, bs_km,
                            &complex_alpha,
                            A(tk, tn),
                            A(tk, tm),
                            &b,
                            C(tn, tm),
                            &d
                        );
                    }
                }
            }
        }
    }

    # undef A
    # undef C

    LOGGER_DEBUG("HERK dependency graph submitted");

    return 0;
}

# if XKRT_SUPPORT_CUDA
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE_REAL, typename CU_TYPE>
static inline void
body_cuda_run(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    cublasHandle_t handle = stream->cu.blas.handle;
    assert(handle);

    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * C = accesses + 1;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            cblas2cublas_uplo(args->uplo), cblas2cublas_op(args->trans),
            (int) args->n, (int) args->k,
            (const CU_TYPE_REAL *) &args->alpha,
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const CU_TYPE_REAL *) &args->beta,
            (      CU_TYPE *) C->device_view.addr, (int) C->device_view.ld
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
    if constexpr (P == xkblas_precision_t::C)
        body_cuda_run<P, cublasCherk, float, cuComplex>(stream, instr, idx);

    if constexpr (P == xkblas_precision_t::C)
        body_cuda_run<P, cublasZherk, double, cuDoubleComplex>(stream, instr, idx);
}
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_HOST
TYPED
static void
body_cpu(void * args)
{
    LOGGER_FATAL("Executing a herk on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_HERK(
    task_format_t * format
) {
    # if XKRT_SUPPORT_HOST
    format->f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_cpu<P>;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKRT_SUPPORT_CUDA */
}

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_HERK<P>(task_format_t * format); \
    template int xkblas_t::herk_async<P>(int uplo, int trans, int n, int k, const xkblas_precision_type_real_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_real_t<P> * beta, xkblas_precision_type_t<P> * C, int ldc);    \
    template int xkblas_t::herk_tile_async<P>(int uplo, int trans, const size_t n, const size_t k, const xkblas_precision_type_real_t<P> * alpha, const xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, const xkblas_precision_type_real_t<P> * beta, xkblas_precision_type_t<P> * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc, distribution_t * d);

// XKBLAS_FORALL_PRECISIONS(DEFINE)
DEFINE(C);
DEFINE(Z);

# undef DEFINE

# if 0
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
# endif
