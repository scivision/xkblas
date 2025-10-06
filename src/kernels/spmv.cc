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

# if XKBLAS_SUPPORT_SYCL
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
        int index_base,
        xkblas_index_t index_type,
        int m,
        int n,
        int nnz,
        TYPE alpha,
        TYPE beta
    ) :
        xkblas(xkblas),
        transA(transA),
        index_base(index_base),
        index_type(index_type),
        m(m),
        n(n),
        nnz(nnz),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    xkblas_t * xkblas;
    const int transA;
    const int index_base;
    const xkblas_index_t index_type;
    const int m;
    const int n;
    const int nnz;
    const TYPE alpha;
    const TYPE beta;
};

TYPED_WITH_INDEX
int
xkblas_t::spmv_tile_async(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    const int m,
    const int n,
    const int nnz,
    const int format,
    const INDEX * row,
    const INDEX * col,
    const TYPE * values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y,
    device_global_id_t device_global_id
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
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(this, transA, index_base, T, m, n, nnz, *alpha, *beta);

    # ifndef XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label), "spmv");
    # endif /* XKRT_SUPPORT_DEBUG */

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t A_X_mode = (*alpha == (const TYPE) 0.0) ? ACCESS_MODE_V : ACCESS_MODE_R;
    access_mode_t Ymode    = (*beta  == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;

    const int nrows = (format == CblasSparseCSR) ? m+1 : 0;
    new (accesses + 0) access_t(task, row,  nrows, sizeof(INDEX), A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, col,    nnz, sizeof(INDEX), A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 2) access_t(task, values, nnz, sizeof(TYPE),  A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 3) access_t(task, X,      n,   sizeof(TYPE),  A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 4) access_t(task, Y,      m,   sizeof(TYPE),  Ymode,     ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED_WITH_INDEX
int
xkblas_t::spmv_async(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    const int m,
    const int n,
    const int nnz,
    const int format,
    const INDEX * row,
    const INDEX * col,
    const TYPE * values,
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

    if (index_base != 0 && index_base != 1)
    {
        LOGGER_FATAL("Invalid index_base");
        return -1;
    }

    if (format != CblasSparseCSR)
    {
        LOGGER_FATAL("Invalid format, only supporting CSR");
        return -2;
    }

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

    // replicate `row` to avoid polluting user matrix if CSR format is used
    INDEX * row_dup;
    if (format == CblasSparseCSR)
    {
        row_dup = (INDEX *) malloc(sizeof(INDEX) * (m+1));
        assert(row_dup);
    }

    // for each tile
    for (size_t tm = 0 ; tm < mt ; ++tm)
    {
        const device_global_id_t device_global_id = distribution1D_get(&d, tm);

        // block size
        size_t m0 = tm*ts;
        size_t m1 = (tm == mt-1) ? m : (tm+1) * ts;
        size_t bs = m1 - m0;

        // compute number of nnz for that tile, to offset values array
        INDEX tile_nnz = (format == CblasSparseCSR) ? (row[m1] - row[m0]) : 0;

        // no elements in that tile
        if (tile_nnz == 0)
            continue ;

        INDEX const * row_ptr;
        INDEX const * col_ptr;
        TYPE  const * values_ptr;

        if (format == CblasSparseCSR)
        {
            // TODO: i dislike this nasty loop over each rows
            // that will probably be a performance issue in iterative solvers where
            // the matrix A does not change, we'll figure something out then
            // maybe there exists a cuda kernel that can automatically offset to offsets[m0] ?

            // adjust tile_row_ptr so first entry is 0
            if (tm == 0)
            {
                row_ptr = row;
            }
            else
            {
                for (size_t i = m0 ; i <= m1 ; ++i)
                    row_dup[i] = row[i] - row[m0] + (INDEX) index_base;
                row_ptr = row_dup;
            }

            row_ptr    = row_ptr + m0;
            col_ptr    = col     + row[m0] - index_base;
            values_ptr = values  + row[m0] - index_base;
        }
        else if (format == CblasSparseCOO)
        {
            LOGGER_FATAL("Not supported");
        }
        else
        {
            LOGGER_FATAL("Not supported");
        }

        // tile spmv
        this->spmv_tile_async<P, T>(
            alpha,
            transA,
            index_base,
            bs, n, tile_nnz,
            format,
            row_ptr, col_ptr, values_ptr,
            X,
            beta,
            Y + m0,
            device_global_id
        );
    }
    LOGGER_WARN("`row_dup` is leaking");

    return 0;
}

# if XKBLAS_SUPPORT_CUSPARSE
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
    const access_t * row = accesses + 0;
    const access_t * col = accesses + 1;
    const access_t * values  = accesses + 2;
    const access_t * X_acc   = accesses + 3;
    const access_t * Y_acc   = accesses + 4;

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    if (args->index_type == I32)
    {
        assert(row->device_view.addr % sizeof(int32_t) == 0);
        assert(col->device_view.addr % sizeof(int32_t) == 0);
    }
    else
    {
        assert(row->device_view.addr % sizeof(int64_t) == 0);
        assert(col->device_view.addr % sizeof(int64_t) == 0);
    }
    assert(values->device_view.addr % sizeof(CU_TYPE) == 0);
    assert(X_acc->device_view.addr  % sizeof(CU_TYPE) == 0);
    assert(Y_acc->device_view.addr  % sizeof(CU_TYPE) == 0);

    // setup matrix desc
    cusparseSpMatDescr_t A;
    cusparseIndexBase_t index_base = (args->index_base == 0) ? CUSPARSE_INDEX_BASE_ZERO : CUSPARSE_INDEX_BASE_ONE;
    cusparseIndexType_t index_type = (args->index_type == I32) ? CUSPARSE_INDEX_32I : CUSPARSE_INDEX_64I;
    CUSPARSE_SAFE_CALL(
        cusparseCreateCsr(
            &A,
            (int64_t) args->m,
            (int64_t) args->n,
            (int64_t) args->nnz,
            (void *) row->device_view.addr,
            (void *) col->device_view.addr,
            (void *) values->device_view.addr,
            index_type,
            index_type,
            index_base,
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
# endif /* XKBLAS_SUPPORT_CUSPARSE */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_SPMV(
    task_format_t * format
) {
    # if XKBLAS_SUPPORT_CUSPARSE
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKBLAS_SUPPORT_CUSPARSE */
}

/* instanciate methods for each precision */

# define DEFINE(P, T)  \
    template int xkblas_t::spmv_async<P, T>(const xkblas_precision_type_t<P> * alpha, int transA, int index_base, int m, const int n, const int nnz, const int format, const xkblas_index_type_t<T> * row, const  xkblas_index_type_t<T> * col, const xkblas_precision_type_t<P> * values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y);  \
    template int xkblas_t::spmv_tile_async<P, T>(const xkblas_precision_type_t<P> * alpha, int transA, int index_base, const int m, const int n, const int nnz, const int format, const xkblas_index_type_t<T> * row, const xkblas_index_type_t<T> * col, const xkblas_precision_type_t<P> * values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS_AND_INDEX(DEFINE);
# undef DEFINE

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_SPMV<P>(task_format_t * format);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
