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

extern backend_t backends[];

# endif /* __BACKEND_H__ */
