/* ************************************************************************** */
/*                                                                            */
/*   task-format.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/23 15:33:40 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 17:58:26 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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

    task_format_t format;
    memset(&format.f, 0, sizeof(format.f));
    snprintf(format.label, sizeof(format.label), "(null)");

    task_format_id_t id = task_format_create(formats, &format);
    assert(id == TASK_FORMAT_NULL);
}

task_format_t *
task_format_get(task_formats_t * formats, task_format_id_t id)
{
    return formats->list + id;
}

task_format_id_t
task_format_create(task_formats_t * formats, task_format_t * format)
{
    task_format_id_t fmtid = (task_format_id_t) formats->next_fmtid.fetch_add(1, std::memory_order_relaxed);
    memcpy(formats->list + fmtid, format, sizeof(task_format_t));
    assert(fmtid < TASK_FORMAT_MAX);
    LOGGER_INFO("Created new task format `%d` named `%s`", fmtid, format->label);
    return fmtid;
}
