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

# include <xkblas/support.h>

# if XKBLAS_SUPPORT_SYCL || XKBLAS_SUPPORT_ZE
#  define XKBLAS_NO_DEFAULT_BLAS_ENUM
#  include <sycl/sycl.hpp>
#  include <oneapi/mkl.hpp>
#  include <sycl/ext/oneapi/backend/level_zero.hpp>
#  include <xkblas/oneapi-mkl-helper.h>
# endif

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

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    args_t(
        int transA, int transB,
        int m, int n, int k,
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
    const int m;
    const int n;
    const int k;
    const TYPE alpha;
    const TYPE beta;
};

/* m, n, k are matrix sizes
 * A_offset_m, A_offset_n, ..., C_offset_n are index of the tile begining */
TYPED
int
xkblas_t::gemm_tile_async(
    int transA, int transB,
    const int m, const int n, const int k,
    const TYPE * alpha,
    const TYPE * A, const int Atm, const int Atn, const int Amb, const int Anb, const int lda,
    const TYPE * B, const int Btm, const int Btn, const int Bmb, const int Bnb, const int ldb,
    const TYPE * beta,
          TYPE * C, const int Ctm, const int Ctn, const int Cmb, const int Cnb, const int ldc,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const int A_offset_m = Atm * Amb;
    const int A_offset_n = Atn * Anb;
    const int B_offset_m = Btm * Bmb;
    const int B_offset_n = Btn * Bnb;
    const int C_offset_m = Ctm * Cmb;
    const int C_offset_n = Ctn * Cnb;

    LOGGER_DEBUG("Submitting tile C=(%d,%d) of size (%d,%d)", C_offset_m, C_offset_n, m, n);

    # define AC 3
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    const task_format_id_t fmtid = XKBLAS_XKRT_TASK_FORMAT_GET(P, GEMM);
    task_t * task = this->task_new(fmtid, flags, task_size + args_size);

    task_det_info_t * det = TASK_DET_INFO(task);
    new (det) task_det_info_t();

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr int ocr_access = 2;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(transA, transB, m, n, k, *alpha, *beta);

    # if XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label),
            "gemm(A=(%d,%d) ; B=(%d,%d) ; C=(%d,%d))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n, C_offset_m, C_offset_n);
    # endif /* XKRT_SUPPORT_DEBUG */

    const int Am = (transA == CblasNoTrans) ? m : k;
    const int An = (transA == CblasNoTrans) ? k : m;
    const int Bm = (transB == CblasNoTrans) ? k : n;
    const int Bn = (transB == CblasNoTrans) ? n : k;
    const int Cm = m;
    const int Cn = n;

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

    const int Am = (transA == CblasNoTrans) ? m : k;
    const int An = (transA == CblasNoTrans) ? k : m;
    const int Bm = (transB == CblasNoTrans) ? k : n;
    // const int Bn = (transB == CblasNoTrans) ? n : k;
    const int Cm = m;
    const int Cn = n;

    if ((int)lda < MAX(1, Am))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    if ((int)ldb < MAX(1, Bm))
    {
        LOGGER_FATAL("illegal value of ldb");
        return -10;
    }

    if ((int)ldc < MAX(1, Cm))
    {
        LOGGER_FATAL("illegal value of ldc");
        return -13;
    }

    xkblas_t * context = xkblas_get();

    int ts = context->conf.kernels[GEMM].tile;
    if (ts == 0)
    {
        int args[3] = {m, n, k};
        xkblas_routine_auto_tile(GEMM, args, &ts);
    }

    /* set tiling parameters */
    const int Amb = ts;
    const int Anb = ts;
    const int Bmb = ts;
    const int Bnb = ts;
    const int Cmb = ts;
    const int Cnb = ts;

    const int Amt = NUM_OF_TILES(Am, Amb);
    const int Ant = NUM_OF_TILES(An, Anb);
    // const int Bmt = NUM_OF_TILES(Bm, Bmb);
    // const int Bnt = NUM_OF_TILES(Bn, Bnb);
    const int Cmt = NUM_OF_TILES(Cm, Cmb);
    const int Cnt = NUM_OF_TILES(Cn, Cnb);

    /* distribute C in a cyclic-block manner */
    const int ngpus = context->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Cm, Cn, Cmb, Cnb);

    const TYPE one = (TYPE) 1.0;

    # define A(I, J) A, (I), (J), Amb, Anb, lda
    # define B(I, J) B, (I), (J), Bmb, Bnb, ldb
    # define C(I, J) C, (I), (J), Cmb, Cnb, ldc

    // iterator on tiles
    for (int tm = 0; tm < Cmt; ++tm)
    {
        int bs_mm = (tm == Cmt-1) ? (m-tm*Cmb) : Cmb;
        for (int tn = 0; tn < Cnt; tn++)
        {
            const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);

            int bs_nn = (tn == Cnt-1) ? (n-tn*Cnb) : Cnb;
            // A: CblasNoTrans / B: CblasNoTrans
            if (transA == CblasNoTrans)
            {
                if (transB == CblasNoTrans)
                {
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        int bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A(tm, tk),
                                B(tk, tn),
                                &zbeta,
                                C(tm, tn),
                                device_global_id
                        );
                    }
                }
                // A: CblasNoTrans / B: CBlasTrans
                else
                {
                    for (int tk = 0; tk < Ant; ++tk)
                    {
                        int bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A(tm, tk),
                                B(tn, tk),
                                &zbeta,
                                C(tm, tn),
                                device_global_id
                        );
                    }
                }
            }
            // A: CblasTrans / B: CblasNoTrans
            else
            {
                if (transB == CblasNoTrans)
                {
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        int bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A(tk, tm),
                                B(tk, tn),
                                &zbeta,
                                C(tm, tn),
                                device_global_id
                        );
                    }
                }
                // A: CblasTrans / B: CBlasTrans
                else
                {
                    for (int tk = 0; tk < Amt; ++tk)
                    {
                        int bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        TYPE zbeta = (tk == 0) ? *beta : one;
                        this->gemm_tile_async<P>(
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A(tk, tm),
                                B(tn, tk),
                                &zbeta,
                                C(tm, tn),
                                device_global_id
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

TYPED
int
xkblas_t::gemm_sync(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    int r = this->gemm_async<P>(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
    this->sync();
    return r;
}

TYPED
int
xkblas_t::gemm(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    this->memory_invalidate_caches();
    int r = this->gemm_async<P>(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
    this->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, C, ldc, m, n, sizeof(TYPE));
    this->sync();
    return r;
}

# if XKBLAS_SUPPORT_CUBLAS
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static inline void
cuda_run(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    cublasHandle_t handle = queue->cu.blas.handle;
    assert(handle);

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
cuda(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_CUBLAS_DISPATCH_PRECISION(gemm);
}
# endif /* XKBLAS_SUPPORT_CUBLAS */


# if XKBLAS_SUPPORT_HIP
#  include <xkblas/hipblas-helper.h>
#  include <xkrt/driver/driver-hip.h>

template <xkblas_precision_t P, auto FUNC, typename HIP_TYPE>
static inline void
hip_run(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    hipblasHandle_t handle = queue->hip.blas.handle;
    assert(handle);

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
hip(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_HIPBLAS_DISPATCH_PRECISION(gemm);
}
# endif /* XKBLAS_SUPPORT_HIP */

# if XKBLAS_SUPPORT_SYCL || XKBLAS_SUPPORT_ZE

TYPED
static sycl::event
sycl_queue_launch(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    sycl::queue & queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    // unpack arguments
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task);

    using mkl_type_t = typename mkl_type<TYPE>::type;

    oneapi::mkl::transpose transa = cblas2mkl_op(args->transA);
    oneapi::mkl::transpose transb = cblas2mkl_op(args->transB);
    std::int64_t m = args->m;
    std::int64_t n = args->n;
    std::int64_t k = args->k;
    const mkl_type_t alpha = *reinterpret_cast<const mkl_type_t *>(&args->alpha);
    const mkl_type_t beta  = *reinterpret_cast<const mkl_type_t *>(&args->beta);
    const mkl_type_t * a   =  reinterpret_cast<const mkl_type_t *>(A->device_view.addr);
    const mkl_type_t * b   =  reinterpret_cast<const mkl_type_t *>(B->device_view.addr);
          mkl_type_t * c   =  reinterpret_cast<      mkl_type_t *>(C->device_view.addr);
    std::int64_t lda = (std::int64_t) A->device_view.ld;
    std::int64_t ldb = (std::int64_t) B->device_view.ld;
    std::int64_t ldc = (std::int64_t) C->device_view.ld;
    oneapi::mkl::blas::compute_mode mode = oneapi::mkl::blas::compute_mode::unset;
    const std::vector<sycl::event> dependencies = {};

    return oneapi::mkl::blas::column_major::gemm(
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

# endif /* XKBLAS_SUPPORT_SYCL || XKBLAS_SUPPORT_ZE */

# if XKBLAS_SUPPORT_SYCL
#  include <xkrt/driver/driver-sycl.h>

TYPED
static void
sycl_launch(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_sycl_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    queue->sycl.events.buffer[idx] = sycl_queue_launch<P>(runtime, device, task, queue->sycl.queue, cmd, idx);
}

# endif /* XKBLAS_SUPPORT_SYCL */

# if XKBLAS_SUPPORT_ZE

#  include <xkrt/driver/driver-ze.h>
#  include <xkrt/logger/logger-ze.h>

TYPED
static void
ze(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_ze_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    sycl::event event = sycl_queue_launch<P>(runtime, device, task, queue->sycl.queue, cmd, idx);
    queue->ze.events.list[idx] = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(event);
}

# endif

# if XKBLAS_SUPPORT_CLBLAST

#  include <xkrt/driver/driver-cl.h>
#  include <xkblas/clblast-helper.h>

template <xkblas_precision_t P, auto FUNC, typename CL_TYPE>
static inline void
cl_run(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_cl_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    assert(queue);

    device_cl_t * device = queue->device;
    assert(device);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task);

    // using offsets
    cl_mem a_buffer, b_buffer, c_buffer;
    int a_offset, b_offset, c_offset;
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
           &queue->cl.queue,
            queue->cl.events + idx
        )
    );
}

TYPED
static void
cl(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_cl_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_CLBLAST_DISPATCH_PRECISION(gemm);
}

# endif /* XKBLAS_SUPPORT_CLBLAST */

# if XKBLAS_SUPPORT_CBLAS

template <xkblas_precision_t P, auto FUNC>
static void
host_run(task_t * task)
{
    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    if constexpr (P == xkblas_precision_t::S || P == xkblas_precision_t::D)
    {
        FUNC(
            CblasColMajor,
            (const enum CBLAS_TRANSPOSE) args->transA,
            (const enum CBLAS_TRANSPOSE) args->transB,
            (int) args->m, (int) args->n, (int) args->k,
            (const TYPE  ) args->alpha,
            (const TYPE *) A->host_view.addr, (int) A->host_view.ld,
            (const TYPE *) B->host_view.addr, (int) B->host_view.ld,
            (const TYPE  ) args->beta,
                  (TYPE *) C->host_view.addr, (int) C->host_view.ld
        );
    }
    else
    {
        FUNC(
            CblasColMajor,
            (const enum CBLAS_TRANSPOSE) args->transA,
            (const enum CBLAS_TRANSPOSE) args->transB,
            (int) args->m, (int) args->n, (int) args->k,
            (const TYPE *) &(args->alpha),
            (const TYPE *) A->host_view.addr, (int) A->host_view.ld,
            (const TYPE *) B->host_view.addr, (int) B->host_view.ld,
            (const TYPE *) &(args->beta),
                  (TYPE *) C->host_view.addr, (int) C->host_view.ld
        );
    }
}

TYPED
static void
host(task_t * task)
{
    if constexpr (P == xkblas_precision_t::S) host_run<P, cblas_sgemm>(task);
    if constexpr (P == xkblas_precision_t::D) host_run<P, cblas_dgemm>(task);
    if constexpr (P == xkblas_precision_t::C) host_run<P, cblas_cgemm>(task);
    if constexpr (P == xkblas_precision_t::Z) host_run<P, cblas_zgemm>(task);
}

# endif /* XKBLAS_SUPPORT_CBLAS */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME GEMM

# define CL   1
# define CUDA 1
# define HIP  1
# define HOST 1
# define SYCL 1
# define ZE   1

# include "task-format.cc"

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template int xkblas_t::gemm<P>(int transA, int transB, int m, int n, int k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_t<P> * B, int ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, int ldc);    \
    template int xkblas_t::gemm_sync<P>(int transA, int transB, int m, int n, int k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_t<P> * B, int ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, int ldc);    \
    template int xkblas_t::gemm_async<P>(int transA, int transB, int m, int n, int k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_t<P> * B, int ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, int ldc);    \
    template int xkblas_t::gemm_tile_async<P>(int transA, int transB, const int m, const int n, const int k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, const int Atm, const int Atn, const int Amb, const int Anb, const int lda, const xkblas_precision_type_t<P> * B, const int Btm, const int Btn, const int Bmb, const int Bnb, const int ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, const int Ctm, const int Ctn, const int Cmb, const int Cnb, const int ldc, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
