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
        size_t n,
        int incx,
        int incy,
        TYPE * r
    ) :
        n(n),
        incx(incx),
        incy(incy),
        r(r)
    {}

    ~args_t() {}

    const size_t n;
    const int incx;
    const int incy;
    TYPE * r;
};

TYPED
int
xkblas_t::dot_tile_async(
    int n,
    const TYPE * x, const int incx,
    const TYPE * y, const int incy,
    const TYPE * temp_r,
          TYPE * r,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    # define AC 3
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_XKRT_TASK_FORMAT_GET(P, DOT), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 0;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(n, incx, incy, r);

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, x, incx*n, sizeof(TYPE), ACCESS_MODE_R,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, y, incy*n, sizeof(TYPE), ACCESS_MODE_R,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);

    if (temp_r)
        new (accesses + 2) access_t(task, temp_r, ACCESS_MODE_VW, ACCESS_CONCURRENCY_CONCURRENT, ACCESS_SCOPE_NONUNIFIED);
    else
        new (accesses + 2) access_t(task, r,      ACCESS_MODE_VW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);

    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::dot_async(
    int n,
    const TYPE * x, const int incx,
    const TYPE * y, const int incy,
          TYPE * r
) {
    if (n == 0)
    {
        *r = 0;
        return 0;
    }

    // get tile size
    xkblas_t * xkblas = xkblas_get();
    size_t ts = xkblas->conf.kernels[DOT].tile;
    if (ts == 0)
    {
        int args[1] = { n };
        xkblas_routine_auto_tile(DOT, args, &ts);
    }
    const size_t nt = NUM_OF_TILES(n, ts);

    if (nt == 1)
    {
        this->dot_tile_async<P>(n, x, incx, y, incy, NULL, r, UNSPECIFIED_DEVICE_GLOBAL_ID);
    }
    else
    {
        // temporary results
        TYPE * temp_r = (TYPE *) malloc(sizeof(TYPE) * nt);
        assert(temp_r);

        // get number of gpus
        const int ngpus = xkblas->runtime.get_ndevices() - 1;
        distribution_t d;
        distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, n, ts);

        // spawn tiles
        for (size_t tn = 0 ; tn < nt ; ++tn)
        {
            size_t bs = (tn == nt-1) ? (n - tn*ts) : ts;
            device_global_id_t device_global_id = distribution1D_get(&d, tn);
            this->dot_tile_async<P>(bs, x + tn*ts*incx, incx, y + tn*ts*incy, incy, temp_r, temp_r + tn, device_global_id);
        }

        xkblas->runtime.task_spawn<2>(
            [=] (task_t * task, access_t * accesses) {
                new (accesses + 0) access_t(task, temp_r, ACCESS_MODE_R, ACCESS_CONCURRENCY_SEQUENTIAL);
                new (accesses + 1) access_t(task,      r, ACCESS_MODE_W, ACCESS_CONCURRENCY_SEQUENTIAL);
            },

            [=] (runtime_t * runtime, device_t * device, task_t * task) {
                (void) runtime; (void) device; (void) task;
                *r = 0;
                for (size_t i = 0 ; i < nt ; ++i)
                    *r += temp_r[i];
                free(temp_r);
            }
        );
    }

    return 0;
}

TYPED
int
xkblas_t::dot_lazy(
    int n,
    const TYPE * x, const int incx,
    const TYPE * y, const int incy,
          TYPE * r
) {
    int rc = this->dot_async<P>(n, x, incx, y, incy, r);
    this->sync();
    return rc;
}

TYPED
int
xkblas_t::dot(
    int n,
    const TYPE * x, const int incx,
    const TYPE * y, const int incy,
          TYPE * r
) {
    this->memory_invalidate_caches();
    int rc = this->dot_async<P>(n, x, incx, y, incy, r);
    this->sync();
    return rc;
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
    queue_command_list_counter_t idx
) {
    hipblasHandle_t handle = queue->hip.blas.handle;
    assert(handle);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x   = accesses + 0;
    const access_t * y   = accesses + 1;

    assert(x->device_view.addr % x->host_view.sizeof_type == 0);
    assert(y->device_view.addr % y->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);
    assert(args->r);

    XKBLAS_HIPBLAS_CALL(
        FUNC(
            handle,
            (int) args->n,
            (const HIP_TYPE *) x->device_view.addr, args->incx,
            (const HIP_TYPE *) y->device_view.addr, args->incy,
            (      HIP_TYPE *) args->r
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
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_REAL(dot);
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
    queue_command_list_counter_t idx
) {
    cublasHandle_t handle = queue->cu.blas.handle;
    assert(handle);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x   = accesses + 0;
    const access_t * y   = accesses + 1;

    assert(x->device_view.addr % x->host_view.sizeof_type == 0);
    assert(y->device_view.addr % y->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);
    assert(args->r);

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            (int) args->n,
            (const CU_TYPE *) x->device_view.addr, args->incx,
            (const CU_TYPE *) y->device_view.addr, args->incy,
            (      CU_TYPE *) args->r
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
    XKBLAS_CUBLAS_DISPATCH_PRECISION_REAL(dot);
}
# endif /* XKBLAS_SUPPORT_CUBLAS */

# if XKBLAS_SUPPORT_CBLAS

template <xkblas_precision_t P, auto FUNC>
static void
host_run(task_t * task)
{
    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x = accesses + 0;
    const access_t * y = accesses + 1;

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    *(args->r) = FUNC(
        (int) args->n,
        (const TYPE *) x->host_view.addr, (int) args->incx,
        (const TYPE *) y->host_view.addr, (int) args->incy
    );
}

TYPED
static void
host(task_t * task)
{
    if constexpr (P == xkblas_precision_t::S) host_run<P, cblas_sdot>(task);
    if constexpr (P == xkblas_precision_t::D) host_run<P, cblas_ddot>(task);
}

# endif /* XKBLAS_SUPPORT_CBLAS */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME DOT

# define CL   0
# define CUDA 1
# define HIP  1
# define HOST 1
# define SYCL 0
# define ZE   0

# include "task-format.cc"

# define DEFINE(P)  \
    template int xkblas_t::dot<P>(int n, const xkblas_precision_type_t<P> * x, const int incx, const xkblas_precision_type_t<P> * y, const int incy, xkblas_precision_type_t<P> * r);   \
    template int xkblas_t::dot_lazy<P>(int n, const xkblas_precision_type_t<P> * x, const int incx, const xkblas_precision_type_t<P> * y, const int incy, xkblas_precision_type_t<P> * r);   \
    template int xkblas_t::dot_async<P>(int n, const xkblas_precision_type_t<P> * x, const int incx, const xkblas_precision_type_t<P> * y, const int incy, xkblas_precision_type_t<P> * r);   \
    template int xkblas_t::dot_tile_async<P>(int n, const xkblas_precision_type_t<P> * x, const int incx, const xkblas_precision_type_t<P> * y, const int incy, const xkblas_precision_type_t<P> * temp_r, xkblas_precision_type_t<P> * r, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
