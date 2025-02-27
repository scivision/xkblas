/* ************************************************************************** */
/*                                                                            */
/*   memory-copy.cc                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/26 17:13:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/driver/thread-producer.hpp>

static task_format_id_t TASK_FORMAT_COPY = -1;

typedef struct  copy_args_t
{
    xkrt_device_t * device;
    size_t size;
    xkrt_device_global_id_t dst_device_global_id;
    uintptr_t dst_device_mem;
    xkrt_device_global_id_t src_device_global_id;
    uintptr_t src_device_mem;
}               copy_args_t;

static void
body_memory_copy(Task * task)
{
    assert(task);

    copy_args_t * args = (copy_args_t *) (task + 1);
    const xkrt_callback_t callback = { .func = NULL };
    args->device->offloader_stream_instruction_submit_copy<size_t, uintptr_t>(
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
    xkrt_device_t * device,
    const xkrt_device_global_id_t dst_device_global_id,
    const uintptr_t dst_device_mem,
    const xkrt_device_global_id_t src_device_global_id,
    const uintptr_t src_device_mem,
    const size_t size
) {
    assert(device);

    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    const size_t task_size = sizeof(Task) + sizeof(copy_args_t);
    uint8_t * mem = thread->allocate(task_size);
    assert(mem);

    Task * task = reinterpret_cast<Task *>(mem);
    new(task) Task(TASK_FORMAT_COPY, UNSPECIFIED_TASK_ACCESS, device->global_id);
    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "copy");
    # endif /* NDEBUG */

    copy_args_t * args = (copy_args_t *) (task + 1);
    args->device = device;
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
    TASK_FORMAT_COPY = task_format_create(&(runtime->task_formats), &format);
}
