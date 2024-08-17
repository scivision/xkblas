#ifndef __MEMORY_BLOCK_HPP__
# define __MEMORY_BLOCK_HPP__

# include "device/consts.h"
# include "logger/todo.h"

# include <cstdint>

typedef enum    memory_block_replicate_state_t : uint8_t
{
    MEMORY_BLOCK_REPLICATE_DEALLOCATED  = 0,
    MEMORY_BLOCK_REPLICATE_ALLOCATED    = 1,
    MEMORY_BLOCK_REPLICATE_TRANSFERING  = 2,
}               memory_block_replicate_state_t;

// device view of the memory block
typedef struct  memory_block_view_t
{
    uintptr_t addr;
    uint32_t  ld;
    uint32_t  m;
    uint32_t  n;
    // uint8_t type;        // assume 2D block always
    // uint8_t storage;     // assume row major storage
}               memory_block_view_t;

# pragma message(TODO "Make this as small as possible")
class MemoryBlockReplicate {

    public:

        /* replicate state */
        memory_block_replicate_state_t state;

        /* view of the block replicate */
        memory_block_view_t view;

    public:
        MemoryBlockReplicate() : state(MEMORY_BLOCK_REPLICATE_DEALLOCATED) {}
        virtual ~MemoryBlockReplicate() {}

};

// if greater than 8, then gotta increase the bitfield type
static_assert(XKBLAS_DEVICES_MAX <= 8);
typedef uint8_t memory_block_bitfield_t;

class MemoryBlock {
    public:

        /* per device replicate info */
        MemoryBlockReplicate replicates[XKBLAS_DEVICES_MAX];

        /* per device validity bit */
        memory_block_bitfield_t valid;

    public:
        MemoryBlock() : replicates(), valid(0) {}
        virtual ~MemoryBlock() {}

};

#endif /* __MEMORY_BLOCK_HPP__ */
