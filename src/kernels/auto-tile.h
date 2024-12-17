/* ************************************************************************** */
/*                                                                            */
/*   auto-tile.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __AUTO_TILE_H__
# define __AUTO_TILE_H__

# include "xkblas-kernel-type.h"

void
xkblas_kernel_auto_tile(
    xkblas_kernel_type_t kernel,
    int * args,
    size_t * bs
);

#endif /* __AUTO_TILE_H__ */
