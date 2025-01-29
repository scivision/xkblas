/* ************************************************************************** */
/*                                                                            */
/*   stats.h                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:51:38 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STATS_H__
# define __STATS_H__

# include <xkrt/device/stream.h>
# include <xkrt/device/task.hpp>

# include <atomic>
# include <stddef.h>

# define XKRT_STATS_TASK_FORMAT_MAX 64
typedef std::atomic<uint64_t> stats_int_t;

typedef struct  xkrt_stats_t
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
    } tasks[XKRT_STATS_TASK_FORMAT_MAX];

    struct {
        struct {
            stats_int_t launched;
            stats_int_t completed;
        } instructions[XKRT_STREAM_INSTR_TYPE_MAX];
        stats_int_t transfered;
    } streams[XKRT_STREAM_TYPE_ALL];

}               xkrt_stats_t;

void xkrt_stats_report(void);
void xkrt_stats_init(xkrt_stats_t * stats);
xkrt_stats_t * xkrt_stats_get(void);

#endif /* __STATS_H__ */
