# include <xkrt/xkrt.h>
# include <xkrt/task/task-format.h>
# include <xkrt/task/task.hpp>
# include <xkrt/driver/thread-producer.hpp>

# include <assert.h>
# include <string.h>

static int r = 0;

static void
func(Task * args)
{
    r = 1;
}

int
main(void)
{
    xkrt_runtime_t runtime;
    assert(xkrt_init(&runtime) == 0);

    // create an empty task format
    task_format_id_t FORMAT;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));
        format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) func;
        FORMAT = task_format_create(&(runtime.task_formats), &format);
    }
    assert(FORMAT);

    // create an host task
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    // Submit the task
    const size_t task_size = sizeof(Task);
    Task * task = (Task *) thread->allocate(task_size);
    new(task) Task(FORMAT, UNSPECIFIED_TASK_ACCESS, UNSPECIFIED_DEVICE_GLOBAL_ID);
    runtime.commit(task);

    // wait
    assert(xkrt_sync(&runtime) == 0);

    // deinit has an implicit taskwait
    assert(xkrt_deinit(&runtime) == 0);
    assert(r == 1);

    return 0;
}
