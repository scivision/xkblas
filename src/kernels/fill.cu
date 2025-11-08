/*
** Copyright 2024,2025 INRIA
**
** Contributors :
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
#include <cuComplex.h>

# define PRECISION_£

__global__
void
kernel_£fill_1x1(
    int n,
    CU_TYPE * x,
    const CU_TYPE value
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    # if 1
    for (int i = idx; i < n; i += stride)
        x[i] = value;
    # else
    // Vectorize global memory write
    # if defined(PRECISION_s)
    static_assert(sizeof(float2) == sizeof(double), "float2 must match double size");
    const float2 val2 = float2(value, value);
    const double vald = *reinterpret_cast<const double *>(&val2);
    const double4 val4 = make_double4(vald, vald, vald, vald);
    const int limit = n >> 3;       // n / 8
    const int rem   = n & (8 - 1);  // n % 8
    const int start = limit << 3;   // n * 8
    # elif defined(PRECISION_d)
    const double4 val4 = make_double4(value, value, value, value);
    const int limit = n >> 2;
    const int rem   = n & (4 - 1);
    const int start = limit << 2;
    # elif defined(PRECISION_c)
    static_assert(sizeof(TYPE) == sizeof(double), "complex must match double size");
    const double  valc = *reinterpret_cast<const double *>(&value);
    const double4 val4 = make_double4(valc, valc, valc, valc);
    const int limit = n >> 2;
    const int rem   = n & (4 - 1);
    const int start = limit << 2;
    # elif defined(PRECISION_z)
    static_assert(sizeof(TYPE) == 2 * sizeof(const double), "double complex must match 2x double size");
    const double4 val4 = make_double4(value.x, value.y, value.x, value.y);
    const int limit = n >> 1;
    const int rem   = n & (2 - 1);
    const int start = limit << 1;
    # endif

    // process 32 bytes at a time
    for (int i = idx; i < limit; i += stride)
        ((double4*)x)[i] = val4;

    // Handle remaining elements
    for (int i = start + idx; i < start + rem; i += stride)
        x[i] = value;
    # endif
}

/* fill the vector 'x' of 'n' elements with the value 'v' */
extern "C"
int
cuda_£fill(
	cudaStream_t cuda_queue,
    int n,
    CU_TYPE * x,
	const CU_TYPE v
) {
    int blockSize = 256;
    int numBlocks = (n + blockSize - 1) / blockSize;
    kernel_£fill_1x1<<<numBlocks, blockSize, 0, cuda_queue>>>(n, x, v);
    // TODO : return errors
    return 0;
}
