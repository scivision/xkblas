#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "logger/todo.h"
# include "scheduler/task.hpp"
# include "sync/access-interval-multi-tree.hpp"

# include <stdint.h>

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
        MemoryBlock() {}
        virtual ~MemoryBlock() {}

        # if 0
        void shrink(Region & to);
        void merge(Region & with);
        # endif

    private:

        # if 0
        /* Matrix */
        uintptr_t A;
        int LD;
        int M;
        int N;
        # endif
};

/**
 *  A tree of memory blocks, to keep track of replicates and task dependences
 */
class MemoryTree : public AccessIntervalMultiTree<2, Task> {

    void
    on_hazard(const Region & rx, Task * x, const Region & ry, Task * y) const
    {
    }
};

#endif /* __MEMORY_TREE_HPP__ */
