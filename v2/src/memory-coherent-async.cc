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
typedef struct  args_t
{
    // TODO
}               args_t;

void
xkblas_memory_coherent_async_register_format(void)
{
    {
        task_format_t format;
        snprintf(format.label, sizeof(format.label), "xkblas_memory_coherent_async");
        // no body, body is implicit, see the main loop
        TASK_FORMAT_COHERENT_ASYNC = task_format_create(&format);
    }

    {
        task_format_t format;
        snprintf(format.label, sizeof(format.label), "xkblas_memory_coherent_async_fetch");
        // TODO : add a body - as this is executed by a device thread
        TASK_FORMAT_COHERENT_ASYNC_FETCH = task_format_create(&format);
    }
}

/////////////////////////////
// HELPER THREAD MAIN LOOP //
/////////////////////////////

static void
xkblas_memory_coherent_async_worker_thread_body(
        const void * args[XKBLAS_STREAM_CALLBACK_ARGS_MAX]
) {
    ThreadWorker * worker = (ThreadWorker *) args[0];
    assert(worker);

    // worker is the memory async thread, self is the device thread
    assert(worker != ThreadWorker::self());

    Task * current = (Task *) args[1];
    assert(current);

    fetch_t * fetch = (fetch_t *) args[2];
    assert(fetch);

    // TODO : have the fetch task executed
    // xkblas_device_task_executed(
    // TODO : submit fetch

    if (current->fetched() == TASK_STATE_DATA_FETCHED)
        worker->complete(current);
}

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
    fetch_list_t * list = (fetch_list_t *) thread->allocate(sizeof(fetch_list_t));
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

        uint8_t * mem  = thread->allocate(task_size + args_size);
        assert(mem);

        Task * task = reinterpret_cast<Task *>  (mem + 0);
        new(task) Task(TASK_FORMAT_COHERENT_ASYNC_FETCH);

        args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
        // TODO : new(args) args_t(thread, current, fetch);

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "xkblas_memory_coherent_async(%p)", fetch);
        #endif /* NDEBUG */

        producer->commit<0>(context, task);

        current->fetched();
    }

    // completion
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
        // If there is no tasks and streams are empty, sleep the thread
        Task * task;
        while ((task = thread->pop()) == NULL)
        {
            thread->deallocate_all();
            thread->pause();
        }

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

extern "C"
void
xkblas_memory_coherent_async(
    int uplo, int memflag,
    int m, int n,
    void * ptr, int ld,
    unsigned int sizeof_type
) {
    XKBLAS_IMPL("in `xkblas_memory_coherent_async` - uplo and memflag parameters not supported");

    xkblas_context_t * ctx = xkblas_context_get();
    assert(ctx);

    /* create a task with a null body that reads the data, and force its scheduling onto the host */

    // TODO : allocate instead on the worker thread ? creates a concurrency issue in the allocator though
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    const uint64_t task_size = sizeof(Task);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    uint8_t * mem = thread->allocate(task_size);
    assert(mem);

    Task * task = reinterpret_cast<Task *>(mem);
    new(task) Task(TASK_FORMAT_COHERENT_ASYNC, TASK_MAX_ACCESSES, HOST_DEVICE_GLOBAL_ID);
    new(task->accesses + 0) Access(ptr, ld, 0, 0, m, n, sizeof_type, ACCESS_MODE_R);

    #ifndef NDEBUG
    strncpy(task->label, "xkblas_memory_coherent_async", sizeof(task->label));
    #endif /* NDEBUG */

    thread->commit<1>(ctx, task);
}
