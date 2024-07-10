# include "conf/conf.h"
# include "logger/logger.h"

# include <assert.h>
# include <stdlib.h>
# include <string.h>

// Runtime configuration
xkblas_conf_t XKBLAS_CONF = {
    .stackblocsize              = (uint64_t)-1,
    .ncpus                      = 1,
    .ngpus                      = 1,
    .gpu_set                    = (uint32_t) ~0,
    .cuda_stream_capacity       = 64,
    .cuda_conc_stream_kernel    = 2,
    .cuda_conc_kernel           = 8,
    .cuda_conc_h2d              = 1,
    .cuda_conc_d2h              = 1,
    .cuda_conc_d2d              = 1,
    .cuda_cache_limit           = 0.98
};

extern char ** environ;

typedef struct  xkblas_conf_parse_t
{
    char const * name;
    void (*parse)(char const * value);
    char const * descr;
}               xkblas_conf_parse_t;

static void
__parse_verbose(char const * value)
{
    if (value)
        XKBLAS_VERBOSE = atoi(value);
}

static void
__parse_ngpus(char const * value)
{
    if (value)
        XKBLAS_CONF.ngpus = atoi(value);
}

static void
__parse_gpuset(char const * value)
{
    if (value)
    {
        unsigned int gpuset = atoi(value);
        if (__builtin_popcount(gpuset) < XKBLAS_CONF.ngpus)
            XKBLAS_CONF.ngpus = __builtin_popcount(gpuset);
        else if (XKBLAS_CONF.ngpus ==0)
            gpuset = 0;
        else
        {
            /* take only the first ngpus bits to 1 in gpuset */
            int tmp = gpuset;
            int idx = 0;
            for (int i=0; i<XKBLAS_CONF.ngpus; ++i)
            {
                idx = __builtin_ffs((unsigned int)tmp);
                assert( idx != 0);
                --idx;
                tmp &= ~(1<<idx);
            }
            /* here idx == index of the ngpus bits to 1 in gpuset */
            gpuset &= ((1<<(1+idx))-1);
        }
        XKBLAS_CONF.gpu_set = gpuset;
        assert(__builtin_popcount(XKBLAS_CONF.gpu_set)  == XKBLAS_CONF.ngpus);
    }
    else
    {
        XKBLAS_CONF.gpu_set = (1 << XKBLAS_CONF.ngpus) - 1;
    }
}

// TODO : implement this
void __parse_help(char const * value);

// variables are parsed in-order
static xkblas_conf_parse_t CONF_PARSE[] = {
    {"XKBLAS_HELP",         __parse_help,       "Show this helper"},
    {"XKBLAS_VERBOSE",      __parse_verbose,    "Verbosity level (the higher the most)"},
    {"XKBLAS_TILE_SIZE",    NULL,               NULL},
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
__parse_help(char const * value)
{
    if (value)
    {
        XKBLAS_INFO("Available environment variables");
        for (xkblas_conf_parse_t * var = CONF_PARSE ; var->name ; ++var)
            XKBLAS_INFO("  '%s' - %s", var->name, var->descr);
    }
}

void
xkblas_init_conf(void)
{
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
            var->parse(getenv(var->name));
        else
            XKBLAS_NOT_IMPLEMENTED_WARN(var->name);
    }

}
