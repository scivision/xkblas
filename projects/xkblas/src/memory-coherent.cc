/* ************************************************************************** */
/*                                                                            */
/*   memory-coherent.cc                                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:48:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
    xkblas_context_t * context = xkblas_context_get();
    assert(context);
    return xkrt_memory_coherent_async(&(context->runtime), uplo, memflag, m, n, ptr, ld, sizeof_type);
}
