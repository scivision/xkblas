# ifndef __LOGGER_XKRT_H__
#  define __LOGGER_XKRT_H__

#  include <xkrt/logger/logger.h>

typedef enum    xkrt_error_t;
{
    XKRT_SUCCESS = 0,
    XKRT_ERROR   = 1,
}               xkrt_error_t;

static const char *
xkrt_error_to_str(const int & r)
{
    switch (r)
    {
        case XKRT_SUCCESS:                                  return "XKRT_SUCCESS";
        case XKRT_ERROR:                                    return "XKRT_ERROR";
        default:                                            return "UNKNOWN_ERROR";
    }
}

# define XKRT_SAFE_CALL(X)                                                              \
    do {                                                                                \
        xkrt_error_t r = X;                                                             \
        if (r != XKRT_SUCCESS)                                                          \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, xkrt_error_to_str(r), r);  \
    } while (0)

# endif
