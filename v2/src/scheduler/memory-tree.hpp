#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "scheduler/memory-block.hpp"
# include "sync/access-interval-multi-tree.hpp"

class MemoryTree : AccessIntervalMultiTree<2, MemoryBlock> {

    void
    on_hazard(const Intervals<2> & rx, MemoryBlock * x, const Intervals<2> & ry, MemoryBlock * y) const
    {
    }
};

#endif /* __MEMORY_TREE_HPP__ */
