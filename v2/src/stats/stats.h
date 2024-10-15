#ifndef __STATS_H__
# define __STATS_H__

# include "device/stream.h"

# include <atomic>
# include <stddef.h>

typedef std::atomic<uint64_t> stats_int_t;

typedef struct  xkblas_stats_t
{
    struct {
        stats_int_t freed;
        struct {
            stats_int_t total;
            stats_int_t currently;
        } allocated;
    } memory;

    struct {
        stats_int_t commited;
        stats_int_t launched;
        stats_int_t completed;
    } tasks;

    struct {
        stats_int_t launched;
        stats_int_t completed;
    } kernels;

    struct {
        stats_int_t launched;
        stats_int_t completed;
    } transfers[XKBLAS_STREAM_TYPE_ALL];

}               xkblas_stats_t;

void xkblas_stats_report(xkblas_stats_t * stats);
void xkblas_stats_init(xkblas_stats_t * stats);
xkblas_stats_t * xkblas_stats_get(void);

#endif /* __STATS_H__ */
