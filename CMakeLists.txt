cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ZBeam)

# Core application sources
target_sources(app PRIVATE 
    src/main.c
    src/key_map.c
    lib/fsm_engine.c
    lib/fsm_worker.c
    lib/multi_tap_input.c
    lib/storage_manager.c
    lib/safety_monitor.c
)

# Platform-specific PWM ramp implementation
if(CONFIG_PWM_RAMP_ESP32_LEDC_INTERPOLATION)
    target_sources(app PRIVATE src/pwm_ramp_esp32.c)
elseif(CONFIG_PWM_RAMP_DMA)
    target_sources(app PRIVATE src/pwm_ramp_dma.c)
else()
    target_sources(app PRIVATE src/pwm_ramp_generic.c)
endif()

target_include_directories(app PRIVATE include)