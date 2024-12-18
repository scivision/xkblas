/* ************************************************************************** */
/*                                                                            */
/*   conf.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/18 14:27:22 by                           \_)     (_/    */
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
__parse_verbose(ptr_conf_t * conf, char const * value)
{
    if (value)
        LOGGER_VERBOSE = atoi(value);
}

static void
__parse_merge_transfers(ptr_conf_t * conf, char const * value)
{
    if (value)
        conf->merge_transfers = atoi(value) ? true : false;
}

static void
__parse_nkernels(ptr_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[PTR_STREAM_TYPE_KERN].concurrency = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_nstreams(ptr_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[PTR_STREAM_TYPE_KERN].n = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_ngpus(ptr_conf_t * conf, char const * value)
{
    conf->ngpus = value ? (uint8_t) atoi(value) : 1;
}

static void
__parse_gpuset(ptr_conf_t * conf, char const * value)
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
__parse_gpu_mem_percent(ptr_conf_t * conf, char const * value)
{
    if (value)
        conf->gpu_mem_percent = (float) atof(value);
}

static void
__parse_offloader_capacity(ptr_conf_t * conf, char const * value)
{
    if (value)
    {
        conf->device.offloader.capacity = (uint16_t) atoi(value);
        LOGGER_INFO("Set offloader capacity to %d", conf->device.offloader.capacity);
    }
}

void __parse_help(ptr_conf_t * conf, char const * value);

extern char ** environ;

typedef struct  ptr_conf_parse_t
{
    char const * name;
    void (*parse)(ptr_conf_t * conf, char const * value);
    char const * descr;
}               ptr_conf_parse_t;


// variables are parsed in-order
static ptr_conf_parse_t CONF_PARSE[] = {
    {"PTR_HELP",                 __parse_help,               "Show this helper"},
    {"PTR_VERBOSE",              __parse_verbose,            "Verbosity level (the higher the most)"},
    {"PTR_MERGE_TRANSFERS",      __parse_merge_transfers,    "Merge memory transfers over continuous virtual memory"},
    {"PTR_PRECISION",            NULL,                       NULL},
    {"PTR_NGPUS",                __parse_ngpus,              "Number of GPUs to use"},
    {"PTR_GPUSET",               __parse_gpuset,             "A bitmask representing GPUs to use"},
    {"PTR_GPU_MEM_PERCENT",      __parse_gpu_mem_percent,    "%% of total memory to allocate initially per GPU (in ]0..100["},
    {"PTR_NSTREAMS",             __parse_nstreams,           "Number of concurrent kernel streams per GPU"},
    {"PTR_NKERNELS",             __parse_nkernels,           "Number of concurrent kernels per stream"},
    {"PTR_CACHE_LIMIT",          NULL,                       NULL},
    {"PTR_OFFLOADER_CAPACITY",   __parse_offloader_capacity, "Maximum number of pending instructions per stream"},
    {"PTR_DEFAULT_MATH",         NULL,                       NULL},
    {NULL,                          NULL,                       NULL}
};

void
__parse_help(ptr_conf_t * conf, char const * value)
{
    if (value)
    {
        LOGGER_INFO("Available environment variables");
        for (ptr_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
            LOGGER_INFO("  '%s' - %s", var->name, var->descr);
    }
}

void
ptr_init_conf(ptr_conf_t * conf)
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

    conf->device.offloader.streams[PTR_STREAM_TYPE_KERN].n = 4;
    conf->device.offloader.streams[PTR_STREAM_TYPE_KERN].concurrency = 8;

    conf->device.offloader.streams[PTR_STREAM_TYPE_D2D].n = 2;
    conf->device.offloader.streams[PTR_STREAM_TYPE_D2D].concurrency = 0;

    conf->device.offloader.streams[PTR_STREAM_TYPE_D2H].n = 2;
    conf->device.offloader.streams[PTR_STREAM_TYPE_D2H].concurrency = 0;

    conf->device.offloader.streams[PTR_STREAM_TYPE_H2D].n = 2;
    conf->device.offloader.streams[PTR_STREAM_TYPE_H2D].concurrency = 0;

    //////////////////
    //  DEVICE CONF //
    //////////////////

    // check all environment variable and report unknown variables
    for (char ** s = environ; *s; ++s)
    {
        if (strstr(*s, "PTR_"))
        {
            int error = 1;
            char const * ss = strchr(*s, '=');
            int len = (int)(ss - *s);
            for (ptr_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
            {
                char const * env;
                if (strncmp(*s, var->name, len))
                {
                    error = 0;
                    break ;
                }
            }
            if (error)
                LOGGER_WARN("Unknown environment variable '%s'", *s);
        }
    }

    // set variables
    for (ptr_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
    {
        if (var->parse)
            var->parse(conf, getenv(var->name));
        else
            LOGGER_NOT_IMPLEMENTED_WARN(var->name);
    }
}
