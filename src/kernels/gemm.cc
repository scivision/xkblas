/* ************************************************************************** */
/*                                                                            */
/*   gemm.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/19 15:02:25 by Romain PEREIRA         / _______ \       */
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
        int transA, int transB,
        size_t m, size_t n, size_t k,
        const TYPE alpha,
        const TYPE beta
    ) :
        transA(transA),
        transB(transB),
        m(m),
        n(n),
        k(k),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    const int transA;
    const int transB;
    const size_t m;
    const size_t n;
    const size_t k;
    const TYPE alpha;
    const TYPE beta;

};

/* m, n, k are matrix sizes
 * A_offset_m, A_offset_n, ..., C_offset_n are index of the tile begining */
TYPED
int
xkblas_t::gemm_tile_async(
    int transA, int transB,
    const size_t m, const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    const TYPE * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    distribution_t * d
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;
    const size_t B_offset_m = Btm * Bmb;
    const size_t B_offset_n = Btn * Bnb;
    const size_t C_offset_m = Ctm * Cmb;
    const size_t C_offset_n = Ctn * Cnb;

    LOGGER_DEBUG("Submitting tile C=(%zd,%zd) of size (%zd,%zd)", C_offset_m, C_offset_n, m, n);

    # define AC 3
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, GEMM), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 2;
    device_global_id_t device_global_id = d ? distribution2D_get(d, Ctm, Ctn) : UNSPECIFIED_DEVICE_GLOBAL_ID;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(transA, transB, m, n, k, *alpha, *beta);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label),
            "gemm(A=(%zd,%zd) ; B=(%zd,%zd) ; C=(%zd,%zd))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    const size_t Am = (transA == CblasNoTrans) ? m : k;
    const size_t An = (transA == CblasNoTrans) ? k : m;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
    const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = m;
    const size_t Cn = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t ABmode = (*alpha == (const TYPE) 0.0) ? ACCESS_MODE_V : ACCESS_MODE_R;
    access_mode_t Cmode  = ( *beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ABmode, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ABmode, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 2) access_t(task, MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, Cm, Cn, sizeof(TYPE),  Cmode, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::gemm_async(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    if (m == 0 || n == 0 ||
            ((*alpha == 0.0 || k == 0) && *beta == 1.0))
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

    if (k < 0)
    {
        LOGGER_FATAL("illegal value of k");
        return -5;
    }

    const size_t Am = (transA == CblasNoTrans) ? m : k;
    const size_t An = (transA == CblasNoTrans) ? k : m;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
    // const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = m;
    const size_t Cn = n;

    if ((size_t)lda < MAX(1, Am))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    if ((size_t)ldb < MAX(1, Bm))
    {
        LOGGER_FATAL("illegal value of ldb");
        return -10;
    }

    if ((size_t)ldc < MAX(1, Cm))
    {
        LOGGER_FATAL("illegal value of ldc");
        return -13;
    }

    xkblas_t * context = xkblas_get();

    size_t ts = context->conf.kernels[GEMM].tile;
    if (ts == 0)
    {
        int args[3] = {m, n, k};
        xkblas_kernel_auto_tile(GEMM, args, &ts);
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
    // const size_t Bmt = NUM_OF_TILES(Bm, Bmb);
    // const size_t Bnt = NUM_OF_TILES(Bn, Bnb);
    const size_t Cmt = NUM_OF_TILES(Cm, Cmb);
    const size_t Cnt = NUM_OF_TILES(Cn, Cnb);

    /* distribute C in a cyclic-block manner */
    const int ngpus = context->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Cm, Cn, Cmb, Cnb);

    const TYPE one = (TYPE) 1.0;

    # define A(I, J) A, (I), (J), Amb, Anb, lda
    # define B(I, J) B, (I), (J), Bmb, Bnb, ldb
    # define C(I, J) C, (I), (J), Cmb, Cnb, ldc

    // iterator on tiles
    for (size_t tm = 0; tm < Cmt; ++tm)
    {
        size_t bs_mm = (tm == Cmt-1) ? (m-tm*Cmb) : Cmb;
        for (size_t tn = 0; tn < Cnt; tn++)
        {
            size_t bs_nn = (tn == Cnt-1) ? (n-tn*Cnb) : Cnb;
            // A: CblasNoTrans / B: CblasNoTrans
            if (transA == CblasNoTrans)
            {
                if (transB == CblasNoTrans)
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A(tm, tk),
                                B(tk, tn),
                                &zbeta,
                                C(tm, tn),
                                &d
                        );
                    }
                }
                // A: CblasNoTrans / B: CBlasTrans
                else
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A(tm, tk),
                                B(tn, tk),
                                &zbeta,
                                C(tm, tn),
                                &d
                        );
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
                        size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A(tk, tm),
                                B(tk, tn),
                                &zbeta,
                                C(tm, tn),
                                &d
                        );
                    }
                }
                // A: CblasTrans / B: CBlasTrans
                else
                {
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A(tk, tm),
                                B(tn, tk),
                                &zbeta,
                                C(tm, tn),
                                &d
                        );
                    }
                }
            }
        }
    }

    # undef A
    # undef B
    # undef C

    LOGGER_DEBUG("GEMM dependency graph submitted");

    return 0;
}

