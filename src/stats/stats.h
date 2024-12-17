/* ************************************************************************** */
/*                                                                            */
/*   stats.h                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STATS_H__
# define __STATS_H__

# include "device/stream.h"
# include "device/task.hpp"

# include <atomic>
# include <stddef.h>

# define XKBLAS_STATS_TASK_FORMAT_MAX 64
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
        stats_int_t states[TASK_STATE_MAX];
    } tasks[XKBLAS_STATS_TASK_FORMAT_MAX];

    struct {
        struct {
            stats_int_t launched;
            stats_int_t completed;
        } instructions[XKBLAS_STREAM_INSTR_TYPE_MAX];
        stats_int_t transfered;
    } streams[XKBLAS_STREAM_TYPE_ALL];

}               xkblas_stats_t;

void xkblas_stats_report(void);
void xkblas_stats_init(xkblas_stats_t * stats);
xkblas_stats_t * xkblas_stats_get(void);

#endif /* __STATS_H__ */
