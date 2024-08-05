#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "logger/todo.h"
# include "sync/access-btree.hpp"

typedef enum    memory_block_replicate_state_t : uint8_t
{
    MEMORY_BLOCK_REPLICATE_DEALLOCATED  = 0,
    MEMORY_BLOCK_REPLICATE_ALLOCATED    = 1,
    MEMORY_BLOCK_REPLICATE_TRANSFERING  = 2,
}               memory_block_replicate_state_t;

# pragma message(TODO "Make this as small as possible")
typedef struct  memory_block_replicate_t
{
    memory_block_replicate_state_t state;

}               memory_block_replicate_t;

class MemoryBlock {

    public:

        /* replicate state per device */
        memory_block_replicate_t replicates[XKBLAS_DEVICES_MAX];
};

class MemoryTree {

    public:




};

#endif /* __MEMORY_TREE_HPP__ */
