/* ************************************************************************** */
/*                                                                            */
/*   memory-distribute.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/27 22:36:28 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/driver/thread-producer.hpp>

extern "C"
void
xkrt_coherency_distribute_cyclic_2D_halo_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t mb, size_t nb,
    size_t sizeof_type,
    size_t hx, size_t hy
) {
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    xkrt_device_global_id_t device_global_id = 0;

    // If there is only 1 device, just send one big access - which allows a
    // single buffer on the device with a LD large enough for all future tasks
    // However, means any tasks on a subdomain cannot start until all the data had been transfered...
    // What do we want here ?

    const size_t mt = NUM_OF_TILES(m, mb);
    const size_t nt = NUM_OF_TILES(n, nb);

    for (size_t tm = 0; tm < mt; ++tm)
    {
        for (size_t tn = 0; tn < nt; ++tn)
        {
            const uint64_t task_size = sizeof(Task);
            uint8_t * mem = thread->allocate(task_size);
            assert(mem);

            Task * task = reinterpret_cast<Task *>  (mem + 0);
            new(task) Task(TASK_FORMAT_NULL, UNSPECIFIED_TASK_ACCESS, device_global_id);

            # define NACCESS 1
            static_assert(NACCESS <= TASK_MAX_ACCESSES);
            {
                const ssize_t  x = tm * mb;
                const ssize_t  y = tn * nb;
                const ssize_t x0 = MAX(x-(ssize_t)hx, 0);
                const ssize_t y0 = MAX(y-(ssize_t)hy, 0);
                const ssize_t x1 = MIN(x+mb+hx, m);
                const ssize_t y1 = MIN(y+nb+hy, n);
                const  size_t sx = x1 - x0;
                const  size_t sy = y1 - y0;
                new(task->accesses + 0) Access(order, ptr, ld, x0, y0, sx, sy, sizeof_type, ACCESS_MODE_R);
            }
            thread->resolve<NACCESS>(task);
            # undef NACCESS

            #ifndef NDEBUG
            snprintf(task->label, sizeof(task->label), "distribute_cyclic_2d_async");
            #endif /* NDEBUG */

            runtime->task_commit(task);

            device_global_id = (xkrt_device_global_id_t) ((device_global_id + 1) % runtime->drivers.devices.n);
        }
    }
}

extern "C"
void
xkrt_coherency_distribute_cyclic_2D_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t mb, size_t nb,
    size_t sizeof_type
){
    xkrt_coherency_distribute_cyclic_2D_halo_async(runtime, order, ptr, ld, m, n, mb, nb, sizeof_type, 0, 0);
}

extern "C"
void
xkrt_coherency_distribute_packed_2D_halo_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t sizeof_type,
    size_t hx, size_t hy
) {
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    // if power of two
    if (__builtin_popcount(runtime->drivers.devices.n) == 1)
    {
        size_t fm = 1;
        size_t fn = 1;
        while (fm * fn != runtime->drivers.devices.n)
        {
            if (fm < fn)
                fm *= 2;
            else
                fn *= 2;
        }

        const size_t mb = m / fm;
        const size_t nb = n / fn;

        xkrt_coherency_distribute_cyclic_2D_halo_async(
            runtime,
            order,
            ptr, ld,
             m,  n,
            mb, nb,
            sizeof_type,
            hx, hy
        );
        # if 0
        const uint64_t task_size = sizeof(Task);
        uint8_t * mem = thread->allocate(task_size);
        assert(mem);

        Task * task = reinterpret_cast<Task *>  (mem + 0);
        const xkrt_device_global_id_t device_global_id = 0;
        new(task) Task(TASK_FORMAT_NULL, UNSPECIFIED_TASK_ACCESS, device_global_id);

        # define NACCESS 1
        static_assert(NACCESS <= TASK_MAX_ACCESSES);
        {
            new(task->accesses + 0) Access(order, ptr, ld, 0, 0, m, n, sizeof_type, ACCESS_MODE_R);
        }
        thread->resolve<NACCESS>(task);
        # undef NACCESS

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "distribute_packed_2d_async");
        #endif /* NDEBUG */

        runtime->task_commit(task);
        # endif
    }
    else
    {
        LOGGER_FATAL("Not implemented");
    }
}

extern "C"
void
xkrt_coherency_distribute_packed_2D_async(
    xkrt_runtime_t * runtime,
    matrix_order_t order,
    void * ptr, size_t ld,
    size_t m, size_t n,
    size_t sizeof_type
) {
    return xkrt_coherency_distribute_packed_2D_halo_async(runtime, order, ptr, ld, m, n, sizeof_type, 0, 0);
}
