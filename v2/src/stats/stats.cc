# include "xkblas-context.h"
# include "logger/logger.h"
# include "stats/stats.h"

# include <string.h>

void
xkblas_stats_report(xkblas_stats_t * stats)
{
    XKBLAS_INFO("----------------- STATS -----------------");
    XKBLAS_INFO("kernels:");
    XKBLAS_INFO("  launched: %u", stats->kernels.launched.load());
    XKBLAS_INFO("  completed: %u", stats->kernels.completed.load());
    XKBLAS_INFO("transfers:");
    XKBLAS_INFO("  launched:");
    for (int i = 0 ; i < XKBLAS_STREAM_TYPE_ALL ; ++i)
        XKBLAS_INFO("    `%4s`: %u",
                xkblas_stream_type_to_str((xkblas_stream_type_t) i),
                stats->transfers[i].launched.load());
    XKBLAS_INFO("  completed:");
    for (int i = 0 ; i < XKBLAS_STREAM_TYPE_ALL ; ++i)
        XKBLAS_INFO("    `%4s`: %u",
                xkblas_stream_type_to_str((xkblas_stream_type_t) i),
                stats->transfers[i].completed.load());

    XKBLAS_INFO("-----------------------------------------");
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
