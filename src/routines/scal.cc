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

# include <xkblas/xkblas.hpp>
# include <xkblas/auto-tile.h>

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    args_t(
        xkblas_t * xkblas,
        int n,
        int incx,
        const TYPE alpha
    ) :
        xkblas(xkblas),
        n(n),
        incx(incx),
        alpha(alpha)
    {}

    ~args_t() {}

    xkblas_t * xkblas;
    const int n;
    const int incx;
    const TYPE alpha;
};

TYPED
int
xkblas_t::scal_tile_async(
    int n,
    const TYPE * alpha,
    TYPE * x,
    const int incx,
    device_unique_id_t device_unique_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    # define AC 1
    const task_format_id_t fmtid = XKBLAS_XKRT_TASK_FORMAT_GET(P, SCAL);
    constexpr size_t args_size = sizeof(args_t<P>);
    constexpr task_access_counter_t ocr_access_idx = 0;
    task_t * task = this->task_new(fmtid, args_size, AC, ocr_access_idx, device_unique_id);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    new (args) args_t<P>(this, n, incx, *alpha);

    static_assert(AC <= XKRT_TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task);
    new (accesses + 0) access_t(task, x, incx*n, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    this->runtime.task_accesses_resolve(accesses, AC);
    # undef AC

    # if XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label), "scale(n=%d)", n);
    # endif /* XKRT_SUPPORT_DEBUG */

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::scal_async(
    int n,
    const TYPE * alpha,
    TYPE * x,
    const int incx
) {
    assert(alpha);

    // TODO: if *alpha == (TYPE) 0.0, can we accelerate with a custom kernel ?

    if (n == 0 || *alpha == (TYPE) 1.0)
        return 0;

    // get tile size
    xkblas_t * xkblas = xkblas_get();
    int ts = xkblas->conf.kernels[SCAL].tile;
    if (ts == 0)
    {
        int args[1] = { n };
        xkblas_routine_auto_tile(SCAL, args, &ts);
    }
    const int nt = NUM_OF_TILES(n, ts);

    // get number of gpus
    const int ngpus = xkblas->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, n, ts);

    // spawn tiles
    for (int tn = 0 ; tn < nt ; ++tn)
    {
        int bs = (tn == nt-1) ? (n - tn*ts) : ts;
        device_unique_id_t device_unique_id = distribution1D_get(&d, tn);
        this->scal_tile_async<P>(bs, alpha, x + tn*ts*incx, incx, device_unique_id);
    }

    return 0;
}

TYPED
int
xkblas_t::scal_sync(
    int n,
    const TYPE * alpha,
    TYPE * x,
    const int incx
) {
    int r = this->scal_async<P>(n, alpha, x, incx);
    this->sync();
    return r;
}

TYPED
int
xkblas_t::scal(
    int n,
    const TYPE * alpha,
    TYPE * x,
    const int incx
) {
    this->memory_invalidate_caches();
    int r = this->scal_async<P>(n, alpha, x, incx);
    this->memory_coherent_async(XKRT_HOST_DEVICE_UNIQUE_ID, x, n*sizeof(TYPE)*incx);
    this->sync();
    return r;
}

# if XKBLAS_SUPPORT_HIPBLAS
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
    command_queue_list_counter_t idx
) {
    hipblasHandle_t handle = queue->hip.blas.handle;
    assert(handle);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x = accesses + 0;
    assert(x->device_view.addr % x->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_HIPBLAS_CALL(
        FUNC(
            handle,
            (int) args->n,
            (const HIP_TYPE *) args->alpha,
            (      HIP_TYPE *) x->device_view.addr, args->incx
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
    command_queue_list_counter_t idx
) {
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_REAL(scal);
}
# endif /* XKBLAS_SUPPORT_HIPBLAS */

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
    command_queue_list_counter_t idx
) {
    cublasHandle_t handle = queue->cu.blas.handle;
    assert(handle);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x = accesses + 0;
    assert(x->device_view.addr % x->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    const device_unique_id_t device_unique_id = dev->elected_device_unique_id;
    const TYPE * alpha = xkblas_cublas_pointer_mode<P>(args->xkblas, device_unique_id, handle, &args->alpha);

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            (int) args->n,
            (const CU_TYPE *) alpha,
            (      CU_TYPE *) x->device_view.addr, args->incx
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
    command_queue_list_counter_t idx
) {
    XKBLAS_CUBLAS_DISPATCH_PRECISION_REAL(scal);
}
# endif /* XKBLAS_SUPPORT_CUBLAS */

# if XKBLAS_SUPPORT_CBLAS

template <xkblas_precision_t P, auto FUNC>
static void
host_run(task_t * task)
{
    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x = accesses + 0;

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    FUNC(
        (int) args->n,
        (const TYPE  ) *(args->alpha),
        (      TYPE *) x->host_view.addr, (int) args->incx
    );
}

TYPED
static void
host(task_t * task)
{
    if constexpr (P == xkblas_precision_t::S) host_run<P, cblas_sscal>(task);
    if constexpr (P == xkblas_precision_t::D) host_run<P, cblas_dscal>(task);
}

# endif /* XKBLAS_SUPPORT_CBLAS */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME SCAL

# define CL   0
# define CUDA 1
# define HIP  1
# define HOST 0
# define SYCL 0
# define ZE   0

# include "task-format.cc"

# define DEFINE(P)  \
    template int xkblas_t::scal<P>(int n, const xkblas_precision_type_t<P> * alpha, xkblas_precision_type_t<P> * x, const int incx);   \
    template int xkblas_t::scal_sync<P>(int n, const xkblas_precision_type_t<P> * alpha, xkblas_precision_type_t<P> * x, const int incx);   \
    template int xkblas_t::scal_async<P>(int n, const xkblas_precision_type_t<P> * alpha, xkblas_precision_type_t<P> * x, const int incx);   \
    template int xkblas_t::scal_tile_async<P>(int n, const xkblas_precision_type_t<P> * alpha, xkblas_precision_type_t<P> * x, const int incx, device_unique_id_t device_unique_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
