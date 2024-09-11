#ifndef __MEMORY_VIEW_HPP__
# define __MEMORY_VIEW_HPP__

# include "matrix-tile.h"

# include <vector>

typedef struct  memory_replicate_view_t
{
    uintptr_t addr; // address of the allocation containing this block on that device
    int ld;         // ld of this replicate view (may be different from
                    // host'ld, as it is allocated compactly on the device)

    memory_replicate_view_t(
    ) :
        addr(0),
        ld(0)
    {}

    memory_replicate_view_t(
        uintptr_t addr,
        int ld
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

}               memory_replicate_view_t;

using memory_view_t = matrix_tile_t;

#endif /* __MEMORY_VIEW_HPP__ */
