/* ************************************************************************** */
/*                                                                            */
/*   touch.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/27 15:08:32 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:57:37 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

typedef struct  memory_touch_async_args_t
{
    xkrt_runtime_t * runtime;
    void * ptr;
    size_t chunk_size;
    int i;
}               memory_touch_async_args_t;

static void
body_memory_touch_async(task_t * task)
{
    assert(task);

    memory_touch_async_args_t * args = (memory_touch_async_args_t *) TASK_ARGS(task);
    size_t pagesize = (size_t) getpagesize();

    // volatile to trick the compiler and avoid optimization of *p = *p
    volatile unsigned char *   p = ((volatile unsigned char *) args->ptr) + (args->i+0) * args->chunk_size;
             unsigned char * end = (         (unsigned char *) args->ptr) + (args->i+1) * args->chunk_size;
    for ( ; p < end ; p += pagesize)
        *p = *p;
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
        constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
        constexpr size_t task_size = task_compute_size(flags, 0);
        constexpr size_t args_size = sizeof(memory_touch_async_args_t);

        task_t * task = thread->allocate_task(task_size + args_size);
        new(task) task_t(this->formats.memory_touch_async, flags);

        memory_touch_async_args_t * args = (memory_touch_async_args_t *) TASK_ARGS(task, task_size);
        args->runtime    = this;
        args->ptr        = ptr;
        args->chunk_size = chunk_size;
        args->i          = i;

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
