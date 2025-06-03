/* ************************************************************************** */
/*                                                                            */
/*   coherent.cc                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/07 22:00:23 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 19:15:02 by Romain PEREIRA         / _______ \       */
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
# include <xkrt/memory/alignedas.h>
# include <xkrt/memory/access/blas/region/memory-tree.hpp>
# include <xkrt/memory/access/blas/region/dependency-tree.hpp>

using fetch_list_t = KMemoryTree<2>::fetch_list_t;
using fetch_t      = KMemoryTree<2>::fetch_t;

// args for 'runtime->coherent_async'
typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    xkrt_runtime_t * runtime;
    std::atomic<int> counter;

    args_t(xkrt_runtime_t * runtime) : runtime(runtime), counter(0) {}
    ~args_t() {}
}                                       args_t;

//////////////////////
// Memory coherency //
//////////////////////

//  How 'xkrt_memory_coherent_async' works
//      - create one successor Yi task per conflicting tasks Xi - to be executed on the helper thread
//      - when Xi complete, it makes Yi ready
//      - When Yi executes,
//          - it find all fetches to do (i.e. which rect on which device) - there should be only one rect on one device at that point
//          - it creates a task Zi pushed into the device's thread queue
//      - Zi is scheduled on the device thread queue, and will launch the asynchronous fetch
//          - Yi completion is deferred to Zi completion

extern "C"
void
xkrt_coherency_host_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t sizeof_type
) {
    // LOGGER_IMPL("in `xkrt_memory_coherent_async` - uplo and memflag parameters not supported");

    # if 0
    // implementation with a single copy once all partites are ready

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);

    task_t * task = thread->allocate_task(task_size);
    new(task) task_t(TASK_FORMAT_NULL, flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "coherency-host-all");
    # endif /* NDEBUG */

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new(accesses + 0) access_t(task, order, ptr, ld, m, n, sizeof_type, ACCESS_MODE_R);
    thread->resolve<AC>(task, accesses);
    # undef AC

    runtime->task_commit(task);

    # else

    // against memory-tree, dep-tree does not know where the data will be once the predecessor task executed.
    // For instance, two continuous partites may end-up being coherent on two different devices, thus cannot be merged
    // Therefore, this impl creates 1 copy per partite (it is not even trivial that merging can improve perf.)

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);
    assert(thread->current_task);

    /* create an access, and retrieve all dependency tree nodes that are in conflict */
    access_t access(NULL, order, ptr, ld, m, n, sizeof_type, ACCESS_MODE_R);
    DependencyDomain * domain = task_get_dependency_domain(thread->current_task, &access);

    std::vector<void *> conflicts;
    ((DependencyTree *) domain)->conflicting(&conflicts, &access);

    LOGGER_DEBUG("`xkrt_memory_coherent_async` found %zu conflicts", conflicts.size());

    /* create one task per conflict responsible to fetch the node */
    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT | TASK_FLAG_DEVICE;
    constexpr size_t args_size = sizeof(args_t);
    constexpr size_t task_size = task_compute_size(flags, AC);

    /* for each node of the dep tree conflicting */
    for (void * & conflict : conflicts)
    {
        /* retrieve the node */
        DependencyTree::Node * node = (DependencyTree::Node *) conflict;
        access_t * write = node->last_write;
        assert(write);
        assert(access.host_view.ld          == write->host_view.ld);
        assert(access.host_view.sizeof_type == write->host_view.sizeof_type);

        /* allocate a task with 1 access */
        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(TASK_FORMAT_NULL, flags);

        task_dev_info_t * dev = TASK_DEV_INFO(task);
        new (dev) task_dev_info_t(HOST_DEVICE_GLOBAL_ID, UNSPECIFIED_TASK_ACCESS);

        args_t * args = (args_t *) TASK_ARGS(task, task_size);
        new (args) args_t(runtime);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        #ifndef NDEBUG
        strncpy(task->label, "xkrt_memory_coherent_async", sizeof(task->label));
        #endif /* NDEBUG */

        access_t * accesses = TASK_ACCESSES(task, flags);
        assert(accesses);

        /* as 'conflicts' are forming a partition of 'access', it must only
         * intersects with a single rects of 'access' : find which of the two */
        bool found = false;
        for (int i = 0 ; i < 2 ; ++i)
        {
            access_t::Hyperrect h;
            access_t::Hyperrect::intersection(&h, access.hyperrects[i], node->hyperrect);

            if (!h.is_empty())
            {
                new (accesses + 0) access_t(task, MATRIX_COLMAJOR, h, access.host_view.ld, access.host_view.sizeof_type, ACCESS_MODE_R);
                __access_precedes(write, accesses + 0);
                found = true;
                break ;
            }
        }
        /* assert to check that we did find a rect from 'access' that intersects with the node */
        assert(found);

        // insert for future tasks dependencies
        domain->put<AC>(accesses);

        // commit the task
        runtime->task_commit(task);
    }

    # undef AC

    # endif /* single copy vs one per partite */
}

/* Allocate incoherent memory replicates onto the passed device */
extern "C"
void
xkrt_coherency_allocate_2D(
    xkrt_runtime_t * runtime,
    xkrt_device_global_id_t device_global_id,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t sizeof_type
) {
    LOGGER_DEBUG("`xkrt_memory_map_alloc` to device %u", device_global_id);

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);
    assert(thread->current_task);

    /* create an access to insert in the memory tree */
    access_t access(NULL, order, ptr, ld, m, n, sizeof_type, ACCESS_MODE_V);
    MemoryTree * memtree = (MemoryTree *) task_get_memory_controller(runtime, thread->current_task, &access);
    memtree->allocate_to_device(&access, device_global_id);
}
