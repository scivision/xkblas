/* ************************************************************************** */
/*                                                                            */
/*   benchmarks.cc                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 20:35:24 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/22 01:23:23 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkbm/allocator.h>
# include <xkbm/benchmark.h>
# include <xkbm/time.h>
# include <xkbm/combinator.h>
# include <xkbm/pp.h>

# include <xkrt/logger/logger.h>
# include <xkrt/logger/logger-aml.h>
# include <xkrt/logger/metric.h>

# include <stddef.h>

# include <aml.h>
# include <aml/utils/features.h>

/////////
// H2D //
/////////

# if 0
typedef struct  aml_h2d_t
{
    struct aml_area dst;
}               aml_h2d_t;

static const aml_h2d_t AML_H2D[] = {
    # if AML_HAVE_BACKEND_CUDA
    { &aml_area_cuda },
    # endif /* AML_HAVE_BACKEND_CUDA */
};


static void
bench_h2d(benchmark_node_t * node)
{
    constexpr size_t size = 1 * 1024 * 1024 * 1024; // 1GB
    void * srcbuf = xkbm_alloc_and_touch(size);
        !! TODO
    struct aml_dma           dma;
    struct aml_dma_request * req;
aml_dma_copy

    struct aml_layout src;
    struct aml_layout dst;


    const aml_dma_operator op      = NULL;
    const void *           op_args = NULL;
    AML_SAFE_CALL(aml_dma_copy_custom(&dma, &dst, &src, op, op_arg));

    AML_SAFE_CALL(aml_dma_wait(&dma, &req));
}

static benchmark_node_t h2d = {
    .name = "h2d",
    .desc = "H2D transfer",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = bench_h2d,
    .enabled = 1
};
    # endif

////////////////////
// AML BENCHMARKS //
////////////////////

static benchmark_node_t aml = {
    .name = "aml",
    .desc = "Metrics through AML",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 1
};

void
aml_benchmark_push(benchmark_node_t * parent)
{
    benchmark_push_children(parent, &aml);

    # define LINK(X, Y) benchmark_push_children(&(X), &(Y))
//    LINK(aml, &h2d);
}

void
aml_benchmark_deinit(void)
{
    AML_SAFE_CALL(aml_finalize());
}

void
aml_benchmark_init(void)
{
    AML_SAFE_CALL(aml_init(NULL, NULL));
    if ((aml_version_major != AML_VERSION_MAJOR) || (aml_version_minor != AML_VERSION_MINOR))
        LOGGER_FATAL("AML version used to compile and run the program are not compatible");
}
