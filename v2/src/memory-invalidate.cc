# include "xkblas-context.h"
# include "device/thread-producer.hpp"

extern "C"
int
xkblas_memory_invalidate_caches(void)
{
    XKBLAS_INFO("Invalidate XKBlas devices memory");

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    // memory tree and device memory
    for (MemoryTree * memtree : context->memtrees)
        memtree->invalidate();

    // coherent worker
    ThreadWorker * worker = context->memory_coherent_worker_thread;
    assert(worker);
    worker->deallocate_all();

    // producer incoming thread
    ThreadProducer * producer = ThreadProducer::self();
    assert(producer);
    producer->deallocate_all();

    return 0;
}
