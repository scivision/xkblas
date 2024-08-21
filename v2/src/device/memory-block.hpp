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
typedef struct  memory_block_view_t
{
    uint32_t  LD;   // leading dimension
    uint32_t  m;    // number of rows
    uint32_t  n;    // number of cols

    memory_block_view_t() : LD(0), m(0), n(0) {}
    memory_block_view_t(const memory_block_view_t & src) : LD(src.LD), m(src.m), n(src.n) {}
    virtual ~memory_block_view_t() {}

}               memory_block_view_t;

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

        MemoryBlock() : view(), replicates(), valid(0), fetching(0) {}

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
