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
        size_t m, size_t n, size_t k,
    ) :
        m(m),
        n(n),
        k(k),
    {}

    ~args_t() {}

    const size_t m;
    const size_t n;
    const size_t k;

};

/* m, n, k are matrix sizes
 * A_offset_m, A_offset_n, ..., C_offset_n are index of the tile begining */
TYPED
int
xkblas_t::pfacldlt_tile_async(
    const size_t m, const size_t n, const size_t k,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    device_global_id_t device_global_id
) {

    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;

    // TODO
    //LOGGER_DEBUG("Submitting tile C=(%zd,%zd) of size (%zd,%zd)", C_offset_m, C_offset_n, m, n);

    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_TASK_FORMAT_GET(P, PFACLDLT), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 0;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>( m, n, k );


    # if XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label),
            "pfacldlt(A=(%zd,%zd))",
            A_offset_m, A_offset_n);
    # endif /* XKRT_SUPPORT_DEBUG */

    const size_t Am = m;
    const size_t An = m;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::pfacldlt_async(
    int m, int n, int k,
    TYPE * A, int lda )
{
  if( n == 0 )
    return 0;

  if( m < 0 )
  {
    LOGGER_FATAL("illegal value of m (<0)");
    return -1;
  }
  if( n < 0 )
  {
    LOGGER_FATAL("illegal value of n (<0)");
    return -2;
  }
  if( m < n )
  {
    LOGGER_FATAL("illegal value of n: should be smaller or equal to m");
    return -3;
  }
  if( n < k )
  {
    LOGGER_FATAL("illegal value of k: should be greater or equal to n");
    return -4;
  }
  if( m < k )
  {
    LOGGER_FATAL("illegal value of k: should be smaller or equal to m");
    return -5;
  }
  if( ((size_t) lda) < m )
  {
    LOGGER_FATAL("illegal value of lda (<m)");
    return -6;
  }

  xkblas_t * context = xkblas_get();
  int b_size = 1024;

  // TODO should we do a distribution ... ?
/*
    const int ngpus = context->runtime.get_ndevices() - 1;
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Cm, Cn, Cmb, Cnb);
    ...
    const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);
*/

  for( int b_start = 0; b_start < n; b_start += b_size )
  {
    TYPE* D = A + b_start * (lda+1);
    int m_loc = min( b_size, m - b_start );
    int n_loc = min( b_size, n - b_start );
    int k_loc = min( n_loc, k - b_start );
    partial_front_factorization_subblock( m_loc, n_loc, k_loc, D, lda );

    # define A(I,J) A, (I), (J), Amb, Anb, lda
    if( b_start + b_size < m )
    {
      TYPE* L = D + b_size * lda;
      TYPE* U = D + b_size;
      TYPE* S = D + b_size * (lda+1);

      LOGGER_FATAL("This part of the code is not implemented yet\n");
      return -100;
      // TODO do TRSM + copyscale
      if( b_start + b_size < k )
      {
        // TODO GEMM
      }
    }
    # undef A
  }
  LOGGER_DEBUG("FACLDLT dependency graph submitted");
  return 0;
}

TYPED
int xkblas_t::pfacldlt(
    int m, int n, int k,
    TYPE * A, int lda )
{
    this->memory_invalidate_caches();
    int r = this->pfacldlt_async<P>( m, n, k, A, lda );
    this->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, A, lda, m, m, sizeof(TYPE));
    return r;
}

#if XKBLAS_SUPPORT_CUBLAS
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

extern "C" {
    int cuda_spfacldlt(cudaStream_t cuda_queue, cublasHandle_t cublas_handle, int m, int n, int k, float* A, int lda );
    int cuda_dpfacldlt(cudaStream_t cuda_queue, cublasHandle_t cublas_handle, int m, int n, int k, double* A, int lda );
    int cuda_cpfacldlt(cudaStream_t cuda_queue, cublasHandle_t cublas_handle, int m, int n, int k, cuComplex* A, int lda );
    int cuda_zpfacldlt(cudaStream_t cuda_queue, cublasHandle_t cublas_handle, int m, int n, int k, cuDoubleComplex* A, int lda );
}

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static inline void
body_cuda_run(
    stream_cu_t * stream,
    stream_instruction_t * instr,
    stream_instruction_counter_t idx
) {
    assert( stream );

    cublasHandle_t cublas_handle = stream->cu.blas.handle;
    cudaStream_t cuda_queue = stream->cu.handle.high;
    assert( cublas_handle );
    assert( cuda_queue );

    task_t * task = (task_t *) cmd->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    assert(A->device_view.addr % A->host_view.sizeof_type == 0);

    args_t * args = (args_t *) TASK_ARGS(task);
    assert(args);

    FUNC(
        cuda_queue, cublas_handle,
        (int) args->m, (int) args->n, (int) args->k,
        (CU_TYPE *) A->device_view.addr, (int) D->device_view.ld
    );
    XKBLAS_CUBLAS_CALL_POST();
}

TYPED
static void
body_cuda(
    stream_cu_t * queue,
    stream_instruction_t  * cmd,
    stream_instruction_counter_t idx
) {
    if constexpr (P == xkblas_precision_t::S) body_cuda_run<P, cuda_scopyscale, float>(queue, cmd, idx);
    if constexpr (P == xkblas_precision_t::D) body_cuda_run<P, cuda_dcopyscale, double>(queue, cmd, idx);
    if constexpr (P == xkblas_precision_t::C) body_cuda_run<P, cuda_ccopyscale, cuComplex>(queue, cmd, idx);
    if constexpr (P == xkblas_precision_t::Z) body_cuda_run<P, cuda_zcopyscale, cuDoubleComplex>(queue, cmd, idx);
}
#endif


//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

TYPED
void
xkblas_t::task_format_create_GEMM(
    task_format_t * format
) {
    # if XKBLAS_SUPPORT_CUDA
    format->f[TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) body_cuda<P>;
    #endif
}

/* instanciate methods for each precision */
# define DEFINE(P)  \
    template void xkblas_t::task_format_create_PFACLDLT<P>(task_format_t * format); \
    template int xkblas_t::pfacldlt<P>(int m, int n, int k, const xkblas_precision_type_t<P> * A, int lda);    \
    template int xkblas_t::pfacldlt_async<P>(int m, int n, int k, const xkblas_precision_type_t<P> * A, int lda);    \
    template int xkblas_t::pfacldlt_tile_async<P>(const size_t m, const size_t n, const size_t k, const xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
