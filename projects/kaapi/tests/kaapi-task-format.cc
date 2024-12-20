# include <kaapi/kaapi.h>
# include <kaapi/device/task-format.h>

# include <assert.h>
# include <string.h>

int
main(void)
{
    assert(kaapi_init() == 0);

    // create an empty task format
    task_format_id_t EMPTY;
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_id_t));
        EMPTY = task_format_create(&format);
    }
    assert(EMPTY);

    assert(kaapi_deinit() == 0);

    return 0;
}
