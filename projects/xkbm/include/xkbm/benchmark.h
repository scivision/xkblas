/* ************************************************************************** */
/*                                                                            */
/*   benchmark.h                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/18 22:56:54 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:37:20 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __BENCHMARK_H__
#  define __BENCHMARK_H__

# define BENCHMARK_MAX_CHILDREN (16)
# define BENCHMARK_NAME_MAX_LEN (1024)

typedef struct  benchmark_node_s
{
    const char * name;                                          /* name */
    const char * desc;                                          /* description */
    struct benchmark_node_s * parent;                           /* parent node */
    struct benchmark_node_s * children[BENCHMARK_MAX_CHILDREN]; /* children nodes */
    int nchildren;                                              /* number of children nodes */
    void (*run)(struct benchmark_node_s * bench);               /* run the benchmark */
    int enabled;                                                /* if this benchmark if enabled or not */
}               benchmark_node_t;

extern benchmark_node_t xkbm;

void benchmark_run(benchmark_node_t * benchmark);
void benchmark_push_children(benchmark_node_t * parent, benchmark_node_t * children);
int  benchmark_recursive_name(benchmark_node_t * benchmark, char recursive_name[BENCHMARK_NAME_MAX_LEN]);

# endif /* __BENCHMARK_H__ */
