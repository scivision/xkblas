/* ************************************************************************** */
/*                                                                            */
/*   conf.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/10 10:59:00 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/08/20 20:45:46 by Romain PEREIRA         / _______ \       */
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

# include <xkblas/conf.h>
# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdlib.h>
# include <string.h>

void
xkblas_init_conf(xkblas_conf_t * conf)
{
    for (int i = 0 ; i < XKBLAS_KERNEL_MAX ; ++i)
        conf->kernels[i].tile = 0;
}
