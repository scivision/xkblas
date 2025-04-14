/* ************************************************************************** */
/*                                                                            */
/*   memory-replicate.cc                                                      */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/11 16:40:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

extern "C"
void
xkrt_coherency_replicate_2D_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t sizeof_type
) {
    assert(runtime->drivers.devices.n >= 2);

    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    for (xkrt_device_global_id_t device_global_id = 1 ; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
    {
        # define AC 1
        constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT | TASK_FLAG_DEVICE;
        constexpr size_t task_size = task_compute_size(flags, AC);

        task_t * task = thread->allocate_task(task_size);
        new(task) task_t(TASK_FORMAT_NULL, flags);

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(AC);

        task_dev_info_t * dev = TASK_DEV_INFO(task);
        new (dev) task_dev_info_t(device_global_id, UNSPECIFIED_TASK_ACCESS);

        access_t * accesses = TASK_ACCESSES(task);
        new(accesses + 0) access_t(task, order, ptr, ld, m, n, sizeof_type, ACCESS_MODE_R);

        thread->resolve<AC>(task, accesses);
        # undef AC

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "replicate_cyclic_2d_async");
        #endif /* NDEBUG */

        runtime->task_commit(task);
    }
}
