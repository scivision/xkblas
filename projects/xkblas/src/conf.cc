/* ************************************************************************** */
/*                                                                            */
/*   conf.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 19:55:42 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "conf.h"

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdlib.h>
# include <string.h>

void
xkblas_init_conf(xkblas_conf_t * conf)
{
    for (int i = 0 ; i < XKBLAS_KERNEL_TYPE_MAX ; ++i)
        conf->kernels[i].tile = 0;
}
