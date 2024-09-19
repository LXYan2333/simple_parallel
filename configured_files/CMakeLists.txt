configure_file("simple_parallel_config.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/simple_parallel_config.h" ESCAPE_QUOTES)

if(NOT TARGET concurrentqueue::concurrentqueue)
    add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)
endif()

# after cmake 3.29: Alias targets to imported libraries are also supported in try_compile.
# add these code to compatible with cmake<3.29
get_property(aliased_target TARGET concurrentqueue::concurrentqueue PROPERTY ALIASED_TARGET)
if(aliased_target)
    set(concurrentqueue_link_name concurrentqueue)
else()
    set(concurrentqueue_link_name concurrentqueue::concurrentqueue)
endif()

try_compile(
    include_moodycamel_concurrentqueue
    SOURCES ${PROJECT_SOURCE_DIR}/src/try_compile/test_include_moodycamel_concurrentqueue.cc
    LINK_LIBRARIES ${concurrentqueue_link_name})

# since `try_compile` only works for imported target, if the target is added by `add_library` or `add_subdirectory`,
# the `try_compile` will always fail. since the `concurrentqueue` target always exists, we just assume the header 
# is included as `#include <concurrentqueue.h>`
if(NOT include_moodycamel_concurrentqueue)
    set(include_concurrentqueue true)
endif()

configure_file("include_concurrentqueue.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/include_concurrentqueue.h" ESCAPE_QUOTES)
configure_file("c_omp_dynamic_schedule_buffer_size_config.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/c_omp_dynamic_schedule_buffer_size_config.h" ESCAPE_QUOTES)