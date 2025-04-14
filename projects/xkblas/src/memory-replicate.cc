/* ************************************************************************** */
/*                                                                            */
/*   memory-replicate.cc                                                      */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/11 16:55:32 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include "context.h"

extern "C"
void
xkblas_replicate_async(
    void * ptr, int ld,
    int m, int n,
    unsigned int sizeof_type
) {
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_coherency_replicate_2D_async(runtime, MATRIX_COLMAJOR, ptr, (size_t) ld, (size_t) m, (size_t) n, sizeof_type);
}
