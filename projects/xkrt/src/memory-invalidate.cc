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
# include <xkrt/driver/thread-producer.hpp>

static inline void
xkrt_memory_deallocate_all(xkrt_runtime_t * runtime)
{
    for (xkrt_device_global_id_t device_global_id = 0 ;
            device_global_id < runtime->drivers.devices.n ;
            ++device_global_id)
    {
        xkrt_device_t * device = runtime->device_get(device_global_id);
        assert(device);

        // device memory
        device->memory_reset();

        // worker thread memory
        ThreadWorker * worker = device->thread;
        assert(worker);
        worker->deallocate_all();
    }

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

extern "C"
void
xkrt_coherency_reset(xkrt_runtime_t * runtime)
{
    LOGGER_INFO("Invalidate XKBlas devices memory");

    // memory tree and device memory
    for (MemoryCoherencyController * memcontroller : runtime->memcontrollers)
        memcontroller->invalidate();

    xkrt_memory_deallocate_all(runtime);
}
