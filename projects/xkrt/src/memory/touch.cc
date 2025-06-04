/* ************************************************************************** */
/*                                                                            */
/*   touch.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/07 14:28:00 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/04 03:10:29 by Romain PEREIRA         / _______ \       */
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

static void
body_memory_touch_async(task_t * task)
{
    assert(task);

    memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task);
    assert(args->runtime);

    // maybe test residency with `mincore`
    // https://man7.org/linux/man-pages/man2/mincore.2.html


        const unsigned char * start = ((const unsigned char *) ptr) + (i+0) * chunk_size;
        const unsigned char *   end = ((const unsigned char *) ptr) + (i+1) * chunk_size;

    // volatile to trick the compiler and avoid optimization of *p = *p
    volatile unsigned char * a = (volatile unsigned char *) args->start;
       const unsigned char * b = (   const unsigned char *) args->end;
         const size_t pagesize = (size_t) getpagesize();
    for ( ; a < b ; a += pagesize)
        *a = *a;

    // no need to requeue ever, because if a page couldnt be
    // touched, it means it is being pinned or copied-to, so
    // its being touched by someone else anyway

}

// TODO: that code is super ugly, split for each op to improve readability
static inline int
__memory_async(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {


int
xkrt_runtime_t::memory_touch_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();

    // if pinning/unpinning, create a fake dependency to serialize, as the cuda
    // driver serializes anyway, to avoid blocking team's threads unnecessarily
    const task_format_id_t fmtid = runtime->formats.memory_touch_async;
    assert(fmtid);

    constexpr int                         ac = 1;
    constexpr task_flag_bitfield_t     flags = TASK_FLAG_DEPENDENT;
    constexpr               size_t task_size = task_compute_size(flags, ac);
    constexpr               size_t args_size = sizeof(memory_touch_async_args_t);

    for (int i = 0 ; i < n ; ++i)
    {
        // inserts the interval in the tree to ensure they exist
        const unsigned char * start = ((const unsigned char *) ptr) + (i+0) * chunk_size;
        const unsigned char *   end = ((const unsigned char *) ptr) + (i+1) * chunk_size;
        const Interval interval((uintptr_t) start, (uintptr_t) end);

        // create a task that will touch/pin/unpin the memory
        task_t * task = tls->allocate_task(task_size + args_size);
        new(task) task_t(fmtid, flags);

        #ifndef NDEBUG
        task_format_t * format = task_format_get(&runtime->formats.list, fmtid);
        snprintf(task->label, sizeof(task->label), "%s", format->label);
        #endif

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(ac);

        // TODO : make it commutative, and favor the one with transfering tasks successors
        access_t * accesses = TASK_ACCESSES(task, flags);
        new(accesses + 0) access_t(task, interval, ACCESS_MODE_W, ACCESS_CONCURRENCY_COMMUTATIVE);

        memory_async_args_t * args = (memory_async_args_t *) TASK_ARGS(task, task_size);
        new (args) memory_async_args_t(runtime, start, end);

        tls->commit(task, xkrt_team_task_enqueue, runtime, team);
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
