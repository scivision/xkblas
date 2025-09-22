/* ************************************************************************** */
/*                                                                            */
/*   potrf.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/19 22:13:20 by Romain PEREIRA         / _______ \       */
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
 * @brief Chameleon zpotrf wrappers
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

# if XKBLAS_SUPPORT_SYCL
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
        const int uplo,
        const int n
    ) :
        uplo(uplo),
        n(n)
    {}

    ~args_t() {}

    const int uplo;
    const int n;
};

/* m, n, k are matrix sizes
 * A_offset_m, A_offset_n, ..., C_offset_n are index of the tile begining */
TYPED
int
xkblas_t::potrf_tile_async(
    int uplo,
    int n,
    TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    distribution_t * d
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;

    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, POTRF), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 0;
    device_global_id_t device_global_id = d ? distribution2D_get(d, Atm, Atn) : UNSPECIFIED_DEVICE_GLOBAL_ID;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(uplo, n);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "potrf(A=(%zd,%zd)", A_offset_m, A_offset_n);
    # endif /* NDEBUG */

    const size_t Am = n;
    const size_t An = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

/**Parallel tile Cholesky factorization - dynamic scheduling */
TYPED
int
xkblas_t::potrf_async(
    int uplo,
    int n,
    TYPE * A,
    int lda
) {
    if ((uplo != CblasUpper) && (uplo != CblasLower))
    {
        LOGGER_FATAL("illegal value of uplo");
        return -2;
    }

    if (n < 0)
    {
        LOGGER_FATAL("illegal value of N");
        return -5;
    }

    if (lda < MAX(1, n))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    xkblas_t * context = xkblas_get();
    size_t ts = context->conf.kernels[POTRF].tile;
    if (ts == 0)
    {
        int args[1] = { n };
        xkblas_kernel_auto_tile(POTRF, args, &ts);
    }

    /* set tiling parameters */
    const size_t Am = n;
    const size_t An = n;

    const size_t Amb = ts;
    const size_t Anb = ts;

    const size_t Amt = NUM_OF_TILES(Am, Amb);
    const size_t Ant = NUM_OF_TILES(An, Anb);

    /* distribute C in a cyclic-block manner */
    const int ngpus = context->runtime.get_ndevices() - 1;

    // TODO: i am not sure what distribution to use for potrf
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Am, An, Amb, Anb);

    const TYPE  one_complex = (TYPE) 1.0;
    const TYPE mone_complex = (TYPE)-1.0;

    const TYPE_REAL  one = (TYPE_REAL) 1.0;
    const TYPE_REAL mone = (TYPE_REAL)-1.0;

    # define A(I, J) A, (I), (J), Amb, Anb, lda

    if (uplo == CblasLower)
    {
        for (size_t tk = 0; tk < Amt; ++tk)
        {
            size_t bs_km = (tk == Amt - 1) ? (Am-tk*Amb) : Amb;

            //options.priority = 2*A->mt - 2*k;
            this->potrf_tile_async<P>(
                CblasLower,
                bs_km,
                A(tk, tk),
                &d
            );

            for (size_t tm = tk+1; tm < Amt; ++tm)
            {
                size_t bs_mm = (tm == Amt-1) ? (Am-tm*Amb) : Amb;
                //options.priority = 2*A->mt - 2*k - m;
                this->trsm_tile_async<P>(
                    CblasRight, CblasLower,
                    CblasConjTrans, CblasNonUnit,
                    bs_mm, Amb,
                    &one_complex,
                    A(tk, tk),
                    A(tm, tk),
                    &d
                );
            }

            for (size_t tn = tk + 1; tn < Ant; ++tn)
            {
                size_t bs_nn = (tn == Ant-1) ? (An-tn*Anb) : Anb;

                //options.priority = 2*A->mt - 2*k - n;

                // TODO: is it really correct replacing the herk with a syrk here ?
                if constexpr (P == xkblas_precision_t::S || P == xkblas_precision_t::D)
                {
                    this->syrk_tile_async<P>(
                        CblasLower, CblasNoTrans,
                        bs_nn, Anb,
                        &mone,
                        A(tn, tk),
                        &one,
                        A(tn, tn),
                        &d
                    );
                }
                else
                {
                    this->herk_tile_async<P>(
                        CblasLower, CblasNoTrans,
                        bs_nn, Anb,
                        &mone,
                        A(tn, tk),
                        &one,
                        A(tn, tn),
                        &d
                    );
                }

                for (size_t tm = tn + 1; tm < Amt ; ++tm)
                {
                    size_t bs_mm = (tm == Amt-1) ? (Am - tm*Amb) : Amb;

                    //options.priority = 2*A->mt - 2*k - n - m;
                    this->gemm_tile_async<P>(
                        CblasNoTrans, CblasConjTrans,
                        bs_mm, bs_nn, Amb,
                        &mone_complex,
                        A(tm, tk),
                        A(tn, tk),
                        &one_complex,
                        A(tm, tn),
                        &d
                    );
                }
            }
        }
    }
    else
    {
        for (size_t tk = 0; tk < Ant; ++tk)
        {
            size_t bs_km = (tk == Ant-1) ? (An-tk*Anb) : Anb;

            //options.priority = 2*A->nt - 2*k;
            this->potrf_tile_async<P>(
                CblasUpper,
                bs_km,
                A(tk, tk),
                &d
            );

            for (size_t tn = tk+1; tn < Ant; ++tn)
            {
                size_t bs_nn = (tn == Ant-1) ? (An - tn*Anb) : Anb;

                //options.priority = 2*A->nt - 2*k - n;
                this->trsm_tile_async<P>(
                    CblasLeft, CblasUpper,
                    CblasConjTrans, CblasNonUnit,
                    Amb, bs_nn,
                    &one_complex,
                    A(tk, tk),
                    A(tk, tn),
                    &d
                );
            }

            for (size_t tm = tk+1; tm < Amt ; ++tm)
            {
                size_t bs_mm = (tm == Amt-1) ? (Am - tm*Amb) : Amb;

                //options.priority = 2*A->nt - 2*k  - m;

                // TODO: is it really correct replacing the herk with a syrk here ?
                if constexpr (P == xkblas_precision_t::S || P == xkblas_precision_t::D)
                {
                    this->syrk_tile_async<P>(
                        CblasUpper, CblasConjTrans,
                        bs_mm, Amb,
                        &mone,
                        A(tk, tm),
                        &one,
                        A(tm, tm),
                        &d
                    );
                }
                else
                {
                    this->herk_tile_async<P>(
                        CblasUpper, CblasConjTrans,
                        bs_mm, Amb,
                        &mone,
                        A(tk, tm),
                        &one,
                        A(tm, tm),
                        &d
                    );
                }

                for (size_t tn = tm+1; tn < Ant; ++tn)
                {
                    size_t bs_nn = (tn == Ant-1) ? (An-tn*Anb) : Anb;

                    //options.priority = 2*A->nt - 2*k - n - m;
                    this->gemm_tile_async<P>(
                        CblasConjTrans, CblasNoTrans,
                        bs_mm, bs_nn, Amb,
                        &mone_complex,
                        A(tk, tm),
                        A(tk, tn),
                        &one_complex,
                        A(tm, tn),
                        &d
                    );
                }
            }
        }
    }

    # undef A

    LOGGER_DEBUG("POTRF dependency graph submitted");

    return 0;
}

