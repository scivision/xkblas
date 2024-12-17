/* ************************************************************************** */
/*                                                                            */
/*   conf.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

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
__parse_merge_transfers(xkblas_conf_t * conf, char const * value)
{
    if (value)
        conf->merge_transfers = atoi(value) ? true : false;
}

static void
__parse_nkernels(xkblas_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].concurrency = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_nstreams(xkblas_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].n = (uint8_t) MAX(atoi(value), 1);
}



static void
__parse_tile_size(xkblas_conf_t * conf, char const * value)
{
    if (value)
    {
        char kernel[32];
        int ts;
        int r = sscanf(value, "%31[^()]%*c%d", kernel, &ts);
        if (r != 2)
            XKBLAS_FATAL("Invalid `XKBLAS_TILE_SIZE` - parsed %s(%d)", kernel, ts);

        XKBLAS_DEBUG("Setting tile size for `%s` to `%d`", kernel, ts);
        if (strcmp(kernel, "gemm") == 0)
            conf->kernels[XKBLAS_KERNEL_TYPE_GEMM].tile = ts;
        else if (strcmp(kernel, "trsm") == 0)
            conf->kernels[XKBLAS_KERNEL_TYPE_TRSM].tile = ts;
        else
            XKBLAS_FATAL("Unknown kernel `%s` set in `XKBLAS_TILE_SIZE`", kernel);
    }
}

static void
__parse_ngpus(xkblas_conf_t * conf, char const * value)
{
    conf->ngpus = value ? (uint8_t) atoi(value) : 1;
}

static void
__parse_gpuset(xkblas_conf_t * conf, char const * value)
{
    if (value)
    {
        unsigned int gpuset = atoi(value);
        if (__builtin_popcount(gpuset) < conf->ngpus)
            conf->ngpus = (uint8_t) __builtin_popcount(gpuset);
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

static void
__parse_gpu_mem_percent(xkblas_conf_t * conf, char const * value)
{
    if (value)
        conf->gpu_mem_percent = (float) atof(value);
}

static void
__parse_offloader_capacity(xkblas_conf_t * conf, char const * value)
{
    if (value)
    {
        conf->device.offloader.capacity = (uint16_t) atoi(value);
        XKBLAS_INFO("Set offloader capacity to %d", conf->device.offloader.capacity);
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
    {"XKBLAS_HELP",                 __parse_help,               "Show this helper"},
    {"XKBLAS_VERBOSE",              __parse_verbose,            "Verbosity level (the higher the most)"},
    {"XKBLAS_MERGE_TRANSFERS",      __parse_merge_transfers,    "Merge memory transfers over continuous virtual memory"},
    {"XKBLAS_TILE_SIZE",            __parse_tile_size,          "Tile size parameter - format is `kernel(m,n)` - example: gemm(16,16)"},
    {"XKBLAS_PRECISION",            NULL,                       NULL},
    {"XKBLAS_NGPUS",                __parse_ngpus,              "Number of GPUs to use"},
    {"XKBLAS_GPUSET",               __parse_gpuset,             "A bitmask representing GPUs to use"},
    {"XKBLAS_GPU_MEM_PERCENT",      __parse_gpu_mem_percent,    "%% of total memory to allocate initially per GPU (in ]0..100["},
    {"XKBLAS_NSTREAMS",             __parse_nstreams,           "Number of concurrent kernel streams per GPU"},
    {"XKBLAS_NKERNELS",             __parse_nkernels,           "Number of concurrent kernels per stream"},
    {"XKBLAS_CACHE_LIMIT",          NULL,                       NULL},
    {"XKBLAS_OFFLOADER_CAPACITY",   __parse_offloader_capacity, "Maximum number of pending instructions per stream"},
    {"XKBLAS_DEFAULT_MATH",         NULL,                       NULL},
    {NULL,                          NULL,                       NULL}
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
    conf->gpu_mem_percent   = (float) 92.0;
    conf->merge_transfers   = false;

    //////////////////
    //  KERNEL CONF //
    //////////////////
    conf->device.offloader.capacity = 512;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].n = 4;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].concurrency = 8;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2D].n = 2;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2D].concurrency = 0;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2H].n = 2;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_D2H].concurrency = 0;

    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_H2D].n = 2;
    conf->device.offloader.streams[XKBLAS_STREAM_TYPE_H2D].concurrency = 0;

    //////////////////
    //  DEVICE CONF //
    //////////////////
    for (int i = 0 ; i < XKBLAS_KERNEL_TYPE_MAX ; ++i)
        conf->kernels[i].tile = 0;

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
