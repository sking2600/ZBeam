cmake_minimum_required(VERSION 3.20.0)

# Point to main Kconfig to get ZBEAM_* definitions
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../Kconfig)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fsm_core_test)

target_sources(app PRIVATE 
    ../../lib/fsm_engine.c
    src/main.c
)
target_include_directories(app PRIVATE ../../include)
