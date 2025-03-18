/* ************************************************************************** */
/*                                                                            */
/*   memory-coherent.cc                                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/18 23:07:32 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/memory-tree.hpp>
# include <xkrt/runtime.h>
# include <xkrt/driver/thread.hpp>
# include <xkrt/memory/alignedas.h>

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
//          - it find all fetches to do (i.e. which cube on which device) - there should be only one cube on one device at that point
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
    Thread * thread = Thread::self();

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
    new(accesses + 0) access_t(task, order, ptr, ld, 0, 0, m, n, sizeof_type, ACCESS_MODE_R);
    thread->resolve<AC>(task, accesses);
    # undef AC

    runtime->task_commit(task);

    # else
    // implementation with 1 copy per partite, launched as soon as they are ready

    // TODO : allocate instead on the thread thread ? creates a concurrency issue in the allocator though
    Thread * thread = Thread::self();
    assert(thread);

    /* create an access, and retrieve all tasks that are in conflict */
    std::vector<access_t *> conflicts;
    access_t access(NULL, order, ptr, ld, 0, 0, m, n, sizeof_type, ACCESS_MODE_R);

    DependencyDomain * domain = thread->get_dependency_domain(&access);
    assert(domain);

    DependencyTree * deptree = (DependencyTree *) domain;
    deptree->conflicting(&conflicts, &access);

    LOGGER_DEBUG("`xkrt_memory_coherent_async` found %zu conflicts", conflicts.size());

    /* create one task per conflict responsible to fetch that chunk */
    # define AC 4
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT | TASK_FLAG_DEVICE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t);

    for (access_t * & conflict : conflicts)
    {
        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(TASK_FORMAT_NULL, flags);

        #ifndef NDEBUG
        strncpy(task->label, "xkrt_memory_coherent_async", sizeof(task->label));
        #endif /* NDEBUG */

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        task_dev_info_t * dev = TASK_DEV_INFO(task);
        new (dev) task_dev_info_t(HOST_DEVICE_GLOBAL_ID, UNSPECIFIED_TASK_ACCESS);

        args_t * args = (args_t *) TASK_ARGS(task, task_size);
        new (args) args_t(runtime);

        // setup the two accesses resulsting from the intersection of 'access' and 'conflict'
        access_t * accesses = TASK_ACCESSES(task, flags);

        // compute the 4 potential cubes
        assert(access.host_view.ld          == conflict->host_view.ld);
        assert(access.host_view.sizeof_type == conflict->host_view.sizeof_type);
        for (int i = 0 ; i < 2 ; ++i)
        {
            for (int j = 0 ; j < 2 ; ++j)
            {
                Cube cube;
                access_t::Cube::intersection(&cube, access.cubes[i], conflict->cubes[j]);
                const int k = i * 2 + j;
                new (accesses + k) access_t(task, MATRIX_COLMAJOR, cube, access.host_view.ld, access.host_view.sizeof_type, ACCESS_MODE_R);
                if (!cube.is_empty())
                    deptree->precedence(conflict, accesses + i);
            }
        }

        // insert for future tasks dependencies
        thread->insert<4>(task, accesses);

        // commit the task
        runtime->task_commit(task);
    }
    # undef AC

    # endif
}
