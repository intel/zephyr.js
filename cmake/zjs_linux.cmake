# Copyright (c) 2017, Intel Corporation.

project(NONE C)
include(ExternalProject)

set(JERRY_BASE ./deps/jerryscript)
set(IOTC_BASE ./deps/iotivity-constrained)

set(JERRY_HEAP 16)
set(JERRY_PROFILE es2015-subset)

set(JERRY_LIBDIR ${CMAKE_BINARY_DIR}/jerry)

# define the modules that will be pulled into the linux build
set(LINUX_MODULES "zjs_board.json, zjs_buffer.json, zjs_console.json, zjs_event.json, zjs_gpio.json,")

# Only build OCF on linux, until iotivity-constrained is fixed on Mac
if(NOT APPLE)
  set(LINUX_MODULES "${LINUX_MODULES} zjs_iotivity_constrained.json, zjs_ocf.json,")
endif()

set(LINUX_MODULES "${LINUX_MODULES} zjs_performance.json, zjs_promise.json, zjs_test_callbacks.json, zjs_test_promise.json")

set(APP_SRC
  ${CMAKE_SOURCE_DIR}/src/main.c
  ${CMAKE_SOURCE_DIR}/src/zjs_board.c
  ${CMAKE_SOURCE_DIR}/src/zjs_buffer.c
  ${CMAKE_SOURCE_DIR}/src/zjs_callbacks.c
  ${CMAKE_SOURCE_DIR}/src/zjs_common.c
  ${CMAKE_SOURCE_DIR}/src/zjs_console.c
  ${CMAKE_SOURCE_DIR}/src/zjs_error.c
  ${CMAKE_SOURCE_DIR}/src/zjs_event.c
  ${CMAKE_SOURCE_DIR}/src/zjs_gpio.c
  ${CMAKE_SOURCE_DIR}/src/zjs_gpio_mock.c
  ${CMAKE_SOURCE_DIR}/src/zjs_linux_ring_buffer.c
  ${CMAKE_SOURCE_DIR}/src/zjs_linux_time.c
  ${CMAKE_SOURCE_DIR}/src/zjs_modules.c
  ${CMAKE_SOURCE_DIR}/src/zjs_performance.c
  ${CMAKE_SOURCE_DIR}/src/zjs_script.c
  ${CMAKE_SOURCE_DIR}/src/zjs_timers.c
  ${CMAKE_SOURCE_DIR}/src/zjs_test_promise.c
  ${CMAKE_SOURCE_DIR}/src/zjs_test_callbacks.c
  ${CMAKE_SOURCE_DIR}/src/zjs_unit_tests.c
  ${CMAKE_SOURCE_DIR}/src/zjs_util.c
  ${CMAKE_SOURCE_DIR}/src/jerry-port/zjs_jerry_port.c
  )

set(APP_INCLUDES
  ${CMAKE_BINARY_DIR}/../include
  ${CMAKE_SOURCE_DIR}/src
  ${JERRY_BASE}/jerry-core/include
  ${JERRY_BASE}/jerry-core/jrt
  ${JERRY_BASE}/jerry-ext/include
  )

add_definitions(
  -DBUILD_MODULE_BOARD
  -DBUILD_MODULE_BUFFER
  -DBUILD_MODULE_EVENTS
  -DBUILD_MODULE_PERFORMANCE
  -DBUILD_MODULE_CONSOLE
  -DBUILD_MODULE_TEST_PROMISE
  -DBUILD_MODULE_PROMISE
  -DBUILD_MODULE_TEST_CALLBACKS
  -DBUILD_MODULE_A101
  -DBUILD_MODULE_GPIO
  -DENABLE_INIT_FINI
  -DJERRY_PORT_ENABLE_JOBQUEUE
  -DZJS_LINUX_BUILD
  -DZJS_GPIO_MOCK
  -DZJS_FIND_FUNC_NAME
  -DZJS_PRINT_FLOATS
  )

set(APP_COMPILE_OPTIONS
  -Wall
  -Os
  -std=gnu99
  )

if(CB_STATS)
  add_definitions(-DCB_STATS=${CB_STATS})
endif()

if(DEBUGGER)
  add_definitions(-DZJS_DEBUGGER=${ZJS_DEBUGGER})
