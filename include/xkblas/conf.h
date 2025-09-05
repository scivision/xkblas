/* ************************************************************************** */
/*                                                                            */
/*   conf.h                                                       .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/04 17:03:17 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/20 20:45:31 by Romain PEREIRA         / _______ \       */
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

#ifndef __XKBLAS_CONF_H__
# define __XKBLAS_CONF_H__

# include <stddef.h>
# include <xkblas/kernel.hpp>

typedef struct  xkblas_conf_kernel_t
{
    size_t tile;
}               xkblas_conf_kernel_t;

typedef struct  xkblas_conf_s
{
    xkblas_conf_kernel_t kernels[XKBLAS_KERNEL_MAX];
}               xkblas_conf_t;

void xkblas_init_conf(xkblas_conf_t * conf);

#endif /* __XKBLAS_CONF_H__ */
