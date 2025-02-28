# include <xkrt/xkrt.h>
# include <xkrt/task/task-format.h>

# include <assert.h>
# include <string.h>

int
main(void)
{
    xkrt_runtime_t runtime;
    assert(xkrt_init(&runtime) == 0);

    // create an empty task format
    task_format_id_t EMPTY;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));
        EMPTY = task_format_create(&(runtime.formats.list), &format);
    }
    assert(EMPTY);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
