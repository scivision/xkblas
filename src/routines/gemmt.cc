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
 * @brief Chameleon zgemm wrappers
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

# include <cassert>

# include <xkblas/auto-tile.h>
# include <xkblas/xkblas.hpp>
# include <xkblas/cblas.h>

# include <xkrt/support.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/utils/min-max.h>
# include <xkrt/memory/access/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    args_t(
        int uplo,
        int transA, int transB,
        size_t n, size_t k,
        const TYPE alpha,
        const TYPE beta
    ) :
        uplo(uplo),
        transA(transA),
        transB(transB),
        n(n),
        k(k),
        alpha(alpha),
        beta(beta)
    {}

    ~args_t() {}

    const int uplo;
    const int transA;
    const int transB;
    const size_t n;
    const size_t k;
    const TYPE alpha;
    const TYPE beta;

};

TYPED
int
xkblas_t::gemmt_tile_async(
    int uplo,
    int transA, int transB,
    const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    const TYPE * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;
    const size_t B_offset_m = Btm * Bmb;
    const size_t B_offset_n = Btn * Bnb;
    const size_t C_offset_m = Ctm * Cmb;
    const size_t C_offset_n = Ctn * Cnb;

    # define AC 3
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, GEMMT), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 2;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(uplo, transA, transB, n, k, *alpha, *beta);

    # if XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label),
            "gemmt(A=(%zu,%zu) ; B=(%zu,%zu) ; C=(%zu,%zu))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n, C_offset_m, C_offset_n);
    # endif /* XKRT_SUPPORT_DEBUG */

    const size_t Am = (transA == CblasNoTrans) ? n : k;
    const size_t An = (transA == CblasNoTrans) ? k : n;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
    const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = n;
    const size_t Cn = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    access_mode_t Cmode = (*beta == (const TYPE) 0.0) ? ACCESS_MODE_W : ACCESS_MODE_RW;
    new(accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new(accesses + 1) access_t(task, MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_R, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new(accesses + 2) access_t(task, MATRIX_COLMAJOR, C, ldc, C_offset_m, C_offset_n, Cm, Cn, sizeof(TYPE), Cmode        , ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::gemmt_async(
    int uplo,
    int transA, int transB,
    int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    if (uplo == 0)
        return this->gemm_async<P>(transA, transB, n, n, k, alpha, A, lda, B, ldb, beta, C, ldc);

    assert(uplo == CblasLower || uplo == CblasUpper);

    if (n == 0 || ((*alpha == 0.0 || k == 0) && *beta == 1.0))
        return 0;

    /* Check input arguments */
    if ((transA < CblasNoTrans) || (transA > CblasConjTrans))
    {
        LOGGER_FATAL("illegal value of transA");
        return -1;
    }

    if ((transB < CblasNoTrans) || (transB > CblasConjTrans))
    {
        LOGGER_FATAL("illegal value of transB");
        return -2;
    }

    if (n < 0)
    {
        LOGGER_FATAL("illegal value of n");
        return -4;
    }

    if (k < 0)
    {
        LOGGER_FATAL("illegal value of k");
        return -5;
    }

    const size_t Am = (transA == CblasNoTrans) ? n : k;
    const size_t An = (transA == CblasNoTrans) ? k : n;
    const size_t Bm = (transB == CblasNoTrans) ? k : n;
//  const size_t Bn = (transB == CblasNoTrans) ? n : k;
    const size_t Cm = n;
    const size_t Cn = n;

    if ((size_t) lda < MAX(1, Am))
    {
        LOGGER_FATAL("illegal value of lda");
        return -8;
    }

    if ((size_t) ldb < MAX(1, Bm))
    {
        LOGGER_FATAL("illegal value of ldb");
        return -10;
    }

    if ((size_t) ldc < MAX(1, Cm))
    {
        LOGGER_FATAL("illegal value of ldc");
        return -13;
    }

    size_t ts = this->conf.kernels[GEMMT].tile;
    if (ts == 0)
    {
        int args[2] = {n, n};
        xkblas_routine_auto_tile(GEMMT, args, &ts);
    }

    /* set tiling parameters */
    const size_t Amb = ts;
    const size_t Anb = ts;
    const size_t Bmb = ts;
    const size_t Bnb = ts;
    const size_t Cmb = ts;
    const size_t Cnb = ts;

    const size_t Amt = NUM_OF_TILES(Am, Amb);
    const size_t Ant = NUM_OF_TILES(An, Anb);
//  const size_t Bmt = NUM_OF_TILES(Bm, Bmb);
//  const size_t Bnt = NUM_OF_TILES(Bn, Bnb);
    const size_t Cmt = NUM_OF_TILES(Cm, Cmb);
    const size_t Cnt = NUM_OF_TILES(Cn, Cnb);

    /* distribute C in a cyclic-block manner */
    const int ngpus = this->runtime.drivers.devices.n - 1;
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Cm, Cn, Cmb, Cnb);

    const TYPE one = (TYPE) 1.0;

    # define A(I, J) A, (I), (J), Amb, Anb, lda
    # define B(I, J) B, (I), (J), Bmb, Bnb, ldb
    # define C(I, J) C, (I), (J), Cmb, Cnb, ldc

    // iterator on tiles
    for (size_t tm = 0; tm < Cmt; ++tm)
    {
        const size_t bs_mm = (tm == Cmt-1) ? (n-tm*Cmb) : Cmb;
        const size_t tn_min = (uplo == CblasLower) ?   0  :  tm;
        const size_t tn_max = (uplo == CblasLower) ? tm+1 : Cnt;

        for (size_t tn = tn_min ; tn < tn_max; ++tn)
        {
            const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);
            const size_t bs_nn = (tn == Cnt-1) ? (Cn-tn*Cnb) : Cnb;

            // A: CblasNoTrans / B: CblasNoTrans
            if (transA == CblasNoTrans)
            {
                if (transB == CblasNoTrans)
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        const size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            this->gemmt_tile_async<P>(uplo, transA, transB,        bs_nn, bs_kn, alpha, A(tm, tk), B(tk, tn), &zbeta, C(tm, tn), device_global_id);
                        else
                             this->gemm_tile_async<P>(transA, transB, bs_mm, bs_nn, bs_kn, alpha, A(tm, tk), B(tk, tn), &zbeta, C(tm, tn), device_global_id);
                    }
                }
                // A: CblasNoTrans / B: CBlasTrans
                else
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        const size_t bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            this->gemmt_tile_async<P>(uplo, transA, transB,        bs_nn, bs_kn, alpha, A(tm, tk), B(tn, tk), &zbeta, C(tm, tn), device_global_id);
                        else
                            this->gemm_tile_async<P>(transA, transB, bs_mm, bs_nn, bs_kn, alpha, A(tm, tk), B(tn, tk), &zbeta, C(tm, tn), device_global_id);
                    }
                }
            }
            // A: CblasTrans / B: CblasNoTrans
            else
            {
                if (transB == CblasNoTrans)
                {
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        const size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            this->gemmt_tile_async<P>(uplo, transA, transB,        bs_nn, bs_km, alpha, A(tk, tm), B(tk, tn), &zbeta, C(tm, tn), device_global_id);
                        else
                            this->gemm_tile_async<P>(transA, transB, bs_mm, bs_nn, bs_km, alpha, A(tk, tm), B(tk, tn), &zbeta, C(tm, tn), device_global_id);
                    }
                }
                // A: CblasTrans / B: CBlasTrans
                else
                {
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        const size_t bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        const TYPE zbeta = (tk == 0) ? *beta : one;
                        if (tm == tn)
                            this->gemmt_tile_async<P>(uplo, transA, transB,        bs_nn, bs_km, alpha, A(tk, tm), B(tn, tk), &zbeta, C(tm, tn), device_global_id);
                        else
                            this->gemm_tile_async<P>(transA, transB, bs_mm, bs_nn, bs_km, alpha, A(tk, tm), B(tn, tk), &zbeta, C(tm, tn), device_global_id);
                    }
                }
            }
        }
    }

    # undef A
    # undef B
    # undef C

    LOGGER_DEBUG("GEMMT dependency graph submitted");

    return 0;
}

