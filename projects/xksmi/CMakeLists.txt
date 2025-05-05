# CMake Minimum version
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKSMI xksmi)

# Project name
project(${XKSMI})

###############
# DEPENDENCES #
###############

SET(SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/main.cc)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# XKSMI version
add_custom_command(
    OUTPUT  always-run
    COMMAND python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xksmi xksmi-version
    COMMENT "Always generate version/support file before building the project"
)
add_custom_target(always_generate_files ALL DEPENDS always-run)

########
# XKSMI #
########

# XKSMI Lib
add_executable(${XKSMI} ${SOURCE_FILES})
add_dependencies(${XKSMI} always_generate_files)

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKSMI} DESTINATION bin)

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")
target_link_libraries(${XKSMI} PRIVATE ${XKRT_LIBRARIES})

# ncurses
target_link_libraries(${XKSMI} PRIVATE ncurses)
