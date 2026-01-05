cmake_minimum_required(VERSION 3.20.0)

# Point to main Kconfig
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../Kconfig)
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../boards/esp32c3_supermini.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(strobe_logic_test)

target_sources(app PRIVATE 
    ../../src/ui_actions.c
    ../../lib/fsm_engine.c
    ../../lib/multi_tap_input.c
    ../../src/batt_check.c
    ../../lib/nvs_manager.c
    ../../lib/thermal_manager.c
    ../../lib/pm_manager.c
    ../../lib/aux_manager.c
    ../../src/channel_manager.c
    ../../src/pwm_ramp_generic.c
    src/main.c
)

target_include_directories(app PRIVATE ../../include)
