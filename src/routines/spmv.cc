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
# include <algorithm> // for std::find_if
# include <vector>    // for std::vector

XKRT_NAMESPACE_USE;

/* caching matrices information for csr */
TYPED
struct xkblas_matrix_csr_t
{
    struct tiling_t {

        /* a tile */
        struct tile_t {

            /* tile cached info */
            device_global_id_t device_global_id;
            size_t m0;
            size_t m1;
            size_t nnz;
            size_t nnz_offset;

            /* driver-specific metadata */
            void * metadata[XKRT_DRIVER_TYPE_MAX];
        };

        /* tiles */
        tile_t * tiles;

        /* number of tiles */
        int mt;

        /* tile sizes */
        size_t ts;

        tiling_t(tile_t * tiles, int mt, size_t ts) :
            tiles(tiles), mt(mt), ts(ts) {}
        ~tiling_t() {}

        /* for finding */
        bool operator==(const tiling_t & other) const { return ts == other.ts; }
    };

    /* matrix info */
    const int m;
    const int n;
    const size_t nnz;
    const void * row;
    const void * col;
    const TYPE * values;

    /* list of tiling for that csr matrix */
    std::vector<tiling_t> tilings;

    xkblas_matrix_csr_t(
        const int m, const int n, const size_t nnz,
        const void * row, const void * col, const TYPE * values
    ) :
        m(m), n(n), nnz(nnz),
        row(row), col(col), values(values)
    {}

    ~xkblas_matrix_csr_t() {}

};

# define MATRIX_T xkblas_matrix_csr_t<P>
# define TILING_T typename xkblas_matrix_csr_t<P>::tiling_t
# define TILE_T   typename xkblas_matrix_csr_t<P>::tiling_t::tile_t
# define ARGS_T   args_t<P>

TYPED
struct args_t
{
    args_t(
        xkblas_t * xkblas,
        int transA,
        int index_base,
        xkblas_index_t index_type,
        int n,
        TYPE alpha,
        TYPE beta,
        TILE_T * tile
    ) :
        xkblas(xkblas),
        transA(transA),
        index_base(index_base),
        index_type(index_type),
        n(n),
        alpha(alpha),
        beta(beta),
        tile(tile)
    {}

    ~args_t() {}

    xkblas_t * xkblas;
    const int transA;
    const int index_base;
    const xkblas_index_t index_type;
    const int n;
    const TYPE alpha;
    const TYPE beta;
    TILE_T * tile;
};

