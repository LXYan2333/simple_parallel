add_library(s_p_c_binding
    c_binding.cc
    c_dynamic_schedule_binding.cc
)
add_library(simple_parallel::s_p_c_binding ALIAS s_p_c_binding)

target_sources(s_p_c_binding
    PUBLIC
    FILE_SET public_header
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/c/c_binding.h
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/c/c_dynamic_schedule_binding.h
)

target_link_libraries(s_p_c_binding
    PUBLIC
    MPI::MPI_C
    Boost::headers # for boost preprocessor
    $<TARGET_NAME_IF_EXISTS:Boost::preprocessor>
    
    PRIVATE
    Microsoft.GSL::GSL
    s_p_init
    s_p_options
    s_p_warings
)

set_target_properties(s_p_c_binding
    PROPERTIES
    C_VISIBILITY_PRESET hidden
    INTERFACE_LINK_LIBRARIES_DIRECT $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:simple_parallel::mimalloc-obj>
)

target_compile_features(s_p_c_binding
    PUBLIC
    c_std_11
)

