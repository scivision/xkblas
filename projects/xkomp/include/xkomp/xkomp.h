#ifndef __XKOMP_H__
# define __XKOMP_H__

# include <xkrt/xkrt.h>

/* environment variables parsed at program starts */
typedef struct  xkomp_env_t
{
    char OMP_DISPLAY_ENV;
     int OMP_NUM_THREADS;
     int OMP_THREAD_LIMIT;

}               xkomp_env_t;

/** global variable that holds the entire openmp context */
typedef struct  xkomp_t
{
    /* underlaying XKaapi runtime */
    xkrt_runtime_t runtime;

    /* environment variables */
    xkomp_env_t env;

}               xkomp_t;

extern xkomp_t * xkomp;
xkomp_t * xkomp_get(void);

/** load env variables */
void xkomp_env_init(xkomp_env_t * env);

# endif /* __XKOMP_H__ */