# if XKBLAS_SUPPORT_CUDA
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
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
    assert(A->device_view.addr % A->host_view.sizeof_type == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    LOGGER_FATAL("Impl me, require cuSolver");
    # if 0
    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            cblas2cublas_op(args->transA), cblas2cublas_op(args->transB),
            (int) args->m, (int) args->n, (int) args->k,
            (const CU_TYPE *) &args->alpha,
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const CU_TYPE *) B->device_view.addr, (int) B->device_view.ld,
            (const CU_TYPE *) &args->beta,
            (      CU_TYPE *) C->device_view.addr, (int) C->device_view.ld
        )
    );
    # endif
}

TYPED
static void
body_cuda(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    LOGGER_FATAL("potrf currently not supported, gotta link with cuSolver and add the kernel to XKBLas");
}
# endif /* XKBLAS_SUPPORT_CUDA */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_POTRF(
    task_format_t * format
) {
    # if XKBLAS_SUPPORT_CUDA
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKBLAS_SUPPORT_CUDA */
}

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_POTRF<P>(task_format_t * format); \
    template int xkblas_t::potrf_async<P>(int uplo, int n, xkblas_precision_type_t<P> * A, int lda);  \
    template int xkblas_t::potrf_tile_async<P>(int uplo, int n, xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, distribution_t * d);
XKBLAS_FORALL_PRECISIONS(DEFINE);

# undef DEFINE

# if 0
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
# endif
