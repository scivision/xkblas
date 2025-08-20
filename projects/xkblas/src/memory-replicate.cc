/* ************************************************************************** */
/*                                                                            */
/*   memory-replicate.cc                                          .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:24:25 by Romain PEREIRA         / _______ \       */
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

# include <xkrt/xkrt.h>
# include "xkblas/v2.hpp"

extern "C"
void
xkblas_memory_replicate_async(
    void * ptr, int ld,
    int m, int n,
    unsigned int sizeof_type
) {
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_coherency_replicate_2D_async(runtime, MATRIX_COLMAJOR, ptr, (size_t) ld, (size_t) m, (size_t) n, sizeof_type);
}
