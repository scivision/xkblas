/* ************************************************************************** */
/*                                                                            */
/*   conf.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:18:51 by                           \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKBLAS_CONF_H__
# define __XKBLAS_CONF_H__

# include <stddef.h>

# include "xkblas/kernel-type.h"

typedef struct  xkblas_conf_kernel_t
{
    size_t tile;
}               xkblas_conf_kernel_t;

typedef struct  xkblas_conf_s
{
    xkblas_conf_kernel_t kernels[XKBLAS_KERNEL_TYPE_MAX];
}               xkblas_conf_t;

void xkblas_init_conf(xkblas_conf_t * conf);

#endif /* __XKBLAS_CONF_H__ */