TYPED_WITH_INDEX
int
xkblas_t::spmv_tile_async(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    const int n,
    const int format,
    const INDEX * row,
    const INDEX * col,
    const TYPE  * values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y,
    /* tile handle */
    void * tile_hdl
) {
    TILE_T * tile = (TILE_T *) tile_hdl;
    assert(tile);
    assert(tile->nnz);

    const int m = (tile->m1 - tile->m0);

    // retrieve producer threads
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    # define AC 5
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(ARGS_T);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_XKRT_TASK_FORMAT_GET(P, SPMV), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 2;
    new (dev) task_dev_info_t(tile->device_global_id, ocr_access);

    ARGS_T * args = (ARGS_T *) TASK_ARGS(task, task_size);
    new (args) ARGS_T(this, transA, index_base, T, n, *alpha, *beta, tile);

    # ifndef XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label), "spmv");
    # endif /* XKRT_SUPPORT_DEBUG */

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t A_X_mode = (*alpha == (const TYPE) 0.0) ? ACCESS_MODE_V : ACCESS_MODE_R;
    access_mode_t Ymode    = (*beta  == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;

    assert(index_base == 0 || index_base == 1);

    new (accesses + 0) access_t(task, row    + tile->m0,         m+1,       sizeof(INDEX), A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, col    + tile->nnz_offset, tile->nnz, sizeof(INDEX), A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 2) access_t(task, values + tile->nnz_offset, tile->nnz, sizeof(TYPE),  A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 3) access_t(task, X,                         n,         sizeof(TYPE),  A_X_mode,  ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 4) access_t(task, Y      + tile->m0,         m,         sizeof(TYPE),  Ymode,     ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
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
    const size_t nnz,
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
        struct {
            int m, n;
            size_t nnz;
        } args;
        args.m = m;
        args.n = n;
        args.nnz = nnz;
        xkblas_routine_auto_tile(SPMV, (int *) &args, &ts);
    }
    const size_t mt = NUM_OF_TILES(m, ts);

    // distribution: cyclic
    const int ngpus = this->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution1D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC1D, ngpus, m, ts);

    //////////////////////////
    // find cached metadata //
    //////////////////////////

    MATRIX_T * matrix = NULL;

    // Acquire read lock first for quick lookup
    pthread_rwlock_rdlock(&this->matrices.csr.rwlock);
    {
        auto it = this->matrices.csr.metadata.find(row);
        if (it != this->matrices.csr.metadata.end())
        {
            // Found, return existing
            matrix = (MATRIX_T *) it->second;
            pthread_rwlock_unlock(&this->matrices.csr.rwlock);
            goto matrix_found;
        }
    }
    pthread_rwlock_unlock(&this->matrices.csr.rwlock);

    // Not found - acquire write lock to insert
    pthread_rwlock_wrlock(&this->matrices.csr.rwlock);
    {
        // Double-check (another thread might have inserted meanwhile)
        auto it = this->matrices.csr.metadata.find(row);
        if (it == this->matrices.csr.metadata.end())
        {
            matrix = (MATRIX_T *) malloc(sizeof(MATRIX_T));
            this->matrices.csr.metadata.emplace(row, matrix);
        }
        else
        {
            matrix = (MATRIX_T *) &it->second;
            goto matrix_found;
        }
    }
    pthread_rwlock_unlock(&this->matrices.csr.rwlock);

    /* construct the matrix */
    new (matrix) MATRIX_T(m, n, nnz, row, col, values);

matrix_found:
    assert(matrix);

    /* insert and/or retrive a tiling */
    auto it = std::find_if(
        matrix->tilings.begin(),
        matrix->tilings.end(),
        [&](const TILING_T & t) { return t.ts == ts; }
    );
    const bool tiling_found = (it != matrix->tilings.end());
    TILING_T * tiling;
    if (tiling_found)
        tiling = &(*it);
    else
    {
        TILE_T *  tiles = (TILE_T *) malloc(mt * sizeof(TILE_T));
        tiling = &matrix->tilings.emplace_back(tiles, mt, ts);
    }

    /////////////////
    // Spawn tiles //
    /////////////////

    // for each tile
    for (size_t tm = 0 ; tm < mt ; ++tm)
    {
        TILE_T * tile = tiling->tiles + tm;
        if (!tiling_found)
        {
            memset(tile->metadata, 0, sizeof(tile->metadata));
            tile->device_global_id = distribution1D_get(&d, tm);
            tile->m0 = tm*ts;
            tile->m1 = (tm == mt-1) ? m : (tm+1) * ts;
            tile->nnz = row[tile->m1] - row[tile->m0];
            tile->nnz_offset = row[tile->m0] - index_base;
        }
        assert(tile);

        // tile spmv
        this->spmv_tile_async<P, T>(alpha, transA, index_base, n, format, row, col, values, X, beta, Y, tile);
    }

    return 0;
}

TYPED_WITH_INDEX
int
xkblas_t::spmv_sync(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    const int m,
    const int n,
    const size_t nnz,
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
    int r = this->spmv_async<P, T>(alpha, transA, index_base, m, n, nnz, format, row, col, values, X, beta, Y);
    this->sync();
    return r;
}

TYPED_WITH_INDEX
int
xkblas_t::spmv(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    const int m,
    const int n,
    const size_t nnz,
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
    this->memory_invalidate_caches();
    int r = this->spmv_async<P, T>(alpha, transA, index_base, m, n, nnz, format, row, col, values, X, beta, Y);
    this->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, Y, m*sizeof(TYPE));
    this->sync();
    return r;
}

# if XKBLAS_SUPPORT_CUBLAS
#  include <xkblas/cusparse-helper.h>
#  include <xkblas/cuda-kernels.h>
#  include <xkrt/driver/driver-cu.h>

# if 0
static void
cuda_run_async_completion(void * args[XKRT_CALLBACK_ARGS_MAX])
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
# endif

struct cuda_csr_tile_metadata_t {
    cusparseSpMatDescr_t A;
    cusparseDnVecDescr_t X;
    cusparseDnVecDescr_t Y;
    area_chunk_t * chunk;
};

