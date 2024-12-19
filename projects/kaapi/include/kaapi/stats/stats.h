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

# include <kaapi/device/stream.h>
# include <kaapi/device/task.hpp>

# include <atomic>
# include <stddef.h>

# define KAAPI_STATS_TASK_FORMAT_MAX 64
typedef std::atomic<uint64_t> stats_int_t;

typedef struct  kaapi_stats_t
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
    } tasks[KAAPI_STATS_TASK_FORMAT_MAX];

    struct {
        struct {
            stats_int_t launched;
            stats_int_t completed;
        } instructions[KAAPI_STREAM_INSTR_TYPE_MAX];
        stats_int_t transfered;
    } streams[KAAPI_STREAM_TYPE_ALL];

}               kaapi_stats_t;

void kaapi_stats_report(void);
void kaapi_stats_init(kaapi_stats_t * stats);
kaapi_stats_t * kaapi_stats_get(void);

#endif /* __STATS_H__ */
