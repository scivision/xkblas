/* ************************************************************************** */
/*                                                                            */
/*   axpy.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/01/30 00:16:18 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/15 18:40:20 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkblas/auto-tile.h>
# include <xkblas/xkblas.hpp>

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    args_t(
        size_t n,
        const TYPE alpha
    ) :
        n(n),
        alpha(alpha)
    {}

    ~args_t() {}

    const size_t n;
    const TYPE alpha;
};

TYPED
int
xkblas_t::axpy_tile_async(
    int n,
    const TYPE alpha,
    const TYPE * A,
          TYPE * B,
    const size_t tn,
    const size_t bs,
    distribution_t * d
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    LOGGER_DEBUG("Submitting tile of axpy");

    # define AC 2
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, AXPY), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 1;
    device_global_id_t device_global_id = d ? distribution1D_get(d, tn) : UNSPECIFIED_DEVICE_GLOBAL_ID;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(n, alpha);

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, A, bs, sizeof(TYPE), ACCESS_MODE_R,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, B, bs, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::axpy_async(
    int n,
    const TYPE alpha,
    const TYPE * A,
          TYPE * B
) {
    if (n == 0)
        return 0;

    if (n < 0)
    {
        LOGGER_FATAL("Negative vector size");
        return -1;
    }

    if (A == NULL)
    {
        LOGGER_FATAL("A is NULL");
        return -2;
    }

    if (B == NULL)
    {
        LOGGER_FATAL("B is NULL");
        return -3;
    }

    // get tile size
    xkblas_t * context = xkblas_get();
    size_t ts = context->conf.kernels[AXPY].tile;
    if (ts == 0)
    {
        int args[1] = { n };
        xkblas_kernel_auto_tile(AXPY, args, &ts);
    }
    const size_t nt = NUM_OF_TILES(n, ts);

    // get number of gpus
    const int ngpus = context->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, n, ts);

    // spawn tiles
    for (size_t tn = 0 ; tn < nt ; ++tn)
    {
        size_t bs = (tn == nt-1) ? (n - tn*ts) : ts;
        this->axpy_tile_async<P>(n, alpha, A, B, tn, bs, &d);
    }

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
    const access_t * X = accesses + 0;
    const access_t * Y = accesses + 1;

    assert(X->device_view.addr % X->host_view.sizeof_type == 0);
    assert(Y->device_view.addr % Y->host_view.sizeof_type == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);
    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            (int) args->n,
            (const CU_TYPE *) &args->alpha,
            (const CU_TYPE *) X->device_view.addr, 1,
            (      CU_TYPE *) Y->device_view.addr, 1
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
    XKBLAS_CUBLAS_DISPATCH_PRECISION(axpy);
}
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_HOST
TYPED
static void
body_cpu(void * args)
{
    LOGGER_FATAL("Executing an axpy on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_AXPY(
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
    template void xkblas_t::task_format_create_AXPY<P>(task_format_t * format); \
    template int xkblas_t::axpy_async<P>(int n, const xkblas_precision_type_t<P> alpha, const xkblas_precision_type_t<P> * A, xkblas_precision_type_t<P> * B); \
    template int xkblas_t::axpy_tile_async<P>(int n, const xkblas_precision_type_t<P> alpha, const xkblas_precision_type_t<P> * A, xkblas_precision_type_t<P> * B, const size_t tn, const size_t bs, distribution_t * d);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
