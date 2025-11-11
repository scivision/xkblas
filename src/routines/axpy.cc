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

# include <xkblas/auto-tile.h>
# include <xkblas/xkblas.hpp>

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    args_t(
        size_t n,
        const TYPE * alpha,
        const int incx,
        const int incy
    ) :
        n(n),
        incx(incx),
        incy(incy),
        alpha(alpha)
    {}

    ~args_t() {}

    const size_t n;
    const int incx;
    const int incy;
    const TYPE * alpha;
};

TYPED
int
xkblas_t::axpy_tile_async(
    int n,
    const TYPE * alpha,
    const TYPE * x,
    const int incx,
          TYPE * y,
    const int incy,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    LOGGER_DEBUG("Submitting tile of axpy");

    # define AC 2
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_XKRT_TASK_FORMAT_GET(P, AXPY), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 1;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(n, alpha, incx, incy);

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, x, incx*n, sizeof(TYPE), ACCESS_MODE_R,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, y, incy*n, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::axpy_async(
    int n,
    const TYPE * alpha,
    const TYPE * x,
    const int incx,
          TYPE * y,
    const int incy
) {
    if (n == 0)
        return 0;

    if (n < 0)
    {
        LOGGER_FATAL("Negative vector size");
        return -1;
    }

    if (x == NULL)
    {
        LOGGER_FATAL("x is NULL");
        return -2;
    }

    if (y == NULL)
    {
        LOGGER_FATAL("y is NULL");
        return -3;
    }

    // get tile size
    xkblas_t * context = xkblas_get();
    size_t ts = context->conf.kernels[AXPY].tile;
    if (ts == 0)
    {
        int args[1] = { n };
        xkblas_routine_auto_tile(AXPY, args, &ts);
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
        device_global_id_t device_global_id = distribution1D_get(&d, tn);
        this->axpy_tile_async<P>(bs, alpha, x + tn*ts*incx, incx, y + tn*ts*incy, incy, device_global_id);
    }

    return 0;
}

TYPED
int
xkblas_t::axpy_lazy(
    int n,
    const TYPE * alpha,
    const TYPE * x,
    const int incx,
          TYPE * y,
    const int incy
) {
    int r = this->axpy_async<P>(n, alpha, x, incx, y, incy);
    this->sync();
    return r;
}

TYPED
int
xkblas_t::axpy(
    int n,
    const TYPE * alpha,
    const TYPE * x,
    const int incx,
          TYPE * y,
    const int incy
) {
    this->memory_invalidate_caches();
    int r = this->axpy_async<P>(n, alpha, x, incx, y, incy);
    this->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, y, n*sizeof(TYPE)*incy);
    this->sync();
    return r;
}

# if XKBLAS_SUPPORT_HIPBLAS
#  include <xkblas/hipblas-helper.h>
#  include <xkrt/driver/driver-hip.h>

template <xkblas_precision_t P, auto FUNC, typename HIP_TYPE>
static inline void
hip_run(
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    hipblasHandle_t handle = queue->hip.blas.handle;
    assert(handle);

    task_t * task = (task_t *) cmd->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * X = accesses + 0;
    const access_t * Y = accesses + 1;

    assert(X->device_view.addr % X->host_view.sizeof_type == 0);
    assert(Y->device_view.addr % Y->host_view.sizeof_type == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);
    XKBLAS_HIPBLAS_CALL(
        FUNC(
            handle,
            (int) args->n,
            (const HIP_TYPE *) args->alpha,
            (const HIP_TYPE *) X->device_view.addr, args->incx,
            (      HIP_TYPE *) Y->device_view.addr, args->incy
        )
    );
}

TYPED
static void
hip(
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_HIPBLAS_DISPATCH_PRECISION(axpy);
}

# endif /* XKBLAS_SUPPORT_HIPBLAS */

# if XKBLAS_SUPPORT_CUBLAS
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static inline void
cuda_run(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    cublasHandle_t handle = queue->cu.blas.handle;
    assert(handle);

    task_t * task = (task_t *) cmd->kern.vargs;
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
            (const CU_TYPE *) args->alpha,
            (const CU_TYPE *) X->device_view.addr, args->incx,
            (      CU_TYPE *) Y->device_view.addr, args->incy
        )
    );
}

TYPED
static void
cuda(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_CUBLAS_DISPATCH_PRECISION(axpy);
}
# endif /* XKBLAS_SUPPORT_CUBLAS */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME AXPY

# define CL   0
# define CUDA 1
# define HIP  1
# define HOST 0
# define SYCL 0
# define ZE   0

# include "task-format.cc"

# define DEFINE(P)  \
    template int xkblas_t::axpy<P>(int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * x, const int incx, xkblas_precision_type_t<P> * y, const int incy); \
    template int xkblas_t::axpy_lazy<P>(int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * x, const int incx, xkblas_precision_type_t<P> * y, const int incy); \
    template int xkblas_t::axpy_async<P>(int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * x, const int incx, xkblas_precision_type_t<P> * y, const int incy); \
    template int xkblas_t::axpy_tile_async<P>(int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * x, const int incx, xkblas_precision_type_t<P> * y, const int incy, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
