/* ************************************************************************** */
/*                                                                            */
/*   invalidate.cc                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/28 19:46:21 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:35:02 by Romain PEREIRA         / _______ \       */
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
xkblas_memory_invalidate_caches(void)
{
    runtime_t * runtime = xkblas_xkrt_runtime_get();
    return runtime->reset();
}
