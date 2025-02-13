/* ************************************************************************** */
/*                                                                            */
/*   conf.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:01:13 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/min-max.h>
# include <xkrt/conf/conf.h>
# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdlib.h>
# include <string.h>

static void
__parse_verbose(xkrt_conf_t * conf, char const * value)
{
    if (value)
        LOGGER_VERBOSE = atoi(value);
}

static void
__parse_merge_transfers(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->merge_transfers = atoi(value) ? true : false;
}

static void
__parse_nkernels(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_KERN].concurrency = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_nstreams(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_KERN].n = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_ngpus(xkrt_conf_t * conf, char const * value)
{
    conf->device.ngpus = value ? (uint8_t) atoi(value) : 1;
}

static void
__parse_gpu_mem_percent(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.gpu_mem_percent = (float) atof(value);
}

static void
__parse_offloader_capacity(xkrt_conf_t * conf, char const * value)
{
    if (value)
    {
        conf->device.offloader.capacity = (uint16_t) atoi(value);
        LOGGER_INFO("Set offloader capacity to %d", conf->device.offloader.capacity);
    }
}

void __parse_help(xkrt_conf_t * conf, char const * value);

extern char ** environ;

typedef struct  xkrt_conf_parse_t
{
    char const * name;
    void (*parse)(xkrt_conf_t * conf, char const * value);
    char const * descr;
}               xkrt_conf_parse_t;

// variables are parsed in-order
static xkrt_conf_parse_t CONF_PARSE[] = {
    {"XKRT_HELP",                 __parse_help,               "Show this helper"},
    {"XKRT_VERBOSE",              __parse_verbose,            "Verbosity level (the higher the most)"},
    {"XKRT_MERGE_TRANSFERS",      __parse_merge_transfers,    "Merge memory transfers over continuous virtual memory"},
    {"XKRT_PRECISION",            NULL,                       NULL},
    {"XKRT_NGPUS",                __parse_ngpus,              "Number of GPUs to use"},
    {"XKRT_GPU_MEM_PERCENT",      __parse_gpu_mem_percent,    "%% of total memory to allocate initially per GPU (in ]0..100["},
    {"XKRT_NSTREAMS",             __parse_nstreams,           "Number of concurrent kernel streams per GPU"},
    {"XKRT_NKERNELS",             __parse_nkernels,           "Number of concurrent kernels per stream"},
    {"XKRT_CACHE_LIMIT",          NULL,                       NULL},
    {"XKRT_OFFLOADER_CAPACITY",   __parse_offloader_capacity, "Maximum number of pending instructions per stream"},
    {"XKRT_DEFAULT_MATH",         NULL,                       NULL},
    {NULL,                       NULL,                       NULL}
};

void
__parse_help(xkrt_conf_t * conf, char const * value)
{
    if (value)
    {
        LOGGER_INFO("Available environment variables");
        for (xkrt_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
            LOGGER_INFO("  '%s' - %s", var->name, var->descr);
    }
}

void
xkrt_init_conf(xkrt_conf_t * conf)
{
    // set default conf
    conf->device.ngpus              = (uint8_t)-1;
    conf->device.gpu_mem_percent    = (float) 50.0;

    //////////////////
    //  KERNEL CONF //
    //////////////////
    conf->device.offloader.capacity = 512;

    conf->device.offloader.streams[XKRT_STREAM_TYPE_KERN].n = 4;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_KERN].concurrency = 8;

    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2D].n = 2;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2D].concurrency = 0;

    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2H].n = 2;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2H].concurrency = 0;

    conf->device.offloader.streams[XKRT_STREAM_TYPE_H2D].n = 2;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_H2D].concurrency = 0;

    //////////////////
    //  DEVICE CONF //
    //////////////////

    // check all environment variable and report unknown variables
    for (char ** s = environ; *s; ++s)
    {
        int error = 1;
        char const * ss = strchr(*s, '=');
        int len = (int)(ss - *s);
        for (xkrt_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
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

    // set variables
    for (xkrt_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
    {
        if (var->parse)
            var->parse(conf, getenv(var->name));
        else
            LOGGER_NOT_IMPLEMENTED_WARN(var->name);
    }
}
