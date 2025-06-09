#ifndef __XKOMP_H__
# define __XKOMP_H__

# include <xkrt/xkrt.h>

typedef struct  xkomp_t
{
    xkrt_runtime_t runtime;
}               xkomp_t;

extern xkomp_t * xkomp;

extern "C"
xkomp_t * xkomp_get(void);

# endif /* __XKOMP_H__ */
