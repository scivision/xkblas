/* ************************************************************************** */
/*                                                                            */
/*   stats.h                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/28 19:46:21 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:05:10 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
