/* ************************************************************************** */
/*                                                                            */
/*   touch.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/05/23 14:58:24 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/28 05:35:27 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

typedef struct  memory_touch_async_args_t
{
    xkrt_runtime_t * runtime;
    const unsigned char * start;
    const unsigned char *   end;

    memory_touch_async_args_t(
        xkrt_runtime_t * r,
        const unsigned char * s,
        const unsigned char * e
    ) : runtime(r), start(s), end(e) {}

    ~memory_touch_async_args_t() {}

}               memory_touch_async_args_t;

static void
body_memory_touch_async(task_t * task)
{
    assert(task);

    memory_touch_async_args_t * args = (memory_touch_async_args_t *) TASK_ARGS(task);
    assert(args->runtime);

    const Interval intervals[1] = { Interval((uintptr_t) args->start, (uintptr_t) args->end) };
    const Hypercube h(intervals);

    args->runtime->memory_register_tree.run(h, blocks, MemoryRegisterTree::Op::TOUCHING);
    if (blocks.size())
    {
        const size_t pagesize = (size_t) getpagesize();
        for (MemoryBlock & block : blocks)
        {
            // maybe test residency with `mincore`
            // https://man7.org/linux/man-pages/man2/mincore.2.html

            // volatile to trick the compiler and avoid optimization of *p = *p
            volatile unsigned char * a = (volatile unsigned char *) block.h[0].a;
               const unsigned char * b = (   const unsigned char *) block.h[0].b;
            for ( ; a < b ; a += pagesize)
                *a = *a;
        }
        args->runtime->memory_register_tree.run(h, blocks, MemoryRegisterTree::Op::TOUCHED);
    }
}

int
xkrt_runtime_t::memory_touch_async(
    void * ptr,
    const size_t chunk_size,
    int nchunks
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    for (int i = 0 ; i < nchunks ; ++i)
    {
        // inserts the interval in the tree to ensure they exist
        const unsigned char * start = ((const unsigned char *) ptr) + (i+0) * chunk_size;
        const unsigned char *   end = ((const unsigned char *) ptr) + (i+1) * chunk_size;
        const Interval intervals[1] = { Interval((uintptr_t) start, (uintptr_t) end) };
        const Hypercube h(intervals);
        runtime->memory_register_tree.ensure(h);

        // create a task that will touch the memory
        constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
        constexpr size_t task_size = task_compute_size(flags, 0);
        constexpr size_t args_size = sizeof(memory_touch_async_args_t);

        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(this->formats.memory_touch_async, flags);

        memory_touch_async_args_t * args = (memory_touch_async_args_t *) TASK_ARGS(task, task_size);
        new (args) memory_touch_async_args_t(runtime, start, end);

        thread->commit(task, xkrt_team_thread_task_enqueue, this, thread->team, thread);
    }
    return 0;
}

void
xkrt_memory_touch_async_register_format(xkrt_runtime_t * runtime)
{
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_touch_async;
    snprintf(format.label, sizeof(format.label), "memory_touch_async");
    runtime->formats.memory_touch_async = task_format_create(&(runtime->formats.list), &format);
}
