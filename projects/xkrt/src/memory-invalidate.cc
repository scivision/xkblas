/* ************************************************************************** */
/*                                                                            */
/*   memory-invalidate.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 02:54:26 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/driver/thread.hpp>

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

        // thread thread memory
        Thread * thread = device->thread;
        assert(thread);
        thread->deallocate_all_tasks();
    }

    # pragma message(TODO "deallocating threads memory causes error: why ?")
    # if 0

    // coherent thread
    Thread * thread = runtime->memory_coherent_thread_thread;
    assert(thread);
    thread->deallocate_all();

    // producer incoming thread
    Thread * producer = Thread::self();
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
