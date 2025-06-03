/* ************************************************************************** */
/*                                                                            */
/*   view.hpp                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/23 15:33:40 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:04:56 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __MEMORY_VIEW_HPP__
# define __MEMORY_VIEW_HPP__

# include <xkrt/memory/access/blas/matrix.h>

# include <vector>

typedef struct  memory_replicate_view_t
{
    uintptr_t addr; // address of the allocation containing this block on that device
    size_t ld;      // ld of this replicate view (may be different from
                    // host'ld, as it is allocated compactly on the device)

    memory_replicate_view_t(
    ) :
        addr(0),
        ld(0)
    {}

    memory_replicate_view_t(
        uintptr_t addr,
        size_t ld
    ) :
        addr(addr),
        ld(ld)
    {}

    memory_replicate_view_t(
        const memory_replicate_view_t & src
    ) :
        addr(src.addr),
        ld(src.ld)
    {}

    ~memory_replicate_view_t() {}

    // user-defined copy assignment (non copy-and-swap idiom)
    // note: copy-and-swap would always reallocate resources
    memory_replicate_view_t & operator=(const memory_replicate_view_t & other)
    {
        this->addr = other.addr;
        this->ld   = other.ld;
        return *this;
    }

}               memory_replicate_view_t;

using memory_view_t = matrix_tile_t;

#endif /* __MEMORY_VIEW_HPP__ */
