/* ************************************************************************** */
/*                                                                            */
/*   register.cc                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/05/23 14:58:24 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/06/01 04:27:09 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

typedef struct  memory_async_args_t
{
    xkrt_runtime_t * runtime;
    const unsigned char * start;
    const unsigned char *   end;

    memory_async_args_t(
        xkrt_runtime_t * r,
        const unsigned char * s,
        const unsigned char * e
    ) : runtime(r), start(s), end(e) {}

    ~memory_async_args_t() {}

}               memory_async_args_t;

template<MemoryRegisterTree::Op op1, MemoryRegisterTree::Op op2>
static void
body_memory_async(task_t * task)
{
    assert(task);

    memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task);
    assert(args->runtime);

    const Interval interval((uintptr_t) args->start, (uintptr_t) args->end);
    std::vector<MemoryRegisterBlock> blocks;    // TODO: cache this to avoid dynamic alloc
    args->runtime->memory_register_tree.run(interval, &blocks, op1);
    if (blocks.size())
    {
        for (MemoryRegisterBlock & block : blocks)
        {
            switch (op1)
            {
                case (MemoryRegisterTree::Op::TOUCHING):
                {
                    assert( block.state.touching);
                    assert(!block.state.touched);
                    assert(!block.state.pinning);
                    assert(!block.state.pinned);

                    // maybe test residency with `mincore`
                    // https://man7.org/linux/man-pages/man2/mincore.2.html

                    // volatile to trick the compiler and avoid optimization of *p = *p
                    volatile unsigned char * a = (volatile unsigned char *) block.interval.a;
                       const unsigned char * b = (   const unsigned char *) block.interval.b;
                       const size_t pagesize = (size_t) getpagesize();
                    for ( ; a < b ; a += pagesize)
                        *a = *a;

                    // no need to requeue ever, because if a page couldnt be
                    // touched, it means it is being pinned or copied-to, so
                    // its being touched by someone else anyway
                    break ;
                }

                case (MemoryRegisterTree::Op::PINNING):
                {
                }

                default:
                    LOGGER_FATAL("Not implemented");
            } /* switch op */
            args->runtime->memory_register_tree.run(block.interval, NULL, op2);
        } /* foreach block */
    }
}

static inline int
__memory_async(
    xkrt_runtime_t * runtime,
    void * ptr,
    const size_t chunk_size,
    int n,
    task_format_id_t format
) {
    xkrt_driver_t * driver = runtime->driver_get(XKRT_DRIVER_TYPE_HOST);
    assert(driver);

    xkrt_team_t * team = &driver->team;

    xkrt_thread_t * tls = xkrt_thread_t::get_tls();

    for (int i = 0 ; i < n ; ++i)
    {
        // inserts the interval in the tree to ensure they exist
        const unsigned char * start = ((const unsigned char *) ptr) + (i+0) * chunk_size;
        const unsigned char *   end = ((const unsigned char *) ptr) + (i+1) * chunk_size;
        const Interval interval((uintptr_t) start, (uintptr_t) end);
        runtime->memory_register_tree.ensure(interval);

        // create a task that will touch the memory
        constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
        constexpr size_t task_size = task_compute_size(flags, 0);
        constexpr size_t args_size = sizeof(memory_async_args_t);

        task_t * task = tls->allocate_task(task_size + args_size);
        new(task) task_t(format, flags);

        memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task, task_size);
        new (args) memory_async_args_t(runtime, start, end);

        tls->commit(task, xkrt_team_thread_task_enqueue, runtime, team, team->priv.threads + 0);
    }
    return 0;
}

int
xkrt_runtime_t::memory_touch_async(
    void * ptr,
    const size_t chunk_size,
    int n
) {
    return __memory_async(this, ptr, chunk_size, n, this->formats.memory_touch_async);
}

int
xkrt_runtime_t::memory_register_async(
    void * ptr,
    const size_t chunk_size,
    int n
) {
    return __memory_async(this, ptr, chunk_size, n, this->formats.memory_pin_async);
}

int
xkrt_runtime_t::memory_unregister_async(
    void * ptr,
    const size_t chunk_size,
    int n
) {
    return __memory_async(this, ptr, chunk_size, n, this->formats.memory_unpin_async);
}

template<MemoryRegisterTree::Op op1, MemoryRegisterTree::Op op2>
static void
__memory_async_register_format(
    xkrt_runtime_t * runtime,
    task_format_id_t * format_id,
    const char * label
) {
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_async<op1, op2>;
    snprintf(format.label, sizeof(format.label), label);
    *format_id = task_format_create(&(runtime->formats.list), &format);
}

void
xkrt_memory_async_register_format(xkrt_runtime_t * runtime)
{
    __memory_async_register_format<MemoryRegisterTree::Op::TOUCHING, MemoryRegisterTree::Op::TOUCHED>(
            runtime, &runtime->formats.memory_touch_async, "memory_touch_async");

    __memory_async_register_format<MemoryRegisterTree::Op::PINNING, MemoryRegisterTree::Op::PINNED>(
            runtime, &runtime->formats.memory_pin_async, "memory_pin_async");

    __memory_async_register_format<MemoryRegisterTree::Op::UNPINNING, MemoryRegisterTree::Op::UNPINNED>(
            runtime, &runtime->formats.memory_unpin_async, "memory_unpin_async");
}
