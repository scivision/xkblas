# include <xkrt/logger/logger.h>

# include <assert.h>
# include <kmp.h>

extern "C"
kmp_task_t *
__kmpc_omp_target_task_alloc(
    ident_t * loc_ref,
    kmp_int32 gtid,
    kmp_int32 flags,
    size_t sizeof_kmp_task_t,
    size_t sizeof_shareds,
    kmp_routine_entry_t task_entry,
    kmp_int64 device_id
) {
    LOGGER_NOT_IMPLEMENTED();

    kmp_task_t * task = (kmp_task_t *) malloc(sizeof(kmp_task_t));
    assert(task);

    return task;
}
