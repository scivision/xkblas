/* ************************************************************************** */
/*                                                                            */
/*   host-thread.cc                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/10 21:28:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/driver/thread.hpp>

# include <pthread.h>

void
xkrt_host_thread_init(xkrt_runtime_t * runtime)
{
    runtime->host_thread = NULL;

    pthread_t thread;
    int err = pthread_create(&thread, NULL, (void * (*)(void *)) xkrt_host_thread_main_loop, runtime);
    if (err)
        LOGGER_FATAL("Could not create thread for host device");

    // TODO : could use a mutex and a cond here
    while ((volatile void *) runtime->host_thread == NULL)
        mem_pause();
}
