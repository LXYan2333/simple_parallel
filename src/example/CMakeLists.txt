add_executable(cxx_example
    cxx_example.cc
)
target_link_libraries(cxx_example
    PRIVATE
    s_p_init
    mimalloc-obj
)