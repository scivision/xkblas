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
        const TYPE * alpha
    ) :
        n(n),
        incx(incx),
        alpha(alpha)
    {}

    ~args_t() {}

    const size_t n;
    const int incx;
    const TYPE * alpha;
};

TYPED
int
xkblas_t::scal_tile_async(
    int n,
    const TYPE * alpha,
    TYPE * x,
    int incx,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, SCAL), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 0;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(n, incx, alpha);

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, x, incx*n, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::scal_async(
    int n,
    const TYPE * alpha,
    TYPE * x,
    int incx
) {
    assert(alpha);

    if (n == 0 || *alpha == (TYPE) 1.0)
        return 0;

    // get tile size
    xkblas_t * xkblas = xkblas_get();
    size_t ts = xkblas->conf.kernels[SCAL].tile;
    if (ts == 0)
    {
        int args[1] = { n };
        xkblas_kernel_auto_tile(SCAL, args, &ts);
    }
    const size_t nt = NUM_OF_TILES(n, ts);

    // get number of gpus
    const int ngpus = xkblas->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, n, ts);

    // spawn tiles
    for (size_t tn = 0 ; tn < nt ; ++tn)
    {
        size_t bs = (tn == nt-1) ? (n - tn*ts) : ts;
        device_global_id_t device_global_id = distribution1D_get(&d, tn);
        this->scal_tile_async<P>(bs, alpha, x + tn*ts*incx, incx, device_global_id);
    }

    return 0;
}


# if XKBLAS_SUPPORT_CUDA
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
    const access_t * x = accesses + 0;
    assert(x->device_view.addr % x->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);
    assert(args->alpha);

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            (int) args->n,
            (const CU_TYPE *) args->alpha,
            (      CU_TYPE *) x->device_view.addr, args->incx
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
    XKBLAS_CUBLAS_DISPATCH_PRECISION_REAL(scal);
}
# endif /* XKBLAS_SUPPORT_CUDA */

# ifdef XKBLAS_SUPPORT_CBLAS

template <xkblas_precision_t P, auto FUNC>
static void
body_cpu_run(task_t * task)
{
    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * x = accesses + 0;

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);
    assert(args->alpha);

    FUNC(
        (int) args->n,
        (const TYPE  ) *(args->alpha),
        (      TYPE *) x->host_view.addr, (int) args->incx
    );
}

TYPED
static void
body_cpu(task_t * task)
{
    if constexpr (P == xkblas_precision_t::S) body_cpu_run<P, cblas_sscal>(task);
    if constexpr (P == xkblas_precision_t::D) body_cpu_run<P, cblas_dscal>(task);
}

# endif /* XKBLAS_SUPPORT_CBLAS */

TYPED
void
xkblas_t::task_format_create_SCAL(
    task_format_t * format
    ) {
    # if XKBLAS_SUPPORT_CBLAS
    format->f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_cpu<P>;
    # endif /* XKBLAS_SUPPORT_CBLAS */

    # if XKBLAS_SUPPORT_CUDA
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKBLAS_SUPPORT_CUDA */
}

# define DEFINE(P)  \
        template void xkblas_t::task_format_create_SCAL<P>(task_format_t * format);                                                                                         \
    template int xkblas_t::scal_async<P>(int n, const xkblas_precision_type_t<P> * alpha, xkblas_precision_type_t<P> * x, int incx);   \
    template int xkblas_t::scal_tile_async<P>(int n, const xkblas_precision_type_t<P> * alpha, xkblas_precision_type_t<P> * x, int incx, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
