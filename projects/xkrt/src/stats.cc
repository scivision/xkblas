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

# include <xkrt/runtime.h>
# include <xkrt/xkrt-support.h>
# include <xkrt/task/task.hpp>
# include <xkrt/logger/logger.h>

# include <string.h>

static inline void
human_readable_size(char * buffer, int bufsize, size_t nbytes)
{
    const char * suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int i = 0;
    double size = (double) nbytes;
    while (size >= 1024 && i < sizeof(suffixes) / sizeof(*suffixes))
    {
        size /= 1024;
        i++;
    }
    snprintf(buffer, bufsize, "%.0lf%s", size, suffixes[i]);
}

void
xkrt_runtime_stats_report(xkrt_runtime_t * runtime)
{
    # if XKRT_SUPPORT_STATS
    LOGGER_WARN("----------------- STATS -----------------");

    # if 0
    LOGGER_WARN("tasks: (partially supported)");
    stats_int_t summary[TASK_STATE_MAX];
    memset(summary, 0, sizeof(summary));
    for (int i = 0 ; i < XKRT_STATS_TASK_FORMAT_MAX ; ++i)
    {
        if (stats->tasks[i].states[0].load())
        {
            task_format_t * format = task_format_get(&(runtime->task_formats), (task_format_id_t)i);
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

    # endif

    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = runtime->drivers.devices.list[device_global_id];
        xkrt_driver_t * driver = runtime->drivers.list + device->driver_type;
        LOGGER_WARN("Device %u", device_global_id);
        LOGGER_WARN("  Info: %s", driver->f_device_info(device->driver_id));

        if (device->stats.memory.allocated.total.load() || device->stats.memory.allocated.currently.load() || device->stats.memory.freed.load())
        {
            LOGGER_WARN("  Memory");

            char buffer[128];

            human_readable_size(buffer, sizeof(buffer), device->stats.memory.allocated.total.load());
            LOGGER_WARN("    Allocated (total): %s", buffer);

            human_readable_size(buffer, sizeof(buffer), device->stats.memory.allocated.currently.load());
            LOGGER_WARN("    Allocated (currently): %s", buffer);

            human_readable_size(buffer, sizeof(buffer), device->stats.memory.freed.load());
            LOGGER_WARN("    Freed: %s", buffer);
        }

        // reduce stream stats per stream type
        struct {
            struct {
                stats_int_t commited;
                stats_int_t completed;
            } instructions[XKRT_STREAM_INSTR_TYPE_MAX];
        } stats;
        memset(&stats, 0, sizeof(stats));

        // print all stream and their types; and reduce stats
        LOGGER_WARN("  Streams");
        for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
        {
            stats_int_t transfered = 0;

            for (int stream_id = 0 ; stream_id < device->offloader.count[stype] ; ++stream_id)
            {
                xkrt_stream_t * stream = device->offloader.streams[stype][stream_id];

                for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
                {
                    stats.instructions[instr_type].commited += stream->stats.instructions[instr_type].commited.load();
                    stats.instructions[instr_type].completed += stream->stats.instructions[instr_type].completed.load();
                }
                transfered += stream->stats.transfered;
            }

            char buffer[128];
            human_readable_size(buffer, sizeof(buffer), transfered.load());
            LOGGER_WARN("    `%4s` - with %u streams - transfered %s", xkrt_stream_type_to_str((xkrt_stream_type_t) stype), device->offloader.count[stype], buffer);
        }

        LOGGER_WARN("  Instructions");
        for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
        {
            LOGGER_WARN(
                "    `%8s` - commited %6zu - completed %6zu",
                xkrt_stream_instruction_type_to_str((xkrt_stream_instruction_type_t) instr_type),
                stats.instructions[instr_type].commited.load(),
                stats.instructions[instr_type].completed.load()
            );
        }
    }

    LOGGER_WARN("-----------------------------------------");

    # else /* XKRT_SUPPORT_STATS */

    LOGGER_WARN("--- STATS DISABLED - ENABLE WITH -DXKRT_SUPPORT_STATS=on WITH CMAKE");

    # endif /* XKRT_SUPPORT_STATS */
}
