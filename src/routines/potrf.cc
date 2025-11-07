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

/**
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon zpotrf wrappers
 *
 * @version 1.0.0
 * @comment This file has been automatically generated
 *          from Plasma 2.5.0 for CHAMELEON 1.0.0
 * @author Mathieu Faverge
 * @author Emmanuel Agullo
 * @author Cedric Castagnede
 * @author Thierry Gautier
 * @date 2018-11-20
 * @precisions normal z -> s d c
 * This file was merged from Chameleon by Thierry Gautier for Kaapi that
 * support natively 2D memory view.
 */

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
        const int uplo,
        const int n
    ) :
        xkblas(xkblas),
        uplo(uplo),
        n(n)
    {}

    ~args_t() {}

    xkblas_t * xkblas;
    const int uplo;
    const int n;
};

/* m, n, k are matrix sizes
 * A_offset_m, A_offset_n, ..., C_offset_n are index of the tile begining */
TYPED
int
xkblas_t::potrf_tile_async(
    int uplo,
    int n,
    TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;

    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, POTRF), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 0;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(this, uplo, n);

    # if XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label), "potrf(A=(%zd,%zd)", A_offset_m, A_offset_n);
    # endif /* XKRT_SUPPORT_DEBUG */

    const size_t Am = n;
    const size_t An = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

