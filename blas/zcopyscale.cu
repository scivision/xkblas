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
	//printf("Start copyscale %d %d \n", m, n);
	kernel_zcopyscale_1x1<<<G,B,shared_size,cuda_stream>>>( m, n, should_copy, D, ldd, L, ldl, U, ldu );
	//printf("End copyscale\n");

}

__global__ void kernel_zniv12( cuDoubleComplex* A, cuDoubleComplex* A_SON, int* IW,
							int nrows, int ncols, int nass1, int nelim, int nfront, int cb_compressed )
{
	// Should be called with 32x32 blocks
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	cooperative_groups::thread_block block_group = cooperative_groups::this_thread_block();
	__shared__ int SI[32];
	__shared__ int SJ[32];
	
	uint64_t i = y;
	uint64_t j = x;

	if( threadIdx.y == 0 && blockIdx.y * blockDim.y + threadIdx.y < nrows )
		SI[threadIdx.x] = IW[blockIdx.y * blockDim.y + threadIdx.x] - 1;
	else if( threadIdx.y == 1 && x < nrows )
		SJ[threadIdx.x] = IW[x] - 1;
	block_group.sync();

	if( i >= nrows || j > i )
		return;

	uint64_t id_son;
	
	if( cb_compressed != 0 )
		id_son = (i*(i+1))/2 + j;
	else
		id_son = (i * ncols) + j;

	//uint64_t apos = ((uint64_t) (IW[i]-1)) * nfront + (IW[j]-1); // Load x2 int
	uint64_t apos = ((uint64_t) SI[threadIdx.y]) * nfront + SJ[threadIdx.x];

#if (PRECISION_s == 1) || (PRECISION_d == 1)
	A[apos] += A_SON[id_son]; // Load x2 + Store x1 type
#else
	A[apos].x += A_SON[id_son].x;
	A[apos].y += A_SON[id_son].y;
#endif
}

/*
__global__ void kernel_zniv12_b( cuDoubleComplex* A, cuDoubleComplex* A_SON, int* IW,
							int nrows, int ncols, int nass1, int nelim, int nfront, int cb_compressed )
{
	// Should be blocks of size 1024 x 1 x 1, grid of size nfront x 1 x 1
	cooperative_groups::thread_block block_group = cooperative_groups::this_thread_block();
	__shared__ cuDoubleComplex shared_son[1024];
	__shared__ cuDoubleComplex shared_a[1024];

	// Algo :
	//  1 - one block per line
	//  2 - load batch of A_SON
	//  3 - place where needed in "shared"

	// The reference is A[nfront x nfront]
	for( int X = 0; X < nfront; X += blockDim.x )
	{
		bool should_store = ((X*blockDim.x+threadIdx.x) < nfront);
		// Load A
		if(should_store)
			shared_a[threadIdx.x] = A[ blockIdx.x * nfront + X * blockDim.x + threadIdx.x ];
		block_group.sync();
		
		// Check if 
		

		// Store A
		block_group.sync();
		if(should_store)
			A[ blockIdx.x * nfront + X * blockDim.x + threadIdx.x ] = shared_a[threadIdx.x];
	}
}
*/
__global__ void kernel_zniv12_c( cuDoubleComplex* A, cuDoubleComplex* A_SON, int* IW,
	int nrows, int ncols, int nass1, int nelim, int nfront, int cb_compressed, int* start, int* stop )
{
	// Block of size 1024x1x1 grid of size nfront/1024x1024x1
	cooperative_groups::thread_block block_group = cooperative_groups::this_thread_block();
	__shared__ cuDoubleComplex SA[1024];
	int min_id = start[blockIdx.x];		// Load should be ~ok but still multiples times
	
	uint64_t x = blockDim.x * blockIdx.x + threadIdx.x;
	uint64_t y = blockIdx.y; // => i

	if( min_id == -1 )
		return; // This block have nothing to process

	// Debug steps:
	// - I assume this load can lead to performance issues
	// 3 - May be double checked with nsys-compute
	// 1 - Preload IW on the GPU to reduce the overhead
	// 2 - Make blocs work on multiple rows
	// => IW[y] is loaded nfront/blockDim.x times => approx: nrows * nfront/blockDim.x loads of the distant cache page...
	uint64_t ay = IW[y] - 1; // Load is ineficient -> one block load one value (probably distant)

	// Load
	if(x < nfront)
	{
		SA[threadIdx.x] = A[ x + ay * nfront ];
	}
	block_group.sync();

	// Process
	int max_id = stop[blockIdx.x];
	if( threadIdx.x < max_id - min_id + 1 )
	{ 
		int j = min_id + threadIdx.x;
		if( j <= y )
		{
			int apos = (IW[ j ]-1) - blockDim.x * blockIdx.x;  // This load should be ok
			int id_son; 
			if( cb_compressed != 0 )		// Write different kernels for compressed and uncompressed
				id_son = (y*(y+1))/2 + j;
			else
				id_son = (y * ncols) + j;
#if (PRECISION_s == 1) || (PRECISION_d == 1)
			SA[apos] += A_SON[ id_son ]; // This load should be ok
#else
			SA[apos].x += A_SON[ id_son ].x; // More tricky but should not be so bad
			SA[apos].y += A_SON[ id_son ].y;
#endif
		}
	}

	// Store
	block_group.sync();
	if(x < nfront)
	{
		A[ x + ay * nfront ] = SA[threadIdx.x]; // This load should be ok
	}
}

