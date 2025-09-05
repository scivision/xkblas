/* ************************************************************************** */
/*                                                                            */
/*   auto-tile.h                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/11 16:58:39 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/20 20:17:26 by Romain PEREIRA         / _______ \       */
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

#ifndef __AUTO_TILE_H__
# define __AUTO_TILE_H__

# include <stddef.h>
# include "xkblas/kernel.hpp"

void
xkblas_kernel_auto_tile(
    xkblas_kernel_t kernel,
    int * args,
    size_t * bs
);

#endif /* __AUTO_TILE_H__ */
