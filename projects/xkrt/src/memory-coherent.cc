/* ************************************************************************** */
/*                                                                            */
/*   memory-coherent.cc                                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:23:14 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/memory-tree.hpp>
# include <xkrt/runtime.h>
# include <xkrt/driver/thread-producer.hpp>
# include <xkrt/memory/alignedas.h>

using fetch_list_t = KMemoryTree<2>::fetch_list_t;
using fetch_t      = KMemoryTree<2>::fetch_t;

//////////////////////////
// COHERENT FETCH TASKS //
//////////////////////////

// generate when executing a 'TASK_FORMAT_COHERENT_ASYNC'
static task_format_id_t TASK_FORMAT_COHERENT_ASYNC_FETCH;

// args for 'TASK_FORMAT_COHERENT_ASYNC'
typedef struct alignas(CACHE_LINE_SIZE) args_fetch_t
{
    /* the runtime */
    xkrt_runtime_t * runtime;

    /* the memory coherent async thread that scheduled the 'parent' task */
    ThreadWorker * worker;

    /* the parent task that launched the fetches on each devices */
    Task * parent;

    /* the fetch to perform */
    fetch_list_t * list;
    uint32_t fetch_idx;

    args_fetch_t(
        xkrt_runtime_t * runtime,
        ThreadWorker * w,
        Task * p,
        fetch_list_t * l,
        uint32_t i
    ) :
        runtime(runtime),
        worker(w),
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
    ThreadWorker * self = ThreadWorker::self();
    assert(self);

    // unpack stuff
    ThreadWorker * worker = (ThreadWorker *) args[0];
    assert(worker);

    // self is a device thread, worker is the asynchronous coherent copy thread
    assert(self != worker);

    Task * parent = (Task *) args[1];
    assert(parent);

    // one fetched completed, notify the parent
    if (parent->fetched() == TASK_STATE_DATA_FETCHED)
        worker->complete(parent);

    fetch_list_t * list = (fetch_list_t *) args[2];
    assert(list);

    if (list->fetched() == 0)
        free(list);
}

