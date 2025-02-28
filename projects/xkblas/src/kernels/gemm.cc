/* ************************************************************************** */
/*                                                                            */
/*   gemm.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/28 01:46:08 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt-support.h>

# if XKRT_SUPPORT_ZE
#  include <xkrt/driver/driver-ze.h>
#  include <xkrt/logger/logger-ze.h>
# endif /* XKRT_SUPPORT_ZE */

# include "auto-tile.h"
# include "context.h"

# include <xkrt/driver/thread-producer.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/min-max.h>
# include <xkrt/memory/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/xkrt-support.h>

# include <cassert>

# include "xkblas/kernel-type.h"

# if XKBLAS_SUPPORT_SYCL
#  include <sycl/sycl.hpp>
#  include <oneapi/mkl.hpp>
#  include <sycl/backend/level_zero.hpp>
# else
#  include "xkblas/cblas.h"
# endif

typedef struct  args_t
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

    Access accesses[3]; // A, B, C
    const int transA;
    const int transB;
    const size_t m;
    const size_t n;
    const size_t k;
    const TYPE alpha;
    const TYPE beta;

}               args_t;

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
    LOGGER_DEBUG("Submitting tile C=(%zd,%zd) of size (%zd,%zd)", C_offset_m, C_offset_n, m, n);

    Thread * thread = Thread::self();
    uint8_t * mem = thread->allocate(sizeof(Task) + sizeof(args_t));
    assert(mem);

    // const size_t ocr_access = UNSPECIFIED_TASK_ACCESS;
    const size_t ocr_access = 2;
    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id, ocr_access, UNSPECIFIED_DEVICE_GLOBAL_ID);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "gemm(A=(%zd,%zd) ; B=(%zd,%zd) ; C=(%zd,%zd))", A_offset_m, A_offset_n, B_offset_m, B_offset_n, C_offset_m, C_offset_n);
    # endif /* NDEBUG */

    args_t  * args = reinterpret_cast<args_t *>(task + 1);
    new(args) args_t(transA, transB, m, n, k, *alpha, *beta);

    // TODO : there is an issue with how trans and accesses are handled
    const size_t Am = m; // (transA == CblasNoTrans) ? m : k;
    const size_t An = k; // (transA == CblasNoTrans) ? k : m;
    const size_t Bm = k; // (transB == CblasNoTrans) ? k : n;
    const size_t Bn = n; // (transB == CblasNoTrans) ? n : k;
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

    context->runtime.task_commit(task);

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

    LOGGER_DEBUG("GEMM dependency graph submitted");

    return 0;
}

# if XKRT_SUPPORT_CUDA
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cuda.h>

static void
body_cuda(
    xkrt_stream_cuda_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    assert(stream);

    cublasHandle_t handle = stream->cu.blas.handle;
    assert(handle);

    Task * task = (Task *) instr->kern.vargs;
    assert(task);

    const Access * A = task->accesses + 0;
    const Access * B = task->accesses + 1;
    const Access * C = task->accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    args_t * args = (args_t *) (task + 1);
    assert(args);

    XKBLAS_CUBLAS_CALL(
        cublas££gemm(
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
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_ZE

static void
body_ze(void * ihandle, void * vargs)
{
    // unpack arguments
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) ihandle;
    assert(stream);

    Task * task = (Task *) vargs;
    assert(task);

    const Access * A = task->accesses + 0;
    const Access * B = task->accesses + 1;
    const Access * C = task->accesses + 2;

    args_t * args = (args_t *) (task + 1);

    # if XKBLAS_SUPPORT_SYCL

    # if 0
    oneapi::mkl::blas::column_major::gemm(
        sycl::_V1::queue &,
        oneapi::mkl::transpose,
        oneapi::mkl::transpose,
        long, long, long,
        oneapi::mkl::value_or_pointer<double>,
        double const*, long, double const*, long,
        oneapi::mkl::value_or_pointer<double>,
        double*,
        long, oneapi::mkl::blas::compute_mode,
        std::vector<sycl::_V1::event, std::allocator<sycl::_V1::event> > const &
    );
    # endif

    # else /* XKBLAS_SUPPORT_SYCL */
    LOGGER_FATAL("no blas impl for ze");
    # endif /* XKBLAS_SUPPORT_SYCL */



    # if 0


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
    # endif
    // TODO ; need an event
}

# endif

# if XKRT_SUPPORT_CL && XKBLAS_SUPPORT_CLBLAST

#  include <xkrt/driver/driver-cl.h>
#  include <xkblas/clblast-helper.h>

static void
body_cl(
    xkrt_stream_cl_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    assert(stream);

    xkrt_device_cl_t * device = stream->device;
    assert(device);

    Task * task = (Task *) instr->kern.vargs;
    assert(task);

    const Access * A = task->accesses + 0;
    const Access * B = task->accesses + 1;
    const Access * C = task->accesses + 2;

    args_t * args = (args_t *) (task + 1);
    assert(args);

    // using offsets
    cl_mem a_buffer, b_buffer, c_buffer;
    size_t a_offset, b_offset, c_offset;
    xkrt_driver_cl_get_buffer_and_offset(device, A->device_view.addr, &a_buffer, &a_offset);
    xkrt_driver_cl_get_buffer_and_offset(device, B->device_view.addr, &b_buffer, &b_offset);
    xkrt_driver_cl_get_buffer_and_offset(device, C->device_view.addr, &c_buffer, &c_offset);
    a_offset /= sizeof(TYPE);
    b_offset /= sizeof(TYPE);
    c_offset /= sizeof(TYPE);

    const CLBlastLayout layout = CLBlastLayoutColMajor;

    CLBLAST_SAFE_CALL(
        CLBlast££gemm(
            layout,
            cblas2clblast_op(args->transA), cblas2clblast_op(args->transB),
            args->m, args->n, args->k,
            args->alpha,
            a_buffer, a_offset, A->device_view.ld,
            b_buffer, b_offset, B->device_view.ld,
            args->beta,
            c_buffer, c_offset, C->device_view.ld,
           &stream->cl.queue,
            stream->cl.events + idx
        )
    );
}

# endif /* XKRT_SUPPORT_CL */

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
    format.f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format.f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda;
    # endif /* XKRT_SUPPORT_CUDA */

    # if XKRT_SUPPORT_ZE
    format.f[XKRT_DRIVER_TYPE_ZE] = (task_format_func_t) body_ze;
    # endif /* XKRT_SUPPORT_ZE */

    # if XKRT_SUPPORT_CL && XKBLAS_SUPPORT_CLBLAST
    format.f[XKRT_DRIVER_TYPE_CL] = (task_format_func_t) body_cl;
    # endif /* XKRT_SUPPORT_CL */

    snprintf(format.label, sizeof(format.label), "£gemm");
    format_id = xkblas_task_format_create(&format);
}
