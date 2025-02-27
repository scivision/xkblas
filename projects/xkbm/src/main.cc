# include <xkbm/backend.h>
# include <xkbm/benchmark.h>
# include <xkbm/topology.h>

# include <xkrt/xkrt-support.h>
# include <xkrt/runtime.h>
# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>

hwloc_topology_t TOPOLOGY;

////////////////
// BENCHMARKS //
////////////////

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

//////////
// CONF //
//////////

# include <fstream>
# include <nlohmann/json.hpp>

using json = nlohmann::json;
static const char * XKBM_CONF = "conf.json";
static const char * XKBM_EXPORT_CONF = "XKBM_EXPORT_CONF";

static json
export_conf_for_bench(benchmark_node_t * bench)
{
    json data;

    data["name"] = bench->name;
    data["desc"] = bench->desc;
    data["enabled"] = 0;

    std::vector<json> children;
    for (int i = 0 ; i < bench->nchildren; ++i)
    {
        benchmark_node_t * child = bench->children[i];
        children.push_back(export_conf_for_bench(child));
    }

    data["list"] = children;

    return data;
}

static void
export_conf(benchmark_node_t * bench)
{
    LOGGER_INFO("Creating conf file to `%s`", XKBM_CONF);

    json data;

    data["benchmarks"] = export_conf_for_bench(bench);
    std::ofstream o(XKBM_CONF);
    if (o.good())
    {
        o << std::setw(2) << data << std::endl;
        LOGGER_INFO("Exporting empty conf to `%s`", XKBM_CONF);
    }
    else
        LOGGER_FATAL("Could not write to `%s`", XKBM_CONF);
    LOGGER_INFO("Conf was exported. Update it and restart the program");
    exit(0);
}

static void
load_conf_for_bench(benchmark_node_t * bench, json & data)
{
    if (bench->name != data["name"])
        LOGGER_FATAL("Mismatch between conf. and program versions. Regenerate a new conf with `%s=1`", XKBM_EXPORT_CONF);

    bench->enabled = data["enabled"];
    if (bench->enabled)
    {
        char name[BENCHMARK_NAME_MAX_LEN] = { 0 };
        benchmark_recursive_name(bench, name);
        LOGGER_DEBUG("Enabled `%s`", name);

        for (int i = 0 ; i < bench->nchildren; ++i)
        {
            benchmark_node_t * child = bench->children[i];
            load_conf_for_bench(child, data["list"][i]);
        }
    }
}

void
load_conf(benchmark_node_t * bench)
{
    if (getenv(XKBM_EXPORT_CONF))
        export_conf(bench);

    std::ifstream f(XKBM_CONF);
    if (f.good() && f.peek() != std::ifstream::traits_type::eof())
    {
        json data = json::parse(f);
        load_conf_for_bench(bench, data["benchmarks"]);
    }
    else
        export_conf(bench);
}

//////////
// MAIN //
//////////

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

    // init
    for (int i = 0 ; i < sizeof(backends) / sizeof(backend_t) ; ++i)
    {
        backend_t * backend = backends + i;
        backend->benchmark_push(&xkbm);
        if (backend->init)
            backend->init();
    }

    // load conf
    load_conf(&xkbm);

    // run
    benchmark_run(&xkbm);

    // deinit
    for (int i = 0 ; i < sizeof(backends) / sizeof(backend_t) ; ++i)
    {
        backend_t * backend = backends + i;
        if (backend->deinit)
            backend->deinit();
    }

    hwloc_topology_destroy(TOPOLOGY);

    return 0;
}
