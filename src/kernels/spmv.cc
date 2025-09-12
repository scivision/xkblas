/* ************************************************************************** */
/*                                                                            */
/*   spmv.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/12 20:18:57 by Romain PEREIRA         / _______ \       */
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
        xkblas_t * xkblas,
        int transA,
        int m,
        int n,
        int nnz,
        TYPE alpha,
        TYPE beta
    ) :
        xkblas(xkblas),
        transA(transA),
        m(m),
        n(n),
        nnz(nnz),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    xkblas_t * xkblas;
    const int transA;
    const int m;
    const int n;
    const int nnz;
    const TYPE alpha;
    const TYPE beta;
};

TYPED
int
xkblas_t::spmv_tile_async(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    const int m,
    const int n,
    const int nnz,
    const int * csr_row_offsets,
    const int * csr_col_indices,
    const TYPE * csr_values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y,
    size_t tm,
    distribution_t * d
) {
    // retrieve producer threads
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    # define AC 5
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, SPMV), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 2;
    device_global_id_t device_global_id = d ? distribution1D_get(d, tm) : UNSPECIFIED_DEVICE_GLOBAL_ID;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(this, transA, m, n, nnz, *alpha, *beta);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "spmv");
    # endif /* NDEBUG */

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t A_X_mode = (*alpha == (const TYPE) 0.0) ? ACCESS_MODE_V : ACCESS_MODE_R;
    access_mode_t Ymode    = (*beta  == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new (accesses + 0) access_t(task, csr_row_offsets,  m+1, sizeof(int),  A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, csr_col_indices,  nnz, sizeof(int),  A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 2) access_t(task, csr_values,       nnz, sizeof(TYPE), A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 3) access_t(task, X,                n,   sizeof(TYPE), A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 4) access_t(task, Y,                m,   sizeof(TYPE), Ymode,     ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve<AC>(task, accesses);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::spmv_async(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    const int m,
    const int n,
    const int nnz,
    const int * csr_row_offsets,
    const int * csr_col_indices,
    const TYPE * csr_values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y
) {
    assert(alpha);
    assert(beta);

    if (*alpha == (TYPE) 0 && *beta == (TYPE) 0)
        return 0;

    // retrieve tile size
    // The tile size is the number of rows `ts` (m) to use per task
    size_t ts = this->conf.kernels[SPMV].tile;
    if (ts == 0)
    {
        int args[3] = {m, n, nnz};
        xkblas_kernel_auto_tile(SPMV, args, &ts);
    }
    const size_t mt = NUM_OF_TILES(m, ts);

    // distribution: cyclic
    const int ngpus = this->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, m, ts);

    // replicate `csr_row_offsets` to avoid polluting user matrix
    int * csr_row_offsets_dup = (int *) malloc(sizeof(int) * (m+1));
    assert(csr_row_offsets_dup);

    // for each tile
    for (size_t tm = 0 ; tm < mt ; ++tm)
    {
       // block size
        size_t m0 = tm*ts;
        size_t m1 = (tm == mt-1) ? m : (tm+1) * ts;
        size_t bs = m1 - m0;

        // compute number of nnz for that tile, to offset values array
        int tile_nnz = csr_row_offsets[m1] - csr_row_offsets[m0];

        // no elements in that tile
        if (tile_nnz == 0)
            continue ;

        // TODO: i dislike this nasty loop over each rows
        // that will probably be a performance issue in iterative solvers where
        // the matrix A does not change,
        // we'll figure something out then
        // maybe there exists a cuda kernel that can automatically offset to offsets[m0] ?

        // adjust tile_row_ptr so first entry is 0
        if (tm != 0)
            for (size_t i = m0 ; i <= m1 ; ++i)
                csr_row_offsets_dup[i] = csr_row_offsets[i] - csr_row_offsets[m0];

        const int * csr_row_offsets_ptr = (tm == 0) ? csr_row_offsets : csr_row_offsets_dup;

        // tile spmv
        this->spmv_tile_async<P>(
            alpha,
            transA,
            bs, n, tile_nnz,
            csr_row_offsets_ptr + m0,
            csr_col_indices + csr_row_offsets[m0],
            csr_values      + csr_row_offsets[m0],
            X,
            beta,
            Y + m0,
            tm,
           &d
        );
    }

    return 0;
}

# if XKRT_SUPPORT_CUDA
#  include <xkblas/cusparse-helper.h>
#  include <xkrt/driver/driver-cu.h>

static void
body_cuda_run_async_completion(void * args[XKRT_CALLBACK_ARGS_MAX])
{
    task_t * task = (task_t *) args[0];
    assert(task);

    area_chunk_t * chunk = (area_chunk_t *) args[1];
    assert(chunk);

    device_global_id_t device_global_id = (device_global_id_t) (uintptr_t) args[2];

    xkblas_t * xkblas = (xkblas_t *) args[3];
    assert(xkblas);

    xkblas->runtime.memory_device_deallocate(device_global_id, chunk);
}

template <xkblas_precision_t P, typename CU_TYPE, cudaDataType CUDA_DATA_TYPE>
static inline void
body_cuda_run(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    cusparseHandle_t handle = stream->cu.sparse.handle;
    assert(handle);

    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * csr_row_offsets = accesses + 0;
    const access_t * csr_col_indices = accesses + 1;
    const access_t * csr_values      = accesses + 2;
    const access_t * X_acc           = accesses + 3;
    const access_t * Y_acc           = accesses + 4;

    assert(csr_row_offsets->device_view.addr % sizeof(int)     == 0);
    assert(csr_col_indices->device_view.addr % sizeof(int)     == 0);
    assert(csr_values->device_view.addr      % sizeof(CU_TYPE) == 0);
    assert(X_acc->device_view.addr           % sizeof(CU_TYPE) == 0);
    assert(Y_acc->device_view.addr           % sizeof(CU_TYPE) == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    // setup matrix desc
    cusparseSpMatDescr_t A;
    CUSPARSE_SAFE_CALL(
        cusparseCreateCsr(
            &A,
            (int64_t) args->m,
            (int64_t) args->n,
            (int64_t) args->nnz,
            (void *) csr_row_offsets->device_view.addr,
            (void *) csr_col_indices->device_view.addr,
            (void *) csr_values->device_view.addr,
            CUSPARSE_INDEX_32I,
            CUSPARSE_INDEX_32I,
            CUSPARSE_INDEX_BASE_ZERO,
            CUDA_DATA_TYPE
        )
    );

    cusparseDnVecDescr_t X;
    CUSPARSE_SAFE_CALL(
        cusparseCreateDnVec(
            &X,
            (int64_t) args->n,
            (void *) X_acc->device_view.addr,
            CUDA_DATA_TYPE
        )
    );

    cusparseDnVecDescr_t Y;
    CUSPARSE_SAFE_CALL(
        cusparseCreateDnVec(
            &Y,
            (int64_t) args->m,
            (void *) Y_acc->device_view.addr,
            CUDA_DATA_TYPE
        )
    );

    cusparseSpMVAlg_t alg = CUSPARSE_SPMV_ALG_DEFAULT;

    // get workspace size
    size_t buffer_size;
    CUSPARSE_SAFE_CALL(
        cusparseSpMV_bufferSize(
            handle,
            cblas2cusparse_op(args->transA),
           &args->alpha,
            A,
            X,
           &args->beta,
           Y,
           CUDA_DATA_TYPE,
           alg,
          &buffer_size
        )
    );

    // preprocess
    task_dev_info_t * dev = TASK_DEV_INFO(task);
    const device_global_id_t device_global_id = dev->elected_device_id;
    area_chunk_t * chunk = args->xkblas->runtime.memory_device_allocate(device_global_id, buffer_size);
    assert(chunk);
    void * external_buffer = (void *) chunk->ptr;

    # if 0
    CUSPARSE_SAFE_CALL(
        cusparseSpMV_preprocess(
            handle,
            cblas2cusparse_op(args->transA),
           &args->alpha,
            A,
            X,
           &args->beta,
            Y,
            CUDA_DATA_TYPE,
            alg,
            external_buffer
        )
    );
    # endif

    // run spmv
    XKBLAS_CUSPARSE_CALL(
        cusparseSpMV(
            handle,
            cblas2cusparse_op(args->transA),
           &args->alpha,
            A,
            X,
           &args->beta,
            Y,
            CUDA_DATA_TYPE,
            alg,
            external_buffer
        )
    );


    // TODO: is it safe to destroy now ?
    CUSPARSE_SAFE_CALL(cusparseDestroySpMat(A));
    CUSPARSE_SAFE_CALL(cusparseDestroyDnVec(X));
    CUSPARSE_SAFE_CALL(cusparseDestroyDnVec(Y));

    // TODO: push callback in instr->callbacks
    assert(XKRT_CALLBACK_ARGS_MAX >= 2);
    callback_t callback;
    callback.func = body_cuda_run_async_completion;
    callback.args[0] = task;
    callback.args[1] = chunk;
    callback.args[2] = (void *) (uintptr_t) device_global_id;
    callback.args[3] = args->xkblas;
    instr->push_callback(callback);
}

TYPED
static void
body_cuda(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    XKBLAS_CUSPARSE_DISPATCH_PRECISION();
}
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_HOST
TYPED
static void
body_cpu(void * args)
{
    LOGGER_FATAL("Executing a spmv on cpu");
}
# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_SPMV(
    task_format_t * format
) {
    # if XKRT_SUPPORT_HOST
    format->f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu<P>;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format->f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKRT_SUPPORT_CUDA */
}

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_SPMV<P>(task_format_t * format); \
    template int xkblas_t::spmv_async<P>(const xkblas_precision_type_t<P> * alpha, int transA, const int m, const int n, const int nnz, const int * csr_row_offsets, const int * csr_col_indices, const xkblas_precision_type_t<P> * csr_values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y);  \
    template int xkblas_t::spmv_tile_async<P>(const xkblas_precision_type_t<P> * alpha, int transA, const int m, const int n, const int nnz, const int * csr_row_offsets, const int * csr_col_indices, const xkblas_precision_type_t<P> * csr_values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y, size_t tm, distribution_t * d);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
