# include "xkblas-context.h"
# include "device/task.hpp"

/* to be called once a task become ready, very dirty, but good enough as long
 * as we have the 'xkblas_context_t' global variable :-( */
void
xkblas_task_ready(Task * task)
{
    xkblas_context_t * context = xkblas_context_get();
    xkblas_drivers_enqueue(&(context->drivers), task);
}
