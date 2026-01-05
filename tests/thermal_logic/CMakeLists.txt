cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(thermal_logic_test)

target_sources(app PRIVATE 
    ../../lib/thermal_manager.c
    src/main.c
)
target_include_directories(app PRIVATE ../../include)
