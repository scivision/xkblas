# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>

typedef struct  omp_task_args_t
{
    kmp_task_t kmp_task;
}               omp_task_args_t;

constexpr size_t
round_up_to_val(size_t size, size_t val)
{
    if (size & (val - 1))
    {
        size &= ~(val - 1);
        if (size <= SIZE_MAX - val)
        {
            size += val;
        }
    }
    return size;
}

constexpr int AC = 0;
constexpr task_flag_bitfield_t xkflags = TASK_FLAG_ZERO;
constexpr size_t task_size = task_compute_size(xkflags, AC);
constexpr size_t args_size = sizeof(omp_task_args_t);
constexpr size_t total_size = round_up_to_val(task_size + args_size, sizeof(void *));
constexpr size_t shareds_offset = total_size - task_size - args_size;

extern "C"
kmp_task_t *
__kmpc_omp_task_alloc(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_int32 flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    task_t * task = thread->allocate_task(total_size + sizeof_shareds);
    new(task) task_t(xkomp->task_format, xkflags);

    omp_task_args_t * args = (omp_task_args_t *) TASK_ARGS(task, task_size);
    assert(args);

    args->kmp_task.shareds = (sizeof_shareds > 0) ? ((char *) task) + total_size : NULL;
    args->kmp_task.routine = task_entry;
    args->kmp_task.part_id = 0;

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "omp-task");
    # endif /* NDEBUG */

    return (kmp_task_t *) args;
}

extern "C"
kmp_int32
__kmpc_omp_task(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_task_t * new_task
) {
    omp_task_args_t * args = (omp_task_args_t *) new_task;
    assert(args);
    assert(&args->kmp_task == new_task);

    task_t * task = (task_t *) (((char *) args) - task_size);
    assert(task);
    assert(TASK_ARGS(task) == (void *) args);

    xkomp_t * xkomp = xkomp_get();
    assert(xkomp);

    xkomp->runtime.task_commit(task);

    return 0;
}

extern "C"
kmp_int32
__kmpc_omp_taskwait(
    ident_t * loc_ref,
    kmp_int32 gtid
) {
    xkomp_t * xkomp = xkomp_get();
    xkomp->runtime.task_wait();
    return 0;
}

static void
body_omp_task(task_t * task)
{
    omp_task_args_t * args = (omp_task_args_t *) TASK_ARGS(task);
    assert(args);

    args->kmp_task.routine(0, &args->kmp_task);
}

void
xkomp_task_register_format(xkomp_t * xkomp)
{
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_omp_task;
    snprintf(format.label, sizeof(format.label), "omp-task");
    xkomp->task_format = task_format_create(&(xkomp->runtime.formats.list), &format);
}
