# include <xkrt/xkrt.h>
# include <xkrt/task/task-format.h>
# include <xkrt/task/task.hpp>
# include <xkrt/driver/thread.hpp>

# include <assert.h>
# include <string.h>

static int r = 0;

static void
func(task_t * task)
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
        FORMAT = task_format_create(&(runtime.formats.list), &format);
    }
    assert(FORMAT);

    Thread * thread = Thread::self();
    assert(thread);

    // Create a task
    # define AC 1
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT | TASK_FLAG_DEVICE;
    constexpr size_t task_size = task_get_size(flags, AC);

    task_t * task = thread->allocate_task(task_size);
    new(task) task_t(FORMAT, flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    new (dev) task_dev_info_t(UNSPECIFIED_DEVICE_GLOBAL_ID, UNSPECIFIED_TASK_ACCESS);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "dependent-task-test");
    # endif

    // set accesses
    access_t * accesses = TASK_ACCESSES(task);
    {
        static_assert(AC <= TASK_MAX_ACCESSES);

        const int ld = 1;
        int * mem = (int *) malloc(ld * ld * sizeof(int));
        const int  m = ld;
        const int  n = ld;
        for (int i = 0 ; i < m ; ++i)
            for (int j = 0 ; j < m ; ++j)
                mem[i*ld+j] = 42;

        new(accesses + 0) access_t(task, MATRIX_COLMAJOR, mem, ld, 0, 0, m, n, sizeof(int), ACCESS_MODE_RW);
        thread->resolve<AC>(task, accesses);
        # undef AC
    }

    // submit it to the runtime
    runtime.task_commit(task);

    // wait
    assert(xkrt_sync(&runtime) == 0);

    // deinit has an implicit taskwait
    assert(xkrt_deinit(&runtime) == 0);
    assert(r == 1);

    return 0;
}
