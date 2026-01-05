cmake_minimum_required(VERSION 3.20.0)
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../Kconfig)
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../boards/esp32c3_supermini.overlay)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(batt_check_test)

# Add the battery check source directly for testing
target_sources(app PRIVATE 
    ../../src/batt_check.c
    src/main.c
)
target_include_directories(app PRIVATE ../../include)
