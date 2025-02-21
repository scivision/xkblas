/* ************************************************************************** */
/*                                                                            */
/*   memory-distribute.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 17:29:22 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/driver/thread-producer.hpp>

static task_format_id_t TASK_FORMAT_DISTRIBUTE_CYCLIC_2D_ASYNC;

extern "C"
void
xkrt_memory_distribute_cyclic_2D_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t mb, size_t nb,
    size_t sizeof_type
) {
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    xkrt_device_global_id_t device_global_id = 0;

    const size_t mt = NUM_OF_TILES(m, mb);
    const size_t nt = NUM_OF_TILES(n, nb);

    for (size_t tm = 0; tm < mt; ++tm)
    {
        size_t bs_m = (tm == mt-1) ? (m-tm*mb) : mb;
        for (size_t tn = 0; tn < nt; ++tn)
        {
            size_t bs_n = (tn == nt-1) ? (n-tn*nb) : nb;

            const uint64_t task_size = sizeof(Task);
            uint8_t * mem = thread->allocate(task_size);
            assert(mem);

            Task * task = reinterpret_cast<Task *>  (mem + 0);
            new(task) Task(TASK_FORMAT_DISTRIBUTE_CYCLIC_2D_ASYNC, UNSPECIFIED_TASK_ACCESS, device_global_id);

            # define NACCESSES 1
            static_assert(NACCESSES <= TASK_MAX_ACCESSES);
            new(task->accesses + 0) Access(order, ptr, ld, tm*mb, tn*nb, bs_m, bs_n, sizeof_type, ACCESS_MODE_R);
            thread->resolve<NACCESSES>(task);
            # undef NACCESSES

            #ifndef NDEBUG
            snprintf(task->label, sizeof(task->label), "distribute_cyclic_2D_async");
            #endif /* NDEBUG */

            runtime->task_commit(task);

            device_global_id = (xkrt_device_global_id_t) ((device_global_id + 1) % runtime->drivers.devices.n);
        }
    }
}

//////////////////////////
// REGISTER TASK FORMAT //
//////////////////////////

void
xkrt_memory_distribute_async_register_format(xkrt_runtime_t * runtime)
{
    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        snprintf(format.label, sizeof(format.label), "distribute-cyclic-2D");
        TASK_FORMAT_DISTRIBUTE_CYCLIC_2D_ASYNC = task_format_create(&(runtime->task_formats), &format);
    }
}