static void
body_memory_coherent_async_fetch(void * vlauncher)
{
    // unpack stuff
    const task_launcher_t * launcher = (task_launcher_t *) vlauncher;
    assert(launcher);

    const Task * task = launcher->task;
    assert(task);

    const args_fetch_t * args = (args_fetch_t *) (task + 1);
    assert(args);

    xkrt_runtime_t * runtime = args->runtime;
    assert(runtime);

    const ThreadWorker * worker = args->worker;
    assert(worker);

    const Task * parent = args->parent;
    assert(parent);

    const fetch_list_t * list = args->list;
    assert(list);
    assert(args->fetch_idx < list->n);
    assert(args->fetch_idx < list->capacity);

    const fetch_t * fetch = list->fetches + args->fetch_idx;
    assert(fetch);

    // worker is the memory async thread, self is the device thread
    assert(worker != ThreadWorker::self());

    // submit fetch - with a callback doing parent->fetched() on completion
    static_assert(XKRT_CALLBACK_ARGS_MAX >= 1);
    xkrt_callback_t callback;
    callback.func    = body_memory_coherent_async_fetch_callback;
    callback.args[0] = worker;
    callback.args[1] = parent;
    callback.args[2] = list;

    xkrt_device_t * device = runtime->device_get(fetch->src_device_global_id);
    assert(device);
    assert(fetch->src_device_global_id == device->global_id);

    // the current task must be executing on the device's thread for that fetch
    assert(device->thread == ThreadWorker::self());

    /* launch asynchronous copy */
    memory_replicate_view_t host_replicate_view(fetch->host_view.begin_addr(), fetch->host_view.ld);
    xkrt_device_stream_instruction_submit_copy(
        device,
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

// generated when executing a memory_cohernet_async
static task_format_id_t TASK_FORMAT_COHERENT_ASYNC;

// args for 'TASK_FORMAT_COHERENT_ASYNC'
typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    Cube cubes[4];
    const size_t ld;
    const size_t sizeof_type;

    args_t(const Access & x, const Access & y) : ld(x.host_view.ld), sizeof_type(x.host_view.sizeof_type)
    {
        assert(x.host_view.ld == y.host_view.ld);
        assert(x.host_view.sizeof_type == y.host_view.sizeof_type);
        Access::Cube::intersection(this->cubes + 0, x.cubes[0], y.cubes[0]);
        Access::Cube::intersection(this->cubes + 1, x.cubes[0], y.cubes[1]);
        Access::Cube::intersection(this->cubes + 2, x.cubes[1], y.cubes[0]);
        Access::Cube::intersection(this->cubes + 3, x.cubes[1], y.cubes[1]);
    }
    ~args_t() {}
}                                       args_t;

static void
xkrt_memory_coherent_async_worker_thread_work(
    xkrt_runtime_t * runtime,
    ThreadWorker * thread,
    Task * current
) {
    assert(runtime);
    assert(thread);
    assert(thread == ThreadWorker::self());
    assert(thread == runtime->memory_coherent_worker_thread);
    assert(current);
    assert(current->wc == 0);
    assert(current->state.value == TASK_STATE_READY);

    LOGGER_DEBUG("Creating a coherent async fetch task");

    const args_t * args = (const args_t *) (current + 1);
    assert(args);

    // this is ugly, result of the memorytree bad design, fix me
    MemoryTree * memtree = (MemoryTree *) runtime->get_or_insert_memory_controller(args->ld, args->sizeof_type);
    assert(memtree);

    fetch_list_t * list = memtree->fetch_list_to_host_from_cubes<4>(args->cubes);
    assert(list);

    // avoid early completion
    current->fetching();

    // launch each fetch
    ThreadProducer * producer = ThreadProducer::self();
    assert(producer);

    for (uint32_t i = 0 ; i < list->n ; ++i)
    {
        current->fetching();

        fetch_t * fetch = list->fetches + i;
        assert(fetch->dst_device_global_id == HOST_DEVICE_GLOBAL_ID);

        const uint64_t task_size = sizeof(Task);
        const uint64_t args_size = sizeof(args_fetch_t);
        assert(is_alignedas(task_size, CACHE_LINE_SIZE));
        assert(is_alignedas(args_size, CACHE_LINE_SIZE));

        uint8_t * mem = thread->allocate(task_size + args_size);
        assert(mem);

        Task * task = reinterpret_cast<Task *>  (mem + 0);
        new(task) Task(TASK_FORMAT_COHERENT_ASYNC_FETCH, UNSPECIFIED_TASK_ACCESS, fetch->src_device_global_id);

        args_fetch_t  * args = reinterpret_cast<args_fetch_t *>(mem + task_size);
        new(args) args_fetch_t(runtime, thread, current, list, i);

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "coherent_fetch(%p)", list);
        #endif /* NDEBUG */

        producer->resolve<0>(task);
        if (producer->commit(task) == TASK_STATE_READY)
            xkrt_runtime_submit_task(runtime, task);
    }

    // if early-completion happened
    if (current->fetched() == TASK_STATE_DATA_FETCHED)
        thread->complete(current);
}

/////////////////////////////
// HELPER THREAD MAIN LOOP //
/////////////////////////////

static void
xkrt_memory_coherent_async_worker_thread_main_loop(xkrt_runtime_t * runtime)
{
    ThreadWorker * thread = ThreadWorker::self();
    assert(thread == runtime->memory_coherent_worker_thread);

    // TODO : instead, while runtime->running
    while (1)
    {
        Task * task;
        while ((task = thread->pop()) == NULL)
            thread->pause();

        assert(task->fmtid == TASK_FORMAT_COHERENT_ASYNC);
        xkrt_memory_coherent_async_worker_thread_work(runtime, thread, task);
    }
}