# if XKRT_SUPPORT_CUDA
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
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

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
}

TYPED
static void
body_cuda(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    XKBLAS_CUBLAS_DISPATCH_PRECISION(gemm);
}
# endif /* XKRT_SUPPORT_CUDA */


# if XKRT_SUPPORT_HIP
#  include <xkblas/hipblas-helper.h>
#  include <xkrt/driver/driver-hip.h>

template <xkblas_precision_t P, auto FUNC, typename HIP_TYPE>
static inline void
body_hip_run(
    stream_hip_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    hipblasHandle_t handle = stream->hip.blas.handle;
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

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_HIPBLAS_CALL(
        FUNC(
            handle,
            cblas2hipblas_op(args->transA), cblas2hipblas_op(args->transB),
            (int) args->m, (int) args->n, (int) args->k,
            (const HIP_TYPE *) &args->alpha,
            (const HIP_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const HIP_TYPE *) B->device_view.addr, (int) B->device_view.ld,
            (const HIP_TYPE *) &args->beta,
            (      HIP_TYPE *) C->device_view.addr, (int) C->device_view.ld
        )
    );
}

TYPED
static void
body_hip(
    stream_hip_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    XKBLAS_HIPBLAS_DISPATCH_PRECISION(gemm);
}
# endif /* XKRT_SUPPORT_HIP */


# if XKRT_SUPPORT_ZE

#  include <xkrt/driver/driver-ze.h>
#  include <xkrt/logger/logger-ze.h>

TYPED
static void
body_ze(
    stream_ze_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    // unpack arguments
    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task);

    # if XKRT_SUPPORT_ZE_SYCL_INTEROP

    sycl::queue & queue = stream->sycl.queue;
    oneapi::mkl::transpose transa = cblas2mkl_op(args->transA);
    oneapi::mkl::transpose transb = cblas2mkl_op(args->transB);
    std::int64_t m = args->m;
    std::int64_t n = args->n;
    std::int64_t k = args->k;
    oneapi::mkl::value_or_pointer<TYPE> alpha = args->alpha;
    oneapi::mkl::value_or_pointer<TYPE> beta = args->beta;
    const TYPE * a   = (const TYPE *) A->device_view.addr;
    const TYPE * b   = (const TYPE *) B->device_view.addr;
          TYPE * c   = (      TYPE *) C->device_view.addr;
    std::int64_t lda = (std::int64_t) A->device_view.ld;
    std::int64_t ldb = (std::int64_t) B->device_view.ld;
    std::int64_t ldc = (std::int64_t) C->device_view.ld;
    oneapi::mkl::blas::compute_mode mode = oneapi::mkl::blas::compute_mode::unset;
    const std::vector<sycl::event> dependencies = {};

    sycl::event event = oneapi::mkl::blas::column_major::gemm(
        queue,
        transa, transb,
        m, n, k,
        alpha,
        a, lda,
        b, ldb,
        beta,
        c, ldc,
        mode,
        dependencies
    );
    stream->ze.events.list[idx] = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(event);

    # else /* XKRT_SUPPORT_ZE_SYCL_INTEROP */
    LOGGER_FATAL("no blas impl for ze");
    # endif /* XKRT_SUPPORT_ZE_SYCL_INTEROP */

    # if 0

    // TODO : Intel do not provides provide the kernels direcly or similar interface to cuBlas,
    // so we could pass them via a zeCommandListAdKernelLaunch - or even to
    // simply call a gemm with a command list/queue

    // Retrieve the Level Zero context and device from the command list
    ze_context_handle_t ze_context;
    ze_device_handle_t ze_device;
    ZE_SAFE_CALL(zeCommandListGetContextHandle(stream->ze.command.list, &ze_context));
    ZE_SAFE_CALL(zeCommandListGetDeviceHandle(stream->ze.command.list, &ze_device));

    // Create SYCL platform and device from Level Zero context and device
    sycl::platform sycl_platform = sycl::platform::ext_oneapi_from_ze_context(ze_context);
    sycl::device sycl_device = sycl::device::ext_oneapi_from_ze_device(ze_device);

    // Create SYCL context from SYCL device
    sycl::context sycl_context(sycl_device);

    // Create SYCL queue from SYCL context and Level Zero command list
    sycl::queue sycl_queue(sycl_context, sycl::ext::oneapi::level_zero::command_list(ze_command_list));

    # endif
}

# endif

# if XKRT_SUPPORT_SYCL

#  include <xkrt/driver/driver-sycl.h>

