/* ************************************************************************** */
/*                                                                            */
/*   memory-invalidate.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 02:03:06 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

static inline void
xkrt_memory_deallocate_all(
    xkrt_runtime_t * runtime
) {
    for (xkrt_device_global_id_t device_global_id = 0 ;
            device_global_id < runtime->drivers.devices.n ;
            ++device_global_id)
    {
        xkrt_device_t * device = runtime->device_get(device_global_id);
        assert(device);

        // device memory
        device->memory_reset();

        // thread thread memory
        uint8_t nthreads = device->nthreads.load(std::memory_order_acq_rel);
        for (uint8_t i = 0 ; i < nthreads ; ++i)
        {
            xkrt_thread_t * thread = device->threads[i];
            assert(thread);
            thread->deallocate_all_tasks();
        }
    }
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