/**Parallel tile Cholesky factorization - dynamic scheduling */
TYPED
int
xkblas_t::potrf_async(
    int uplo,
    int n,
    TYPE * A,
    int lda
) {
    if ((uplo != CblasUpper) && (uplo != CblasLower))
    {
        LOGGER_FATAL("illegal value of uplo");
        return -2;
    }

    if (n < 0)
    {
        LOGGER_FATAL("illegal value of N");
        return -5;
    }

    if (lda < MAX(1, n))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    xkblas_t * context = xkblas_get();
    size_t ts = context->conf.kernels[POTRF].tile;
    if (ts == 0)
    {
        int args[1] = { n };
        xkblas_routine_auto_tile(POTRF, args, &ts);
    }

    /* set tiling parameters */
    const size_t Am = n;
    const size_t An = n;

    const size_t Amb = ts;
    const size_t Anb = ts;

    const size_t Amt = NUM_OF_TILES(Am, Amb);
    const size_t Ant = NUM_OF_TILES(An, Anb);

    /* distribute C in a cyclic-block manner */
    const int ngpus = context->runtime.get_ndevices() - 1;

    // TODO: i am not sure what distribution to use for potrf
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Am, An, Amb, Anb);

    const TYPE  one_complex = (TYPE) 1.0;
    const TYPE mone_complex = (TYPE)-1.0;

    const TYPE_REAL  one = (TYPE_REAL) 1.0;
    const TYPE_REAL mone = (TYPE_REAL)-1.0;

    # define A(I, J) A, (I), (J), Amb, Anb, lda

    // TODO: double-check distribution

    if (uplo == CblasLower)
    {
        for (size_t tk = 0; tk < Amt; ++tk)
        {
            const size_t bs_km = (tk == Amt - 1) ? (Am-tk*Amb) : Amb;
            const device_global_id_t device_global_id = distribution2D_get(&d, tk, tk);

            //options.priority = 2*A->mt - 2*k;
            this->potrf_tile_async<P>(
                CblasLower,
                bs_km,
                A(tk, tk),
                device_global_id
            );

            for (size_t tm = tk+1; tm < Amt; ++tm)
            {
                const device_global_id_t device_global_id = distribution2D_get(&d, tm, tk);
                const size_t bs_mm = (tm == Amt-1) ? (Am-tm*Amb) : Amb;
                //options.priority = 2*A->mt - 2*k - m;
                this->trsm_tile_async<P>(
                    CblasRight, CblasLower,
                    CblasConjTrans, CblasNonUnit,
                    bs_mm, Amb,
                    &one_complex,
                    A(tk, tk),
                    A(tm, tk),
                    device_global_id
                );
            }

            for (size_t tn = tk + 1; tn < Ant; ++tn)
            {
                const size_t bs_nn = (tn == Ant-1) ? (An-tn*Anb) : Anb;
                const device_global_id_t device_global_id = distribution2D_get(&d, tn, tn);

                //options.priority = 2*A->mt - 2*k - n;

                if constexpr (P == xkblas_precision_t::S || P == xkblas_precision_t::D)
                {
                    this->syrk_tile_async<P>(
                        CblasLower, CblasNoTrans,
                        bs_nn, Anb,
                        &mone,
                        A(tn, tk),
                        &one,
                        A(tn, tn),
                        device_global_id
                    );
                }
                else
                {
                    this->herk_tile_async<P>(
                        CblasLower, CblasNoTrans,
                        bs_nn, Anb,
                        &mone,
                        A(tn, tk),
                        &one,
                        A(tn, tn),
                        device_global_id
                    );
                }

                for (size_t tm = tn + 1; tm < Amt ; ++tm)
                {
                    const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);
                    const size_t bs_mm = (tm == Amt-1) ? (Am - tm*Amb) : Amb;

                    //options.priority = 2*A->mt - 2*k - n - m;
                    this->gemm_tile_async<P>(
                        CblasNoTrans, CblasConjTrans,
                        bs_mm, bs_nn, Amb,
                        &mone_complex,
                        A(tm, tk),
                        A(tn, tk),
                        &one_complex,
                        A(tm, tn),
                        device_global_id
                    );
                }
            }
        }
    }
    else
    {
        for (size_t tk = 0; tk < Ant; ++tk)
        {
            const device_global_id_t device_global_id = distribution2D_get(&d, tk, tk);
            const size_t bs_km = (tk == Ant-1) ? (An-tk*Anb) : Anb;

            //options.priority = 2*A->nt - 2*k;
            this->potrf_tile_async<P>(
                CblasUpper,
                bs_km,
                A(tk, tk),
                device_global_id
            );

            for (size_t tn = tk+1; tn < Ant; ++tn)
            {
                const device_global_id_t device_global_id = distribution2D_get(&d, tk, tn);
                const size_t bs_nn = (tn == Ant-1) ? (An - tn*Anb) : Anb;

                //options.priority = 2*A->nt - 2*k - n;
                this->trsm_tile_async<P>(
                    CblasLeft, CblasUpper,
                    CblasConjTrans, CblasNonUnit,
                    Amb, bs_nn,
                    &one_complex,
                    A(tk, tk),
                    A(tk, tn),
                    device_global_id
                );
            }

            for (size_t tm = tk+1; tm < Amt ; ++tm)
            {
                const device_global_id_t device_global_id = distribution2D_get(&d, tm, tm);
                const size_t bs_mm = (tm == Amt-1) ? (Am - tm*Amb) : Amb;

                //options.priority = 2*A->nt - 2*k  - m;

                if constexpr (P == xkblas_precision_t::S || P == xkblas_precision_t::D)
                {
                    this->syrk_tile_async<P>(
                        CblasUpper, CblasConjTrans,
                        bs_mm, Amb,
                        &mone,
                        A(tk, tm),
                        &one,
                        A(tm, tm),
                        device_global_id
                    );
                }
                else
                {
                    this->herk_tile_async<P>(
                        CblasUpper, CblasConjTrans,
                        bs_mm, Amb,
                        &mone,
                        A(tk, tm),
                        &one,
                        A(tm, tm),
                        device_global_id
                    );
                }

                for (size_t tn = tm+1; tn < Ant; ++tn)
                {
                    const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);
                    const size_t bs_nn = (tn == Ant-1) ? (An-tn*Anb) : Anb;

                    //options.priority = 2*A->nt - 2*k - n - m;
                    this->gemm_tile_async<P>(
                        CblasConjTrans, CblasNoTrans,
                        bs_mm, bs_nn, Amb,
                        &mone_complex,
                        A(tk, tm),
                        A(tk, tn),
                        &one_complex,
                        A(tm, tn),
                        device_global_id
                    );
                }
            }
        }
    }

    # undef A

    LOGGER_DEBUG("POTRF dependency graph submitted");

    return 0;
}

