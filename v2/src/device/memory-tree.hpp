#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/memory-block.hpp"
# include "logger/todo.h"

class MemoryTree {

    public:
        MemoryTree() {}
        virtual ~MemoryTree() {}

    public:

        /** initiate memory transfer to ensure coherency */
        void fetch(xkblas_device_t * device, Task * task);

};

#endif /* __MEMORY_TREE_HPP__ */
