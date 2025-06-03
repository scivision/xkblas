/* ************************************************************************** */
/*                                                                            */
/*   register.cc                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/07 14:28:00 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 22:55:18 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
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

template<MemoryRegisterTree::Op op>
static void
body_memory_async(task_t * task)
{
    assert(task);

    memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task);
    assert(args->runtime);

    const Interval interval((uintptr_t) args->start, (uintptr_t) args->end);
    std::vector<MemoryRegisterBlock> blocks;    // TODO: cache this to avoid dynamic alloc
    args->runtime->memory_register_tree.run(interval, &blocks, op);
    if (blocks.size())
    {
        switch (op)
        {
            case (MemoryRegisterTree::Op::TOUCHING):
            {
                for (MemoryRegisterBlock & block : blocks)
                {
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

                    // mark touched
                    args->runtime->memory_register_tree.run(block.interval, NULL, MemoryRegisterTree::Op::TOUCHED);
                }
                return ;
            }

            case (MemoryRegisterTree::Op::PINNING):
            {
                for (MemoryRegisterBlock & block : blocks)
                {
                    assert( block.state.pinning);
                    assert(!block.state.pinned);

                    // get segment to pin
                    void  * ptr = (void *) block.interval.a;
                    size_t size = (size_t) (block.interval.b - block.interval.a);
                    xkrt_memory_register(args->runtime, ptr, size);
                    LOGGER_DEBUG("Registered %p of size %zu", ptr, size);

                    // TODO : this will trigger another search, if thats a perf
                    // bottleneck, then maybe cache the node in the
                    // `MemoryRegisterBlock`

                    // mark pinned
                    args->runtime->memory_register_tree.run(block.interval, NULL, MemoryRegisterTree::Op::PINNED);
                }
                return ;
            }

            case (MemoryRegisterTree::Op::UNPINNING):
            {
                for (MemoryRegisterBlock & block : blocks)
                {
                    assert(!block.state.pinning);
                    assert( block.state.pinned);
                    assert( block.state.unpinning);

                    // get segment to pin
                    void  * ptr = (void *) block.interval.a;
                    size_t size = (size_t) (block.interval.b - block.interval.a);
                    xkrt_memory_unregister(args->runtime, ptr, size);
                    LOGGER_DEBUG("Unregistered %p of size %zu", ptr, size);

                    // TODO : this will trigger another search, if thats a perf
                    // bottleneck, then maybe cache the node in the
                    // `MemoryRegisterBlock`

                    // mark pinned
                    args->runtime->memory_register_tree.run(block.interval, NULL, MemoryRegisterTree::Op::UNPINNED);
                }
                return ;
            }

            default:
                LOGGER_FATAL("Not implemented");

        } /* switch op */
    }
}

// TODO: that code is super ugly, split for each op to improve readability
template<MemoryRegisterTree::Op op>
static inline int
__memory_async(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();

    // if pinning/unpinning, create a fake dependency to serialize, as the cuda
    // driver serializes anyway, to avoid blocking team's threads unnecessarily
    const task_format_id_t fmtid  = (op == MemoryRegisterTree::Op::TOUCHING)  ? runtime->formats.memory_touch_async :
                                    (op == MemoryRegisterTree::Op::PINNING)   ? runtime->formats.memory_pin_async   :
                                    (op == MemoryRegisterTree::Op::UNPINNING) ? runtime->formats.memory_unpin_async :
                                     0;
    assert(fmtid);

    constexpr int                         ac = (op == MemoryRegisterTree::Op::TOUCHING) ?              0 :                   1;
    constexpr task_flag_bitfield_t     flags = (op == MemoryRegisterTree::Op::TOUCHING) ? TASK_FLAG_ZERO : TASK_FLAG_DEPENDENT;
    constexpr               size_t task_size = task_compute_size(flags, ac);
    constexpr               size_t args_size = sizeof(memory_async_args_t);

    for (int i = 0 ; i < n ; ++i)
    {
        // inserts the interval in the tree to ensure they exist
        const unsigned char * start = ((const unsigned char *) ptr) + (i+0) * chunk_size;
        const unsigned char *   end = ((const unsigned char *) ptr) + (i+1) * chunk_size;
        const Interval interval((uintptr_t) start, (uintptr_t) end);
        runtime->memory_register_tree.ensure(interval);

        // create a task that will touch/pin/unpin the memory
        task_t * task = tls->allocate_task(task_size + args_size);
        new(task) task_t(fmtid, flags);

        #ifndef NDEBUG
        task_format_t * format = task_format_get(&runtime->formats.list, fmtid);
        snprintf(task->label, sizeof(task->label), "%s", format->label);
        #endif

        if constexpr (ac == 1)
        {
            task_dep_info_t * dep = TASK_DEP_INFO(task);
            new (dep) task_dep_info_t(ac);

            // TODO : make it commutative, and favor the one with transfering tasks successors
            access_t * accesses = TASK_ACCESSES(task, flags);
            new(accesses + 0) access_t(task, ACCESS_MODE_W);

            if (tls->last_register_memory_access)
                __access_precedes(tls->last_register_memory_access, accesses);

            tls->last_register_memory_access = accesses;
        }

        memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task, task_size);
        new (args) memory_async_args_t(runtime, start, end);

        tls->commit(task, xkrt_team_task_enqueue, runtime, team);
    }

    return 0;
}

