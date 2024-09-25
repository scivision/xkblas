# include "xkblas-context.h"
# include "device/thread-producer.hpp"
# include "sync/alignedas.h"

using fetch_list_t = KMemoryTree<2>::fetch_list_t;
using fetch_t      = KMemoryTree<2>::fetch_t;

//////////////////
// TASKS FORMAT //
//////////////////

// generated when the user request a memory coherency (no body, no args)
static task_format_id_t TASK_FORMAT_COHERENT_ASYNC;

// generated when executing a 'TASK_FORMAT_COHERENT_ASYNC' - children tasks
// doing the actual fetch on each device
static task_format_id_t TASK_FORMAT_COHERENT_ASYNC_FETCH;

// args for 'TASK_FORMAT_COHERENT_ASYNC_FETCH'
typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    /* the memory coherent async thread that scheduled the 'parent' task */
    ThreadWorker * worker;

    /* the parent task that launched the fetches on each devices */
    Task * parent;

    /* the fetch to perform */
    fetch_t * fetch;

    args_t(ThreadWorker * w, Task * p, fetch_t * f) : worker(w), parent(p), fetch(f) {}
    ~args_t() {}

}                                       args_t;

static void
body_memory_coherent_async_fetch_callback(
    const void * args[XKBLAS_CALLBACK_ARGS_MAX]
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

    Task * child = (Task *) args[2];
    assert(child);

   // one fetched completed, notify the parent
    if (parent->fetched() == TASK_STATE_DATA_FETCHED)
    {
        worker->complete(parent);
        XKBLAS_INFO("Coherent async completed");
    }
}

static void
body_memory_coherent_async_fetch(void * vlauncher)
{
    // unpack stuff
    const task_launcher_t * launcher = (task_launcher_t *) vlauncher;
    assert(launcher);

    const args_t * args = (args_t *) (launcher->task + 1);
    assert(args);

    const ThreadWorker * worker = args->worker;
    assert(worker);

    const Task * parent = args->parent;
    assert(parent);

    const fetch_t * fetch = args->fetch;
    assert(fetch);

    const Task * task = launcher->task;
    assert(task);

    // worker is the memory async thread, self is the device thread
    assert(worker != ThreadWorker::self());

    // TODO : submit fetch - with a callback doing parent->fetched() on completion
    static_assert(XKBLAS_CALLBACK_ARGS_MAX >= 4);
    xkblas_callback_t callback;
    callback.func    = body_memory_coherent_async_fetch_callback;
    callback.args[0] = worker;
    callback.args[1] = parent;
    callback.args[2] = task;
    callback.args[3] = fetch;

    # if !USE_CUDA
    XKBLAS_FATAL("Only supporting CUDA driver for D2H transfers");
    # endif

    # pragma message(TODO "Instead, get the driver or the func associated to the 'src' device")
    xkblas_driver_t * driver = xkblas_driver_get(XKBLAS_DRIVER_TYPE_CUDA);
    assert(driver);

    xkblas_device_t * device = xkblas_device_get(fetch->src_device_global_id);
    assert(device);
    assert(fetch->src_device_global_id == device->global_id);

    // the current task must be executing on the device's thread for that fetch
    assert(device->thread == ThreadWorker::self());

    /* launch asynchronous copy */
    memory_replicate_view_t host_replicate_view(fetch->host_view.begin_addr(), fetch->host_view.ld);
    xkblas_stream_instruction_submit_copy(
        driver,
        device,
        fetch->host_view,
        HOST_DEVICE_GLOBAL_ID,
        host_replicate_view,
        fetch->src_device_global_id,
        fetch->src_view,
        callback
    );
}

void
xkblas_memory_coherent_async_register_format(void)
{
    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        snprintf(format.label, sizeof(format.label), "coherent");
        // no body, body is implicit, see the main loop
        format.target = TASK_FORMAT_TARGET_HOST;
        TASK_FORMAT_COHERENT_ASYNC = task_format_create(&format);
    }

    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[XKBLAS_DRIVER_TYPE_CPU] = body_memory_coherent_async_fetch;
        snprintf(format.label, sizeof(format.label), "coherent_fetch");
        format.target = TASK_FORMAT_TARGET_HOST;
        TASK_FORMAT_COHERENT_ASYNC_FETCH = task_format_create(&format);
    }
}

/////////////////////////////
// HELPER THREAD MAIN LOOP //
/////////////////////////////

