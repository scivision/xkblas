# include "xkblas-context.h"
# include "device/thread-producer.hpp"
# include "sync/alignedas.h"

static void
xkblas_memory_coherent_async_worker_thread_main_loop(xkblas_context_t * context)
{
    ThreadWorker * thread = ThreadWorker::get();
    assert(thread == context->memory_coherent_worker_thread);

    // TODO : instead, while context->running
    while (1)
    {
        // If there is no tasks and streams are empty, sleep the thread
        Task * task;
        while ((task = thread->pop()) == NULL)
            thread->pause();

        assert(task);
        assert(task->wc == 0);
        assert(task->state.value == TASK_STATE_READY);

        if (context->memtree.fetch_on_host(thread, task) == TASK_STATE_DATA_FETCHED)
            thread->complete(task);
    }
}

static void *
xkblas_memory_coherent_async_worker_thread_main(void * arg)
{
    xkblas_context_t * context = (xkblas_context_t *) arg;
    assert(context);

    context->memory_coherent_worker_thread = ThreadWorker::get();

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

void
xkblas_memory_coherent_async_impl(
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
    ThreadProducer * thread = ThreadProducer::get();
    assert(thread);

    const uint64_t task_size = sizeof(Task);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    uint8_t * mem = thread->allocate(task_size);
    assert(mem);

    // TODO : use task format writeback instead
    Task * task = reinterpret_cast<Task *>(mem);
    new(task) Task(TASK_FORMAT_NULL, TASK_MAX_ACCESSES, HOST_DEVICE_GLOBAL_ID);
    new(task->accesses + 0) Access(ptr, ld, 0, 0, m, n, sizeof_type, ACCESS_MODE_R);

    #ifndef NDEBUG
    strncpy(task->label, "xkblas_memory_coherent_async", sizeof(task->label));
    #endif /* NDEBUG */

    thread->commit<1>(ctx, task);
}

