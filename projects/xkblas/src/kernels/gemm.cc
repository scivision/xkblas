/* ************************************************************************** */
/*                                                                            */
/*   gemm.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 19:56:40 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt-support.h>

# if XKRT_SUPPORT_ZE
#  include <xkrt/driver/driver-ze.h>
#  include <xkrt/logger/logger-ze.h>
#  include <sycl/sycl.hpp>
#  include <oneapi/mkl.hpp>
# endif /* XKRT_SUPPORT_ZE */

# include "auto-tile.h"
# include "context.h"

# include <xkrt/driver/thread-producer.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/min-max.h>
# include <xkrt/memory/access.hpp>
# include <xkrt/memory/alignedas.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/xkrt-support.h>

# include <cassert>

# include "xkblas/kernel-type.h"
# include "xkblas/cblas.h"

typedef struct alignas(CACHE_LINE_SIZE) args_t
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

} args_t;

static task_format_id_t format_id;

/* m, n, k are matrix sizes
 * A_offset_m, A_offset_n, ..., C_offset_n are index of the tile begining */
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
) {
    LOGGER_INFO("Submitting tile C=(%zd,%zd) of size (%zd,%zd)", C_offset_m, C_offset_n, m, n);

    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    assert(is_alignedas(args_size, CACHE_LINE_SIZE));

    ThreadProducer * thread = ThreadProducer::self();
    uint8_t * mem = thread->allocate(task_size + args_size);
    assert(mem);

    // const size_t ocr_access = UNSPECIFIED_TASK_ACCESS;
    const size_t ocr_access = 2;
    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id, ocr_access, UNSPECIFIED_DEVICE_GLOBAL_ID);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "gemm(A=(%zd,%zd) ; B=(%zd,%zd) ; C=(%zd,%zd))", A_offset_m, A_offset_n, B_offset_m, B_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(transA, transB, m, n, k, *alpha, *beta);

    const size_t Am = (transA == CblasNoTrans) ? m : k;
    const size_t An = (transA == CblasNoTrans) ? k : m;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
    const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = m;
    const size_t Cn = n;

    # define NACCESSES 3
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 2) Access(MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, Cm, Cn, sizeof(TYPE), Cmode        );
    thread->resolve<NACCESSES>(task);
    # undef NACCESSES

    context->runtime.commit(task);

    return 0;
}

extern "C"
int
xkblas_£gemm_async(
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
    const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = m;
    const size_t Cn = n;

    if (lda < MAX(1, Am))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    if (ldb < MAX(1, Bm))
    {
        LOGGER_FATAL("illegal value of ldb");
        return -10;
    }

    if (ldc < MAX(1, Cm))
    {
        LOGGER_FATAL("illegal value of ldc");
        return -13;
    }

    xkblas_context_t * context = xkblas_context_get();

    size_t ts = context->conf.kernels[XKBLAS_KERNEL_TYPE_GEMM].tile;
    if (ts == 0)
    {
        int args[2] = {m, n};
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_GEMM, args, &ts);
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
    const size_t Bmt = NUM_OF_TILES(Bm, Bmb);
    const size_t Bnt = NUM_OF_TILES(Bn, Bnb);
    const size_t Cmt = NUM_OF_TILES(Cm, Cmb);
    const size_t Cnt = NUM_OF_TILES(Cn, Cnb);

    const TYPE one = (TYPE) 1.0;

    # define A(I, J) A, (I)*Amb, (J)*Anb, lda
    # define B(I, J) B, (I)*Bmb, (J)*Bnb, ldb
    # define C(I, J) C, (I)*Cmb, (J)*Cnb, ldc

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
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A(tm, tk),
                                B(tk, tn),
                                &zbeta,
                                C(tm, tn)
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
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                alpha,
                                A(tm, tk),
                                B(tn, tk),
                                &zbeta,
                                C(tm, tn)
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
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A(tk, tm),
                                B(tk, tn),
                                &zbeta,
                                C(tm, tn)
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
                        xkblas_£gemm_tile_async(
                                context,
                                transA, transB,
                                bs_mm, bs_nn, bs_km,
                                alpha,
                                A(tk, tm),
                                B(tn, tk),
                                &zbeta,
                                C(tm, tn)
                        );
                    }
                }
            }
        }
    }

    # undef A
    # undef B
    # undef C

    LOGGER_INFO("GEMM dependency graph submitted");

    return 0;
}

