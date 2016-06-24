ifeq ("$(origin TEST)", "command line")
	KBUILD_TEST = $(TEST)
endif

subdir-ccflags-y += -D__ZEPHYR__
subdir-ccflags-y += -DJERRY_ENABLE_SNAPSHOT_EXEC
subdir-ccflags-y += -DJERRY_ENABLE_SNAPSHOT_SAVE
subdir-ccflags-y += -DJERRY_NDEBUG
subdir-ccflags-y += -DJERRY_DISABLE_HEAVY_DEBUG
subdir-ccflags-y += -DMEM_STATS
subdir-ccflags-y += -DCONFIG_MEM_HEAP_AREA_SIZE="22768"

ifeq ($(KBUILD_TEST), date-helpers)
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_ARRAY_BUILTIN
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_BOOLEAN_BUILTIN
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_ERROR_BUILTINS
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_JSON_BUILTIN
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_MATH_BUILTIN
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_NUMBER_BUILTIN
	subdir-ccflags-y += -DCONFIG_ECMA_COMPACT_PROFILE_DISABLE_STRING_BUILTIN
endif

obj-y += ../../../deps/jerryscript/jerry-core/

ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/lit
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/rcs
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/mem
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/vm
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/ecma/builtin-objects
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/ecma/base
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/ecma/operations
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/parser/js
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/parser/js/collections
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/parser/regexp
ccflags-y += -I$(src)/../../../deps/jerryscript/jerry-core/jrt
ccflags-y += -I$(src)/../../../jerryscript/third-party/fdlibm/include

test_dir = ../../../deps/jerryscript/tests/unit

ifeq ($(KBUILD_TEST), api)
	subdir-ccflags-y += -DCONFIG_ECMA_NUMBER_TYPE=CONFIG_ECMA_NUMBER_FLOAT64
	obj-y += $(test_dir)/test-api.o
else
ifeq ($(KBUILD_TEST), date-helpers)
	obj-y += $(test_dir)/test-date-helpers.o
else
ifeq ($(KBUILD_TEST), libm)
	obj-y += $(test_dir)/test-libm.o
else
ifeq ($(KBUILD_TEST), heap)
	obj-y += $(test_dir)/test-heap.o
else
ifeq ($(KBUILD_TEST), lit-char-helpers)
	obj-y += $(test_dir)/test-lit-char-helpers.o
else
ifeq ($(KBUILD_TEST), literal-storage)
	obj-y += $(test_dir)/test-literal-storage.o
else
ifeq ($(KBUILD_TEST), longjmp)
	obj-y += $(test_dir)/test-longjmp.o
else
ifeq ($(KBUILD_TEST), number-to-integer)
	obj-y += $(test_dir)/test-number-to-integer.o
else
ifeq ($(KBUILD_TEST), number-to-string)
	obj-y += $(test_dir)/test-number-to-string.o
else
ifeq ($(KBUILD_TEST), poolman)
	obj-y += $(test_dir)/test-poolman.o
else
ifeq ($(KBUILD_TEST), strings)
	obj-y += $(test_dir)/test-strings.o
else
ifeq ($(KBUILD_TEST), string-to-number)
	obj-y += $(test_dir)/test-string-to-number.o
endif #test-string-to-number
endif #test-strings
endif #test-poolman
endif #test-number-to-string
endif #test-number-to-integer
endif #test-longjmp
endif #test-literal-storage
endif #test-lit-char-helpers
endif #test-heap
endif #test-libm
endif #test-date-helpers
endif #test-api

obj-y += ../../../deps/jerryscript/jerry-core/lib.a
