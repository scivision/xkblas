/* ************************************************************************** */
/*                                                                            */
/*   coherent.cc                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/18 16:26:39 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkblas/xkblas.hpp>
# include <xkrt/runtime.h>

XKRT_NAMESPACE_USE;

extern "C"
void
xkblas_memory_segment_coherent_async(
    void * ptr, size_t size
) {
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, ptr, size);
}

extern "C"
void
xkblas_memory_matrix_coherent_async(
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t sizeof_type
) {
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, ptr, ld, m, n, sizeof_type);
}

extern "C"
int
xkblas_memory_coherent_async(
    int uplo, int memflag,
    size_t M, size_t N,
    void* A, size_t LD, size_t eltsize
) {
    xkblas_memory_matrix_coherent_async(A, LD, M, N, eltsize);
    return 0;
}
