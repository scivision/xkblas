/* ************************************************************************** */
/*                                                                            */
/*   task.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "xkblas-context.h"
# include "device/task.hpp"
# include "sync/mem.h"

/* to be called once a task become ready, very dirty, but good enough as long
 * as we have the 'xkblas_context_t' global variable :-( */
void
xkblas_task_ready(Task * task)
{
    xkblas_context_t * context = xkblas_context_get();
    xkblas_context_submit_task(context, task);
}
