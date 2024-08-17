#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/device.h"
# include "device/memory-block.hpp"
# include "logger/todo.h"

class MemoryTree {

    public:
        MemoryTree();
        virtual ~MemoryTree();

    public:

        /** find memory blocks required for the given task - if not found, new
         * blocks are inserted assumed valid on the host device only */
        void find(task_access_t * access, std::vector<MemoryBlock> & blocks);

        /** initiate memory transfer to ensure coherency */
        void fetch(xkblas_device_t * device, Task * task);

};

#endif /* __MEMORY_TREE_HPP__ */
