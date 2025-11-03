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

#include <stdio.h>

#include <cooperative_groups.h>
#include <cuComplex.h>

# define PRECISION_£

# define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

const int _block_max_size = 1024;
const int _kernel_max_size = 32;

__global__ void kernel_£pfacldlt( int m, int n, int k, CU_TYPE* A, int lda );
int cuda_£pfacldlt( cudaStream_t stream, int m, int n, int k, CU_TYPE* A, int lda );

int cuda_£pfacldlt(
    cudaStream_t stream, cublasHandle_t handle,
    int m, int n, int k,
    CU_TYPE* A, int lda )
{
  assert( m <= _block_max_size );
  assert( k >= n && k <= m )

  int b_size = _kernel_max_size;
  for( int b_start = 0; b_start < n; b_start += b_size )
  {
    CU_TYPE* D = A + b_start * (lda+1);
    int m_loc = m - b_start;
    int n_loc = MIN( b_size, n - b_start );
    int k_loc = MIN( n_loc, k - b_start );

    cuda_£pfacldlt_cols( stream, m_loc, n_loc, k_loc, D, lda );
    if( b_start + b_size < k )
    {
      CU_TYPE* U = D + b_size;
      CU_TYPE* L = D + b_size * lda;
      CU_TYPE* S = D + b_size * (lda+1);
      CU_TYPE alpha, beta;
#if defined(PRECISION_s) || defined(PRECISION_d)
      alpha = -1.0;
      beta  = +1.0;
#else
      alpha = {-1.0,0.0};
      beta  = {+1.0,0.0};
#endif
      int _m = m - b_start - b_size;
      int _n = k - b_start - b_size;
      int _k = b_size;

      cublas££gemm( handle,
          CUBLAS_OP_N, CUBLAS_OP_N,
          _n, _m, _k,
          &alpha,
          U, lda,
          L, lda,
          &beta,
          S, lda );
    }
  }
  return 0;
}

int cuda_£pfacldlt( cudaStream_t stream, int m, int n, int k, CU_TYPE* A, int lda )
{ // TODO remove hardcoded dims
  dim3 Bdim = {32,32,1};
  dim3 Gdim = {(m+Bdim.x+1)/Bdim.x,1,1};
  void* args[] = { &m, &n, &k, &A, &lda };

  cudaError_t err = cudaLaunchCooperativeKernel( (void*) kernel_£pfacldlt, Gdim, Bdim, (void**) args, 0, NULL );
  if( err != cudaSuccess )
    printf("FS_cols: cuda error %d\n", err); // TODO clean all error checks
  return 0;
}

__global__ void kernel_£pfacldlt( int m, int n, int k, CU_TYPE* A, int lda )
{
  int id_x = threadIdx.x;
  int id_y = threadIdx.y + blockIdx.x * blockDim.y;
  cooperative_groups::grid_group g = cooperative_groups::this_grid();
  __shared__ CU_TYPE L[32];

  bool active = (id_x <= id_y) && (id_y < m) && (id_x < k);

  CU_TYPE val;
  if(active)
    val = A[id_x + id_y * lda ];

  for( int i = 0; i < n; i++ )
  {
    // Save pivot
    if( id_x == i && id_y && active )
    {
      A[ id_x + id_y * lda ] = val; // save pivot
      active = false;
    }
    g.sync();

    // Update L (and store U)
    if( id_x == i && id_y > i && active )
    {
      A[ id_y + id_x * lda ] = val; // Save transpose
#if defined(PRECISION_s) || defined(PRECISION_d)
      val *= 1/A[ i + i * lda ];    // Scale value
#elif defined(PRECISION_c)
      val = cuCmulf(val, cuCdivf( make_cuDoubleComplex(1.0, 0.0), A[i + i*lda]));
#elif defined(PRECISION_z)
      val = cuCmul(val, cuCdiv( make_cuDoubleComplex(1.0, 0.0), A[i + i*lda]));
#endif
      L[ threadIdx.y ] = val;       // Store scaled value in shared
    }
    g.sync();

    // Update others
    if( id_x > i && id_y > i && active )
    {
#if defined(PRECISION_s) || defined(PRECISION_d)
      val -= A[ id_x + i * lda ] * L[threadIdx.y];
#elif defined(PRECISION_c)
      val = cuCsubf(val, cuCmulf(A[id_x + i * lda], L[threadIdx.y]));
#elif defined(PRECISION_z)
      val = cuCsub(val, cuCmul(A[id_x + i * lda], L[threadIdx.y]));
#endif
    }
  }

  // Store L
  if(active)
    A[id_x + id_y * lda] = val;

}
