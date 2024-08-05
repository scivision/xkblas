#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# include "device/task.hpp"
# include "sync/access-btree.hpp"

class DependencyTree : public AccessBtree<2, Task> {

   /**
    *  Callback when a dependence is detected
    */
    void
    on_hazard(const Region & rx, Task * x, const Region & ry, Task * y) const
    {
        x->precedes(y, rx.intersection(ry));
    }
};

#endif /* __DEPENDENCY_TREE_HPP__ */
