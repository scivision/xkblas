# CMake Minimum version
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKOMP xkomp)

# Project name
project(${XKOMP})

###############
# DEPENDENCES #
###############

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/api.cc
    ${CMAKE_SOURCE_DIR}/src/task.cc
    ${CMAKE_SOURCE_DIR}/src/target.cc
)

# XKOMP version
add_custom_command(
    OUTPUT  always-run
    COMMAND python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkomp xkomp-version
    COMMENT "Always generate version/support file before building the project"
)
add_custom_target(always_generate_files ALL DEPENDS always-run)

#########
# XKOMP #
#########

# XKOMP Lib
add_library(${XKOMP} SHARED ${SOURCE_FILES})
add_custom_target( omp  ALL COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libomp.so)
add_custom_target(iomp5 ALL COMMAND ${CMAKE_COMMAND} -E create_symlink lib${XKOMP}.so libiomp5.so)
add_dependencies(${XKOMP} always_generate_files)

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKOMP} DESTINATION bin)

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")
target_link_libraries(${XKOMP} PRIVATE ${XKRT_LIBRARIES})

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
