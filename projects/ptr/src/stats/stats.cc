/* ************************************************************************** */
/*                                                                            */
/*   stats.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:04:51 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <ptr/runtime.h>
# include <ptr/device/task.hpp>
# include <ptr/logger/logger.h>
# include <ptr/stats/stats.h>

# include <string.h>

void
ptr_stats_task_state_incr(task_format_id_t fmtid, task_state_t state)
{
    ptr_runtime_t * context = ptr_runtime_get();
    assert(context);
    ++context->stats.tasks[fmtid].states[state];
}

void
ptr_stats_report(void)
{
    ptr_runtime_t * context = ptr_runtime_get();
    assert(context);
    ptr_stats_t * stats = &(context->stats);

    LOGGER_WARN("----------------- STATS -----------------");

    LOGGER_WARN("memory:");
    LOGGER_WARN("  allocated:");
    LOGGER_WARN("    total: %zu", stats->memory.allocated.total.load());
    LOGGER_WARN("    currently: %zu", stats->memory.allocated.currently.load());
    LOGGER_WARN("  freed: %zu", stats->memory.freed.load());

    LOGGER_WARN("tasks:");
    stats_int_t summary[TASK_STATE_MAX];
    memset(summary, 0, sizeof(summary));
    for (int i = 0 ; i < PTR_STATS_TASK_FORMAT_MAX ; ++i)
    {
        if (stats->tasks[i].states[0].load())
        {
            task_format_t * format = task_format_get((task_format_id_t)i);
            LOGGER_WARN("  `%s`", format ? format->label : "unk");
            for (int j = 0 ; j < TASK_STATE_MAX ; ++j)
            {
                LOGGER_WARN("     %s: %zu", task_state_to_str((task_state_t) j), stats->tasks[i].states[j].load());
                summary[j] += stats->tasks[i].states[j].load();
            }
        }
    }

    LOGGER_WARN("tasks (summary):");
    for (int j = 0 ; j < TASK_STATE_MAX ; ++j)
        LOGGER_WARN("  %12s: %zu", task_state_to_str((task_state_t) j), summary[j].load());

    LOGGER_WARN("streams:");
    for (int i = 0 ; i < PTR_STREAM_TYPE_ALL ; ++i)
    {
        LOGGER_WARN("  `%s`", ptr_stream_type_to_str((ptr_stream_type_t) i));
        LOGGER_WARN("    instructions");
        for (int j = 0 ; j < PTR_STREAM_INSTR_TYPE_MAX ; ++j)
        {
            if (stats->streams[i].instructions[j].launched.load())
            {
                LOGGER_WARN("      `%s`", ptr_stream_instruction_type_to_str((ptr_stream_instruction_type_t) j));
                LOGGER_WARN("        launched: %zu", stats->streams[i].instructions[j].launched.load());
                LOGGER_WARN("        completed: %zu", stats->streams[i].instructions[j].completed.load());
            }
        }
        LOGGER_WARN("    transfered (bytes): %zu", stats->streams[i].transfered.load());
    }

    LOGGER_WARN("-----------------------------------------");
}

void
ptr_stats_init(ptr_stats_t * stats)
{
    memset(stats, 0, sizeof(ptr_stats_t));
}

ptr_stats_t *
ptr_stats_get(void)
{
    return &(ptr_runtime_get()->stats);
}