extern "C" {
void cuda_zniv12( cudaStream_t cuda_stream, cuDoubleComplex* A, cuDoubleComplex* A_SON, int* IW, int nrows, int ncols, int nass1, int nelim, int nfront, int cb_compressed );
}

#define CUDACHECK(call) { cudaError_t err = call; if(err!=cudaSuccess){ printf("[XKBLAS] cuda error at %s:%d - %s\n", __FILE__, __LINE__, cudaGetErrorString(err)); exit(1); }}

void cuda_zniv12( cudaStream_t cuda_stream, cuDoubleComplex* A, cuDoubleComplex* A_SON, int* IW, int nrows, int ncols, int nass1, int nelim, int nfront, int cb_compressed )
{
	if( nelim != 0 )
	{
		printf("[ERROR][XKBLAS] cuda niv12 elim != 0 not implemented\n");
		exit(1);
	}

	struct timespec t0, t1;
	clock_gettime( CLOCK_MONOTONIC, &t0 );

#define niv12_B
#ifdef niv12_A 
	dim3 T = { (unsigned int) nrows, (unsigned int) nrows, 1 }; // How many threads we need
	dim3 B = { 32, 32, 1 }; // Bloc shape
	dim3 G = { (T.x + B.x - 1)/B.x,  (T.y + B.y - 1)/B.y, (T.z + B.z - 1)/B.z }; // Grid
	kernel_zniv12<<<G,B,0,cuda_stream>>>( A, A_SON, IW, nrows, ncols, nass1, nelim, nfront, cb_compressed );
#endif	
	//printf("nrows %d ncols %d nfront %d\n", nrows, ncols, nfront);
	/*
		printf("IW = ");
			for( int i = 0; i < nrows; i++ )
				printf( "%d ", IW[i] );
			printf("\n");
	*/
#ifdef niv12_B
	struct timespec t2;
	dim3 T = { (unsigned int) nfront, (unsigned int) nrows, 1 }; // How many threads we need
	dim3 B = { 512, 1, 1 }; // Bloc shape
	dim3 G = { (T.x + B.x - 1)/B.x,  (T.y + B.y - 1)/B.y, (T.z + B.z - 1)/B.z }; // Grid
	
	int* starts = (int*) alloca( G.x * sizeof(int) );
	int* stops  = (int*) alloca( G.x * sizeof(int) );

	for( int i = 0; i < G.x; i++ )
	{
		starts[i] = -1;
		stops[i] = -1;
	}

	int last_visited = -1;
	for( int i = 0; i < nrows; i++ )
	{
		int x = (IW[i]-1)/B.x;

		if( starts[x] == -1 )
			starts[x] = i;
		stops[x] = i;
	}
	clock_gettime( CLOCK_MONOTONIC, &t2 );

	kernel_zniv12_c<<<G,B,0,cuda_stream>>>( A, A_SON, IW, nrows, ncols, nass1, nelim, nfront, cb_compressed, starts, stops );
#endif

	CUDACHECK( cudaPeekAtLastError() );
	CUDACHECK( cudaDeviceSynchronize() );
	clock_gettime( CLOCK_MONOTONIC, &t1 );
	
	//if( nrows > 5000 )
	//{
		double t = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)/1e9;
		double bw = (((uint64_t) nrows)*nrows/2)/t/1e9;
		double bw_all = (((uint64_t) nfront)*nfront/2)/t/1e9;
#ifdef niv12_B
		double t_2 = (t2.tv_sec - t0.tv_sec) + (t2.tv_nsec - t0.tv_nsec)/1e9; 
		printf("run [%06d,%d] r in %lf ms, %lf Ge/s %lf Ge/s all - cpu part %lf\n", nrows, nfront, t*1000, bw, bw_all, t_2*1000 );
#else	
		printf("run [%06d,%d] r in %lf ms, %lf Ge/s %lf Ge/s all\n", nrows, nfront, t*1000, bw, bw_all);
#endif
		
		/*
		//if( nrows==16671 && nfront==23250 )
		if( nrows==61789 && nfront==61792 )
		{
			printf("IW = ");
			for( int i = 0; i < nrows; i++ )
				printf( "%d ", IW[i] );
			printf("\n");
		}
		*/
	//}
}