# if XKBLAS_SUPPORT_CUBLAS
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static void
body_cuda_run(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    assert(queue);

    cublasHandle_t handle = queue->cu.blas.handle;
    assert(handle);

    task_t * task = (task_t *) cmd->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    # if XKRT_SUPPORT_DEBUG
    LOGGER_DEBUG("Calling cublasGemmt(m=%zu, n=%zu, k=%zu, A=%p, lda=%zu, B=%p, ldb=%zu, C=%p, ldc=%zu) on task=`%s`",
        args->n, args->n, args->k,
        (void *) A->device_view.addr,
        A->device_view.ld,
        (void *) B->device_view.addr,
        B->device_view.ld,
        (void *) C->device_view.addr,
        C->device_view.ld,
        task->label
    );
    #endif /* XKRT_SUPPORT_DEBUG */

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            cblas2cublas_op(args->transA), cblas2cublas_op(args->transB),
            (int) args->n, (int) args->n, (int) args->k,
            (const CU_TYPE *) &args->alpha,
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const CU_TYPE *) B->device_view.addr, (int) B->device_view.ld,
            (const CU_TYPE *) &args->beta,
            (      CU_TYPE *) C->device_view.addr, (int) C->device_view.ld
        )
    );
}

TYPED
static void
body_cuda(
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_CUBLAS_DISPATCH_PRECISION(gemm);
}

