/* ************************************************************************** */
/*                                                                            */
/*   memory-invalidate.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/20 03:22:22 by Romain PEREIRA            \_)     (_/    */
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

# pragma message(TODO "This interface definition is fucked: deallocating all device memory is not safe here if there is multiple threads submitting tasks to the device")
extern "C"
void
xkrt_coherency_reset(xkrt_runtime_t * runtime)
{
    LOGGER_DEBUG("Invalidate XKBlas devices memory");

    // remove all memory controllers of the current task
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    task_dom_info_t * dom = TASK_DOM_INFO(thread->current_task);
    assert(dom);

    for (MemoryCoherencyController * mem : dom->mems)
        delete mem;

    dom->mems.clear();

    // deallocate all device memory
    xkrt_memory_deallocate_all(runtime);
}
