#ifndef __XKBLAS_CALLBACK_H__
# define __XKBLAS_CALLBACK_H__

# define XKBLAS_CALLBACK_ARGS_MAX 5

typedef struct  xkblas_callback_t
{
    void (*func)(const void * [XKBLAS_CALLBACK_ARGS_MAX]);
    const void * args[XKBLAS_CALLBACK_ARGS_MAX];
}               xkblas_callback_t;

# endif /* __XKBLAS_CALLBACK_H__ */
