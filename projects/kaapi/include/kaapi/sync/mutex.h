/* ************************************************************************** */
/*                                                                            */
/*   mutex.h                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __MUTEX_H__
# define __MUTEX_H__

# include <pthread.h>

typedef struct  kaapi_mutex_t
{
    pthread_mutex_t _pthread_mutex;

}               kaapi_mutex_t;

# define KAAPI_MUTEX_INITIALIZER { ._pthread_mutex=PTHREAD_MUTEX_INITIALIZER }
# define KAAPI_MUTEX_INIT(L) { L._pthread_mutex=PTHREAD_MUTEX_INITIALIZER; }
# define KAAPI_MUTEX_LOCK(L)   pthread_mutex_lock(&(L._pthread_mutex))
# define KAAPI_MUTEX_UNLOCK(L) pthread_mutex_unlock(&(L._pthread_mutex))

#endif /* __MUTEX_H__ */
