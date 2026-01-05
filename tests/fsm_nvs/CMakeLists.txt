cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fsm_nvs_test)

# Add the main project's include directories
target_include_directories(app PRIVATE ../../include)

# Add the main project's source files directly
target_sources(app PRIVATE 
    ../../src/key_map.c
    ../../lib/fsm_engine.c
    ../../lib/nvs_manager.c
    # We do NOT include main.c as the test has its own main
    # We do NOT include multi_tap_input.c unless needed for linking, 
    # but fsm_engine might depend on it.
    ../../lib/multi_tap_input.c
)

target_sources(app PRIVATE 
    src/main.c 
    src/test_core_events.c
    src/test_multi_tap.c
)
