#ifndef __MEMORY_BLOCK_HPP__
# define __MEMORY_BLOCK_HPP__

# include "device/consts.h"
# include "logger/todo.h"

# include <cstdint>

// device starting addr of the block
typedef struct  memory_block_replicate_view_t
{
    uintptr_t addr; // starting address of the block

    memory_block_replicate_view_t() : addr(0) {}
    memory_block_replicate_view_t(const memory_block_replicate_view_t & src) : addr(src.addr) {}
    virtual ~memory_block_replicate_view_t() {}

}               memory_block_replicate_view_t;

// view of the memory block
using memory_block_view_t = matrix_tile_t;

// if greater than 8, then gotta increase the bitfield type
static_assert(XKBLAS_DEVICES_MAX <= 8);
typedef uint8_t memory_block_bitfield_t;

class MemoryBlock {

    public:

        /* memory view of that block */
        memory_block_view_t view;

        /* per device replicate info */
        memory_block_replicate_view_t replicates[XKBLAS_DEVICES_MAX];

        /* if i-th bit is set, the i-th device has a valid copy */
        volatile memory_block_bitfield_t valid;

        /* if i-th bit is set, the i-th device is fetching */
        volatile memory_block_bitfield_t fetching;

    public:

        /* a new memory block, assume it is valid on the host (device id = 0) */
        MemoryBlock(const matrix_tile_t & tile) :
            view(tile),
            replicates(),
            valid(0),
            fetching(0)
        {
            const int devid = 0;
            this->valid = (1 << devid);
            this->replicates[devid].addr = XKBLAS_MATRIX_TILE(tile.addr, tile.LD, tile.tm, tile.tn, tile.bs_m, tile.bs_m);
        }

        MemoryBlock(
            const MemoryBlock & block
        ) :
            view(block.view),
            replicates(block.replicates),
            valid(block.valid),
            fetching(block.fetching)
        {}

        virtual ~MemoryBlock() {}

}; /* MemoryBlock */

#endif /* __MEMORY_BLOCK_HPP__ */
