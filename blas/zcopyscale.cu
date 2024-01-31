/**
 *
 * @file zcopyscale.cu
 *
 * @copyright ???
 *
 ***
 *
 * @brief zcopyscale cuda kernels and wrappers
 *
 * @version 1.0.0
 * @comment ...
 * @author Pierre-Etienne Polet
 * @date 2024-01-24
 * @precisions normal z -> s d c
 */
#include <cooperative_groups.h>
#include <cuComplex.h>
#include <stdbool.h>
#include <stdio.h>

#include<ztask_internal.h>

// Define fuctions as extern C so they can be linked with a C linker
extern "C" {
void cuda_zcopyscale( cudaStream_t, size_t, size_t, bool, int*, const cuDoubleComplex*, size_t, cuDoubleComplex*, size_t, cuDoubleComplex*, size_t );
}

__global__ void kernel_zcopyscale_1x1( 
		int m, int n, bool should_copy, 
		const cuDoubleComplex* D, size_t ldd, 
		cuDoubleComplex* L, size_t ldl,
		cuDoubleComplex* U, size_t ldu )
{	// Expect squared Blocks
	extern __shared__ cuDoubleComplex shm_ptr[];
	cuDoubleComplex* shm_L = shm_ptr;
	cuDoubleComplex* shm_X = shm_ptr + blockDim.x * blockDim.y;
	cooperative_groups::thread_block block_group = cooperative_groups::this_thread_block();

	int idx_x = blockDim.x * blockIdx.x + threadIdx.x;
	int idx_y = blockDim.y * blockIdx.y + threadIdx.y;

	int col_count = min( blockDim.x * (blockIdx.x + 1), n ) - blockDim.x * blockIdx.x;
	int row_count = min( blockDim.y * (blockIdx.y + 1), m ) - blockDim.y * blockIdx.y;

	// Load L in shared
	if( threadIdx.x < col_count && threadIdx.y < row_count )
	{ // TODO deal with bank conflict (cf nvidia blog)
		size_t s_pos = threadIdx.x + threadIdx.y * blockDim.x;
		size_t l_pos = idx_x + idx_y * ldl;
		shm_L[ s_pos ] = L[ l_pos ];
	}
	block_group.sync(); // Wait L loaded in shared

	// Transpose in U
	if( should_copy && threadIdx.x < row_count && threadIdx.y < col_count )
	{
		size_t pos_block_u = (blockDim.x * blockIdx.x) * ldu + (blockDim.y * blockIdx.y);
		size_t s_pos = threadIdx.x * blockDim.x + threadIdx.y;
		size_t u_pos = pos_block_u + threadIdx.x + threadIdx.y * ldu;

		U[ u_pos ] = shm_L[ s_pos ];	
	}

	// Update L
	if( threadIdx.y == 0 && idx_x < n )
	{
		cuDoubleComplex Dxx = D[ idx_x + idx_x * ldd ];
		cuDoubleComplex X;
#if (PRECISION_s == 1) || (PRECISION_d == 1)
		X = 1/Dxx;
#else
		X.x = +Dxx.x / ( Dxx.x*Dxx.x + Dxx.y*Dxx.y );
		X.y = -Dxx.y / ( Dxx.x*Dxx.x + Dxx.y*Dxx.y );
#endif
		shm_X[threadIdx.x] = X;
	}
	block_group.sync(); // Wait D loaded in shared

	if( threadIdx.x < col_count && threadIdx.y < row_count )
	{
		cuDoubleComplex V = shm_L[ threadIdx.x + threadIdx.y * blockDim.x ];
		cuDoubleComplex	X = shm_X[ threadIdx.x ];
		cuDoubleComplex ret;

#if (PRECISION_s == 1) || (PRECISION_d == 1)
		ret = V * X;
#else
		ret.x = V.x * X.x - V.y * X.y;
		ret.y = V.x * X.y + V.y * X.x;
#endif
		L[ idx_x + idx_y * ldl ] = ret;
	}
}


void cuda_zcopyscale( 
	cudaStream_t cuda_stream, size_t m, size_t n, bool should_copy,
        int* IW,
	const cuDoubleComplex* D, size_t ldd,
	cuDoubleComplex* L, size_t ldl,
	cuDoubleComplex* U, size_t ldu )
{
	/* First setup:
	 *	- Bloc of size 32x32 (1024 threads)
	 *	- Each block work on a 32x32 submatrice of L
	 *	- Load L in shared
	 *	- Compute D-1 in shared
	 *	- ...
	 */

	dim3 T = { (unsigned int) n, (unsigned int) m, 1 }; // How many threads we need
	dim3 B = { 32, 32, 1 }; // Bloc shape
	dim3 G = { (T.x + B.x - 1)/B.x,  (T.y + B.y - 1)/B.y, (T.z + B.z - 1)/B.z }; // Grid

	// We will store L and D-1 (as a vector)
	int element_in_shared = B.x*B.y + 1*B.x;
	size_t shared_size = sizeof(cuDoubleComplex) * element_in_shared; // ~16ko, all cuda GPU have 48ko available
	printf("Start copyscale %d %d \n", m, n);
	kernel_zcopyscale_1x1<<<G,B,shared_size,cuda_stream>>>( m, n, should_copy, D, ldd, L, ldl, U, ldu );
	cudaStreamSynchronize(cuda_stream);
	printf("End copyscale\n");

}
