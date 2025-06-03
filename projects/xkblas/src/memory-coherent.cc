/* ************************************************************************** */
/*                                                                            */
/*   memory-coherent.cc                                           .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:24:12 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include "context.h"

extern "C"
void
xkblas_memory_coherent_async(
    int uplo, int memflag,
    int m, int n,
    void * ptr, int ld,
    unsigned int sizeof_type
) {
    (void) uplo;
    (void) memflag;
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_coherency_host_async(runtime, MATRIX_COLMAJOR, ptr, (size_t) ld, (size_t) m, (size_t) n, sizeof_type);
}
