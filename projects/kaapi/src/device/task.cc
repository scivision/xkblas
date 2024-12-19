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

# include <kaapi/device/task.hpp>
# include <kaapi/runtime.h>
# include <kaapi/sync/mem.h>

/* to be called once a task become ready, very dirty, but good enough as long
 * as we have the 'kaapi_runtime_t' global variable :-( */
void
kaapi_task_ready(Task * task)
{
    kaapi_runtime_t * context = kaapi_runtime_get();
    kaapi_runtime_submit_task(context, task);
}
