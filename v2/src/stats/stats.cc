# include "xkblas-context.h"
# include "device/task.hpp"
# include "logger/logger.h"
# include "stats/stats.h"

# include <string.h>

void
xkblas_stats_task_state_incr(task_format_id_t fmtid, task_state_t state)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);
    ++context->stats.tasks[fmtid].states[state];
}

void
xkblas_stats_report(void)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);
    xkblas_stats_t * stats = &(context->stats);

    XKBLAS_WARN("----------------- STATS -----------------");

    XKBLAS_WARN("memory:");
    XKBLAS_WARN("  allocated:");
    XKBLAS_WARN("    total: %zu", stats->memory.allocated.total.load());
    XKBLAS_WARN("    currently: %zu", stats->memory.allocated.currently.load());
    XKBLAS_WARN("  freed: %zu", stats->memory.freed.load());

    XKBLAS_WARN("tasks:");
    stats_int_t summary[TASK_STATE_MAX];
    memset(summary, 0, sizeof(summary));
    for (int i = 0 ; i < XKBLAS_STATS_TASK_FORMAT_MAX ; ++i)
    {
        if (stats->tasks[i].states[0].load())
        {
            task_format_t * format = task_format_get((task_format_id_t)i);
            XKBLAS_WARN("  `%s`", format ? format->label : "unk");
            for (int j = 0 ; j < TASK_STATE_MAX ; ++j)
            {
                XKBLAS_WARN("     %s: %zu", task_state_to_str((task_state_t) j), stats->tasks[i].states[j].load());
                summary[j] += stats->tasks[i].states[j].load();
            }
        }
    }

    XKBLAS_WARN("tasks (summary):");
    for (int j = 0 ; j < TASK_STATE_MAX ; ++j)
        XKBLAS_WARN("  %12s: %zu", task_state_to_str((task_state_t) j), summary[j].load());

    XKBLAS_WARN("streams:");
    for (int i = 0 ; i < XKBLAS_STREAM_TYPE_ALL ; ++i)
    {
        XKBLAS_WARN("  `%s`", xkblas_stream_type_to_str((xkblas_stream_type_t) i));
        XKBLAS_WARN("    instructions");
        for (int j = 0 ; j < XKBLAS_STREAM_INSTR_TYPE_MAX ; ++j)
        {
            if (stats->streams[i].instructions[j].launched.load())
            {
                XKBLAS_WARN("      `%s`", xkblas_stream_instruction_type_to_str((xkblas_stream_instruction_type_t) j));
                XKBLAS_WARN("        launched: %zu", stats->streams[i].instructions[j].launched.load());
                XKBLAS_WARN("        completed: %zu", stats->streams[i].instructions[j].completed.load());
            }
        }
        XKBLAS_WARN("    transfered (bytes): %zu", stats->streams[i].transfered.load());
    }

    XKBLAS_WARN("-----------------------------------------");
}

void
xkblas_stats_init(xkblas_stats_t * stats)
{
    memset(stats, 0, sizeof(xkblas_stats_t));
}

xkblas_stats_t *
xkblas_stats_get(void)
{
    return &(xkblas_context_get()->stats);
}
