#ifndef __KAAPI_TASK_DEPENDENCES_H__
# define __KAAPI_TASK_DEPENDENCES_H__

# ifdef __cplusplus
#  define EXTERNC extern "C"
# else
#  define EXTERNC
# endif

# include "kaapi.h"

EXTERNC void
kaapi_dependences_cartesian2D(
    kaapi_thread_t * thread,
    kaapi_task_t * task,
    kaapi_access_mode_t mode,
    int x0, int y0,
    int x1, int y1
);

EXTERNC void
kaapi_dependences_cartesian2D_3(
    kaapi_thread_t * thread,
    kaapi_task_t * task,
    kaapi_access_mode_t mode[3],
    int coords[3][4]
);

#undef EXTERNC

#endif /* __KAAPI_TASK_DEPENDENCES_H__ */
