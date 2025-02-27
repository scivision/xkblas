# include <xkbm/backend.h>
# include <xkbm/benchmark.h>
# include <xkbm/topology.h>

# include <xkrt/xkrt-support.h>
# include <xkrt/runtime.h>
# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>

hwloc_topology_t TOPOLOGY;

void  cuda_benchmark_push(benchmark_node_t *);

void hwloc_benchmark_push(benchmark_node_t *);

void    ze_benchmark_init(void);
void    ze_benchmark_deinit(void);
void    ze_benchmark_push(benchmark_node_t *);

void  xkrt_benchmark_init(void);
void  xkrt_benchmark_deinit(void);
void  xkrt_benchmark_push(benchmark_node_t *);

backend_t backends[] = {
    {"XKRT",    1,                  XKRT_DRIVER_TYPE_MAX , xkrt_benchmark_push , xkrt_benchmark_init, xkrt_benchmark_deinit    },
    //{"ZE",      XKRT_SUPPORT_ZE,    XKRT_DRIVER_TYPE_ZE  , ze_benchmark_push   , ze_benchmark_init  , ze_benchmark_deinit      },
    //{"HWLOC",   1,                  XKRT_DRIVER_TYPE_MAX , hwloc_benchmark_push, NULL               , NULL                     },
//    {"CUDA",    XKRT_SUPPORT_CUDA,  XKRT_DRIVER_TYPE_CUDA, cuda_benchmark_push , NULL               , NULL                     },
};

benchmark_node_t xkbm = {
    .name = "xkbm",
    .desc = "The XKBM benchmarks suite",
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 1,
};

int
main(int argc, char ** argv)
{
    LOGGER_INFO("----------------------------------");
    LOGGER_INFO("%9s | %9s", "Backend", "supported");
    LOGGER_INFO("----------------------------------");
    for (int i = 0 ; i < sizeof(backends) / sizeof(backend_t) ; ++i)
    {
        backend_t * backend = backends + i;
        const int supported = backend->supported ? 1 : 0;
        const char * yn[] = {"no", "yes"};
        LOGGER_INFO("%9s | %9s", backend->name, yn[supported]);
    }
    LOGGER_INFO("----------------------------------");

    HWLOC_SAFE_CALL(hwloc_topology_init(&TOPOLOGY));
    HWLOC_SAFE_CALL(hwloc_topology_load( TOPOLOGY));

    for (int i = 0 ; i < sizeof(backends) / sizeof(backend_t) ; ++i)
    {
        backend_t * backend = backends + i;
        backend->benchmark_push(&xkbm);
        if (backend->init)
            backend->init();
    }

    benchmark_run(&xkbm);

    for (int i = 0 ; i < sizeof(backends) / sizeof(backend_t) ; ++i)
    {
        backend_t * backend = backends + i;
        if (backend->deinit)
            backend->deinit();
    }

    hwloc_topology_destroy(TOPOLOGY);

    return 0;
}
