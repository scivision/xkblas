# include <xkrt/logger/logger.h>

extern "C"
void
__tgt_target_data_begin_mapper(
    ident_t * Loc,
    int64_t DeviceId,
    int32_t ArgNum,
    void ** ArgsBase,
    void ** Args, int64_t * ArgSizes,
    int64_t * ArgTypes,
    map_var_info_t * ArgNames,
    void ** ArgMappers
) {
    LOGGER_NOT_IMPLEMENTED();
}

extern "C"
void __tgt_target_data_end_mapper(ident_t *Loc, int64_t DeviceId,
                                         int32_t ArgNum, void **ArgsBase,
                                         void **Args, int64_t *ArgSizes,
                                         int64_t *ArgTypes,
                                         map_var_info_t *ArgNames,
                                         void **ArgMappers) {
    LOGGER_NOT_IMPLEMENTED();
}

extern "C"
int
__tgt_target_teams_mapper(
    ident_t *Loc,
    int64_t DeviceId,
    void * HostPtr,
    uint32_t ArgNum,
    void ** ArgsBase,
    void ** Args,
    int64_t * ArgSizes,
    int64_t * ArgTypes,
    map_var_info_t * ArgNames,
    void ** ArgMappers,
    int32_t NumTeams,
    int32_t ThreadLimit
) {
    LOGGER_NOT_IMPLEMENTED();
    return 0;
}

extern "C"
int __tgt_target_mapper(ident_t *Loc, int64_t DeviceId, void *HostPtr,
                 uint32_t ArgNum, void **ArgsBase, void **Args,
                 int64_t *ArgSizes, int64_t *ArgTypes,
                 map_var_info_t *ArgNames, void **ArgMappers) {
    LOGGER_NOT_IMPLEMENTED();
    return 0;
}

extern "C"
void __tgt_target_data_update_mapper(ident_t *Loc, int64_t DeviceId,
                                            int32_t ArgNum, void **ArgsBase,
                                            void **Args, int64_t *ArgSizes,
                                            int64_t *ArgTypes,
                                            map_var_info_t *ArgNames,
                                            void **ArgMappers) {
    LOGGER_NOT_IMPLEMENTED();
}

extern "C"
void
__tgt_register_requires(int64_t Flags)
{
    LOGGER_NOT_IMPLEMENTED();
}

extern "C"
void
__tgt_register_lib(__tgt_bin_desc * Desc)
{
    LOGGER_NOT_IMPLEMENTED();
    # if 0
    initRuntime();
    if (PM->delayRegisterLib(Desc))
        return;

    PM->registerLib(Desc);
    # endif
}
