# Copyright (c) 2017, Intel Corporation.

cmake_minimum_required(VERSION 3.8.2)

if("${BOARD}" STREQUAL "linux")
  include (cmake/zjs_linux.cmake)
else()
  if(NOT DEFINED ENV{ZEPHYR_BASE})
    message(FATAL_ERROR "Missing Zephyr base, did you source zephyr-env.sh?")
  endif()
  set(ZEPHYR_BASE $ENV{ZEPHYR_BASE})

  include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
  project(NONE)

  include(cmake/zjs.cmake)
endif()
