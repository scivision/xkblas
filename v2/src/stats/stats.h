#ifndef __STATS_H__
# define __STATS_H__

# include "device/stream.h"

# include <atomic>
# include <stdint.h>

typedef struct  xkblas_stats_t
{
    struct {
        std::atomic<uint32_t> launched;
        std::atomic<uint32_t> completed;
    } tasks;

    struct {
        std::atomic<uint32_t> launched;
        std::atomic<uint32_t> completed;
    } kernels;

    struct {
        std::atomic<uint32_t> launched;
        std::atomic<uint32_t> completed;
    } transfers[XKBLAS_STREAM_TYPE_ALL];

}               xkblas_stats_t;

void xkblas_stats_report(xkblas_stats_t * stats);
void xkblas_stats_init(xkblas_stats_t * stats);
xkblas_stats_t * xkblas_stats_get(void);

#endif /* __STATS_H__ */
