# Copyright (c) 2017, Intel Corporation.

include(cmake/jerry.cmake)

if(ASHELL)
  add_definitions(-DASHELL=${ASHELL})
endif()

if(BLE_ADDR)
  add_definitions(-DZJS_CONFIG_BLE_ADDRESS="${BLE_ADDR}")
endif()

if(CB_STATS)
  add_definitions(-DCB_STATS=${CB_STATS})
endif()

if(NETWORK_BUILD)
  add_definitions(-DNETWORK_BUILD=${NETWORK_BUILD})
endif()

if(PRINT_FLOAT)
  add_definitions(-DPRINT_FLOAT=${PRINT_FLOAT})
endif()

if("${VARIANT}" STREQUAL "debug")
  add_definitions(-DDEBUG_BUILD -DOC_DEBUG)
endif()

if(ZJS_FLAGS)
  set(ZJS_FLAGS_LIST ${ZJS_FLAGS})
  separate_arguments(ZJS_FLAGS_LIST)
  add_definitions(${ZJS_FLAGS_LIST})
endif()

target_compile_options(app PRIVATE
  -Wall
  -Wno-implicit-function-declaration
  )

target_include_directories(app PRIVATE ./src)
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/drivers)
target_include_directories(app PRIVATE ${JERRY_BASE}/jerry-core/include)
target_include_directories(app PRIVATE ${JERRY_BASE}/jerry-core/jrt)
target_include_directories(app PRIVATE ${JERRY_BASE}/jerry-ext/include)
target_include_directories(app PRIVATE ${CMAKE_BINARY_DIR}/../include)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/zjs_callbacks.c)
target_sources(app PRIVATE src/zjs_common.c)
target_sources(app PRIVATE src/zjs_error.c)
target_sources(app PRIVATE src/zjs_modules.c)
target_sources(app PRIVATE src/zjs_script.c)
target_sources(app PRIVATE src/zjs_timers.c)
target_sources(app PRIVATE src/zjs_util.c)
target_sources(app PRIVATE src/jerry-port/zjs_jerry_port.c)

target_link_libraries(app jerry-core jerry-ext)

# additional configuration will be generated by analyze script
include(${CMAKE_BINARY_DIR}/generated.cmake)