# if XKRT_SUPPORT_CUDA
#  include <xkblas/cblas-to-cublas.h>
#  include <xkrt/driver/driver-cuda.h>
#  include <xkrt/driver/cublas-helper.h>

static void
body_cuda(void * ihandle, void * vargs)
{
    xkrt_stream_cuda_t * stream = (xkrt_stream_cuda_t *) ihandle;
    assert(stream);

    cublasHandle_t handle = stream->cu.blas.handle;
    assert(handle);

    Task * task = (Task *) vargs;
    assert(task);

    const Access * A = task->accesses + 0;
    const Access * B = task->accesses + 1;
    const Access * C = task->accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    args_t * args = (args_t *) (task + 1);
    assert(args);

    # ifndef NDEBUG
    LOGGER_INFO("Calling cublasGemm(m=%zu, n=%zu, k=%zu, A=%p, lda=%zu, B=%p, ldb=%zu, C=%p, ldc=%zu) on task=`%s`",
        args->m, args->n, args->k,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) B->device_view.addr,
        B->device_view.ld,
        (void *) C->device_view.addr,
        C->device_view.ld,
        task->label
    );
    #endif /* NDEBUG */

    cublasStatus_t res = cublas££gemm(
        handle,
        cblas2cublas_op(args->transA), cblas2cublas_op(args->transB),
        (int) args->m, (int) args->n, (int) args->k,
        (const CU_TYPE *) &args->alpha,
        (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
        (const CU_TYPE *) B->device_view.addr, (int) B->device_view.ld,
        (const CU_TYPE *) &args->beta,
        (      CU_TYPE *) C->device_view.addr, (int) C->device_view.ld
    );
    xkrt_cublas_status_check(res);
    assert(res == CUBLAS_STATUS_SUCCESS);
}
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_ZE

static void
body_ze(void * ihandle, void * vargs)
{
    LOGGER_FATAL("In kernel impl");

    // unpack arguments
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) ihandle;
    assert(stream);

    Task * task = (Task *) vargs;
    assert(task);

    const Access * A = task->accesses + 0;
    const Access * B = task->accesses + 1;
    const Access * C = task->accesses + 2;

    args_t * args = (args_t *) (task + 1);
    assert(args);

    // TODO : having to use sycl here is super ugly, but Intel do not seems to
    // provide the kernels direcly, so we could pass them via a
    // zeCommandListAdKernelLaunch - or even to simply call a gemm with a command list/queue

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

    # if 0
    oneapi::mkl::blas::compute_mode mode = oneapi::mkl::blas::compute_mode::standard;
    oneapi::mkl::blas::column_major::gemm(
        sycl_queue,
        args->transA, args->transB,
        args->m, args->n, args->k,
       &args->alpha,
        A->device_view.addr, A->device_view.ld,
        B->device_view.addr, B->device_view.ld,
       &args->beta,
        C->device_view.addr, C->device_view.ld
    );
    # endif

    // TODO ; need an event
}

# endif

# if XKRT_SUPPORT_HOST
static void
body_cpu(void * args)
{
    LOGGER_FATAL("Executing a gemm on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£gemm_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

    # if XKRT_SUPPORT_HOST
    format.f[XKRT_DRIVER_TYPE_HOST] = body_cpu;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format.f[XKRT_DRIVER_TYPE_CUDA] = body_cuda;
    # endif /* XKRT_SUPPORT_CUDA */

    # if XKRT_SUPPORT_ZE
    format.f[XKRT_DRIVER_TYPE_ZE] = body_ze;
    # endif /* XKRT_SUPPORT_ZE */

    snprintf(format.label, sizeof(format.label), "£gemm");
    format_id = xkblas_task_format_create(&format);
}
