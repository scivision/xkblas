/* ************************************************************************** */
/*                                                                            */
/*   stats.h                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STATS_H__
# define __STATS_H__

# include <xkrt/support.h>

# if XKRT_SUPPORT_STATS

# include <atomic>
# include <stddef.h>

# define XKRT_STATS_TASK_FORMAT_MAX 128
typedef std::atomic<uint64_t> stats_int_t;

#  define XKRT_STATS_INCR(X, INCR)  \
    do {                            \
        X += INCR;                  \
    } while (0)

#  define XKRT_STATS_DECR(X, DECR)  \
    do {                            \
        X -= DECR;                  \
    } while (0)

#  define XKRT_STATS_SET(X, V)      \
    do {                            \
        X = V;                      \
    } while (0)

# else /* XKRT_SUPPORT_STATS */

#  define XKRT_STATS_INCR(X, INCR)
#  define XKRT_STATS_DECR(X, DECR)
#  define XKRT_STATS_SET(X, V)

# endif /* XKRT_SUPPORT_STATS */

#endif /* __STATS_H__ */
