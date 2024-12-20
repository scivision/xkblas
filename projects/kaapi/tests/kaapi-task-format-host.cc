# include <kaapi/kaapi.h>
# include <kaapi/device/task-format.h>
# include <kaapi/device/task.hpp>
# include <kaapi/device/thread-producer.hpp>

# include <assert.h>
# include <string.h>

static int r = 0;

static void
func(void * args)
{
    r = 1;
}

int
main(void)
{
    assert(kaapi_init() == 0);

    // create an empty task format
    task_format_id_t FORMAT;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));
        format.f[TASK_FORMAT_TARGET_HOST] = func;
        FORMAT = task_format_create(&format);
    }
    assert(FORMAT);

    // create an host task
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    const size_t task_size = sizeof(Task);
    Task * task = (Task *) thread->allocate(task_size);
    {
        new(task) Task(FORMAT, UNSPECIFIED_TASK_ACCESS, HOST_DEVICE_GLOBAL_ID);
        thread->commit(task);
    }

    // Submit the task

    // deinit has an implicit taskwait
    assert(kaapi_deinit() == 0);
    assert(r == 1);

    return 0;
}
