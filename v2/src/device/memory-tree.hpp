#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/memory-block.hpp"
# include "logger/todo.h"

class MemoryTree {

    protected:
        class Node {

            typedef union
            {
                Node * children[2];
                struct {
                    Node * left;
                    Node * right;
                };
            } subtree_t;

        }; /* class Node */

    public:
        MemoryTree() {}
        virtual ~MemoryTree() {}


    public:

        /** initiate memory transfer to ensure coherency */
        void fetch(xkblas_device_t * device, Task * task);

};

#endif /* __MEMORY_TREE_HPP__ */
