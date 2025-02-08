/* ************************************************************************** */
/*                                                                            */
/*   task-format.cc                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:59:14 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/task/task-format.h>
# include <xkrt/logger/logger.h>

# include <atomic>
# include <cassert>
# include <cstring>

void
task_formats_init(task_formats_t * formats)
{
    memset(formats, 0, sizeof(task_formats_t));
}

task_format_t *
task_format_get(task_formats_t * formats, task_format_id_t id)
{
    return formats->list + id;
}

task_format_id_t
task_format_create(task_formats_t * formats, task_format_t * format)
{
    task_format_id_t fmtid = 1 + formats->next_fmtid.fetch_add(1, std::memory_order_relaxed);
    memcpy(formats->list + fmtid, format, sizeof(task_format_t));
    assert(fmtid < TASK_FORMAT_MAX);
    LOGGER_INFO("Created new task format `%d` named `%s`", fmtid, format->label);
    return fmtid;
}
