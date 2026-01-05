cmake_minimum_required(VERSION 3.20.0)

# Point to main Kconfig
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../Kconfig)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(strobe_logic_test)

target_sources(app PRIVATE 
    ../../src/key_map.c
    ../../lib/fsm_engine.c
    ../../lib/multi_tap_input.c
    ../../src/batt_check.c
    src/main.c
)

target_include_directories(app PRIVATE ../../include)
