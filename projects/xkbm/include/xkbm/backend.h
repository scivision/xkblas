/* ************************************************************************** */
/*                                                                            */
/*   backend.h                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/18 22:56:54 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:37:19 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __BACKEND_H__
#  define __BACKEND_H__

# include "benchmark.h"

typedef struct  backend_s
{
    const char * name;                                      /* XKRT name */
    const int enabled;                                    /* if enabled */
    void (*benchmark_push)(benchmark_node_t * parent);      /* to push a new benchmark */
    void (*init)(void);
    void (*deinit)(void);
}               backend_t;

# endif /* __BACKEND_H__ */
