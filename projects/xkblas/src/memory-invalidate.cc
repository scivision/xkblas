/* ************************************************************************** */
/*                                                                            */
/*   memory-invalidate.cc                                         .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/28 19:46:21 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:24:15 by Romain PEREIRA         / _______ \       */
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
    # include "context.h"

extern "C"
void
xkblas_memory_invalidate_caches(void)
{
    xkrt_runtime_t * runtime = xkblas_xkrt_runtime_get();
    return xkrt_coherency_reset(runtime);
}