static void *
xkrt_memory_coherent_async_worker_thread_main(void * arg)
{
    xkrt_runtime_t * runtime = (xkrt_runtime_t *) arg;
    assert(runtime);

    runtime->memory_coherent_worker_thread = ThreadWorker::self();

    unsigned int cpu, node;
    getcpu(&cpu, &node);
    LOGGER_INFO("Starting thread for async host copy on cpu %d of node %d", cpu, node);

    /* infinite loop with the device runtime */
    xkrt_memory_coherent_async_worker_thread_main_loop(runtime);

    return NULL;
}

////////////////////////
// HELPER THREAD INIT //
////////////////////////

void
xkrt_memory_coherent_async_worker_thread_init(xkrt_runtime_t * runtime)
{
    runtime->memory_coherent_worker_thread = NULL;

    pthread_t thread;
    int err = pthread_create(&thread, NULL, xkrt_memory_coherent_async_worker_thread_main, runtime);
    if (err)
        LOGGER_FATAL("Could not create thread for async host copy");

    // TODO : likely need a volatile here
    while (runtime->memory_coherent_worker_thread == NULL)
        mem_pause();
}

//////////////////////
// Memory coherency //
//////////////////////

# pragma message(TODO "'xkrt_memory_coherent_async' should take a row/col major parameter")

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
xkrt_memory_coherent_async(
    xkrt_runtime_t * runtime,
    int uplo, int memflag,
    int m, int n,
    void * xkrt, int ld,
    unsigned int sizeof_type
) {
    LOGGER_IMPL("in `xkrt_memory_coherent_async` - uplo and memflag parameters not supported");

    // TODO : allocate instead on the worker thread ? creates a concurrency issue in the allocator though
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    /* create an access, and retrieve all tasks that are in conflict */
    std::unordered_map<Task *, std::array<bool, TASK_MAX_ACCESSES>> conflicts;
    const Access access(MATRIX_COLMAJOR, xkrt, ld, 0, 0, m, n, sizeof_type, ACCESS_MODE_R);

    DependencyTree * deptree = thread->get_dependency_tree_for_ld(ld);
    assert(deptree);
    deptree->conflicting(&conflicts, &access);

    LOGGER_DEBUG("`xkrt_memory_coherent_async` found %zu conflicts", conflicts.size());

    /* create one task per conflict shrinking the access, responsible of fetching the chunk */
    const uint64_t task_size = sizeof(Task);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));

    for (const auto & [conflicting_task, conflicting_accesses] : conflicts)
    {
        assert(conflicting_task);

        const uint64_t task_size = sizeof(Task);
        const uint64_t args_size = sizeof(args_t);
        assert(is_alignedas(task_size, CACHE_LINE_SIZE));
        assert(is_alignedas(args_size, CACHE_LINE_SIZE));

        uint8_t * mem = thread->allocate(task_size + args_size);
        assert(mem);

        for (int access_id = 0 ; access_id < TASK_MAX_ACCESSES ; ++access_id)
        {
            if (conflicting_accesses[access_id])
            {
                Task * task = reinterpret_cast<Task *>(mem);
                new(task) Task(TASK_FORMAT_COHERENT_ASYNC, UNSPECIFIED_TASK_ACCESS, HOST_DEVICE_GLOBAL_ID);
                deptree->precedence(conflicting_task, task);

                args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
                new (args) args_t(access, conflicting_task->accesses[access_id]);

                #ifndef NDEBUG
                strncpy(task->label, "xkrt_memory_coherent_async", sizeof(task->label));
                #endif /* NDEBUG */

                if (thread->commit(task) == TASK_STATE_READY)
                    xkrt_runtime_submit_task(runtime, task);
            }
        }
    }
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
        // no body, body is implicit, see the main loop
        TASK_FORMAT_COHERENT_ASYNC = task_format_create(&(runtime->task_formats), &format);
    }

    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = body_memory_coherent_async_fetch;
        snprintf(format.label, sizeof(format.label), "coherent_fetch");
        TASK_FORMAT_COHERENT_ASYNC_FETCH = task_format_create(&(runtime->task_formats), &format);
    }
}
