/* ************************************************************************** */
/*                                                                            */
/*   register.cc                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/07 14:28:00 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/04 03:22:14 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>

constexpr int                         ac = 2;
constexpr task_flag_bitfield_t     flags = TASK_FLAG_DEPENDENT;
constexpr               size_t task_size = task_compute_size(flags, ac);
constexpr               size_t args_size = sizeof(memory_register_async_args_t);

typedef struct  memory_async_args_t
{
    xkrt_runtime_t * runtime;
    memory_async_args_t(xkrt_runtime_t * r) : runtime(r) {}
    ~memory_async_args_t() {}

}               memory_async_args_t;

static void
body_memory_async(task_t * task)
{
    assert(task);

    memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task);
    assert(args->runtime);

    access_t * accesses = TASK_ACCESSES(task, flags);
    const Interval & interval = (accesses + 0)->segment;

    // get segment to pin
    void  * ptr = (void *) args->start;
    size_t size = (size_t) (args->end - args->start);
    xkrt_memory_register(args->runtime, ptr, size);

    LOGGER_WARN("TODO: MARKED PINNED");
    // mark pinned
    // args->runtime->memory_register_tree.run(block.interval, NULL, MemoryRegisterTree::Op::PINNED);
}

int
xkrt_runtime_t::memory_register_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();

    // if pinning/unpinning, create a fake dependency to serialize, as the cuda
    // driver serializes anyway, to avoid blocking team's threads unnecessarily
    const task_format_id_t fmtid = runtime->formats.memory_register_async;
    assert(fmtid);

    for (int i = 0 ; i < n ; ++i)
    {
        // inserts the interval in the tree to ensure they exist
        const unsigned char * start = ((const unsigned char *) ptr) + (i+0) * chunk_size;
        const unsigned char *   end = ((const unsigned char *) ptr) + (i+1) * chunk_size;
        const Interval interval((uintptr_t) start, (uintptr_t) end);

        // create a task that will register/pin/unpin the memory
        task_t * task = tls->allocate_task(task_size + args_size);
        new(task) task_t(fmtid, flags);

        #ifndef NDEBUG
        task_format_t * format = task_format_get(&runtime->formats.list, fmtid);
        snprintf(task->label, sizeof(task->label), "%s", format->label);
        #endif

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(ac);

        access_t * accesses = TASK_ACCESSES(task, flags);
        new(accesses + 0) access_t(task, interval, ACCESS_MODE_W, ACCESS_CONCURRENCY_COMMUTATIVE);

        access_t * accesses = TASK_ACCESSES(task, flags);
        new(accesses + 1) access_t(task, ACCESS_MODE_W, ACCESS_CONCURRENCY_COMMUTATIVE);u

        memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task, task_size);
        new (args) memory_async_args_t(runtime, start, end);

        // serialize register/unregister tasks
        access_t * last = tls->last_register_memory_access.exchange(accesses + 1);
        if (last)
            __access_precedes(last, accesses + 1);

        tls->commit(task, xkrt_team_task_enqueue, runtime, team);
    }

    return 0;
}

void
xkrt_memory_async_register_format(xkrt_runtime_t * runtime)
{
    {
        task_format_t format;
        memset(format.f, 0, sizeof(format.f));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_register_async;
        snprintf(format.label, sizeof(format.label), "memory_register_async");
        runtime->formats.memory_register_async = task_format_create(&(runtime->formats.list), &format);
    }
}
