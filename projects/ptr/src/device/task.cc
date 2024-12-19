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

# include <ptr/device/task.hpp>
# include <ptr/runtime.h>
# include <ptr/sync/mem.h>

/* to be called once a task become ready, very dirty, but good enough as long
 * as we have the 'ptr_runtime_t' global variable :-( */
void
ptr_task_ready(Task * task)
{
    ptr_runtime_t * context = ptr_runtime_get();
    ptr_runtime_submit_task(context, task);
}
