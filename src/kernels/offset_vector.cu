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

template <typename T>
__global__
void
kernel_offset_vector_1x1(int n, T * x, const T value)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = idx; i < n; i += stride)
        x[i] += value;
}

template <typename T>
int
cuda_offset_vector(cudaStream_t cuda_queue, int n, T * x, const T v)
{
    int blockSize = 256;
    int numBlocks = (n + blockSize - 1) / blockSize;
    kernel_offset_vector_1x1<T><<<numBlocks, blockSize, 0, cuda_queue>>>(n, x, v);
    return 0;
}

# define T int32_t
extern "C"
int
cuda_offset_vector_i32(cudaStream_t cuda_queue, int n, T * x, const T v)
{
    return cuda_offset_vector<T>(cuda_queue, n, x, v);
}
# undef T

# define T int64_t
extern "C"
int
cuda_offset_vector_i64(cudaStream_t cuda_queue, int n, T * x, const T v)
{
    return cuda_offset_vector<T>(cuda_queue, n, x, v);
}
# undef T
