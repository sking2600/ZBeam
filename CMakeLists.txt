cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zlight)

target_sources(app PRIVATE 
    src/main.c
    lib/multi_tap_input.c
)
target_include_directories(app PRIVATE include)