/* ************************************************************************** */
/*                                                                            */
/*   memory-copy.cc                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/28 01:12:01 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/driver/thread.hpp>

typedef struct  copy_args_t
{
    // the runtime
    xkrt_runtime_t * runtime;

    // the device responsible to perform the copy
    xkrt_device_global_id_t device_global_id;

    // pointers
    xkrt_device_global_id_t dst_device_global_id;
    uintptr_t dst_device_mem;

    xkrt_device_global_id_t src_device_global_id;
    uintptr_t src_device_mem;

    // size of the copy
    size_t size;

}               copy_args_t;

static void
body_memory_copy_callback(const void * vargs [XKRT_CALLBACK_ARGS_MAX])
{
    Task * task = (Task *) vargs[0];
    assert(task);

    copy_args_t * args = (copy_args_t *) (task + 1);
    args->runtime->task_detachable_post(task);
}

static void
body_memory_copy(Task * task)
{
    assert(task);
    copy_args_t * args = (copy_args_t *) (task + 1);

    xkrt_callback_t callback;
    callback.func    = body_memory_copy_callback;
    callback.args[0] = task;

    args->runtime->copy(
        args->device_global_id,
        args->size,
        args->dst_device_global_id,
        args->dst_device_mem,
        args->src_device_global_id,
        args->src_device_mem,
        callback
    );
}

extern "C"
void
xkrt_memory_copy_async(
    xkrt_runtime_t * runtime,
    const xkrt_device_global_id_t device_global_id,
    const xkrt_device_global_id_t dst_device_global_id,
    const uintptr_t dst_device_mem,
    const xkrt_device_global_id_t src_device_global_id,
    const uintptr_t src_device_mem,
    const size_t size
) {
    Thread * thread = Thread::self();
    assert(thread);

    uint8_t * mem = thread->allocate(sizeof(Task) + sizeof(copy_args_t));
    assert(mem);

    Task * task = reinterpret_cast<Task *>(mem);
    new(task) Task(runtime->formats.copy_async, UNSPECIFIED_TASK_ACCESS, device_global_id, TASK_FLAG_DETACHABLE);
    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "copy");
    # endif /* NDEBUG */

    copy_args_t * args = (copy_args_t *) (task + 1);
    args->runtime = runtime;
    args->device_global_id = device_global_id;
    args->dst_device_global_id = dst_device_global_id;
    args->dst_device_mem = dst_device_mem;
    args->src_device_global_id = src_device_global_id;
    args->src_device_mem = src_device_mem;
    args->size = size;

    thread->resolve<0>(task);
    runtime->task_commit(task);
}

void
xkrt_memory_copy_async_register_format(xkrt_runtime_t * runtime)
{
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_memory_copy;
    snprintf(format.label, sizeof(format.label), "memory_copy");
    runtime->formats.copy_async = task_format_create(&(runtime->formats.list), &format);
}
