# include "xkblas-context.h"
# include "logger/logger.h"
# include "stats/stats.h"

# include <string.h>

void
xkblas_stats_report(xkblas_stats_t * stats)
{
    XKBLAS_WARN("----------------- STATS -----------------");
    XKBLAS_WARN("memory:");
    XKBLAS_WARN("  allocated:");
    XKBLAS_WARN("    total: %zu", stats->memory.allocated.total.load());
    XKBLAS_WARN("    currently: %zu", stats->memory.allocated.currently.load());
    XKBLAS_WARN("  freed: %zu", stats->memory.freed.load());
    XKBLAS_WARN("tasks:");
    XKBLAS_WARN("  launched: %zu", stats->tasks.launched.load());
    XKBLAS_WARN("  completed: %zu", stats->tasks.completed.load());
    XKBLAS_WARN("kernels:");
    XKBLAS_WARN("  launched: %zu", stats->kernels.launched.load());
    XKBLAS_WARN("  completed: %zu", stats->kernels.completed.load());
    XKBLAS_WARN("transfers:");
    XKBLAS_WARN("  launched:");
    for (int i = 0 ; i < XKBLAS_STREAM_TYPE_ALL ; ++i)
        XKBLAS_WARN("    `%4s`: %zu",
                xkblas_stream_type_to_str((xkblas_stream_type_t) i),
                stats->transfers[i].launched.load());
    XKBLAS_WARN("  completed:");
    for (int i = 0 ; i < XKBLAS_STREAM_TYPE_ALL ; ++i)
        XKBLAS_WARN("    `%4s`: %zu",
                xkblas_stream_type_to_str((xkblas_stream_type_t) i),
                stats->transfers[i].completed.load());

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
