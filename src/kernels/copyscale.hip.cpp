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

#define __HIP_PLATFORM_AMD__
#include <hip/hip_runtime.h>
#include <hipblas/hipblas.h>
#include <stdio.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

# define PRECISION_£   // define your precision (s, d, c, z)

__global__ void kernel_£copyscale_1x1(
    int m, int n, int should_copy,
    const HIP_TYPE* D, int ldd,
    HIP_TYPE* L, int ldl,
    HIP_TYPE* U, int ldu
) {
    extern __shared__ HIP_TYPE shm_ptr[];
    HIP_TYPE* shm_L = shm_ptr;
    HIP_TYPE* shm_X = shm_ptr + blockDim.x * blockDim.y;

    unsigned int idx_x = blockDim.x * blockIdx.x + threadIdx.x;
    unsigned int idx_y = blockDim.y * blockIdx.y + threadIdx.y;

    unsigned int col_count = MIN( blockDim.x * (blockIdx.x + 1), (unsigned int) n ) - blockDim.x * blockIdx.x;
    unsigned int row_count = MIN( blockDim.y * (blockIdx.y + 1), (unsigned int) m ) - blockDim.y * blockIdx.y;

    // Load L in shared
    if( threadIdx.x < col_count && threadIdx.y < row_count )
    {
        size_t s_pos = threadIdx.x + threadIdx.y * blockDim.x;
        size_t l_pos = idx_x + idx_y * ldl;
        shm_L[ s_pos ] = L[ l_pos ];
    }
    __syncthreads();

    // Transpose in U
    if( should_copy && threadIdx.x < row_count && threadIdx.y < col_count )
    {
        size_t pos_block_u = (blockDim.x * blockIdx.x) * ldu + (blockDim.y * blockIdx.y);
        size_t s_pos = threadIdx.x * blockDim.x + threadIdx.y;
        size_t u_pos = pos_block_u + threadIdx.x + threadIdx.y * ldu;

        U[ u_pos ] = shm_L[ s_pos ];
    }

    // Update L
    if( threadIdx.y == 0 && idx_x < (unsigned int) n )
    {
        HIP_TYPE Dxx = D[ idx_x + idx_x * ldd ];
        HIP_TYPE X;
#if defined(PRECISION_s) || defined(PRECISION_d)
        X = 1/Dxx;
#elif defined(PRECISION_c) || defined(PRECISION_z)
        REAL_TYPE Dxx_x = HIP_REAL(Dxx);
        REAL_TYPE Dxx_y = HIP_IMAG(Dxx);
        REAL_TYPE Xx = +Dxx_x / ( Dxx_x*Dxx_x + Dxx_y*Dxx_y );
        REAL_TYPE Xy = -Dxx_y / ( Dxx_x*Dxx_x + Dxx_y*Dxx_y );
        X = make_HIP_TYPE(Xx, Xy);
#else
# error "Unknown precision"
#endif
        shm_X[threadIdx.x] = X;
    }
    __syncthreads();

    if( threadIdx.x < col_count && threadIdx.y < row_count )
    {
        HIP_TYPE V = shm_L[ threadIdx.x + threadIdx.y * blockDim.x ];
        HIP_TYPE X = shm_X[ threadIdx.x ];
        HIP_TYPE ret;

#if defined(PRECISION_s) || defined(PRECISION_d)
        ret = V * X;
#elif defined(PRECISION_c) || defined(PRECISION_z)
        ret.x = V.x * X.x - V.y * X.y;
        ret.y = V.x * X.y + V.y * X.x;
#else
# error "Unknown precision"
#endif
        L[ idx_x + idx_y * ldl ] = ret;
    }
}

// Expose as C-linkable
extern "C"
int hip_£copyscale(
    hipStream_t queue,
    int m, int n,
    int should_copy, int* IW,
    const HIP_TYPE * D, int ldd,
          HIP_TYPE * L, int ldl,
          HIP_TYPE * U, int ldu
) {
    dim3 T = { (unsigned int) n, (unsigned int) m, 1 };
    dim3 B = { 32, 32, 1 };
    dim3 G = { (T.x + B.x - 1)/B.x,  (T.y + B.y - 1)/B.y, (T.z + B.z - 1)/B.z };

    int element_in_shared = B.x*B.y + 1*B.x;
    size_t shared_size = sizeof(HIP_TYPE) * element_in_shared;

    // Launch kernel using HIP
    hipLaunchKernelGGL(kernel_£copyscale_1x1, G, B, shared_size, queue,
                       m, n, should_copy, D, ldd, L, ldl, U, ldu);

    return 0;
}

