# include <xkrt/logger/logger.h>
# include <kmp.h>
# include <stdint.h>

extern "C"
kmp_int32
__kmpc_global_thread_num(ident_t * loc)
{
    LOGGER_NOT_IMPLEMENTED();
    return 0;
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
    LOGGER_NOT_IMPLEMENTED();
    return 0;
}
