#ifndef __MEMORY_VIEW_HPP__
# define __MEMORY_VIEW_HPP__

# include "matrix-tile.h"

typedef struct  memory_replicate_view_t
{
    uintptr_t addr; // starting address of the block
    int LD;         // LD of this replicate view (may be different from
                    // host'LD, as it is allocated compactly on the device)

    memory_replicate_view_t() : addr(0), LD(0) {}
    memory_replicate_view_t(const memory_replicate_view_t & src) : addr(src.addr), LD(src.LD) {}
    virtual ~memory_replicate_view_t() {}

}               memory_replicate_view_t;

using memory_view_t = matrix_tile_t;

#endif /* __MEMORY_VIEW_HPP__ */
