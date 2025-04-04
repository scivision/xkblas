# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Project name
SET(XKDNN xkdnn)
project(${XKDNN})

# XKRT version and supports
add_custom_command(
    OUTPUT  always-run
    COMMAND python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkdnn xkdnn-version
    COMMENT "Always generate version/support file before building the project"
)
add_custom_target(always_generate_files ALL DEPENDS always-run)

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/xkdnn.cc
)

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")

##########
# XKDNN #
##########

# XKDNN Lib
add_library(${XKDNN} SHARED ${SOURCE_FILES})
add_dependencies(${XKDNN} always_generate_files)
target_link_libraries(${XKDNN} PRIVATE ${XKRT_LIBRARIES})

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKDNN} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

option(STRICT "Enable strict compilation mode" ON)
if (STRICT)
    SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${XKDNN} PRIVATE ${STRICT_OPTIONS})
endif()

################
# DEPENDENCIES #
################

option(USE_CUDNN "Enable cudnn" ON)
if (USE_CUDNN)
    find_package(CUDNN REQUIRED)
    include_directories("${CUDNN_INCLUDE_DIRS}")
    target_link_libraries(${XKDNN} PRIVATE ${CUDNN_LIBRARIES})
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
