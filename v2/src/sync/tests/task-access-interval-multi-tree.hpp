#ifndef __TASK_ACCESS_INTERVAL_MULTI_TREE__
# define __TASK_ACCESS_INTERVAL_MULTI_TREE__

# include "access-interval-multi-tree.hpp"

template<int K>
class TaskAccessIntervalMultiTree : public AccessIntervalMultiTree<K, Task> {

    void
    on_hazard(const Intervals<K> & rx, Task * x, const Intervals<K> & ry, Task * y) const
    {
        task_link(rx, x, ry, y);
    }
};

#endif /* __TASK_ACCESS_INTERVAL_MULTI_TREE__ */
