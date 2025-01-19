set(MI_BUILD_SHARED OFF)
set(MI_BUILD_STATIC OFF)
set(MI_BUILD_OBJECT ON)
add_subdirectory(mimalloc_for_simple_parallel SYSTEM)

add_subdirectory(dynamic_patch_elf)
add_subdirectory(simple_parallel)
if(SIMPLE_PARALLEL_C_BINDING)
    add_subdirectory(c_binding)
endif()

if(SIMPLE_PARALLEL_ENABLE_EXAMPLES)
    add_subdirectory(example)
endif()