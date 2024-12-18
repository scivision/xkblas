/* ************************************************************************** */
/*                                                                            */
/*   memory-invalidate.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "runtime.h"
# include "device/thread-producer.hpp"

extern "C"
int
ptr_memory_invalidate_caches(void)
{
    LOGGER_INFO("Invalidate XKBlas devices memory");

    ptr_runtime_t * context = ptr_runtime_get();
    assert(context);

    // memory tree and device memory
    for (MemoryTree * memtree : context->memtrees)
        memtree->invalidate();

    # pragma message(TODO "deallocating threads memory causes error: why ?")
    # if 0

    // coherent worker
    ThreadWorker * worker = context->memory_coherent_worker_thread;
    assert(worker);
    worker->deallocate_all();

    // producer incoming thread
    ThreadProducer * producer = ThreadProducer::self();
    assert(producer);
    producer->deallocate_all();

    # endif

    return 0;
}