# if XKBLAS_SUPPORT_CUBLAS
#  include <xkblas/cusolver-helper.h>
#  include <xkrt/driver/driver-cu.h>
#  include <cusolverDn.h>

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

template <xkblas_precision_t P, auto FUNC_SIZE, auto FUNC, typename CU_TYPE>
static inline void
cuda_run(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    (void) cmd;
    (void) idx;

    cusolverDnHandle_t handle = queue->cu.solver.handle;
    assert(handle);

    task_t * task = (task_t *) cmd->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * access   = accesses + 0;
    CU_TYPE * A = (CU_TYPE *) access->device_view.addr;
    int lda = access->device_view.ld;
    assert((uintptr_t)A % access->host_view.sizeof_type == 0);

    const args_t<P> * args = (args_t<P> *) TASK_ARGS(task);
    assert(args);

    int work_size = 0;
    FUNC_SIZE(handle, CUBLAS_FILL_MODE_LOWER, args->n, A, lda, &work_size);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    const device_global_id_t device_global_id = dev->elected_device_id;
    const size_t buffer_size = work_size * sizeof(CU_TYPE) + sizeof(int);
    area_chunk_t * chunk = args->xkblas->runtime.memory_device_allocate(device_global_id, buffer_size);
    assert(chunk);

    CU_TYPE * work = (CU_TYPE *) chunk->ptr;
    assert(work);

    int * dev_info = (int *) (chunk->ptr + work_size * sizeof(CU_TYPE));

    // Perform Cholesky factorization: A = L * L^T
    XKBLAS_CUSOLVER_CALL(
        FUNC(
            handle,
            CUBLAS_FILL_MODE_LOWER,
            args->n,
            A, lda,
            work,
            work_size,
            dev_info
        )
    );

    // Push callback in cmd->callbacks
    assert(XKRT_CALLBACK_ARGS_MAX >= 4);
    callback_t callback;
    callback.func = cuda_run_async_completion;
    callback.args[0] = task;
    callback.args[1] = chunk;
    callback.args[2] = (void *) (uintptr_t) device_global_id;
    callback.args[3] = args->xkblas;
    cmd->push_callback(callback);
}

TYPED
static void
cuda(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    if constexpr (P == xkblas_precision_t::S)
        cuda_run<P, cusolverDnSpotrf_bufferSize, cusolverDnSpotrf, float>(queue, cmd, idx);

    if constexpr (P == xkblas_precision_t::D)
        cuda_run<P, cusolverDnDpotrf_bufferSize, cusolverDnDpotrf, double>(queue, cmd, idx);

    if constexpr (P == xkblas_precision_t::C)
        cuda_run<P, cusolverDnCpotrf_bufferSize, cusolverDnCpotrf, cuComplex>(queue, cmd, idx);

    if constexpr (P == xkblas_precision_t::Z)
        cuda_run<P, cusolverDnZpotrf_bufferSize, cusolverDnZpotrf, cuDoubleComplex>(queue, cmd, idx);
}
# endif /* XKBLAS_SUPPORT_CUBLAS */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME POTRF

# define CL   0
# define CUDA 1
# define HIP  0
# define HOST 0
# define SYCL 0
# define ZE   0

# include "task-format.cc"

/* instanciate methods for each precision */

# define DEFINE(P)  \
    template int xkblas_t::potrf_async<P>(int uplo, int n, xkblas_precision_type_t<P> * A, int lda);  \
    template int xkblas_t::potrf_tile_async<P>(int uplo, int n, xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);

# undef DEFINE
