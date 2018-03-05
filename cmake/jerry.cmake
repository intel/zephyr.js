# Copyright (c) 2017, Intel Corporation.

include(ExternalProject)

# Additional build flags to work around JerryScript warnings
set(jerry_cflags " \
 -Wall \
 -Werror \
 -Wno-conversion \
 -Wno-implicit-function-declaration \
 -Wno-old-style-declaration \
 -Wno-undef"
)

zephyr_get_include_directories_for_lang_as_string(C includes)
zephyr_get_system_include_directories_for_lang_as_string(C system_includes)
zephyr_get_compile_definitions_for_lang_as_string(C definitions)
zephyr_get_compile_options_for_lang_as_string(C options)

set(external_project_cflags
  "${includes} ${definitions} ${options} ${system_includes}${jerry_cflags}"
  )

if("${SNAPSHOT}" STREQUAL "on")
  set(JERRY_LIBDIR ${JERRY_OUTPUT}/snapshot)
else()
  set(JERRY_LIBDIR ${JERRY_OUTPUT}/parser)
endif()

set(CMAKE_ARGS
  -B${JERRY_LIBDIR}
  -DCMAKE_BUILD_TYPE=MinSizeRel
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  -DCMAKE_C_COMPILER_WORKS=TRUE
  -DCMAKE_SYSTEM_NAME=Zephyr
  -DENABLE_ALL_IN_ONE=${ALL_IN_ONE}
  -DENABLE_LTO=OFF
  -DEXTERNAL_COMPILE_FLAGS=${external_project_cflags}
  -DFEATURE_ERROR_MESSAGES=ON
  -DFEATURE_INIT_FINI=ON
  -DFEATURE_PROFILE=${JERRY_PROFILE}
  -DFEATURE_SNAPSHOT_EXEC=OFF
  -DFEATURE_VALGRIND=OFF
  -DJERRY_CMDLINE=OFF
  -DJERRY_LIBC=OFF
  -DJERRY_LIBM=OFF
  -DJERRY_PORT_DEFAULT=OFF
  -DMEM_HEAP_SIZE_KB=${JERRY_HEAP}
  -H${JERRY_BASE}
)

if("${SNAPSHOT}" STREQUAL "on")
  list(APPEND CMAKE_ARGS
    -DFEATURE_JS_PARSER=OFF
  )
else()
  list(APPEND CMAKE_ARGS
    -DFEATURE_JS_PARSER=ON
  )
endif()

# build libjerry as a static library
ExternalProject_Add(
  jerry_project
  PREFIX     "deps/jerryscript"
  SOURCE_DIR ${JERRY_BASE}
  BINARY_DIR ${JERRY_LIBDIR}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ${CMAKE_COMMAND} ${CMAKE_ARGS}
  COMMAND cd ${JERRY_LIBDIR}
  COMMAND make jerry-core jerry-ext -j4
  INSTALL_COMMAND ""
  )

add_library(jerry-core STATIC IMPORTED)
add_dependencies(jerry-core jerry_project)
set_target_properties(jerry-core PROPERTIES IMPORTED_LOCATION ${JERRY_LIBDIR}/lib/libjerry-core.a)

add_library(jerry-ext STATIC IMPORTED)
add_dependencies(jerry-ext jerry_project)
set_target_properties(jerry-ext PROPERTIES IMPORTED_LOCATION ${JERRY_LIBDIR}/lib/libjerry-ext.a)