TYPED
static void
body_sycl(
    stream_sycl_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    // unpack arguments
    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task);

    sycl::queue & queue = stream->sycl.queue;
    oneapi::mkl::transpose transa = cblas2mkl_op(args->transA);
    oneapi::mkl::transpose transb = cblas2mkl_op(args->transB);
    std::int64_t m = args->m;
    std::int64_t n = args->n;
    std::int64_t k = args->k;
    oneapi::mkl::value_or_pointer<TYPE> alpha = args->alpha;
    oneapi::mkl::value_or_pointer<TYPE> beta = args->beta;
    const TYPE * a   = (const TYPE *) A->device_view.addr;
    const TYPE * b   = (const TYPE *) B->device_view.addr;
          TYPE * c   = (      TYPE *) C->device_view.addr;
    std::int64_t lda = (std::int64_t) A->device_view.ld;
    std::int64_t ldb = (std::int64_t) B->device_view.ld;
    std::int64_t ldc = (std::int64_t) C->device_view.ld;
    oneapi::mkl::blas::compute_mode mode = oneapi::mkl::blas::compute_mode::unset;
    const std::vector<sycl::event> dependencies = {};

    stream->sycl.events.buffer[idx] = oneapi::mkl::blas::column_major::gemm(
        queue,
        transa, transb,
        m, n, k,
        alpha,
        a, lda,
        b, ldb,
        beta,
        c, ldc,
        mode,
        dependencies
    );
}

# endif /* XKRT_SUPPORT_SYCL */

# if XKRT_SUPPORT_CL && XKBLAS_SUPPORT_CLBLAST

#  include <xkrt/driver/driver-cl.h>
#  include <xkblas/clblast-helper.h>

template <xkblas_precision_t P, auto FUNC, typename CL_TYPE>
static inline void
body_cl_run(
    stream_cl_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    assert(stream);

    device_cl_t * device = stream->device;
    assert(device);

    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task);

    // using offsets
    cl_mem a_buffer, b_buffer, c_buffer;
    size_t a_offset, b_offset, c_offset;
    driver_cl_get_buffer_and_offset_1D(device, A->device_view.addr, &a_buffer, &a_offset);
    driver_cl_get_buffer_and_offset_1D(device, B->device_view.addr, &b_buffer, &b_offset);
    driver_cl_get_buffer_and_offset_1D(device, C->device_view.addr, &c_buffer, &c_offset);
    a_offset /= sizeof(TYPE);
    b_offset /= sizeof(TYPE);
    c_offset /= sizeof(TYPE);

    const CLBlastLayout layout = CLBlastLayoutColMajor;

    CLBLAST_SAFE_CALL(
        FUNC(
            layout,
            cblas2clblast_op(args->transA), cblas2clblast_op(args->transB),
            args->m, args->n, args->k,
            *((const CL_TYPE *) &args->alpha),
            a_buffer, a_offset, A->device_view.ld,
            b_buffer, b_offset, B->device_view.ld,
            *((const CL_TYPE *) &args->beta),
            c_buffer, c_offset, C->device_view.ld,
           &stream->cl.queue,
            stream->cl.events + idx
        )
    );
}

TYPED
static void
body_cl(
    stream_cl_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    XKBLAS_CLBLAST_DISPATCH_PRECISION(gemm);
}

# endif /* XKRT_SUPPORT_CL */

# if XKRT_SUPPORT_HOST
TYPED
static void
body_cpu(void * args)
{
    LOGGER_FATAL("Executing a gemm on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_GEMM(
    task_format_t * format
) {
    # if XKRT_SUPPORT_HOST
    format->f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_cpu<P>;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKRT_SUPPORT_CUDA */

    # if XKRT_SUPPORT_HIP
    format->f[TASK_FORMAT_TARGET_HIP] = (task_format_func_t) body_hip<P>;
    # endif /* XKRT_SUPPORT_HIP */

    # if XKRT_SUPPORT_ZE
    format->f[TASK_FORMAT_TARGET_ZE] = (task_format_func_t) body_ze<P>;
    # endif /* XKRT_SUPPORT_ZE */

    # if XKRT_SUPPORT_CL && XKBLAS_SUPPORT_CLBLAST
    format->f[TASK_FORMAT_TARGET_CL] = (task_format_func_t) body_cl<P>;
    # endif /* XKRT_SUPPORT_CL */

    # if XKRT_SUPPORT_SYCL
    format->f[TASK_FORMAT_TARGET_SYCL] = (task_format_func_t) body_sycl<P>;
    # endif /* XKRT_SUPPORT_SYCL */
}

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_GEMM<P>(task_format_t * format); \
    template int xkblas_t::gemm_async<P>(int transA, int transB, int m, int n, int k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_t<P> * B, int ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, int ldc);    \
    template int xkblas_t::gemm_tile_async<P>(int transA, int transB, const size_t m, const size_t n, const size_t k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, const xkblas_precision_type_t<P> * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc, distribution_t * d);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE

# if 0
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
# endif
