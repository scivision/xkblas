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
# include <xkblas/cblas.h>

# include <xkrt/support.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/utils/min-max.h>
# include <xkrt/memory/access/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>

# include <cassert>

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    public:
        const int n;
        const TYPE value;

        args_t(int n, const TYPE value) : n(n), value(value) {}
        ~args_t() {}

};

/* m, n, k are matrix sizes
 * Am, An, ..., Cn are index of the tile begining */
TYPED
int
xkblas_t::fill_tile_async(
    int n,
    TYPE * x,
    const TYPE value,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, FILL), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 0;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(n, value);

    # if XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label), "fill(x)");
    # endif /* XKRT_SUPPORT_DEBUG */

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, x, n, sizeof(TYPE), ACCESS_MODE_W, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::fill_async(
    int n,
    TYPE * x,
    const TYPE value
) {
    xkblas_t * xkblas = xkblas_get();
    assert(xkblas);

    size_t ts = xkblas->conf.kernels[FILL].tile;
    if (ts == 0)
    {
        int args[1] = {n};
        xkblas_routine_auto_tile(FILL, args, &ts);
    }
    const size_t nt = NUM_OF_TILES(n, ts);

    const int ngpus = xkblas->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, n, ts);

    // spawn tiles
    for (size_t tn = 0 ; tn < nt ; ++tn)
    {
        const size_t bs = (tn == nt-1) ? (n - tn*ts) : ts;
        const device_global_id_t device_global_id = distribution1D_get(&d, tn);
        this->fill_tile_async<P>(bs, x+tn*ts, value, device_global_id);
    }

    return 0;
}

TYPED
int
xkblas_t::fill_lazy(
    int n,
    TYPE * x,
    const TYPE value
) {
    int r = this->fill_async<P>(n, x, value);
    this->sync();
    return r;
}

TYPED
int
xkblas_t::fill(
    int n,
    TYPE * x,
    const TYPE value
) {
    this->memory_invalidate_caches();
    int r = this->fill_async<P>(n, x, value);
    this->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, x, n*sizeof(TYPE));
    this->sync();
    return r;
}

# if XKBLAS_SUPPORT_CUDA

#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

extern "C" {
    int cuda_sfill(cudaStream_t cuda_queue, int n, float * x,           const float           value);
    int cuda_dfill(cudaStream_t cuda_queue, int n, double * x,          const double          value);
    int cuda_cfill(cudaStream_t cuda_queue, int n, cuComplex * x,       const cuComplex       value);
    int cuda_zfill(cudaStream_t cuda_queue, int n, cuDoubleComplex * x, const cuDoubleComplex value);
};

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static inline void
cuda_run(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    assert(queue);

    cudaStream_t cuda_queue = queue->cu.handle.high;
    assert(cuda_queue);

    task_t * task = (task_t *) cmd->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x = accesses + 0;
    assert(x->device_view.addr % x->host_view.sizeof_type == 0);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    FUNC(
        cuda_queue,
        (int) args->n,
        (CU_TYPE *) x->device_view.addr,
        *reinterpret_cast<const CU_TYPE*>(&args->value)
    );

    XKBLAS_CUBLAS_CALL_POST();
}

TYPED
static void
cuda(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    if constexpr (P == xkblas_precision_t::S) cuda_run<P, cuda_sfill, float>(queue, cmd, idx);
    if constexpr (P == xkblas_precision_t::D) cuda_run<P, cuda_dfill, double>(queue, cmd, idx);
    if constexpr (P == xkblas_precision_t::C) cuda_run<P, cuda_cfill, cuComplex>(queue, cmd, idx);
    if constexpr (P == xkblas_precision_t::Z) cuda_run<P, cuda_zfill, cuDoubleComplex>(queue, cmd, idx);
}

# endif /* XKBLAS_SUPPORT_CUDA */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME FILL

# define CL   0
# define CUDA XKBLAS_SUPPORT_CUDA
# define HIP  0
# define HOST 0
# define SYCL 0
# define ZE   0

# include "task-format.cc"

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template int xkblas_t::fill<P>(int n, xkblas_precision_type_t<xkblas_precision_t::P> * x, const xkblas_precision_type_t<xkblas_precision_t::P> value);  \
    template int xkblas_t::fill_lazy<P>(int n, xkblas_precision_type_t<xkblas_precision_t::P> * x, const xkblas_precision_type_t<xkblas_precision_t::P> value);  \
    template int xkblas_t::fill_async<P>(int n, xkblas_precision_type_t<xkblas_precision_t::P> * x, const xkblas_precision_type_t<xkblas_precision_t::P> value);  \
    template int xkblas_t::fill_tile_async<P>(int n, xkblas_precision_type_t<xkblas_precision_t::P> * x, const xkblas_precision_type_t<xkblas_precision_t::P> value, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
