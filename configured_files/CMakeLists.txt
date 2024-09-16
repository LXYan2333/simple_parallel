configure_file("simple_parallel_config.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/simple_parallel_config.h" ESCAPE_QUOTES)

set(SIMPLE_PARALLEL_CONCURRENTQUEUE_INCLUDE_DIR "" CACHE STRING "due to https://github.com/cameron314/concurrentqueue/issues/384 the include path of concurrentqueue is not fixed. leave it blank to auto detect the actual path, set it to \"concurrentqueue\" to \#include <concurrentqueue.h>, set it to \"moodycamel_concurrentqueue\" to \#include <moodycamel/concurrentqueue.h>, or set it to \"concurrentqueue_concurrentqueue\" to \#include <concurrentqueue/concurrentqueue.h>")

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

if (SIMPLE_PARALLEL_CONCURRENTQUEUE_INCLUDE_DIR STREQUAL "")
    try_compile(
        include_concurrentqueue
        SOURCES ${PROJECT_SOURCE_DIR}/src/try_compile/test_include_concurrentqueue.cc
        LINK_LIBRARIES ${concurrentqueue_link_name})
    try_compile(
        include_concurrentqueue_concurrentqueue
        SOURCES ${PROJECT_SOURCE_DIR}/src/try_compile/test_include_concurrentqueue_concurrentqueue.cc
        LINK_LIBRARIES ${concurrentqueue_link_name})
    try_compile(
        include_moodycamel_concurrentqueue
        SOURCES ${PROJECT_SOURCE_DIR}/src/try_compile/test_include_moodycamel_concurrentqueue.cc
        LINK_LIBRARIES ${concurrentqueue_link_name})
elseif(SIMPLE_PARALLEL_CONCURRENTQUEUE_INCLUDE_DIR STREQUAL "concurrentqueue")
    set(include_concurrentqueue_concurrentqueue TRUE)
elseif (SIMPLE_PARALLEL_CONCURRENTQUEUE_INCLUDE_DIR STREQUAL "moodycamel_concurrentqueue")
    set(include_moodycamel_concurrentqueue TRUE)
elseif(SIMPLE_PARALLEL_CONCURRENTQUEUE_INCLUDE_DIR STREQUAL "concurrentqueue_concurrentqueue")
    set(include_concurrentqueue TRUE)
endif()

configure_file("include_concurrentqueue.h.in" "${CMAKE_BINARY_DIR}/configured_files/include/internal_use_only/include_concurrentqueue.h" ESCAPE_QUOTES)