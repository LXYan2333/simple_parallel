add_executable(cxx_example
    cxx_example.cc
)
target_link_libraries(cxx_example
    PRIVATE
    s_p_init
    mimalloc-obj
)

add_executable(c_example
    c_example.c
)
target_link_libraries(c_example
    PRIVATE
    s_p_c_binding
    mimalloc-obj
)