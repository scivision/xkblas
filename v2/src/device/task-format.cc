# include "task-format.h"
# include "logger/logger.h"

# include <atomic>
# include <cassert>
# include <cstring>

static std::atomic<task_format_id_t> next_fmtid;
task_format_t task_formats[TASK_FORMAT_MAX];

task_format_t *
task_format_get(task_format_id_t id)
{
    return task_formats + id;
}

task_format_id_t
task_format_create(task_format_t * format)
{
    task_format_id_t fmtid = next_fmtid.fetch_add(1, std::memory_order_relaxed);
    memcpy(task_formats + fmtid, format, sizeof(task_format_t));
    assert(fmtid < TASK_FORMAT_MAX);
    XKBLAS_DEBUG("created new task format `%s`", format->label);
    return fmtid;
}
