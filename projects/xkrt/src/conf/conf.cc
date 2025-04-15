/* ************************************************************************** */
/*                                                                            */
/*   conf.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/14 23:45:31 by Romain PEREIRA            \_)     (_/    */
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
    (void) conf;
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
__parse_kern_per_stream(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_KERN].concurrency = (uint32_t) MAX(atoi(value), 1);
}

static void
__parse_h2d_per_stream(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_H2D].concurrency = (uint32_t) MAX(atoi(value), 1);
}

static void
__parse_d2h_per_stream(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_D2H].concurrency = (uint32_t) MAX(atoi(value), 1);
}

static void
__parse_d2d_per_stream(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_D2D].concurrency = (uint32_t) MAX(atoi(value), 1);
}

static void
__parse_nstreams_h2d(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_H2D].n = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_nstreams_d2h(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_D2H].n = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_nstreams_d2d(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.streams[XKRT_STREAM_TYPE_D2D].n = (uint8_t) MAX(atoi(value), 1);
}

static void
__parse_nstreams_kern(xkrt_conf_t * conf, char const * value)
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

static void
__parse_nthreads_per_device(xkrt_conf_t * conf, char const * value)
{
    if (value)
        conf->device.offloader.nthreads_per_device = (uint8_t) atoi(value);

    if (conf->device.offloader.nthreads_per_device < 1)
    {
        conf->device.offloader.nthreads_per_device = 1;
        LOGGER_WARN("Invalid number of threads per device, set it to 1");
    }

    if (conf->device.offloader.nthreads_per_device > XKRT_MAX_THREADS_PER_DEVICE)
    {
        conf->device.offloader.nthreads_per_device = XKRT_MAX_THREADS_PER_DEVICE;
        LOGGER_WARN("Requested too many threads per device, increase `XKRT_MAX_THREADS_PER_DEVICE` and recompile if you want more threads per device");
    }
}

static void
__parse_stats(xkrt_conf_t * conf, char const * value)
{
    conf->report_stats_on_deinit = value ? atoi(value) : 0;
}

static void
__parse_drivers(xkrt_conf_t * conf, char const * value)
{
    conf->drivers_mask = value ? atoi(value) : ~0;
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
    {"XKRT_HELP",                 __parse_help,                "Show this helper"},
    {"XKRT_VERBOSE",              __parse_verbose,             "Verbosity level (the higher the most)"},
    {"XKRT_MERGE_TRANSFERS",      __parse_merge_transfers,     "Merge memory transfers over continuous virtual memory"},
    {"XKRT_PRECISION",            NULL,                        NULL},
    {"XKRT_NGPUS",                __parse_ngpus,               "Number of gpus to use"},
    {"XKRT_GPU_MEM_PERCENT",      __parse_gpu_mem_percent,     "%% of total memory to allocate initially per GPU (in ]0..100["},
    {"XKRT_NTHREADS_PER_DEVICE",  __parse_nthreads_per_device, "Number of threads per device to poll streams"},
    {"XKRT_NSTREAMS_H2D",         __parse_nstreams_h2d,        "Number of H2D streams per GPU"},
    {"XKRT_NSTREAMS_D2H",         __parse_nstreams_d2h,        "Number of D2H streams per GPU"},
    {"XKRT_NSTREAMS_D2D",         __parse_nstreams_d2d,        "Number of D2D streams per GPU"},
    {"XKRT_NSTREAMS_KERN",        __parse_nstreams_kern,       "Number of KERN streams per GPU"},
    {"XKRT_KERN_PER_STREAM",      __parse_kern_per_stream,     "Number of concurrent kernels per KERN stream before throttling device-thread"},
    {"XKRT_H2D_PER_STREAM",       __parse_h2d_per_stream,      "Number of concurrent copies per H2D stream before throttling device-thread"},
    {"XKRT_D2H_PER_STREAM",       __parse_d2h_per_stream,      "Number of concurrent copies per D2H stream before throttling device-thread"},
    {"XKRT_D2D_PER_STREAM",       __parse_d2d_per_stream,      "Number of concurrent copies per D2D stream before throttling device-thread"},
    {"XKRT_CACHE_LIMIT",          NULL,                        NULL},
    {"XKRT_OFFLOADER_CAPACITY",   __parse_offloader_capacity,  "Maximum number of pending instructions per stream"},
    {"XKRT_DEFAULT_MATH",         NULL,                        NULL},
    {"XKRT_STATS",                __parse_stats,               "Boolean to dump stats on deinit"},
    {"XKRT_DRIVERS",              __parse_drivers,             "A bitmask to set enabled drivers"},
    {NULL,                       NULL,                         NULL}
};

void
__parse_help(xkrt_conf_t * conf, char const * value)
{
    (void) conf;
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
    conf->report_stats_on_deinit    = 0;
    conf->device.ngpus              = (uint8_t)-1;
    conf->device.gpu_mem_percent    = (float) 90.0;
    conf->merge_transfers           = false; // true;

    //////////////////
    //  KERNEL CONF //
    //////////////////
    conf->device.offloader.capacity = 512;
    conf->device.offloader.nthreads_per_device = 1;

    // set to -1 so the driver's stream-suggest API fills these values if not
    // set by an env variable
    conf->device.offloader.streams[XKRT_STREAM_TYPE_KERN].n = -1;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_KERN].concurrency = 8;

    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2D].n = -1;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2D].concurrency = 64;

    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2H].n = -1;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_D2H].concurrency = 64;

    conf->device.offloader.streams[XKRT_STREAM_TYPE_H2D].n = -1;
    conf->device.offloader.streams[XKRT_STREAM_TYPE_H2D].concurrency = 64;

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
