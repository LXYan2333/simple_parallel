include(cmake/SystemLink.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)

macro(simple_parallel_setup_options)

    option(simple_parallel_PACKAGING_MAINTAINER_MODE "PACKAGING_MAINTAINER_MODE" OFF)
    

    if(NOT PROJECT_IS_TOP_LEVEL OR simple_parallel_PACKAGING_MAINTAINER_MODE)
        option(simple_parallel_ENABLE_IPO "Enable IPO/LTO" OFF)
        option(simple_parallel_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
        option(simple_parallel_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
        option(simple_parallel_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
        option(simple_parallel_ENABLE_CACHE "Enable ccache" OFF)
        option(simple_parallel_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
        option(simple_parallel_ENABLE_TESTING "Enable Testing" OFF)
        option(CPM_LOCAL_PACKAGES_ONLY "forward CPM to find_package REQUIRED" ON)
        option(simple_parallel_MPI_BIG_COUNT "use MPI 4.0's bigcount specification" OFF)
        set(simple_parallel_DEFAULT_C_TASK_BUFFER_SIZE "64" CACHE STRING "Default task buffer size for c omp dynamic schedule")
    else()
        option(simple_parallel_ENABLE_IPO "Enable IPO/LTO" ON)
        option(simple_parallel_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
        option(simple_parallel_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
        option(simple_parallel_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
        option(simple_parallel_ENABLE_CACHE "Enable ccache" ON)
        option(simple_parallel_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
        option(simple_parallel_ENABLE_TESTING "Enable Testing" ON)
        option(CPM_LOCAL_PACKAGES_ONLY "forward CPM to find_package REQUIRED" OFF)
        option(simple_parallel_MPI_BIG_COUNT "use MPI 4.0's bigcount specification" OFF)
        set(simple_parallel_DEFAULT_C_TASK_BUFFER_SIZE "64" CACHE STRING "Default task buffer size for c omp dynamic schedule")
    endif()

    if(NOT PROJECT_IS_TOP_LEVEL)
        mark_as_advanced(
        simple_parallel_ENABLE_IPO
        simple_parallel_WARNINGS_AS_ERRORS
        simple_parallel_ENABLE_USER_LINKER
        simple_parallel_ENABLE_CLANG_TIDY
        simple_parallel_ENABLE_CACHE
        simple_parallel_ENABLE_CPPCHECK)
    endif()

endmacro()

macro(simple_parallel_global_options)

    if(simple_parallel_ENABLE_IPO)
        include(cmake/InterproceduralOptimization.cmake)
        simple_parallel_enable_ipo()
    endif()

    if(simple_parallel_ENABLE_USER_LINKER)
        include(cmake/Linker.cmake)
        simple_parallel_configure_linker(simple_parallel_options)
    endif()

endmacro()

macro(simple_parallel_local_options)

    if(PROJECT_IS_TOP_LEVEL)
        include(cmake/StandardProjectSettings.cmake)
    endif()

    add_library(simple_parallel_warnings INTERFACE)
    add_library(simple_parallel_options INTERFACE)

    include(cmake/CompilerWarnings.cmake)
    simple_parallel_set_project_warnings(
        simple_parallel_warnings
        ${simple_parallel_WARNINGS_AS_ERRORS}
        ""
        ""
        ""
        "")

    if(simple_parallel_ENABLE_CACHE)
        include(cmake/Cache.cmake)
        simple_parallel_enable_cache()
    endif()

    include(cmake/StaticAnalyzers.cmake)
    if(simple_parallel_ENABLE_CLANG_TIDY)
        simple_parallel_enable_clang_tidy(simple_parallel_options ${simple_parallel_WARNINGS_AS_ERRORS})
    endif()

    if(simple_parallel_ENABLE_CPPCHECK)
        simple_parallel_enable_cppcheck(${simple_parallel_WARNINGS_AS_ERRORS} "" # override cppcheck options
        )
    endif()

    if(simple_parallel_WARNINGS_AS_ERRORS)
        check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
        if(LINKER_FATAL_WARNINGS)
        # This is not working consistently, so disabling for now
        # target_link_options(simple_parallel_options INTERFACE -Wl,--fatal-warnings)
        endif()
    endif()

endmacro()
