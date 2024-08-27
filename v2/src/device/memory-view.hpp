#ifndef __MEMORY_VIEW_HPP__
# define __MEMORY_VIEW_HPP__

# include "matrix-tile.h"

# include <vector>

# pragma message(TODO "These views are wrong, a block may not be tight to a matrix block as currently. One idea could be to add an 'offset-m' and 'offset-n' to each view - so when a tile is splitted, we keep the same 'matrix tile 'with a differnet offset")

typedef struct  memory_replicate_view_t
{
    uintptr_t addr; // address of the allocation containing this block on that device
    int LD;         // LD of this replicate view (may be different from
                    // host'LD, as it is allocated compactly on the device)

    int valid;      // '1' if the view is valid
    int fetching;   // '1' is the view is being fetched

    memory_replicate_view_t(
    ) :
        addr(0),
        LD(0),
        valid(0),
        fetching(0)
    {}

    memory_replicate_view_t(
        uintptr_t addr,
        int LD
    ) :
        addr(addr),
        LD(LD),
        valid(0),
        fetching(0)
    {}

    memory_replicate_view_t(
        const memory_replicate_view_t & src
    ) :
        addr(src.addr),
        LD(src.LD),
        valid(0),
        fetching(0)
    {}

    virtual ~memory_replicate_view_t() {}

}               memory_replicate_view_t;

typedef struct  memory_replicate_t
{
    /* List of views for this device replicate.  A device may have several
     * views (and allocation) of the same 'host memory' - as it may
     * asynchronously be read by different concurrent kernel requiring
     * different memory alignment for BLAS operations)
     */
    std::vector<memory_replicate_view_t> views;

    memory_replicate_t() : views() {}
    memory_replicate_t(const memory_replicate_t & r) : views(r.views) {}
    ~memory_replicate_t() {}

}               memory_replicate_t;

using memory_view_t = matrix_tile_t;

#endif /* __MEMORY_VIEW_HPP__ */
