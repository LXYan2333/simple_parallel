# detect page size. this needs to be a compile-time constant for performance
add_executable(detect_page_size detect_page_size.cc)
target_link_libraries(detect_page_size PRIVATE s_p_options)
add_custom_command(
    OUTPUT page_size.h
    COMMAND detect_page_size ${CMAKE_CURRENT_BINARY_DIR}/page_size.h
    DEPENDS detect_page_size
    VERBATIM
)


add_library(s_p_init
    init.cc
    leader.cc
    worker.cc
    bigmpi.cc
    pagemap.cc
    $<$<BOOL:SIMPLE_PARALLEL_OVERRIDE_MAIN>:override_main.cc>
)

target_sources(s_p_init
    PRIVATE
    FILE_SET private_header
    TYPE HEADERS
    BASE_DIRS include
    FILES 
    include/bigmpi.h
    include/fake_main.h
    include/init.h
    include/leader.h
    include/worker.h
    include/pagemap.h
    include/internal_types.h

    PRIVATE
    FILE_SET private_header
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/page_size.h

    PUBLIC
    FILE_SET public_header
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/cxx/simple_parallel.h
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/cxx/types_fwd.h
)

target_include_directories(s_p_init
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(s_p_init
    PUBLIC
    Boost::mpi
    
    PRIVATE
    Boost::thread
    s_p_options
    s_p_warings
    mimalloc-obj
    Microsoft.GSL::GSL
    hwy
)

set_target_properties(s_p_init
    PROPERTIES
    CXX_VISIBILITY_PRESET hidden
)

# due to https://github.com/boostorg/mpi/pull/160 these definitions is manually added
target_compile_definitions(s_p_init
    PRIVATE
    MPICH_SKIP_MPICXX
    OMPI_SKIP_MPICXX
    _MPICC_H
)

if(SIMPLE_PARALLEL_OVERRIDE_MAIN)
    target_link_options(s_p_init
        INTERFACE
        "LINKER:--wrap=__libc_start_main"
    )
endif()

configure_file(
    s_p_launcher.sh.in
    s_p_launcher.sh
    FILE_PERMISSIONS 
    OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    COPYONLY
)

include(GNUInstallDirs)
install(PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/s_p_launcher.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)