#ifndef __MEMORY_BLOCK_HPP__
# define __MEMORY_BLOCK_HPP__

# include "logger/todo.h"

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

    private:

        # if 0
        /* Matrix */
        uintptr_t A;
        int LD;
        int M;
        int N;
        # endif
};

#endif /* __MEMORY_BLOCK_HPP__ */
