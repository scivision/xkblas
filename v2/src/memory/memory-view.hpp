#ifndef __MEMORY_VIEW_HPP__
# define __MEMORY_VIEW_HPP__

# include "matrix-tile.h"

# include <vector>

typedef struct  memory_replicate_view_t
{
    uintptr_t addr; // starting address of the matrix on that device
    int LD;         // LD of this replicate view (may be different from
                    // host'LD, as it is allocated compactly on the device)
    int tm;         // tile number (row)
    int tn;         // tile number (col)

    int valid;      // '1' if the view is valid
    int fetching;   // '1' is the view is being fetched

    memory_replicate_view_t() : addr(0), LD(0), tm(0), tn(0), valid(0), fetching(0) {}
    memory_replicate_view_t(const memory_replicate_view_t & src) : addr(src.addr), LD(src.LD), tm(src.tm), tn(src.tn), valid(0), fetching(0) {}
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

}               memory_replicate_t;

using memory_view_t = matrix_tile_t;

#endif /* __MEMORY_VIEW_HPP__ */
