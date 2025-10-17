/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

/**
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon zgemv wrappers
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
        int transA,
        size_t m, size_t n,
        int incx, int incy,
        const TYPE alpha,
        const TYPE beta
    ) :
        transA(transA),
        m(m),
        n(n),
        incx(incx),
        incy(incy),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    const int transA;
    const size_t m;
    const size_t n;
    const int incx;
    const int incy;
    const TYPE alpha;
    const TYPE beta;
};

/** y := alpha.A.x + beta.y */
TYPED
int
xkblas_t::gemv_tile_async(
    int transA,
    const size_t m, const size_t n,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * x, const int incx,
    const TYPE * beta,
          TYPE * y, const size_t tm, const size_t mb, const int incy,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t y_offset_m = tm * mb ;

    # define AC 3
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, GEMV), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 2;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(transA, m, n, incx, incy, *alpha, *beta);

    # if XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label), "gemv(A ; x ; y=(%zd,))", y_offset_m);
    # endif /* XKRT_SUPPORT_DEBUG */

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t Axmode = (*alpha == (const TYPE) 0.0) ? ACCESS_MODE_V : ACCESS_MODE_R;
    access_mode_t  ymode = ( *beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, m, n, sizeof(TYPE), Axmode, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, x +     0, incx*n, sizeof(TYPE), Axmode, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 2) access_t(task, y + tm*mb, incy*m, sizeof(TYPE), ymode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::gemv_async(
    int transA,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * x, const int incx,
    const TYPE * beta,
          TYPE * y, const int incy
) {
    if (m == 0 || n == 0 || (*alpha == 0.0 && *beta == 1.0))
        return 0;

    /* Check input arguments */
    if (transA < CblasNoTrans || transA > CblasConjTrans)
    {
        LOGGER_FATAL("illegal value of transA");
        return -1;
    }

    if (m < 0)
    {
        LOGGER_FATAL( "illegal value of m");
        return -3;
    }

    if (n < 0)
    {
        LOGGER_FATAL("illegal value of n");
        return -4;
    }

    const size_t Am = (transA == CblasNoTrans) ? m : n;

    if ((size_t)lda < MAX(1, Am))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    if (incx != 1)
    {
        LOGGER_FATAL("incx 1 != not supported");
        return -10;
    }

    if (incy != 1)
    {
        LOGGER_FATAL("incy 1 != not supported");
        return -13;
    }

    xkblas_t * xkblas = xkblas_get();
    size_t ts = xkblas->conf.kernels[GEMV].tile;
    if (ts == 0)
    {
        int args[2] = {m, n};
        xkblas_routine_auto_tile(GEMV, args, &ts);
    }

    /* set tiling parameters */
    const size_t mb = ts;
    const size_t mt = NUM_OF_TILES(m, mb);

    /* distribute C in a cyclic-block manner */
    const int ngpus = xkblas->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, m, mb);

    // iterator on tiles
    for (size_t tm = 0; tm < mt; ++tm)
    {
        device_global_id_t device_global_id = distribution1D_get(&d, tm);
        size_t bs_mm = (tm == mt-1) ? (m-tm*mb) : mb;
        this->gemv_tile_async<P>(
            transA,
            bs_mm, n,
            alpha,
            A, lda,
            x, incx,
            beta,
            y, tm, mb, incy,
            device_global_id
        );
    }
    LOGGER_DEBUG("GEMV dependency graph submitted");

    return 0;
}

# if XKBLAS_SUPPORT_CUBLAS
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
    const access_t * x = accesses + 1;
    const access_t * y = accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(x->device_view.addr % x->host_view.sizeof_type == 0);
    assert(y->device_view.addr % y->host_view.sizeof_type == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            cblas2cublas_op(args->transA),
            (int) args->m, (int) args->n,
            (const CU_TYPE *) &args->alpha,
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const CU_TYPE *) x->device_view.addr, (int) args->incx,
            (const CU_TYPE *) &args->beta,
            (      CU_TYPE *) y->device_view.addr, (int) args->incy
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
    XKBLAS_CUBLAS_DISPATCH_PRECISION(gemv);
}
# endif /* XKBLAS_SUPPORT_CUBLAS */


# if XKBLAS_SUPPORT_CBLAS

template <xkblas_precision_t P, auto FUNC>
static void
body_cpu_run(task_t * task)
{
    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * x = accesses + 1;
    const access_t * y = accesses + 2;

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    if constexpr (P == xkblas_precision_t::S || P == xkblas_precision_t::D)
    {
        FUNC(
            CblasColMajor,
            (const enum CBLAS_TRANSPOSE) args->transA,
            (int) args->m, (int) args->n,
            (const TYPE  ) args->alpha,
            (const TYPE *) A->host_view.addr, (int) A->host_view.ld,
            (const TYPE *) x->host_view.addr, (int) args->incx,
            (const TYPE  ) args->beta,
                  (TYPE *) y->host_view.addr, (int) args->incy
        );
    }
    else
    {
        FUNC(
            CblasColMajor,
            (const enum CBLAS_TRANSPOSE) args->transA,
            (int) args->m, (int) args->n,
            (const TYPE *) &(args->alpha),
            (const TYPE *) A->host_view.addr, (int) A->host_view.ld,
            (const TYPE *) x->host_view.addr, (int) args->incx,
            (const TYPE *) &(args->beta),
                  (TYPE *) y->host_view.addr, (int) args->incy
        );
    }
}

TYPED
static void
body_cpu(task_t * task)
{
    if constexpr (P == xkblas_precision_t::S) body_cpu_run<P, cblas_sgemv>(task);
    if constexpr (P == xkblas_precision_t::D) body_cpu_run<P, cblas_dgemv>(task);
    if constexpr (P == xkblas_precision_t::C) body_cpu_run<P, cblas_cgemv>(task);
    if constexpr (P == xkblas_precision_t::Z) body_cpu_run<P, cblas_zgemv>(task);
}

# endif /* XKBLAS_SUPPORT_CBLAS */


//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_GEMV(
    task_format_t * format
) {
    # if XKBLAS_SUPPORT_CBLAS
    format->f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_cpu<P>;
    # endif /* XKBLAS_SUPPORT_CBLAS */

    # if XKBLAS_SUPPORT_CUBLAS
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKBLAS_SUPPORT_CUBLAS */
}

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_GEMV<P>(task_format_t * format); \
    template int xkblas_t::gemv_async<P>(int transA, int m, int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_t<P> * x, const int incx, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * y, const int incy);    \
    template int xkblas_t::gemv_tile_async<P>(int transA, const size_t m, const size_t n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_t<P> * x, const int incx, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * y, const size_t ytm, const size_t ymb, const int incy, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE

# if 0
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
# endif
