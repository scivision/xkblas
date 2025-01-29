# include <xkrt/xkrt.h>
# include <xkrt/device/task-format.h>

# include <assert.h>
# include <string.h>

int
main(void)
{
    assert(xkrt_init() == 0);

    // create an empty task format
    task_format_id_t EMPTY;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_id_t));
        EMPTY = task_format_create(&format);
    }
    assert(EMPTY);

    assert(xkrt_deinit() == 0);

    return 0;
}
