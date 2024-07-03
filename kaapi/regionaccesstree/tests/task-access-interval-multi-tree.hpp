#ifndef __TASK_ACCESS_INTERVAL_MULTI_TREE__
# define __TASK_ACCESS_INTERVAL_MULTI_TREE__

# include "access-interval-multi-tree.hpp"

template<int K>
class TaskAccessIntervalMultiTree : public AccessIntervalMultiTree<K, Task> {

    void
    on_hazard(const Region<K> & rx, void * x, const Region<K> & ry, void * y) const
    {
        task_link(rx, x, ry, y);
    }
};

#endif /* __TASK_ACCESS_INTERVAL_MULTI_TREE__ */
