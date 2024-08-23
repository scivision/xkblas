#ifndef __TASK_FORMAT_H__
# define __TASK_FORMAT_H__

# include <stdint.h>

# if 0

enum task_kern_t : uint8_t
{
    TASK_KERN_NOOP      = 0,

    TASK_KERN_GEMM      = 1,
    TASK_KERN_TRSM      = 2,
    TASK_KERN_COPYSCALE = 3,

    TASK_KERN_MAX       = 4,
};

enum task_kern_precision_t : uint8_t
{
    TASK_KERN_PRECISION_NONE = 0,
    TASK_KERN_PRECISION_B    = 1,    /* byte */
    TASK_KERN_PRECISION_S    = 2,    /* float */
    TASK_KERN_PRECISION_C    = 3,    /* float complex */
    TASK_KERN_PRECISION_D    = 4,    /* double */
    TASK_KERN_PRECISION_Z    = 5,    /* double complex */
};

# endif

/* maximum number of function version per task */
# define TASK_FORMAT_FUNC_MAX 4

typedef struct  task_format_t
{
    # if 0
    /* kernel infos */
    task_kern_t kern;
    task_kern_precision_t precision;
    # endif

    /* kernel launch */
    void (*f[TASK_FORMAT_FUNC_MAX])(void * args);

    /* a label */
    char label[16];

}               task_format_t;

/* maximum number of task format */
typedef uint8_t task_format_id_t;
# define TASK_FORMAT_MAX ((1 << (sizeof(task_format_id_t) * 8)) - 1)
extern task_format_t task_formats[TASK_FORMAT_MAX];

# define TASK_FORMAT_NULL 0

task_format_id_t task_format_create(task_format_t * format);
task_format_t * task_format_get(task_format_id_t id);

#endif /* __TASK_FORMAT_H__ */
