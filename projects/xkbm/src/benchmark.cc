/* ************************************************************************** */
/*                                                                            */
/*   benchmark.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/18 22:56:54 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:39:31 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkbm/benchmark.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <string.h>

void
benchmark_run(benchmark_node_t * benchmark)
{
    if (!benchmark->enabled)
        return ;

    if (benchmark->run)
    {
        char recursive_name[BENCHMARK_NAME_MAX_LEN] = { 0 };
        benchmark_recursive_name(benchmark, recursive_name);
        LOGGER_INFO("----------------------------------");
        LOGGER_INFO("Running %s", recursive_name);
        LOGGER_INFO("----------------------------------");
        benchmark->run(benchmark);
    }

    for (int i = 0 ; i < benchmark->nchildren; ++i)
    {
        benchmark_node_t * child = benchmark->children[i];
        benchmark_run(child);
    }
}

void
benchmark_push_children(benchmark_node_t * parent, benchmark_node_t * children)
{
    assert(parent->nchildren < BENCHMARK_MAX_CHILDREN);
    parent->children[parent->nchildren++] = children;
    parent->children[parent->nchildren] = NULL;
    children->parent = parent;

    char recursive_name[BENCHMARK_NAME_MAX_LEN] = { 0 };
    benchmark_recursive_name(children, recursive_name);
    LOGGER_DEBUG("Inserted new benchmark `%s`", recursive_name);
}

int
benchmark_recursive_name(benchmark_node_t * benchmark, char recursive_name[BENCHMARK_NAME_MAX_LEN])
{
    int r = 0;
    if (benchmark->parent)
        r = benchmark_recursive_name(benchmark->parent, recursive_name);

    const int recursive_len = strlen(recursive_name);
    const int benchmark_len = strlen(benchmark->name);
    assert(recursive_len + 1 + benchmark_len < BENCHMARK_NAME_MAX_LEN);
    r += snprintf(recursive_name + recursive_len, BENCHMARK_NAME_MAX_LEN - recursive_len - 1 - 1, ".%s", benchmark->name);

    return r;
}
