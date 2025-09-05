/* ************************************************************************** */
/*                                                                            */
/*   replicate.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:43:33 by Romain PEREIRA         / _______ \       */
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

# if 0
extern "C"
void
xkblas_memory_replicate_async(
    void * ptr, int ld,
    int m, int n,
    unsigned int sizeof_type
) {
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->memory_replicate_coherent_async(MATRIX_COLMAJOR, ptr, (size_t) ld, (size_t) m, (size_t) n, sizeof_type);
}

# endif
