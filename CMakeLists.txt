# Copyright (c) 2017, Intel Corporation.

if(NOT DEFINED ENV{ZEPHYR_BASE})
  message(FATAL_ERROR "Missing Zephyr base, did you source zephyr-env.sh?")
endif()
set(ZEPHYR_BASE $ENV{ZEPHYR_BASE})

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

set_ifndef(JERRY_BASE ./deps/jerryscript)
set_ifndef(IOTC_BASE ./deps/iotivity-constrained)

# build in src directory if not set
set_ifndef(JERRY_OUTPUT ${JERRY_BASE})

set_ifndef(BOARD arduino_101)
set(ENV{BOARD} ${BOARD})

set_ifndef(JERRY_HEAP 16)

set_ifndef(JERRY_PROFILE minimal)

if("${BOARD}" STREQUAL "arduino_101")
  # ALL-IN-ONE build, slightly shrink build size, may not work on all platforms
  set(ALL_IN_ONE ON)
else()
  set(ALL_IN_ONE OFF)
endif()

if(VERBOSITY)
  set(VERBOSE BOOL=TRUE)
else()
  set(VERBOSE BOOL=FALSE)
endif()

if("${SNAPSHOT}" STREQUAL "on")
  set(JS_PARSER OFF)
else()
  set(JS_PARSER ON)
endif()

include(cmake/zjs.cmake)
