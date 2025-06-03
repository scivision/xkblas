/* ************************************************************************** */
/*                                                                            */
/*   syrk.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/03 15:23:28 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:35:59 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
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
 * @brief Chameleon zgemm wrappers
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

# include "auto-tile.h"
# include "context.h"
# include "xkblas/kernel-type.h"
# include "xkblas/cblas.h"
# include "xkblas/xkblas-experimental.h"

# include <xkrt/support.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/utils/min-max.h>
# include <xkrt/memory/access/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>

# include <cassert>

typedef struct args_t
{
    args_t(
        int uplo,
        int trans,
        size_t n, size_t k,
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
    const size_t n;
    const size_t k;
    const TYPE alpha;
    const TYPE beta;

} args_t;

static task_format_id_t format_id;

/* m, n, k are matrix sizes
 * Am, An, ..., Cn are index of the tile begining */
int
xkblas_£syrk_tile_async(
    xkblas_context_t * context,
    xkrt_distribution_t * d,
    int uplo, int trans,
    size_t n, size_t k,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;
    const size_t C_offset_m = Ctm * Cmb;
    const size_t C_offset_n = Ctn * Cnb;

    LOGGER_INFO("Submitting tile C=(%zu,%zu) of size (%zu,%zu)", C_offset_m, C_offset_n, n, k);

    # define AC 2
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t);

    task_t * task = thread->allocate_task(task_size + args_size);
    new(task) task_t(format_id, flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 1;
    xkrt_device_global_id_t device_global_id = d ? xkrt_distribution_get(d, Ctm, Ctn) : UNSPECIFIED_DEVICE_GLOBAL_ID;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t * args = (args_t *) TASK_ARGS(task, task_size);
    new(args) args_t(uplo, trans, n, k, *alpha, *beta);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label),
            "syrk(A=(%zu,%zu) ; C=(%zu,%zu))",
            A_offset_m, A_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    const size_t Am = n; // (transA == CblasNoTrans) ? m : k;
    const size_t An = k; // (transA == CblasNoTrans) ? k : m;
    const size_t Cm = n;
    const size_t Cn = n;

    # define AC 2
    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new(accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R, ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    new(accesses + 1) access_t(task, MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, Cm, Cn, sizeof(TYPE), Cmode        , ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    thread->resolve<AC>(task, accesses);
    # undef AC

    context->runtime.task_commit(task);

    return 0;
}

int
xkblas_£gemm_tile_async(
    xkblas_context_t * context,
    xkrt_distribution_t * d,
    int transA, int transB,
    const size_t m, const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    const TYPE * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc
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
        LOGGER_FATAL("illegal value of uplo");
        return -1;
    }

    if ((trans != CblasNoTrans) && (trans != CblasTrans))
    {
        LOGGER_FATAL("illegal value of trans");
        return -2;
    }

    if (n < 0)
    {
        LOGGER_FATAL("illegal value of N");
        return -3;
    }

    if (k < 0)
    {
        LOGGER_FATAL("illegal value of K");
        return -4;
    }

    const size_t Am = (trans == CblasNoTrans) ? n : k;
    const size_t An = (trans == CblasNoTrans) ? k : n;

    if (lda < MAX(1, Am))
    {
        LOGGER_FATAL("illegal value of lda");
        return -7;
    }

    if (ldc < MAX(1, n))
    {
        LOGGER_FATAL("illegal value of ldc");
        return -10;
    }

    const size_t Cm = n;
    const size_t Cn = n;

    xkblas_context_t * context = xkblas_context_get();

    size_t ts = context->conf.kernels[XKBLAS_KERNEL_TYPE_SYRK].tile;
    if (ts == 0)
    {
        int args[2] = {n, k};
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_SYRK, args, &ts);
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
    const int ngpus = context->runtime.drivers.devices.n - 1;
    xkrt_distribution_t d;
    xkrt_distribution_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Cm, Cn, Cmb, Cnb);

    const TYPE one = (TYPE) 1.0;

    # define A(tm, tn) A, tm, tn, Amb, Anb
    # define C(tm, tn) C, tm, tn, Cmb, Cnb

    for (int tn = 0; tn < Cnt; ++tn)
    {
        const size_t bs_nn = (tn == Cnt-1) ? (Cn-tn*Cnb) : Cnb;
        if (trans == CblasNoTrans)
        {
            for (int tk = 0; tk < Ant; ++tk)
            {
                const size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                const TYPE zbeta = (tk == 0) ? *beta : one;
                xkblas_£syrk_tile_async(
                    context,
                    &d,
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
                    const size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        const size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            &d,
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
                    const size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        const size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            &d,
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
                const size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                const TYPE zbeta = (tk == 0) ? *beta : one;
                xkblas_£syrk_tile_async(
                    context,
                    &d,
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
                    const size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        const size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            &d,
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
                    const size_t bs_mm = (tm == Cmt-1) ? (Cm-tm*Cmb) : Cmb;
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        const size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        xkblas_£gemm_tile_async(
                            context,
                            &d,
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

    LOGGER_INFO("TRSM dependency graph submitted");
    return 0;
}

# pragma message(TODO "The current design has the following flaws: (1) per-driver routine should be implemented in the driver(so they can be loaded dynamically), (2) there is yet another global 'task format' variable and (3) task format must be explicitely registered")

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
    const access_t * C = accesses + 1;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    args_t * args = (args_t *) TASK_ARGS(task);
    assert(args);

    # ifndef NDEBUG
    LOGGER_INFO("Calling cublasSyrk(n=%zu, k=%zu, A=%p, lda=%zu, C=%p, ldc=%zu) on task=`%s`",
        args->n, args->k,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) C->device_view.addr,
        C->device_view.ld,
        task->label
    );
    #endif /* NDEBUG */

    XKBLAS_CUBLAS_CALL(
        cublas££syrk(
            handle,
            cblas2cublas_uplo(args->uplo), cblas2cublas_op(args->trans),
            (int) args->n, (int) args->k,
            (const CU_TYPE *) &args->alpha,
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const CU_TYPE *) &args->beta,
            (      CU_TYPE *) C->device_view.addr, (int) C->device_view.ld
        )
    );
}
# endif /* XKRT_SUPPORT_CUDA */

# ifdef XKRT_SUPPORT_HOST
static void
body_cpu(void * args)
{
    LOGGER_DEBUG("Executing a syrk on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£syrk_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

    # if XKRT_SUPPORT_HOST
    format.f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format.f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda;
    # endif /* XKRT_SUPPORT_CUDA */

    snprintf(format.label, sizeof(format.label), "£syrk");
    format_id = xkblas_task_format_create(&format);
}
