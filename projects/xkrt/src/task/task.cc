/* ************************************************************************** */
/*                                                                            */
/*   task.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/04/03 04:44:23 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/05 02:22:49 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/memory/access/blas/dependency-tree.hpp>
# include <xkrt/memory/access/blas/memory-tree.hpp>
# include <xkrt/memory/access/interval/dependency-tree.hpp>
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
    assert(access->type >= 0 && access->type < ACCESS_TYPE_MAX);

    task_dom_info_t * dom = TASK_DOM_INFO(task);
    assert(dom);

    /* if not found, create a new memory coherency controller dependending on
     * the access type */
    MemoryCoherencyController * mcc;
    switch (access->type)
    {
        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            // TODO : have this list dynamically switch to a hashmap ift here
            // is too many differnet matrices LD

            /* find previous mcc for that ld */
            for (MemoryCoherencyController * mcc : dom->mccs.blas)
            {
                BLASMemoryTree * memtree = (BLASMemoryTree *) mcc;
                if (memtree->ld == access->host_view.ld &&
                        memtree->sizeof_type == access->host_view.sizeof_type)
                    return mcc;
            }

            /* else insert a new one */
            mcc = new BLASMemoryTree(
                runtime,
                access->host_view.ld,
                access->host_view.sizeof_type,
                runtime->conf.merge_transfers
            );
            dom->mccs.blas.push_back(mcc);

            /* insert regions that represents registered memory segment, to
             * enforce the split in multiple copies */
            # if XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION
            if (runtime->conf.protect_registered_memory_overflow)
                for (const auto & [ptr, size] : runtime->registered_memory)
                    ((BLASMemoryTree *) mcc)->registered(ptr, size);
            # endif /* XKRT_MEMORY_REGISTER_OVERFLOW_PROTECTION */

            LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
                    access->host_view.ld, access->host_view.sizeof_type, runtime->conf.merge_transfers ? "true" : "false");

            break ;
        }

        case (ACCESS_TYPE_INTERVAL):
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
    assert(access->type >= 0 && access->type < ACCESS_TYPE_MAX);

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    task_dom_info_t * dom = TASK_DOM_INFO(thread->current_task);
    assert(dom);

    /* create a new domain */
    switch (access->type)
    {
        case (ACCESS_TYPE_BLAS_MATRIX):
        {
            // TODO
            /* find previous deptree for that ld */
           for (DependencyDomain * domain : dom->deps.blas)
           {
                BLASDependencyTree * deptree = (BLASDependencyTree *) domain;
                if (deptree->ld == access->host_view.ld &&
                    deptree->sizeof_type == access->host_view.sizeof_type)
                {
                    return deptree;
                }
            }

           DependencyDomain * deptree = new BLASDependencyTree(access->host_view.ld, access->host_view.sizeof_type);
           dom->deps.blas.push_back(deptree);
           return deptree;
        }

        case (ACCESS_TYPE_INTERVAL):
        {
            if (dom->deps.interval == NULL)
                dom->deps.interval = new IntervalDependencyTree();
            return dom->deps.interval;
        }

        case (ACCESS_TYPE_POINT):
        {
            if (dom->deps.point == NULL)
                dom->deps.point = new DependencyMap();
            return dom->deps.point;
        }

        default:
            LOGGER_FATAL("Tried to run a dependency domain on an unsupported access");
    }
}
