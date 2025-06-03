/* ************************************************************************** */
/*                                                                            */
/*   mutex.h                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/10 17:00:08 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:06:27 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __MUTEX_H__
# define __MUTEX_H__

# include <pthread.h>

typedef struct  xkrt_mutex_t
{
    pthread_mutex_t _pthread_mutex;

}               xkrt_mutex_t;

# define XKRT_MUTEX_INITIALIZER { ._pthread_mutex=PTHREAD_MUTEX_INITIALIZER }
# define XKRT_MUTEX_INIT(L) { L._pthread_mutex=PTHREAD_MUTEX_INITIALIZER; }
# define XKRT_MUTEX_LOCK(L)   pthread_mutex_lock(&(L._pthread_mutex))
# define XKRT_MUTEX_UNLOCK(L) pthread_mutex_unlock(&(L._pthread_mutex))

#endif /* __MUTEX_H__ */
