add_library(s_p_dynamic_patch_elf SHARED
    patch_elf.cc
)

target_link_libraries(s_p_dynamic_patch_elf
    PUBLIC
    ${CMAKE_DL_LIBS}
    PRIVATE
    s_p_warings
    s_p_options
    mimalloc-obj
)

target_sources(s_p_dynamic_patch_elf
    PRIVATE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../simple_parallel/include
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/../simple_parallel/include/fake_main.h
)