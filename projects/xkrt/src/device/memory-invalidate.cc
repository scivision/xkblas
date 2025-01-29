/* ************************************************************************** */
/*                                                                            */
/*   memory-invalidate.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:37:16 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/device/thread-producer.hpp>

extern "C"
void
xkrt_memory_invalidate(void)
{
    LOGGER_INFO("Invalidate XKBlas devices memory");

    xkrt_runtime_t * runtime = xkrt_runtime_get();
    assert(runtime);

    // memory tree and device memory
    for (MemoryTree * memtree : runtime->memtrees)
        memtree->invalidate();

    # pragma message(TODO "deallocating threads memory causes error: why ?")
    # if 0

    // coherent worker
    ThreadWorker * worker = runtime->memory_coherent_worker_thread;
    assert(worker);
    worker->deallocate_all();

    // producer incoming thread
    ThreadProducer * producer = ThreadProducer::self();
    assert(producer);
    producer->deallocate_all();

    # endif
}
