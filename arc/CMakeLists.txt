# Copyright (c) 2017, Intel Corporation.

cmake_minimum_required(VERSION 3.8.2)

if(NOT DEFINED ENV{ZEPHYR_BASE})
  message(FATAL_ERROR "Missing Zephyr base, did you source zephyr-env.sh?")
endif()
set(ZEPHYR_BASE $ENV{ZEPHYR_BASE})

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

target_compile_options(app PRIVATE -Wall)

target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../src)
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/drivers)

target_sources(app PRIVATE src/main.c)

include(${CMAKE_BINARY_DIR}/generated.cmake)
