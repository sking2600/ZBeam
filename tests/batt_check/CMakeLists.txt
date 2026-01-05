cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(batt_check_test)

# Add the battery check source directly for testing
target_sources(app PRIVATE 
    ../../src/batt_check.c
    src/main.c
)
target_include_directories(app PRIVATE ../../include)
