/* ************************************************************************** */
/*                                                                            */
/*   preallocate.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:36:39 by Romain PEREIRA         / _______ \       */
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
xkblas_memory_preallocate(
    void * ptr, int ld,
    int m, int n,
    unsigned int sizeof_type
) {
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    for (device_global_id_t device_global_id = 0; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
        if (device_global_id != HOST_DEVICE_GLOBAL_ID)
            runtime->incoherent_allocate_2D(runtime, device_global_id, MATRIX_COLMAJOR, ptr, (size_t) ld, (size_t) m, (size_t) n, sizeof_type);
}
 #endif