# endif /* XKBLAS_SUPPORT_CUBLAS */


# if XKBLAS_SUPPORT_HIP
#  include <xkblas/hipblas-helper.h>
#  include <xkrt/driver/driver-hip.h>

template <xkblas_precision_t P, auto FUNC, typename HIP_TYPE>
static void
body_hip_run(
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    assert(queue);

    hipblasHandle_t handle = queue->hip.blas.handle;
    assert(handle);

    task_t * task = (task_t *) cmd->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;
    const access_t * C = accesses + 2;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);
    assert(C->device_view.addr % C->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_HIPBLAS_CALL(
        FUNC(
            handle,
            cblas2hipblas_op(args->transA), cblas2hipblas_op(args->transB),
            (int) args->n, (int) args->n, (int) args->k,
            (const HIP_TYPE *) &args->alpha,
            (const HIP_TYPE *) A->device_view.addr, (int) A->device_view.ld,
            (const HIP_TYPE *) B->device_view.addr, (int) B->device_view.ld,
            (const HIP_TYPE *) &args->beta,
            (      HIP_TYPE *) C->device_view.addr, (int) C->device_view.ld
        )
    );
}

TYPED
static void
body_hip(
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_HIPBLAS_DISPATCH_PRECISION(gemm);
}

# endif /* XKBLAS_SUPPORT_HIP */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_GEMMT(
    task_format_t * format
) {
    # if XKBLAS_SUPPORT_CUBLAS
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    # endif /* XKBLAS_SUPPORT_CUBLAS */

    # if XKBLAS_SUPPORT_HIP
    format->f[TASK_FORMAT_TARGET_HIP] = (task_format_func_t) body_hip<P>;
    # endif /* XKBLAS_SUPPORT_HIP */
}

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_GEMMT<P>(task_format_t * format); \
    template int xkblas_t::gemmt_async<P>(int uplo, int transA, int transB, int n, int k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, const xkblas_precision_type_t<P> * B, int ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, int ldc);    \
    template int xkblas_t::gemmt_tile_async<P>(int uplo, int transA, int transB, const size_t n, const size_t k, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, const xkblas_precision_type_t<P> * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb, const xkblas_precision_type_t<P> * beta, xkblas_precision_type_t<P> * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
