#ifndef __MEMORY_VIEW_HPP__
# define __MEMORY_VIEW_HPP__

# include "matrix-tile.h"

# include <vector>

# pragma message(TODO "These views are wrong, a block may not be tight to a matrix block as currently. One idea could be to add an 'offset-m' and 'offset-n' to each view - so when a tile is splitted, we keep the same 'matrix tile 'with a differnet offset")

typedef struct  memory_replicate_view_t
{
    uintptr_t addr; // address of the allocation containing this block on that device
    int ld;         // ld of this replicate view (may be different from
                    // host'ld, as it is allocated compactly on the device)

    int valid;      // '1' if the view is valid
    int fetching;   // '1' is the view is being fetched

    memory_replicate_view_t(
    ) :
        addr(0),
        ld(0),
        valid(0),
        fetching(0)
    {}

    memory_replicate_view_t(
        uintptr_t addr,
        int ld
    ) :
        addr(addr),
        ld(ld),
        valid(0),
        fetching(0)
    {}

    memory_replicate_view_t(
        const memory_replicate_view_t & src
    ) :
        addr(src.addr),
        ld(src.ld),
        valid(0),
        fetching(0)
    {}

    ~memory_replicate_view_t() {}

}               memory_replicate_view_t;

using memory_view_t = matrix_tile_t;

#endif /* __MEMORY_VIEW_HPP__ */