endif()

if("${VARIANT}" STREQUAL "debug")
  add_definitions(-DDEBUG_BUILD -DOC_DEBUG)
  list(APPEND APP_COMPILE_OPTIONS -g)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/debug)
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release)
endif()

if(ZJS_FLAGS)
  set(ZJS_FLAGS_LIST ${ZJS_FLAGS})
  separate_arguments(ZJS_FLAGS_LIST)
  add_definitions(${ZJS_FLAGS_LIST})
endif()

# Only build OCF on linux, until iotivity-constrained is fixed on Mac
if(NOT APPLE)
  file(GLOB IOTC_SRC
    ${IOTC_BASE}/api/*.c
    ${IOTC_BASE}/messaging/coap/*.c
    ${IOTC_BASE}/port/linux/*.c
    ${IOTC_BASE}/util/*.c
    )

  list(APPEND APP_SRC
    ${CMAKE_SOURCE_DIR}/src/zjs_ocf_common.c
    ${CMAKE_SOURCE_DIR}/src/zjs_ocf_client.c
    ${CMAKE_SOURCE_DIR}/src/zjs_ocf_server.c
    ${IOTC_BASE}/deps/tinycbor/src/cborencoder.c
    ${IOTC_BASE}/deps/tinycbor/src/cborencoder_close_container_checked.c
    ${IOTC_BASE}/deps/tinycbor/src/cborparser.c
    ${IOTC_SRC}
    )

  list(APPEND APP_INCLUDES
    ${CMAKE_BINARY_DIR}/../include
    ${IOTC_BASE}
    ${IOTC_BASE}/deps/tinycbor/src
    ${IOTC_BASE}/deps/tinydtls
    ${IOTC_BASE}/api
    ${IOTC_BASE}/include
    ${IOTC_BASE}/messaging/coap
    ${IOTC_BASE}/util
    )

  add_definitions(
      -DOC_SERVER
      -DOC_CLIENT
      -DOC_DYNAMIC_ALLOCATION
      -DOC_CLOCK_CONF_TICKS_PER_SECOND=1000
      -DMAX_APP_DATA_SIZE=1024
      -DBUILD_MODULE_OCF
      -DZJS_GPIO_MOCK
      )

  # these flags are needed to get rid of warnings in iotivity-constrained
  list(APPEND APP_COMPILE_OPTIONS -Wno-pointer-sign)
endif()

# build libjerry as a static library
add_custom_command(
  OUTPUT
    ${JERRY_LIBDIR}/lib/libjerry-core.a
    ${JERRY_LIBDIR}/lib/libjerry-ext.a
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/analyze
    --board=linux
    --output-dir=${CMAKE_BINARY_DIR}/..
    --restrict=${LINUX_MODULES}
    --verbose=${V}
    --json-dir=${CMAKE_SOURCE_DIR}/src/
  COMMAND ${PYTHON_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/deps/jerryscript/tools/build.py
    --builddir=${JERRY_LIBDIR}
    --error-messages=ON
    --mem-heap=${JERRY_HEAP}
    --profile=${JERRY_PROFILE}
    --jerry-port-default=OFF
    --jerry-cmdline=OFF
    --jerry-libc=OFF
    --jerry-debugger=${DEBUGGER}
  )

add_executable(jslinux ${APP_SRC})
target_include_directories(jslinux PRIVATE ${APP_INCLUDES})
target_compile_options(jslinux PRIVATE ${APP_COMPILE_OPTIONS})

add_custom_target(jerry_libs DEPENDS
  ${JERRY_LIBDIR}/lib/libjerry-core.a
  ${JERRY_LIBDIR}/lib/libjerry-ext.a
  )

add_library(jerry-core STATIC IMPORTED)
add_dependencies(jerry-core jerry_libs)
set_target_properties(jerry-core PROPERTIES IMPORTED_LOCATION ${JERRY_LIBDIR}/lib/libjerry-core.a)

add_library(jerry-ext STATIC IMPORTED)
add_dependencies(jerry-ext jerry_libs)
set_target_properties(jerry-ext PROPERTIES IMPORTED_LOCATION ${JERRY_LIBDIR}/lib/libjerry-ext.a)

target_link_libraries(jslinux jerry-core jerry-ext m pthread)
