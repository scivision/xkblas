/* ************************************************************************** */
/*                                                                            */
/*   memory-copy.cc                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 03:38:26 by Romain PEREIRA            \_)     (_/    */
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
    task_t * task = (task_t *) vargs[0];
    assert(task);

    copy_args_t * args = (copy_args_t *) (task + 1);
    args->runtime->task_detachable_post(task);
}

static void
body_memory_copy(task_t * task)
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

    constexpr task_flag_bitfield_t flags = TASK_FLAG_DETACHABLE | TASK_FLAG_DEVICE;
    constexpr size_t task_size = task_get_size(flags, 0);
    constexpr size_t args_size = sizeof(copy_args_t);

    task_t * task = thread->allocate_task(task_size + args_size);
    new(task) task_t(runtime->formats.copy_async, flags);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    new (dev) task_dev_info_t(device_global_id, UNSPECIFIED_TASK_ACCESS);

    task_det_info_t * det = TASK_DET_INFO(task);
    new (det) task_det_info_t();

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "copy");
    # endif /* NDEBUG */

    copy_args_t * args = (copy_args_t *) TASK_ARGS(task, task_size);
    args->runtime = runtime;
    args->device_global_id = device_global_id;
    args->dst_device_global_id = dst_device_global_id;
    args->dst_device_mem = dst_device_mem;
    args->src_device_global_id = src_device_global_id;
    args->src_device_mem = src_device_mem;
    args->size = size;

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
