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

# include <xkblas/xkblas.hpp>
# include <xkblas/xkblas.h>

XKRT_NAMESPACE_USE;

extern "C"
void
xkblas_memory_segment_coherent_async(
    void * ptr, size_t size
) {
    runtime_t * runtime = xkblas_xkrt_runtime();
    return runtime->memory_coherent_async(XKRT_HOST_DEVICE_UNIQUE_ID, ptr, size);
}

extern "C"
void
xkblas_memory_matrix_coherent_async(
    void * ptr, int ld,
    int m, int n,
    size_t sizeof_type
) {
    runtime_t * runtime = xkblas_xkrt_runtime();
    return runtime->memory_coherent_async(XKRT_HOST_DEVICE_UNIQUE_ID, MATRIX_COLMAJOR, ptr, ld, m, n, sizeof_type);
}

extern "C"
int
xkblas_memory_coherent_async(
    int uplo, int memflag,
    int m, int n,
    void* A, int ld, size_t eltsize
) {
    xkblas_memory_matrix_coherent_async(A, ld, m, n, eltsize);
    return 0;
}

extern "C"
void
xkblas_memory_segment_coherent_sync(
    void * ptr, size_t size
) {
    runtime_t * runtime = xkblas_xkrt_runtime();
    return runtime->memory_coherent_sync(XKRT_HOST_DEVICE_UNIQUE_ID, ptr, size);
}

extern "C"
void
xkblas_memory_matrix_coherent_sync(
    void * ptr, int ld,
    int m, int n,
    size_t sizeof_type
) {
    runtime_t * runtime = xkblas_xkrt_runtime();
    return runtime->memory_coherent_sync(XKRT_HOST_DEVICE_UNIQUE_ID, MATRIX_COLMAJOR, ptr, ld, m, n, sizeof_type);
}

extern "C"
int
xkblas_memory_coherent_sync(
    int uplo, int memflag,
    int m, int n,
    void* A, int ld, size_t eltsize
) {
    xkblas_memory_matrix_coherent_sync(A, ld, m, n, eltsize);
    return 0;
}