int
xkrt_runtime_t::memory_touch_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    return __memory_async<MemoryRegisterTree::Op::TOUCHING>(this, team, ptr, chunk_size, n);
}

int
xkrt_runtime_t::memory_register_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    return __memory_async<MemoryRegisterTree::Op::PINNING>(this, team, ptr, chunk_size, n);
}

int
xkrt_runtime_t::memory_unregister_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    return __memory_async<MemoryRegisterTree::Op::UNPINNING>(this, team, ptr, chunk_size, n);
}

typedef struct  memory_transfer_async_args_t
{
    xkrt_runtime_t                * runtime;
    const xkrt_device_global_id_t   device_global_id;
    const uintptr_t                 device_addr;
    const uintptr_t                 host_addr;
    const size_t                    size;
    const bool                      h2d;
    std::atomic<int>                ref;

    memory_transfer_async_args_t(
        xkrt_runtime_t                * runtime,
        const xkrt_device_global_id_t   device_global_id,
        const uintptr_t                 device_addr,
        const uintptr_t                 host_addr,
        const size_t                    size,
        const bool                      h2d
    ) :
        runtime(runtime),
        device_global_id(device_global_id),
        device_addr(device_addr),
        host_addr(host_addr),
        size(size),
        h2d(h2d),
        ref(0)
    {}

}               memory_transfer_async_args_t;

// TODO:
//  - create a task that write on the segment
//  - update touching/registering/unregistering so that it creates tasks that read on the segment
//  - implement commutative accesses so register tasks are prioritize depending on the transfers to come

static inline void
__memory_transfer_async_block(
    memory_transfer_async_args_t * args,
    MemoryRegisterBlock          & block
) {
    // get host segment
    const Interval & interval = block.interval;
    uintptr_t segment_ptr  = (uintptr_t) interval.a;
       size_t segment_size = (size_t) (interval.b - interval.a);
    assert(args->host_addr <= segment_ptr);
    assert(segment_ptr <= args->host_addr + args->size);
    assert(segment_ptr + segment_size < args->host_addr + args->size);

    LOGGER_DEBUG("Transfering %lu of size %zu", segment_ptr, segment_size);

    const uintptr_t offset = segment_ptr - args->host_addr;

    // get src/dst
    const xkrt_device_global_id_t dst_device_global_id = args->h2d ? args->device_global_id     : HOST_DEVICE_GLOBAL_ID;
    const uintptr_t               dst_device_mem       = args->h2d ? args->device_addr + offset : segment_ptr;
    const xkrt_device_global_id_t src_device_global_id = args->h2d ? HOST_DEVICE_GLOBAL_ID      : args->device_global_id;
    const uintptr_t               src_device_mem       = args->h2d ? segment_ptr                : args->device_addr + offset;

    # if 0
    // spawn the task - that eventually depend on if currently pinning etc...
    xkrt_memory_copy_async(
        args->runtime,
        args->device_global_id,
              dst_device_global_id,
              dst_device_mem,
              src_device_global_id,
              src_device_mem,
              segment_size
    );
    # endif

    // TODO : this will trigger another search, if thats a perf
    // bottleneck, then maybe cache the node in the
    // `MemoryRegisterBlock`

    // mark pinned in the callback
    args->runtime->memory_register_tree.run(interval, NULL, MemoryRegisterTree::Op::TRANSFERED);
}

int
xkrt_runtime_t::memory_transfer_async(
    const xkrt_device_global_id_t   device_global_id,
    const uintptr_t                 device_addr,
    const uintptr_t                 host_addr,
    const size_t                    size,
    const bool                      h2d
) {
    const Interval interval(host_addr, host_addr + size);
    memory_transfer_async_args_t * args = (memory_transfer_async_args_t *) malloc(sizeof(memory_transfer_async_args_t));
    assert(args);
    new (args) memory_transfer_async_args_t(this, device_global_id, device_addr, host_addr, size, h2d);

    this->memory_register_tree.transfer(interval, __memory_transfer_async_block, args);

    if (args->ref.fetch_sub(1, std::memory_order_relaxed) == 1)
        free(args);

    return 0;
}

template<MemoryRegisterTree::Op op>
static void
__memory_async_register_format(
    xkrt_runtime_t * runtime,
    task_format_id_t * format_id,
    const char * label
) {
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_async<op>;
    snprintf(format.label, sizeof(format.label), "%s", label);
    *format_id = task_format_create(&(runtime->formats.list), &format);
}

void
xkrt_memory_async_register_format(xkrt_runtime_t * runtime)
{
    __memory_async_register_format<MemoryRegisterTree::Op::TOUCHING>(
            runtime, &runtime->formats.memory_touch_async, "memory_touch_async");

    __memory_async_register_format<MemoryRegisterTree::Op::PINNING>(
            runtime, &runtime->formats.memory_pin_async, "memory_pin_async");

    __memory_async_register_format<MemoryRegisterTree::Op::UNPINNING>(
            runtime, &runtime->formats.memory_unpin_async, "memory_unpin_async");

    __memory_async_register_format<MemoryRegisterTree::Op::TRANSFERING>(
            runtime, &runtime->formats.memory_transfer_async, "memory_transfer_async");
}
