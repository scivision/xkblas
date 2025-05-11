/* ************************************************************************** */
/*                                                                            */
/*   memory-preallocate.cc                                                    */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/11 21:46:25 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include "context.h"

extern "C"
void
xkblas_memory_preallocate(
    void * ptr, int ld,
    int m, int n,
    unsigned int sizeof_type
) {
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    for (xkrt_device_global_id_t device_global_id = 0; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
        xkrt_coherency_allocate_2D(runtime, device_global_id, MATRIX_COLMAJOR, ptr, (size_t) ld, (size_t) m, (size_t) n, sizeof_type);
}