template <xkblas_precision_t P, typename CU_TYPE, cudaDataType CUDA_DATA_TYPE>
static inline void
cuda_run(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    cusparseHandle_t handle = queue->cu.sparse.handle;
    assert(handle);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * row    = accesses + 0;
    const access_t * col    = accesses + 1;
    const access_t * values = accesses + 2;
    const access_t * X_acc  = accesses + 3;
    const access_t * Y_acc  = accesses + 4;

    const ARGS_T * args = (ARGS_T *) TASK_ARGS(task);
    assert(args);

    TILE_T * tile = (TILE_T *) args->tile;
    assert(tile);

    assert(values->device_view.addr % sizeof(CU_TYPE) == 0);
    assert( X_acc->device_view.addr % sizeof(CU_TYPE) == 0);
    assert( Y_acc->device_view.addr % sizeof(CU_TYPE) == 0);

    // atm, only support if the spmv tile has its 'tiling cache'
    cusparseSpMVAlg_t alg = CUSPARSE_SPMV_ALG_DEFAULT;
    cuda_csr_tile_metadata_t * mdt = (cuda_csr_tile_metadata_t *) tile->metadata[XKRT_DRIVER_TYPE_CUDA];

    int64_t m = (tile->m1 - tile->m0);
    assert(m > 0);

    if (mdt == NULL)
    {
        // offset row indices, pushing a kernel into the same cuda stream
        // that ensures proper synchronization
        if (args->index_type == I32)
        {
            assert(row->device_view.addr % sizeof(int32_t) == 0);
            assert(col->device_view.addr % sizeof(int32_t) == 0);
            cuda_offset_vector_i32(queue->cu.handle.high, m+1, (int32_t *) row->device_view.addr, -((const int32_t) tile->nnz_offset));
        }
        else if (args->index_type == I64)
        {
            assert(row->device_view.addr % sizeof(int64_t) == 0);
            assert(col->device_view.addr % sizeof(int64_t) == 0);
            cuda_offset_vector_i64(queue->cu.handle.high, m+1, (int64_t *) row->device_view.addr, -((const int64_t) tile->nnz_offset));
        }
        else
            LOGGER_FATAL("Invalid index_type");

        // cache spmv metadata
        mdt = (cuda_csr_tile_metadata_t *) malloc(sizeof(cuda_csr_tile_metadata_t));
        tile->metadata[XKRT_DRIVER_TYPE_CUDA] = mdt;

        cusparseIndexBase_t index_base = (args->index_base == 0)   ? CUSPARSE_INDEX_BASE_ZERO : CUSPARSE_INDEX_BASE_ONE;
        cusparseIndexType_t index_type = (args->index_type == I32) ? CUSPARSE_INDEX_32I       : CUSPARSE_INDEX_64I;

        // setup vector desc
        CUSPARSE_SAFE_CALL(
            cusparseCreateDnVec(
                &mdt->X,
                (int64_t) args->n,
                (void *) X_acc->device_view.addr,
                CUDA_DATA_TYPE
            )
        );

        CUSPARSE_SAFE_CALL(
            cusparseCreateDnVec(
                &mdt->Y,
                (int64_t) m,
                (void *) Y_acc->device_view.addr,
                CUDA_DATA_TYPE
            )
        );

        // setup matrix desc
        static_assert(sizeof(TYPE) == sizeof(CU_TYPE));
        CUSPARSE_SAFE_CALL(
            cusparseCreateCsr(
                &mdt->A,
                (int64_t) m,
                (int64_t) args->n,
                (int64_t) tile->nnz,
                (void *)    row->device_view.addr,
                (void *)    col->device_view.addr,
                (void *) values->device_view.addr,
                index_type,
                index_type,
                index_base,
                CUDA_DATA_TYPE
            )
        );

        // get workspace size
        size_t buffer_size;
        CUSPARSE_SAFE_CALL(
            cusparseSpMV_bufferSize(
                handle,
                cblas2cusparse_op(args->transA),
                &args->alpha,
                mdt->A,
                mdt->X,
                &args->beta,
                mdt->Y,
                CUDA_DATA_TYPE,
                alg,
                &buffer_size
            )
        );

        // preprocess
        task_dev_info_t * dev = TASK_DEV_INFO(task);
        const device_global_id_t device_global_id = dev->elected_device_id;
        mdt->chunk = args->xkblas->runtime.memory_device_allocate(device_global_id, buffer_size);
        assert(mdt->chunk);
        assert(tile->device_global_id == device_global_id);

        CUSPARSE_SAFE_CALL(
            cusparseSpMV_preprocess(
                handle,
                cblas2cusparse_op(args->transA),
                &args->alpha,
                mdt->A,
                mdt->X,
                &args->beta,
                mdt->Y,
                CUDA_DATA_TYPE,
                alg,
                (void *) mdt->chunk->ptr
            )
        );
    }
    else
    {
        // to allow different (x, y) for the same (A)
        // I assumed the 'buffer_size' and 'preprocess' do not depend on (x, y)
        CUSPARSE_SAFE_CALL(cusparseDnVecSetValues(mdt->X, (void *) X_acc->device_view.addr));
        CUSPARSE_SAFE_CALL(cusparseDnVecSetValues(mdt->Y, (void *) Y_acc->device_view.addr));
    }

    // TODO: is it safe to use the same 'workspace' (tile->chunk->ptr) in parallel ?
    // run spmv
    XKBLAS_CUSPARSE_CALL(
        cusparseSpMV(
            handle,
            cblas2cusparse_op(args->transA),
            &args->alpha,
            mdt->A,
            mdt->X,
            &args->beta,
            mdt->Y,
            CUDA_DATA_TYPE,
            alg,
            (void *) mdt->chunk->ptr
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
    XKBLAS_CUSPARSE_DISPATCH_PRECISION();
}
# endif /* XKBLAS_SUPPORT_CUBLAS */

void
xkblas_t::matrices_reset(void)
{
    # if 0
    // TODO: deallocate correctly
    pthread_rwlock_wrlock(&this->matrices.csr.rwlock);
    {
        for (const auto & pair : this->matrices.csr.metadata)
        {
            const void * row = pair.first;
            MATRIX_T * matrix = (MATRIX_T *) pair.second;
            for (const & tiling : matrix->tilings)
            {
                // Use element
            }
            free(matrix);
        }
    }

    // TODO: not freeing that, it is cached in the metadata
    # if 0
    CUSPARSE_SAFE_CALL(cusparseDestroySpMat(A));
    CUSPARSE_SAFE_CALL(cusparseDestroyDnVec(X));
    CUSPARSE_SAFE_CALL(cusparseDestroyDnVec(Y));

    // Push callback in cmd->callbacks
    assert(XKRT_CALLBACK_ARGS_MAX >= 2);
    callback_t callback;
    callback.func = cuda_run_async_completion;
    callback.args[0] = task;
    callback.args[1] = chunk;
    callback.args[2] = (void *) (uintptr_t) device_global_id;
    callback.args[3] = args->xkblas;
    cmd->push_callback(callback);
    # endif

    pthread_rwlock_unlock(&this->matrices.csr.rwlock);
    # endif
    this->matrices.csr.metadata.clear();
}

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME SPMV

# define CL   0
# define CUDA 1
# define HIP  0
# define HOST 0
# define SYCL 0
# define ZE   0

# include "task-format.cc"

/* instanciate methods for each precision */

# define DEFINE(P, T)  \
    template int xkblas_t::spmv_async<P, T>(const xkblas_precision_type_t<P> * alpha, int transA, int index_base, int m, const int n, const size_t nnz, const int format, const xkblas_index_type_t<T> * row, const  xkblas_index_type_t<T> * col, const xkblas_precision_type_t<P> * values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y);  \
    template int xkblas_t::spmv_sync<P, T>(const xkblas_precision_type_t<P> * alpha, int transA, int index_base, int m, const int n, const size_t nnz, const int format, const xkblas_index_type_t<T> * row, const  xkblas_index_type_t<T> * col, const xkblas_precision_type_t<P> * values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y);  \
    template int xkblas_t::spmv<P, T>(const xkblas_precision_type_t<P> * alpha, int transA, int index_base, int m, const int n, const size_t nnz, const int format, const xkblas_index_type_t<T> * row, const  xkblas_index_type_t<T> * col, const xkblas_precision_type_t<P> * values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y);  \
    template int xkblas_t::spmv_tile_async<P, T>(const xkblas_precision_type_t<P> * alpha, int transA, int index_base, const int n, const int format, const xkblas_index_type_t<T> * row, const xkblas_index_type_t<T> * col, const xkblas_precision_type_t<P> * values, xkblas_precision_type_t<P> * X, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * Y, void * tile_hdl);
XKBLAS_FORALL_PRECISIONS_AND_INDEX(DEFINE);
# undef DEFINE
