#ifndef __STREAM_CALLBACK_H__
# define __STREAM_CALLBACK_H__

# define XKBLAS_STREAM_CALLBACK_ARGS_MAX 3

typedef struct  xkblas_stream_callback_t
{
    void (*func)(void * [XKBLAS_STREAM_CALLBACK_ARGS_MAX]);
    const void * args[XKBLAS_STREAM_CALLBACK_ARGS_MAX];
}               xkblas_stream_callback_t;

# endif /* __STREAM_CALLBACK_H__ */
