/* ************************************************************************** */
/*                                                                            */
/*   task.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/04/20 03:00:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/20 03:50:56 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/memory/memory-tree.hpp>

/**
 * Retrieve or (insert and return) the memory controller of the passed task for the given access
 */
MemoryCoherencyController *
task_get_memory_controller(
    xkrt_runtime_t * runtime,
    task_t * task,
    const access_t * access
) {
    assert(task);
    assert(task->flags & TASK_FLAG_DOMAIN);

    task_dom_info_t * dom = TASK_DOM_INFO(task);
    assert(dom);

    SPINLOCK_LOCK(dom->mems_lock);

    /* find previous memtree for that ld */
    for (MemoryCoherencyController * mem : dom->mems)
    {
        MemoryTree * tree = (MemoryTree *) mem;
        if (tree->can_resolve(access))
        {
            SPINLOCK_UNLOCK(dom->mems_lock);
            return mem;
        }
    }

    LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
            access->host_view.ld, access->host_view.sizeof_type, runtime->conf.merge_transfers ? "true" : "false");

    /* if not found, create a new memtree */
    MemoryCoherencyController * mem = new MemoryTree(runtime, access->host_view.ld, access->host_view.sizeof_type, runtime->conf.merge_transfers);
    assert(mem);
    dom->mems.push_back(mem);

    SPINLOCK_UNLOCK(dom->mems_lock);

    return mem;
}

/**
 * Retrieve or (insert and return) the dependency domain of the passed task for the given access
 */
DependencyDomain *
task_get_dependency_domain(
    task_t * task,
    const access_t * access
) {
    assert(task);
    assert(task->flags & TASK_FLAG_DOMAIN);

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    task_dom_info_t * dom = TASK_DOM_INFO(thread->current_task);
    assert(dom);

    /* find previous deptree for that ld */
    for (DependencyDomain * domain : dom->deps)
    {
        DependencyTree * tree = (DependencyTree *) domain;
        if (tree->can_resolve(access))
            return domain;
    }

    /* create a new domain */
    DependencyDomain * domain = new DependencyTree(access->host_view.ld, access->host_view.sizeof_type);
    dom->deps.push_back(domain);

    return domain;
}
