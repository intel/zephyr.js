subdir-ccflags-y += -D__ZEPHYR__
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_ARRAY_BUILTIN
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_BOOLEAN_BUILTIN
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_ERROR_BUILTINS
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_JSON_BUILTIN
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_MATH_BUILTIN
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_NUMBER_BUILTIN
subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_STRING_BUILTIN
subdir-ccflags-y += -DJERRY_ENABLE_SNAPSHOT_EXEC
subdir-ccflags-y += -DJERRY_ENABLE_SNAPSHOT_SAVE
subdir-ccflags-y += -DJERRY_NDEBUG
subdir-ccflags-y += -DJERRY_DISABLE_HEAVY_DEBUG
subdir-ccflags-y += -DMEM_STATS
subdir-ccflags-y += -DCONFIG_MEM_HEAP_AREA_SIZE="22768"

obj-y += ../../../deps/jerryscript/jerry-core/
obj-y += ../../../zjs_api/src/

ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core
ccflags-y += -I$(src)/../../../zjs_api/src

obj-y += blink.o
obj-y += ../../../deps/jerryscript/jerry-core/lib.a
obj-y += ../../../zjs_api/src/lib.a
