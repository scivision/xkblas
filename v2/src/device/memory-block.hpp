#ifndef __MEMORY_BLOCK_HPP__
# define __MEMORY_BLOCK_HPP__

# include "logger/todo.h"

typedef enum    memory_block_replicate_state_t : uint8_t
{
    MEMORY_BLOCK_REPLICATE_DEALLOCATED  = 0,
    MEMORY_BLOCK_REPLICATE_ALLOCATED    = 1,
    MEMORY_BLOCK_REPLICATE_TRANSFERING  = 2,
}               memory_block_replicate_state_t;

# pragma message(TODO "Make this as small as possible")
class MemoryBlockReplicate {

    public:
        /* replicate state */
        memory_block_replicate_state_t state;

    public:
        MemoryBlockReplicate() : state(MEMORY_BLOCK_REPLICATE_DEALLOCATED) {}
        virtual ~MemoryBlockReplicate() {}

};

class MemoryBlock {

};

#endif /* __MEMORY_BLOCK_HPP__ */
