/* ************************************************************************** */
/*                                                                            */
/*   memory-coherent.cc                                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/10 23:00:20 by Romain PEREIRA            \_)     (_/    */
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
    access_t access;
    Cube cubes[4];
    std::atomic<int> counter;

    args_t(xkrt_runtime_t * runtime, task_t * task, const access_t & x, const access_t & y) :
        runtime(runtime), access(task, MATRIX_COLMAJOR, NULL, x.host_view.ld, 0, 0, 0, 0, x.host_view.sizeof_type, ACCESS_MODE_V), counter(0)
    {
        assert(x.host_view.ld          == y.host_view.ld);
        assert(x.host_view.sizeof_type == y.host_view.sizeof_type);
        access_t::Cube::intersection(this->cubes + 0, x.cubes[0], y.cubes[0]);
        access_t::Cube::intersection(this->cubes + 1, x.cubes[0], y.cubes[1]);
        access_t::Cube::intersection(this->cubes + 2, x.cubes[1], y.cubes[0]);
        access_t::Cube::intersection(this->cubes + 3, x.cubes[1], y.cubes[1]);
    }
    ~args_t() {}
}                                       args_t;

//////////////////////////
// COHERENT FETCH TASKS //
//////////////////////////

// args for 'runtime->coherent_async'
typedef struct alignas(CACHE_LINE_SIZE) args_fetch_t
{
    /* the runtime */
    xkrt_runtime_t * runtime;

    /* the memory coherent async thread that scheduled the 'parent' task */
    Thread * thread;

    /* the parent task that launched the fetches on each devices */
    task_t * parent;

    /* the fetch to perform */
    fetch_list_t * list;
    uint32_t fetch_idx;

    args_fetch_t(
        xkrt_runtime_t * runtime,
        Thread * w,
        task_t * p,
        fetch_list_t * l,
        uint32_t i
    ) :
        runtime(runtime),
        thread(w),
        parent(p),
        list(l),
        fetch_idx(i)
    {}

    ~args_fetch_t() {}

}                                       args_fetch_t;

static void
body_memory_coherent_async_fetch_callback(
    const void * args[XKRT_CALLBACK_ARGS_MAX]
) {
    // self
    Thread * self = Thread::self();
    assert(self);

    // unpack stuff
    xkrt_runtime_t * runtime = (xkrt_runtime_t *) args[0];
    assert(runtime);

    Thread * thread = (Thread *) args[1];
    assert(thread);

    // self is a device thread, thread is the asynchronous coherent copy thread
    assert(self != thread);

    task_t * parent = (task_t *) args[2];
    assert(parent);

    // one fetched completed, notify the parent
    args_t * parent_args = (args_t *) TASK_ARGS(parent);
    if (parent_args->counter.fetch_sub(1, std::memory_order_seq_cst) == 1)
        runtime->task_detachable_post(parent);

    fetch_list_t * list = (fetch_list_t *) args[3];
    assert(list);

    if (list->fetched() == 0)
        free(list);
}

static void
body_memory_coherent_async_fetch(task_t * task)
{
    const args_fetch_t * args = (args_fetch_t *) TASK_ARGS(task);
    assert(args);

    xkrt_runtime_t * runtime = args->runtime;
    assert(runtime);

    const Thread * thread = args->thread;
    assert(thread);

    const task_t * parent = args->parent;
    assert(parent);

    const fetch_list_t * list = args->list;
    assert(list);
    assert(args->fetch_idx < list->n);
    assert(args->fetch_idx < list->capacity);

    const fetch_t * fetch = list->fetches + args->fetch_idx;
    assert(fetch);

    // thread is the memory async thread, self is the device thread
    assert(thread != Thread::self());

    // submit fetch - with a callback doing parent->fetched() on completion
    static_assert(XKRT_CALLBACK_ARGS_MAX >= 4);
    xkrt_callback_t callback;
    callback.func    = body_memory_coherent_async_fetch_callback;
    callback.args[0] = runtime;
    callback.args[1] = thread;
    callback.args[2] = parent;
    callback.args[3] = list;

    /* launch asynchronous copy */
    memory_replicate_view_t host_replicate_view(fetch->host_view.begin_addr(), fetch->host_view.ld);
    runtime->copy(
        fetch->src_device_global_id,
        fetch->host_view,
        HOST_DEVICE_GLOBAL_ID,
        host_replicate_view,
        fetch->src_device_global_id,
        fetch->src_view,
        callback
    );
}