static void
xkblas_memory_coherent_async_worker_thread_work(
    xkblas_context_t * context,
    ThreadWorker * thread,
    Task * current
) {
    assert(current);
    assert(current->wc == 0);
    assert(current->state.value == TASK_STATE_READY);

    // find all the fetches to do
    // fetch_list_t * list = (fetch_list_t *) thread->allocate(sizeof(fetch_list_t));
    fetch_list_t * list = (fetch_list_t *) malloc(sizeof(fetch_list_t));
    assert(list);

    context->memtree.create_fetch_list_for_host(
        thread,
        current->accesses,
        current->naccesses,
        list
    );

    if (list->fetches == NULL)
        return thread->complete(current);

    // avoid early completion
    current->fetching();

    // foreach fetch, create a task with
    //  - no dependences, as we are already in acontext where we know the data
    //  is being read and valid)
    //  - a body performing the fetch (with a callback on task_executed +
    //  decrement fetch count on the current task)
    ThreadProducer * producer = ThreadProducer::self();
    for (fetch_t * fetch = list->fetches ; fetch != NULL ; fetch = fetch->next)
    {
        current->fetching();

        assert(thread);

        const uint64_t task_size = sizeof(Task);
        const uint64_t args_size = sizeof(args_t);
        assert(is_alignedas(task_size, CACHE_LINE_SIZE));
        assert(is_alignedas(args_size, CACHE_LINE_SIZE));

        // uint8_t * mem  = thread->allocate(task_size + args_size);
        uint8_t * mem  = (uint8_t *) malloc(task_size + args_size);
        assert(mem);

        Task * task = reinterpret_cast<Task *>  (mem + 0);
        new(task) Task(TASK_FORMAT_COHERENT_ASYNC_FETCH, UNSPECIFIED_TASK_ACCESS, fetch->src_device_global_id);

        args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
        new(args) args_t(thread, current, fetch);

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "coherent_fetch(%p)", fetch);
        #endif /* NDEBUG */

        producer->commit<0>(context, task);
    }

    // if early-completion happened
    if (current->fetched() == TASK_STATE_DATA_FETCHED)
        thread->complete(current);
}

static void
xkblas_memory_coherent_async_worker_thread_main_loop(xkblas_context_t * context)
{
    ThreadWorker * thread = ThreadWorker::self();
    assert(thread == context->memory_coherent_worker_thread);

    // TODO : instead, while context->running
    while (1)
    {
        Task * task;
        while ((task = thread->pop()) == NULL)
            thread->pause();

        assert(task->fmtid == TASK_FORMAT_COHERENT_ASYNC);
        xkblas_memory_coherent_async_worker_thread_work(context, thread, task);
    }
}

static void *
xkblas_memory_coherent_async_worker_thread_main(void * arg)
{
    xkblas_context_t * context = (xkblas_context_t *) arg;
    assert(context);

    context->memory_coherent_worker_thread = ThreadWorker::self();

    unsigned int cpu, node;
    getcpu(&cpu, &node);
    XKBLAS_INFO("Starting thread for async host copy on cpu %d of node %d", cpu, node);

    /* infinite loop with the device context */
    xkblas_memory_coherent_async_worker_thread_main_loop(context);

    return NULL;
}

void
xkblas_memory_coherent_async_worker_thread_init(xkblas_context_t * context)
{
    context->memory_coherent_worker_thread = NULL;

    pthread_t thread;
    int err = pthread_create(&thread, NULL, xkblas_memory_coherent_async_worker_thread_main, context);
    if (err)
        XKBLAS_FATAL("Could not create thread for async host copy");

    // TODO : likely need a volatile here
    while (context->memory_coherent_worker_thread == NULL)
        mem_pause();
}

//////////////////////
// Memory coherency //
//////////////////////

# pragma message(TODO "'xkblas_memory_coherent_async' should take a row/col major parameter")

extern "C"
void
xkblas_memory_coherent_async(
    int uplo, int memflag,
    int m, int n,
    void * ptr, int ld,
    unsigned int sizeof_type
) {
    XKBLAS_IMPL("in `xkblas_memory_coherent_async` - uplo and memflag parameters not supported");

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    /* create a task with a null body that reads the data, and force its scheduling onto the host */

    // TODO : allocate instead on the worker thread ? creates a concurrency issue in the allocator though
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    const uint64_t task_size = sizeof(Task);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    // uint8_t * mem = thread->allocate(task_size);
    uint8_t * mem = (uint8_t *) malloc(task_size);
    assert(mem);

    Task * task = reinterpret_cast<Task *>(mem);
    new(task) Task(TASK_FORMAT_COHERENT_ASYNC, TASK_MAX_ACCESSES, HOST_DEVICE_GLOBAL_ID);
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, ptr, ld, 0, 0, m, n, sizeof_type, ACCESS_MODE_R);

    #ifndef NDEBUG
    strncpy(task->label, "xkblas_memory_coherent_async", sizeof(task->label));
    #endif /* NDEBUG */

    thread->commit<1>(context, task);
}
