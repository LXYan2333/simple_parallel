# detect page size. this needs to be a compile-time constant for performance
# a block is used to avoid `-march` flags being applied to the detect_page_size executable
block()
    add_executable(detect_page_size detect_page_size.cc)
    target_link_libraries(detect_page_size PRIVATE s_p_options)
    add_custom_command(
        OUTPUT page_size.h
        COMMAND detect_page_size ${CMAKE_CURRENT_BINARY_DIR}/page_size.h
        DEPENDS detect_page_size
        VERBATIM
    )
endblock()


add_library(s_p_init
    init.cc
    leader.cc
    worker.cc
    bigmpi.cc
    pagemap.cc
    $<$<BOOL:${SIMPLE_PARALLEL_OVERRIDE_MAIN}>:override_main.cc>
    $<$<BOOL:${SIMPLE_PARALLEL_Fortran_BINDING}>:sync_fortran_global_vars.cc>
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
    include/sync_fortran_global_vars.h

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
    ${PROJECT_SOURCE_DIR}/include/simple_parallel/cxx/lib_visibility.h
)

target_include_directories(s_p_init
    SYSTEM
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../mimalloc_for_simple_parallel/include/
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/configured_files/include
)

target_link_libraries(s_p_init
    PUBLIC
    Boost::headers
    Boost::container
    Boost::mpi
    MPI::MPI_CXX
    
    PRIVATE
    Boost::thread
    s_p_options
    s_p_warings
    Microsoft.GSL::GSL
    hwy
    $<$<BOOL:${SIMPLE_PARALLEL_Fortran_BINDING}>:re2::re2 PkgConfig::libelf ${CMAKE_DL_LIBS}>
)

if(NOT BOOST_IS_IMPORTED)
    target_link_libraries(s_p_init
        PUBLIC
        Boost::circular_buffer

        PRIVATE
        Boost::icl
    )
endif()

include(CheckCompilerFlag)
check_compiler_flag(CXX -fno-stack-protector S_P_CXX_HAS_NO_STACK_PROTECTOR)
if(S_P_CXX_HAS_NO_STACK_PROTECTOR)
    target_compile_options(s_p_init
        PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-stack-protector>
    )
endif()

if(CMAKE_C_COMPILER_LOADED)
    check_compiler_flag(C -fno-stack-protector S_P_C_HAS_NO_STACK_PROTECTOR)
    if(S_P_C_HAS_NO_STACK_PROTECTOR)
        target_compile_options(s_p_init
            PUBLIC $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
        )
    endif()
endif()

if(CMAKE_Fortran_COMPILER_LOADED)
    check_compiler_flag(Fortran -fno-stack-protector S_P_Fortran_HAS_NO_STACK_PROTECTOR)
    if(S_P_Fortran_HAS_NO_STACK_PROTECTOR)
        target_compile_options(s_p_init
            PUBLIC $<$<COMPILE_LANGUAGE:Fortran>:-fno-stack-protector>
        )
    endif()

    # static array can not be automatically synchronized, thus force the compiler to place them in the stack
    check_compiler_flag(Fortran -frecursive S_P_Fortran_HAS_RECURSIVE)
    if(S_P_Fortran_HAS_RECURSIVE)
        target_compile_options(s_p_init
            PUBLIC $<$<COMPILE_LANGUAGE:Fortran>:-frecursive>
        )
    endif()
endif()

set_target_properties(s_p_init
    PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    INTERFACE_LINK_LIBRARIES_DIRECT $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:simple_parallel::mimalloc-obj>
)

target_link_options(s_p_init
    PUBLIC
    $<$<BOOL:${SIMPLE_PARALLEL_OVERRIDE_MAIN}>:LINKER:--wrap=__libc_start_main>
    $<$<BOOL:${SIMPLE_PARALLEL_Fortran_BINDING}>:LINKER:--export-dynamic>
)

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