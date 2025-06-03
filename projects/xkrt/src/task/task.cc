/* ************************************************************************** */
/*                                                                            */
/*   task.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/04/03 04:44:23 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:58:19 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/memory/access/blas/region/dependency-tree.hpp>
# include <xkrt/memory/access/blas/region/memory-tree.hpp>
# include <xkrt/memory/access/point/dependency-map.hpp>

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

    SPINLOCK_LOCK(dom->mccs_lock);

    /* find previous mcc for that ld */
    for (MemoryCoherencyController * mcc : dom->mccs)
    {
        if (mcc->can_resolve(access))
        {
            SPINLOCK_UNLOCK(dom->mccs_lock);
            return mcc;
        }
    }

    LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
            access->host_view.ld, access->host_view.sizeof_type, runtime->conf.merge_transfers ? "true" : "false");

    /* if not found, create a new memory coherency controller dependending on
     * the access type */
    MemoryCoherencyController * mcc;
    switch (access->type)
    {
        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            mcc = new MemoryTree(
                runtime,
                access->host_view.ld,
                access->host_view.sizeof_type,
                runtime->conf.merge_transfers
            );
            break ;
        }

        case (ACCESS_TYPE_POINT):
        {
            mcc = NULL;
            break ;
        }

        default:
        {
            LOGGER_FATAL("Tried to run coherency controller on an unsupported access");
            break ;
        }
    }

    if (mcc)
    {
        assert(mcc->can_resolve(access));
        dom->mccs.push_back(mcc);
    }

    SPINLOCK_UNLOCK(dom->mccs_lock);

    return mcc;
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
        if (domain->can_resolve(access))
            return domain;

    /* create a new domain */
    DependencyDomain * domain;
    switch (access->type)
    {
        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            domain = new DependencyTree(access->host_view.ld, access->host_view.sizeof_type);
            break ;
        }

        case (ACCESS_TYPE_POINT):
        {
            domain = new DependencyMap();
            break ;
        }

        default:
        {
            LOGGER_FATAL("Tried to run a dependency domain on an unsupported access");
        }
    }
    dom->deps.push_back(domain);

    return domain;
}
