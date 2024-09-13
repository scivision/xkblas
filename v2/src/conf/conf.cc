# include "conf/conf.h"
# include "logger/logger.h"

# include <assert.h>
# include <stdlib.h>
# include <string.h>

static void
__parse_verbose(xkblas_conf_t * conf, char const * value)
{
    if (value)
        XKBLAS_VERBOSE = atoi(value);
}

static void
__parse_tile_size(xkblas_conf_t * conf, char const * value)
{
    if (value)
    {
        char kernel[32];
        int m, n;
        int r = sscanf(value, "%31[^()]%*c%d,%d", kernel, &m, &n);
        if (r != 3)
            XKBLAS_FATAL("Invalid `XKBLAS_TILE_SIZE` - parsed %s(%d,%d)", kernel, m, n);

        XKBLAS_DEBUG("Setting default tile size for `%s` to `(%d, %d)`", kernel, m, n);
        if (strcmp(kernel, "gemm") == 0)
        {
            conf->kernels.gemm.tile[0] = m;
            conf->kernels.gemm.tile[1] = n;
        }
        else
        {
            XKBLAS_FATAL("Unknown kernel `%s` set in `XKBLAS_TILE_SIZE`", kernel);
        }
    }
}

static void
__parse_ngpus(xkblas_conf_t * conf, char const * value)
{
    if (value)
        conf->ngpus = atoi(value);
    else
        conf->ngpus = UINT8_MAX;
}

static void
__parse_gpuset(xkblas_conf_t * conf, char const * value)
{
    if (value)
    {
        unsigned int gpuset = atoi(value);
        if (__builtin_popcount(gpuset) < conf->ngpus)
            conf->ngpus = __builtin_popcount(gpuset);
        else if (conf->ngpus ==0)
            gpuset = 0;
        else
        {
            /* take only the first ngpus bits to 1 in gpuset */
            int tmp = gpuset;
            int idx = 0;
            for (int i=0; i<conf->ngpus; ++i)
            {
                idx = __builtin_ffs((unsigned int)tmp);
                assert( idx != 0);
                --idx;
                tmp &= ~(1<<idx);
            }
            /* here idx == index of the ngpus bits to 1 in gpuset */
            gpuset &= ((1<<(1+idx))-1);
        }
        conf->gpu_set = gpuset;
        assert(__builtin_popcount(conf->gpu_set)  == conf->ngpus);
    }
    else
    {
        conf->gpu_set = (1 << conf->ngpus) - 1;
    }
}

void __parse_help(xkblas_conf_t * conf, char const * value);

extern char ** environ;

typedef struct  xkblas_conf_parse_t
{
    char const * name;
    void (*parse)(xkblas_conf_t * conf, char const * value);
    char const * descr;
}               xkblas_conf_parse_t;


// variables are parsed in-order
static xkblas_conf_parse_t CONF_PARSE[] = {
    {"XKBLAS_HELP",         __parse_help,       "Show this helper"},
    {"XKBLAS_VERBOSE",      __parse_verbose,    "Verbosity level (the higher the most)"},
    {"XKBLAS_TILE_SIZE",    __parse_tile_size,  "Tile size parameter - format is `kernel(m,n)` - example: gemm(16,16)"},
    {"XKBLAS_PRECISION",    NULL,               NULL},
    {"XKBLAS_NGPUS",        __parse_ngpus,      "Number of GPUs to use"},
    {"XKBLAS_GPUSET",       __parse_gpuset,     "A bitmask representing GPUs to use"},
    {"XKBLAS_NSTREAMS",     NULL,               NULL},
    {"XKBLAS_NKERNELS",     NULL,               NULL},
    {"XKBLAS_CACHE_LIMIT",  NULL,               NULL},
    {"XKBLAS_DEFAULT_MATH", NULL,               NULL},
    {NULL,                  NULL,               NULL}
};

void
__parse_help(xkblas_conf_t * conf, char const * value)
{
    if (value)
    {
        XKBLAS_INFO("Available environment variables");
        for (xkblas_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
            XKBLAS_INFO("  '%s' - %s", var->name, var->descr);
    }
}

void
xkblas_init_conf(xkblas_conf_t * conf)
{
    // set default conf
    conf->stackblocsize     = (uint64_t)-1;
    conf->ngpus             = (uint8_t)-1;
    conf->gpu_set           = (uint32_t) ~0;
    conf->cuda_cache_limit  = 0.98f;

    //////////////////
    //  KERNEL CONF //
    //////////////////
    conf->device.offloader.capacity = 64;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].n = 1;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].concurrency = 8;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2D].n = 1;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2D].concurrency = 0;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2H].n = 1;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2H].concurrency = 0;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_H2D].n = 1;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_H2D].concurrency = 0;

    //////////////////
    //  DEVICE CONF //
    //////////////////
    conf->kernels.gemm.tile[0] = 0;
    conf->kernels.gemm.tile[1] = 0;

    // check all environment variable and report unknown variables
    for (char ** s = environ; *s; ++s)
    {
        if (strstr(*s, "XKBLAS_"))
        {
            int error = 1;
            char const * ss = strchr(*s, '=');
            int len = (int)(ss - *s);
            for (xkblas_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
            {
                char const * env;
                if (strncmp(*s, var->name, len))
                {
                    error = 0;
                    break ;
                }
            }
            if (error)
                XKBLAS_WARN("Unknown environment variable '%s'", *s);
        }
    }

    // set variables
    for (xkblas_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
    {
        if (var->parse)
            var->parse(conf, getenv(var->name));
        else
            XKBLAS_NOT_IMPLEMENTED_WARN(var->name);
    }
}
