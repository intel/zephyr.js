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
subdir-ccflags-y += -DCONFIG_MEM_HEAP_AREA_SIZE="39280"

obj-y += ../deps/jerryscript/jerry-core/

ccflags-y += -I$(src)/../deps/jerryscript/jerry-core -I$(src)/../build
ccflags-y += -I$(srctree)/drivers

obj-y += main.o \
         zjs_aio.o \
         zjs_gpio.o \
         zjs_ipm.o \
         zjs_modules.o \
         zjs_pwm.o \
         zjs_timers.o \
         zjs_util.o

obj-y += ../deps/jerryscript/jerry-core/lib.a
