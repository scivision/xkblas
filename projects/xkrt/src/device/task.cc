/* ************************************************************************** */
/*                                                                            */
/*   task.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:58:59 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/device/task.hpp>
# include <xkrt/runtime.h>
# include <xkrt/sync/mem.h>

/* to be called once a task become ready, very dirty, but good enough as long
 * as we have the 'xkrt_runtime_t' global variable :-( */
void
xkrt_task_ready(Task * task)
{
    xkrt_runtime_t * runtime = xkrt_runtime_get();
    xkrt_runtime_submit_task(runtime, task);
}
