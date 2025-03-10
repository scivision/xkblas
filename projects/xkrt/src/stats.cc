/* ************************************************************************** */
/*                                                                            */
/*   stats.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/10 17:16:03 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/xkrt-support.h>
# include <xkrt/task/task.hpp>
# include <xkrt/task/task-format.h>
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

typedef struct  device_stats_t
{
    struct {
        stats_int_t freed;
        struct {
            stats_int_t total;
            stats_int_t currently;
        } allocated;
    } memory;

    struct {
        stats_int_t n;
        stats_int_t transfered;
    } streams[XKRT_STREAM_TYPE_ALL];

    struct {
        stats_int_t commited;
        stats_int_t completed;
    } instructions[XKRT_STREAM_INSTR_TYPE_MAX];

}               device_stats_t;

static void
xkrt_runtime_stats_device_agg(device_stats_t * src, device_stats_t * agg)
{
    agg->memory.freed += src->memory.freed;
    agg->memory.allocated.total += src->memory.allocated.total;
    agg->memory.allocated.currently += src->memory.allocated.currently;

    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        agg->streams[stype].n += src->streams[stype].n;
        agg->streams[stype].transfered += src->streams[stype].transfered;
    }

    for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
    {
        agg->instructions[instr_type].commited += src->instructions[instr_type].commited;
        agg->instructions[instr_type].completed += src->instructions[instr_type].completed;
    }
}

static void
xkrt_runtime_stats_device_report(xkrt_runtime_t * runtime, device_stats_t * stats)
{
    char buffer[128];

    LOGGER_WARN("  Memory");

    human_readable_size(buffer, sizeof(buffer), stats->memory.allocated.total.load());
    LOGGER_WARN("    Allocated (total): %s", buffer);

    human_readable_size(buffer, sizeof(buffer), stats->memory.allocated.currently.load());
    LOGGER_WARN("    Allocated (currently): %s", buffer);

    human_readable_size(buffer, sizeof(buffer), stats->memory.freed.load());
    LOGGER_WARN("    Freed: %s", buffer);

    LOGGER_WARN("  Streams");
    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        human_readable_size(buffer, sizeof(buffer), stats->streams[stype].transfered.load());
        LOGGER_WARN("    `%4s` - with %2lu streams - transfered %s", xkrt_stream_type_to_str((xkrt_stream_type_t) stype), stats->streams[stype].n.load(), buffer);
    }

    LOGGER_WARN("  Instructions");
    for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
    {
        LOGGER_WARN(
            "    `%12s` - commited %6zu - completed %6zu",
            xkrt_stream_instruction_type_to_str((xkrt_stream_instruction_type_t) instr_type),
            stats->instructions[instr_type].commited.load(),
            stats->instructions[instr_type].completed.load()
        );
    }
}

static void
xkrt_runtime_stats_device_gather(xkrt_runtime_t * runtime, xkrt_device_t * device, device_stats_t * stats)
{
    memset(stats, 0, sizeof(device_stats_t));

    stats->memory.freed = device->stats.memory.freed.load();
    stats->memory.allocated.total = device->stats.memory.allocated.total.load();
    stats->memory.allocated.currently = device->stats.memory.allocated.currently.load();

    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        for (int stream_id = 0 ; stream_id < device->count[stype] ; ++stream_id)
        {
            xkrt_stream_t * stream = device->streams[stype][stream_id];
            for (int instr_type = 0 ; instr_type < XKRT_STREAM_INSTR_TYPE_MAX ; ++instr_type)
            {
                stats->instructions[instr_type].commited += stream->stats.instructions[instr_type].commited.load();
                stats->instructions[instr_type].completed += stream->stats.instructions[instr_type].completed.load();
            }
            stats->streams[stype].transfered += stream->stats.transfered.load();
        }
        stats->streams[stype].n += device->count[stype];
    }
}

static void
xkrt_runtime_stats_tasks_report(xkrt_runtime_t * runtime)
{
    for (size_t i = 0 ; i < TASK_FORMAT_MAX ; ++i)
    {
        task_format_t * format = task_format_get(&runtime->formats.list, (task_format_id_t) i);
        if (format == NULL)
            break ;
        if (runtime->stats.tasks[i].submitted)
            LOGGER_WARN("  `%16s` - %6zu submitted - %6zu commited - %6zu completed",
                format->label,
                runtime->stats.tasks[i].submitted.load(),
                runtime->stats.tasks[i].commited.load(),
                runtime->stats.tasks[i].completed.load()
            );
    }
}

void
xkrt_runtime_stats_report(xkrt_runtime_t * runtime)
{
    # if XKRT_SUPPORT_STATS
    LOGGER_WARN("----------------- STATS -----------------");
    device_stats_t agg;
    memset(&agg, 0, sizeof(agg));

    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = runtime->drivers.devices.list[device_global_id];

        xkrt_driver_t * driver = runtime->driver_get(device->driver_type);
        LOGGER_WARN("Device %u", device->global_id);
        LOGGER_WARN("  Info: %s", driver->f_device_info(device->driver_id));

        device_stats_t stats;
        xkrt_runtime_stats_device_gather(runtime, device, &stats);
        xkrt_runtime_stats_device_report(runtime, &stats);
        xkrt_runtime_stats_device_agg(&stats, &agg);
    }

    LOGGER_WARN("-----------------------------------------");
    LOGGER_WARN("All Devices");
    xkrt_runtime_stats_device_report(runtime, &agg);
    LOGGER_WARN("-----------------------------------------");
    LOGGER_WARN("Tasks");
    xkrt_runtime_stats_tasks_report(runtime);
    LOGGER_WARN("-----------------------------------------");

    # else /* XKRT_SUPPORT_STATS */

    LOGGER_WARN("--- STATS DISABLED - ENABLE WITH -DXKRT_SUPPORT_STATS=on WITH CMAKE");

    # endif /* XKRT_SUPPORT_STATS */
}
