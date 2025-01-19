add_library(s_p_c_binding
    c_binding.cc
)

target_sources(s_p_c_binding
    PUBLIC
    FILE_SET public_header
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/c/c_binding.h
)

target_link_libraries(s_p_c_binding
    PUBLIC
    MPI::MPI_C
    
    PRIVATE
    s_p_init
    s_p_options
    s_p_warings
    Boost::headers
)

set_target_properties(s_p_c_binding
    PROPERTIES
    C_VISIBILITY_PRESET hidden
)

