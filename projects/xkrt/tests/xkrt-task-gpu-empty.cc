# include <xkrt/xkrt.h>
# include <xkrt/task/task-format.h>
# include <xkrt/task/task.hpp>
# include <xkrt/driver/thread-producer.hpp>

# include <assert.h>
# include <string.h>

static int r = 0;

static void
func(void * handle, void * args)
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
        format.f[TASK_FORMAT_TARGET_HOST] = func;
        FORMAT = task_format_create(&(runtime.task_formats), &format);
    }
    assert(FORMAT);

    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    // Create a task
    const size_t task_size = sizeof(Task);
    Task * task = (Task *) thread->allocate(task_size);
    new(task) Task(FORMAT, UNSPECIFIED_TASK_ACCESS, UNSPECIFIED_DEVICE_GLOBAL_ID);

    // set accesses
    {
        # define NACCESSES 1
        static_assert(NACCESSES <= TASK_MAX_ACCESSES);

        const int ld = 1024;
        int * mem = (int *) malloc(ld * ld * sizeof(int));
        const int  m = 1024;
        const int  n = 1024;
        for (int i = 0 ; i < m ; ++i)
            for (int j = 0 ; j < m ; ++j)
                mem[i*ld+j] = 42;

        new(task->accesses + 0) Access(MATRIX_COLMAJOR, mem, ld, 0, 0, m, n, sizeof(int), ACCESS_MODE_RW);
        thread->resolve<NACCESSES>(task);
        # undef NACCESSES
    }

    // submit it to the runtime
    runtime.commit(task);

    // wait
    assert(xkrt_sync(&runtime) == 0);

    // deinit has an implicit taskwait
    assert(xkrt_deinit(&runtime) == 0);
    assert(r == 1);

    return 0;
}
