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
add_library(simple_parallel::s_p_init ALIAS s_p_init)

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
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/cxx/dynamic_schedule.h
)

target_include_directories(s_p_init
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(s_p_init
    PUBLIC
    Boost::headers
    Boost::mpi
    MPI::MPI_CXX
    
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