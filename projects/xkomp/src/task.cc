# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>

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
    LOGGER_NOT_IMPLEMENTED();

    kmp_task_t * task = (kmp_task_t *) malloc(sizeof(kmp_task_t));
    assert(task);

    return task;
}

extern "C"
kmp_int32
__kmpc_omp_task(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_task_t * new_task)
{
    LOGGER_NOT_IMPLEMENTED();
    return 0;
}

extern "C"
kmp_int32
__kmpc_omp_taskwait(
    ident_t * loc_ref,
    kmp_int32 gtid
) {
    runtime->task_wait();
    return 0;
}