/////////////////////////
// COHERENT ASYNC TASK //
/////////////////////////

static void
body_memory_coherent_async(task_t * current)
{
    args_t * args = (args_t *) TASK_ARGS(current);
    assert(args);

    xkrt_runtime_t * runtime = args->runtime;
    assert(runtime);

    Thread * thread = Thread::self();
    assert(thread);

    // this is ugly, result of the memorytree bad design, fix me
    MemoryTree * memtree = (MemoryTree *) runtime->get_or_insert_memory_controller(args->access.host_view.ld, args->access.host_view.sizeof_type);
    assert(memtree);

    fetch_list_t * list = memtree->fetch_list_to_host_from_cubes<4>(args->cubes);
    assert(list);

    // avoid early completion
    args->counter.fetch_add(list->n + 1, std::memory_order_seq_cst);

    // launch each fetch
    for (uint32_t i = 0 ; i < list->n ; ++i)
    {
        fetch_t * fetch = list->fetches + i;
        assert(fetch->src_device_global_id != HOST_DEVICE_GLOBAL_ID);
        assert(fetch->dst_device_global_id == HOST_DEVICE_GLOBAL_ID);

        constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE;
        const size_t task_size = task_compute_size(flags, 0);
        const size_t args_size = sizeof(args_fetch_t);

        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(runtime->formats.coherent_async_fetch, flags);

        task_dev_info_t * dev = TASK_DEV_INFO(task);
        new (dev) task_dev_info_t(fetch->src_device_global_id, UNSPECIFIED_TASK_ACCESS);

        args_fetch_t * args = (args_fetch_t *) TASK_ARGS(task, task_size);
        new(args) args_fetch_t(runtime, thread, current, list, i);

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "coherent_fetch(%p)", list);
        #endif /* NDEBUG */

        runtime->task_commit(task);
    }

    // if early-completion happened
    if (args->counter.fetch_sub(1, std::memory_order_seq_cst) == 1)
        runtime->task_detachable_post(current);
}

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

    /* create one task per conflict shrinking the access (stored in cubes)
     * responsible of fetching the chunk */
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, 0);
    constexpr size_t args_size = sizeof(args_t);

    for (access_t * & conflict : conflicts)
    {
        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(runtime->formats.coherent_async, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(0);

        /* link the virtual access to the task, so it is activated correctly
         * when predecessors completes.  This access is virtual though, no need
         * to fetch */
        args_t * args = (args_t *) TASK_ARGS(task, task_size);
        new (args) args_t(runtime, task, access, *conflict);

        #ifndef NDEBUG
        strncpy(task->label, "xkrt_memory_coherent_async", sizeof(task->label));
        #endif /* NDEBUG */

        deptree->precedence(conflict, &args->access);
        runtime->task_commit(task);
    }
    # endif
}

//////////////////////////
// REGISTER TASK FORMAT //
//////////////////////////

void
xkrt_memory_coherent_async_register_format(xkrt_runtime_t * runtime)
{
    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        snprintf(format.label, sizeof(format.label), "coherent");
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_coherent_async;
        runtime->formats.coherent_async = task_format_create(&(runtime->formats.list), &format);
    }

    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_coherent_async_fetch;
        snprintf(format.label, sizeof(format.label), "coherent_fetch");
        runtime->formats.coherent_async_fetch = task_format_create(&(runtime->formats.list), &format);
    }
}
